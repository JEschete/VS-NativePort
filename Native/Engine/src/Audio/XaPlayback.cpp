#include "Audio/XaPlayback.h"

#include <SDL3/SDL.h>
#include <dr_flac.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "Audio/AudioDevice.hpp"
#include "Audio/SoundControlLive.hpp"
#include "Audio/XaMix.hpp"
#include "Audio/XaMixLive.hpp"
#include "Content/AudioStream.hpp"
#include "Content/DataStore.hpp"
#include "Core/Log.hpp"
#include "Legacy/Psx.h"
#include "Platform/Paths.hpp"

extern "C" {
#include "Audio/Control/SoundControl.h"
#include "Audio/Driver/SoundDriver.h"
#include "DiscQueue.h"
}

namespace {
constexpr unsigned kMaximumVolume = 0x7FFF;
constexpr unsigned kMono = 2;
constexpr unsigned kInactiveMarker = 128;
constexpr unsigned kSampleBits = 16;
constexpr unsigned kMinimumRate = 8000;
constexpr unsigned kMaximumRate = 192000;
constexpr VS::usize kBlockFrames = 1024;
constexpr float kVolumeUnity = 32768;
constexpr float kMixUnity = 128;
constexpr unsigned kRouteBits = 8;
constexpr unsigned kRouteMask = 0xFF;
constexpr unsigned kVolumeShift = 32;
// Byte positions of the four routes in the packed controls.
constexpr unsigned kLeftToLeft = 0;
constexpr unsigned kLeftToRight = 1;
constexpr unsigned kRightToLeft = 2;
constexpr unsigned kRightToRight = 3;
std::uint32_t g_mix = VS::Audio::kDefaultDiscMixVolume;

// Volume in bits 32-47 and the four routes in bits 0-31, so the audio thread
// reads one consistent set.
std::uint64_t PackControls(std::int16_t volume,
                           const VS::Audio::DiscMixMatrix& mix) {
  return (static_cast<std::uint64_t>(static_cast<std::uint16_t>(volume))
          << kVolumeShift) |
         (static_cast<std::uint32_t>(mix.left_to_left)
          << (kRouteBits * kLeftToLeft)) |
         (static_cast<std::uint32_t>(mix.left_to_right)
          << (kRouteBits * kLeftToRight)) |
         (static_cast<std::uint32_t>(mix.right_to_left)
          << (kRouteBits * kRightToLeft)) |
         (static_cast<std::uint32_t>(mix.right_to_right)
          << (kRouteBits * kRightToRight));
}

class XaPlayback {
 public:
  bool Start(const VS::Content::DataStore& store,
             const VS::Content::DataStore::Entry& entry, unsigned sector) {
    Stop();
    auto* device = VS::Audio::AudioDevice::Primary();
    if (!device || !device->IsOpen()) {
      VS_LOG_ERROR("audio", "XA playback requires an open audio device");
      return false;
    }
    const auto path = VS::Content::XaAudioPath(store.Root(), entry.path);
    const std::unique_ptr<drflac, decltype(&drflac_close)> decoder(
        drflac_open_file_w(path.c_str(), nullptr), drflac_close);
    if (!decoder) {
      VS_LOG_ERROR("audio", "cannot decode XA FLAC {}; run VS.Extract streams",
                   VS::Platform::PathText(path));
      return false;
    }
    const auto frames = decoder->totalPCMFrameCount;
    const unsigned channels = decoder->channels;
    const unsigned rate = decoder->sampleRate;
    if (decoder->bitsPerSample != kSampleBits ||
        (channels != 1 && channels != 2) || rate < kMinimumRate ||
        rate > kMaximumRate || frames == 0 ||
        frames >
            std::numeric_limits<int>::max() / (channels * sizeof(VS::i16))) {
      VS_LOG_ERROR("audio", "XA asset must be nonempty mono/stereo PCM16: {}",
                   VS::Platform::PathText(path));
      return false;
    }
    m_channels_ = channels;
    m_frames_ = frames;
    const auto first_frame = static_cast<VS::u64>(sector - entry.lba) * rate /
                             VS::Content::kXaSectorsPerSecond;
    if (first_frame >= m_frames_) {
      VS_LOG_ERROR("audio", "XA sector {} is past decoded EOF: {}", sector,
                   VS::Platform::PathText(path));
      return false;
    }
    m_samples_.resize(frames * channels);
    if (drflac_read_pcm_frames_s16(decoder.get(), frames, m_samples_.data()) !=
        frames) {
      VS_LOG_ERROR("audio", "XA FLAC ended before its declared frame count: {}",
                   VS::Platform::PathText(path));
      Stop();
      return false;
    }
    m_position_ = first_frame;
    m_failed_ = false;
    m_stream_ = device->OpenSource(static_cast<int>(rate), 2);
    if (!m_stream_ ||
        !SDL_SetAudioStreamGetCallback(
            m_stream_,
            [](void* context, SDL_AudioStream*, int additional, int) {
              static_cast<XaPlayback*>(context)->Supply(additional);
            },
            this)) {
      VS_LOG_ERROR("audio", "starting XA output: {}", SDL_GetError());
      Stop();
      return false;
    }
    VS_LOG_INFO("audio", "XA start: {} sector={} rate={} channels={} frames={}",
                VS::Platform::PathText(path), sector, rate, m_channels_,
                m_frames_);
    return true;
  }

  void Stop() {
    // Destroying the stream waits for its callback before releasing the PCM.
    VS::Audio::AudioDevice::CloseSource(m_stream_);
    m_stream_ = nullptr;
    std::vector<VS::i16>().swap(m_samples_);
    m_frames_ = 0;
    m_position_ = 0;
    m_failed_ = false;
  }

  int Status() const {
    if (!m_stream_) return 0;
    if (m_failed_.load()) return -1;
    if (m_position_.load() < m_frames_) return 1;
    const int available = SDL_GetAudioStreamAvailable(m_stream_);
    const int queued = SDL_GetAudioStreamQueued(m_stream_);
    if (available < 0 || queued < 0) return -1;
    return available != 0 || queued != 0 ? 1 : 0;
  }

  void Configure(std::int16_t volume, const VS::Audio::DiscMixMatrix& mix) {
    m_controls_.store(PackControls(volume, mix));
  }

 private:
  void Supply(int additional) {
    std::array<VS::i16, kBlockFrames * 2> output{};
    const auto controls = m_controls_.load();
    const float gain =
        static_cast<float>(static_cast<VS::i16>(controls >> kVolumeShift)) /
        kVolumeUnity / kMixUnity;
    const auto route = [&](unsigned index) {
      return static_cast<float>((controls >> (kRouteBits * index)) &
                                kRouteMask) *
             gain;
    };
    const float left_to_left = route(kLeftToLeft);
    const float left_to_right = route(kLeftToRight);
    const float right_to_left = route(kRightToLeft);
    const float right_to_right = route(kRightToRight);
    auto position = m_position_.load();
    while (additional > 0 && position < m_frames_ && !m_failed_.load()) {
      const auto frames =
          std::min<VS::u64>(output.size() / 2, m_frames_ - position);
      for (VS::u64 frame = 0; frame < frames; ++frame) {
        const auto source = (position + frame) * m_channels_;
        const float input_left = m_samples_[source];
        const float input_right = m_samples_[source + m_channels_ - 1];
        const float left =
            input_left * left_to_left + input_right * right_to_left;
        const float right =
            input_left * left_to_right + input_right * right_to_right;
        constexpr float kMinimumSample = std::numeric_limits<VS::i16>::min();
        constexpr float kMaximumSample = std::numeric_limits<VS::i16>::max();
        output[frame * 2] = static_cast<VS::i16>(
            std::clamp(left, kMinimumSample, kMaximumSample));
        output[frame * 2 + 1] = static_cast<VS::i16>(
            std::clamp(right, kMinimumSample, kMaximumSample));
      }
      const auto count = static_cast<int>(frames * 2 * sizeof(VS::i16));
      if (!SDL_PutAudioStreamData(m_stream_, output.data(), count)) {
        m_failed_ = true;
        return;
      }
      position += frames;
      additional -= count;
      if (position == m_frames_ && !SDL_FlushAudioStream(m_stream_)) {
        m_failed_ = true;
        return;
      }
      m_position_ = position;
    }
  }

  SDL_AudioStream* m_stream_ = nullptr;
  std::vector<VS::i16> m_samples_;
  VS::u64 m_frames_ = 0;
  unsigned m_channels_ = 0;
  std::atomic<VS::u64> m_position_{0};
  std::atomic<std::uint64_t> m_controls_{0};
  std::atomic<bool> m_failed_{false};
};

XaPlayback& Playback() {
  static XaPlayback playback;
  return playback;
}
}  // namespace

// decomp: func_800443CC, diskReadXaInit (SLUS_010.40/main.c:9164).
// The second request field is initial CD volume, not a playback byte count.
int Audio_Native_StartXaStream(int sector, int volume) {
  const auto* store = VS::Content::DataStore::Get();
  const auto* entry = store && sector >= 0
                          ? store->FindBySector(static_cast<unsigned>(sector))
                          : nullptr;
  if (!entry || !entry->path.ends_with(".XA") || volume < 0 ||
      static_cast<unsigned>(volume) > kMaximumVolume) {
    VS_LOG_ERROR("audio", "invalid XA request: sector={} volume={}", sector,
                 volume);
    return 0;
  }
  VS::Audio::Control::LiveSetDiscVolume(volume);
  VS::Audio::Control::SetLiveDiscMixVolume(VS::Audio::kDefaultDiscMixVolume);
  return Playback().Start(*store, *entry, static_cast<unsigned>(sector)) ? 1
                                                                         : 0;
}

void Audio_Native_StopXaStream(void) {
  Playback().Stop();
  auto& drive = Content_discDriveState;
  if (drive.state == Content_DiscDriveState_ReadXaInit ||
      drive.state == Content_DiscDriveState_StreamXa ||
      drive.state == Content_DiscDriveState_StreamXaEnd) {
    Audio_Seq_discVolume = 0;
    Audio_Seq_discVolumeFadeLength = 0;
    drive.xaModeMarker = static_cast<char>(kInactiveMarker);
    drive.position.minute = kInactiveMarker;
    drive.state = Content_DiscDriveState_Idle;
  }
}

int Audio_Native_XaStreamStatus(void) {
  const int status = Playback().Status();
  if (status < 0) {
    VS_LOG_ERROR("audio", "XA output failed while supplying or draining PCM");
    Playback().Stop();
  }
  return status;
}

void Audio_Native_UpdateXaControls(void) {
  const bool mono = (Audio_Driver_controlLatches.mixBehavior & kMono) != 0;
  Playback().Configure(VS::Audio::Seq::WriteDiscVolume(Audio_Seq_discVolume),
                       VS::Audio::Control::SetDiscMixVolume(g_mix, mono)
                           .value_or(VS::Audio::DiscMixMatrix{}));
}

namespace VS::Audio::Control {
bool SetLiveDiscMixVolume(std::uint32_t volume) {
  if (!SetDiscMixVolume(volume, false)) {
    VS_LOG_ERROR("audio", "invalid XA mix attenuation: {}", volume);
    return false;
  }
  g_mix = volume;
  Audio_Native_UpdateXaControls();
  return true;
}
}  // namespace VS::Audio::Control

namespace VS::Audio::Seq {
void WriteLiveDiscVolume() { Audio_Native_UpdateXaControls(); }
}  // namespace VS::Audio::Seq
