// Hand-maintained (tools/port/owned.csv).
#include "Audio/StreamControlLive.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <utility>

#include "Audio/SequencerOutput.h"
#include "Audio/StreamBuffersLive.hpp"
#include "Audio/StreamControl.hpp"
#include "Audio/StreamPlayback.h"
#include "Audio/VoiceOutputLive.hpp"
#include "Content/AudioStream.hpp"
#include "Content/DataStore.hpp"
#include "Core/Log.hpp"
#include "Legacy/Psx.h"
#include "Platform/Paths.hpp"

extern "C" {
#include "Audio/Cutscene/Stream.h"
#include "Audio/Driver/SoundDriver.h"
#include "Audio/Mixer/MixControl.h"
#include "Audio/Mixer/VoiceMixer.h"
#include "Audio/SoundBanks.h"
}

namespace {
namespace Cutscene = VS::Audio::Cutscene;

constexpr int kRoomSlot = 2;
constexpr unsigned kVoiceModesDirty = 0x100;
constexpr unsigned kMonoOutput = 2;
constexpr int kStereoSource = 1;
constexpr int kMaximumVolume = static_cast<int>(Cutscene::kMaximumStreamVolume);

struct BoundStream {
  void* source = nullptr;
  std::size_t byte_count = 0;
  VS::Content::AudioStreamInfo info;
  std::array<std::string, Cutscene::kStreamChannelCount> instruments;
};
std::optional<BoundStream> g_bound;
std::optional<Cutscene::StreamGains> g_gains;

// Stream commands name their source by the pointer the room stream was bound
// with; the identity is compared and never read through.
Cutscene::StreamSourceId SourceIdOf(const void* source) {
  return static_cast<Cutscene::StreamSourceId>(
      reinterpret_cast<std::uintptr_t>(source));
}
int g_fade_target = kMaximumVolume;

// The driver's stream state (Audio_Cutscene_streamState), the sequencer's
// voice scheduler and the room stream slot of Audio_soundState.
class LiveStreamHost final : public Cutscene::StreamHost {
 public:
  explicit LiveStreamHost(void* data = nullptr, std::size_t byte_count = 0)
      : data_(data), byte_count_(byte_count) {}

  [[nodiscard]] Cutscene::StreamVoices Voices() const override {
    const auto& state = Audio_Cutscene_streamState;
    return {state.voiceIndex, static_cast<std::uint32_t>(state.voiceMask)};
  }

  [[nodiscard]] Cutscene::StreamMix Mix() const override {
    const auto& state = Audio_Cutscene_streamState;
    return {static_cast<std::uint32_t>(state.volume),
            static_cast<std::uint32_t>(state.pan.word),
            (state.formatFlags & kStereoSource) != 0,
            (Audio_Driver_controlLatches.mixBehavior & kMonoOutput) != 0};
  }

  [[nodiscard]] std::span<const std::int16_t, Cutscene::kPanGainCount>
  PanGains() const override {
    return Audio_Mixer_kPanGains;
  }

  [[nodiscard]] std::optional<Cutscene::StreamFormat> BoundFormat(
      Cutscene::StreamSourceId source) const override {
    if (!g_bound || source != SourceIdOf(g_bound->source)) return std::nullopt;
    const auto& info = g_bound->info;
    return Cutscene::StreamFormat{info.channels, info.repeat, info.source_pitch,
                                  info.sample_rate};
  }

  std::optional<std::int32_t> FindFreeVoicePair() override {
    const int voice = Cutscene::LiveFindFreeVoicePair();
    if (voice < 0) return std::nullopt;
    return voice;
  }

  void PauseVoice(std::int32_t voice, bool paused) override {
    Audio_Native_PauseVoice(static_cast<unsigned>(voice), paused ? 1 : 0);
  }

  void KeyOffVoices(std::uint32_t mask) override {
    VS::Audio::Driver::KeyOffLiveVoices(mask);
  }

  void ReleaseVoices(std::uint32_t mask) override {
    Audio_Cutscene_streamState.voiceMask = 0;
    Audio_Mixer_voiceScheduler.reverbVoices &= ~mask;
    Audio_Driver_controlLatches.updateFlags |= kVoiceModesDirty;
    VS_LOG_INFO("audio", "stream stopped: voices={:06x}", mask);
  }

  void BeginStream(const Cutscene::StreamStart& start) override {
    auto& state = Audio_Cutscene_streamState;
    state.pan.word = static_cast<int>(start.pan);
    state.formatFlags = static_cast<int>(start.format_flags);
    state.sampleRate = static_cast<int>(start.sample_rate);
    const unsigned mask = start.voices.mask;
    state.voiceIndex = start.voices.first;
    state.voiceMask = static_cast<int>(mask);
    auto& scheduler = Audio_Mixer_voiceScheduler;
    scheduler.activeVoices &= ~mask;
    scheduler.pendingKeyOn &= ~mask;
    scheduler.pendingKeyOff &= ~mask;
    scheduler.keyedVoices &= ~mask;
    scheduler.reverbVoices &= ~mask;
    if (start.reverb) scheduler.reverbVoices |= mask;
    scheduler.fmVoices &= ~mask;
    scheduler.noiseVoices &= ~mask;
    Audio_Driver_controlLatches.updateFlags |= kVoiceModesDirty;
  }

  bool StartVoice(std::size_t channel, std::int32_t voice, float rate,
                  std::uint32_t left, std::uint32_t right,
                  bool reverb) override {
    if (!g_bound || channel >= g_bound->instruments.size()) return false;
    return Audio_Native_StartStreamVoice(static_cast<unsigned>(voice),
                                         g_bound->instruments[channel].c_str(),
                                         rate, left, right,
                                         reverb ? 1 : 0) != 0;
  }

  void SetVoiceGains(std::int32_t voice, std::uint32_t left,
                     std::uint32_t right) override {
    VS::Audio::Driver::SetLiveVoiceVolume(voice, left, right, 0);
  }

  [[nodiscard]] std::optional<Cutscene::StreamGains> AppliedGains()
      const override {
    return g_gains;
  }

  void SetAppliedGains(
      const std::optional<Cutscene::StreamGains>& gains) override {
    g_gains = gains;
  }

  void SetVolume(std::uint32_t volume) override {
    Audio_Cutscene_streamState.volume = static_cast<int>(volume);
    Audio_Cutscene_streamState.volumeFadeTicksLeft = 0;
  }

  void StartFade(const Cutscene::StreamFade& fade) override {
    g_fade_target = static_cast<int>(fade.target);
    Audio_Cutscene_streamState.volumeFadeStep = fade.step;
    Audio_Cutscene_streamState.volumeFadeTicksLeft =
        static_cast<int>(fade.ticks);
  }

  void SetPan(std::uint32_t pan) override {
    Audio_Cutscene_streamState.pan.word = static_cast<int>(pan);
  }

  [[nodiscard]] bool SlotBound() const override {
    return Audio_soundState.sfxStreamData[kRoomSlot] != nullptr;
  }

  std::expected<void, std::string> ValidateAssets(
      const VS::Content::AudioStreamInfo& info) override {
    const auto* store = VS::Content::DataStore::Get();
    if (store == nullptr) {
      return std::unexpected("stream binding requires an open data store");
    }
    return VS::Content::ValidateAudioStreamAssets(info, store->Root());
  }

  void BindSlot(const VS::Content::AudioStreamInfo& info) override {
    const auto* store = VS::Content::DataStore::Get();
    BoundStream binding{data_, byte_count_, info, {}};
    for (std::size_t channel = 0; channel < binding.instruments.size();
         ++channel) {
      binding.instruments[channel] =
          store == nullptr
              ? std::string{}
              : VS::Platform::PathText(VS::Content::AudioStreamInstrumentPath(
                    store->Root(), info, channel % info.channels));
    }
    g_bound = std::move(binding);
    Audio_soundState.sfxStreamData[kRoomSlot] = data_;
    VS_LOG_INFO("audio", "bound stream {}: {} channels, {} frames", info.id,
                info.channels, info.frames);
  }

  void UnbindSlot() override {
    Audio_soundState.sfxStreamData[kRoomSlot] = nullptr;
    g_bound.reset();
  }

 private:
  void* data_;
  std::size_t byte_count_;
};

const char* StatusText(Cutscene::StreamStatus status) {
  switch (status) {
    case Cutscene::StreamStatus::kApplied:
      return "applied";
    case Cutscene::StreamStatus::kUnbound:
      return "the stream is not the bound room stream";
    case Cutscene::StreamStatus::kInvalidPan:
      return "invalid packed pan";
    case Cutscene::StreamStatus::kInvalidFormat:
      return "invalid stream format";
    case Cutscene::StreamStatus::kNoVoicePair:
      return "no voice pair available";
    case Cutscene::StreamStatus::kVoiceFailed:
      return "a stream voice failed to start";
    case Cutscene::StreamStatus::kInvalidVolume:
      return "invalid volume";
    case Cutscene::StreamStatus::kInvalidFade:
      return "invalid fade duration or target";
  }
  return "unknown";
}

void Report(const char* command, Cutscene::StreamStatus status,
            std::uintptr_t first, std::uintptr_t second) {
  if (status == Cutscene::StreamStatus::kApplied) return;
  if (status == Cutscene::StreamStatus::kNoVoicePair) {
    VS_LOG_WARN("audio", "{}: {}", command, StatusText(status));
    return;
  }
  VS_LOG_ERROR("audio", "{} ({:#x}, {:#x}): {}", command, first, second,
               StatusText(status));
}

// A native-width parameter as the 32-bit command word; wider values saturate
// so the core refuses them instead of seeing their truncated low word.
std::uint32_t CommandWord(std::uintptr_t value) {
  return static_cast<std::uint32_t>(std::min<std::uintptr_t>(
      value, std::numeric_limits<std::uint32_t>::max()));
}

bool HasParams(const Audio_CommandParams* params, const char* command) {
  if (params != nullptr) return true;
  VS_LOG_ERROR("audio", "{}: null command parameters", command);
  return false;
}
}  // namespace

namespace VS::Audio::Cutscene {
void PauseLiveStream(Audio_CommandParams* /*params*/) {
  LiveStreamHost host;
  PauseStream(host);
}

void ResumeLiveStream(Audio_CommandParams* /*params*/) {
  LiveStreamHost host;
  ResumeStream(host);
}

// The command carries the stream pointer at native width in param1.
void ReplaceLiveStream(Audio_CommandParams* params) {
  if (!HasParams(params, "replace stream")) return;
  LiveStreamHost host;
  const auto source = static_cast<StreamSourceId>(params->param1);
  Report("replace stream",
         ReplaceStream(host, source, params->param2, params->param3 != 0),
         params->param1, params->param2);
}

void StopLiveStream(Audio_CommandParams* /*params*/) {
  LiveStreamHost host;
  StopStream(host);
}

void SetLiveStreamVolume(Audio_CommandParams* params) {
  if (!HasParams(params, "stream volume")) return;
  LiveStreamHost host;
  Report("stream volume", SetStreamVolume(host, CommandWord(params->param1)),
         params->param1, 0);
}

void SlideLiveStreamVolume(Audio_CommandParams* params) {
  if (!HasParams(params, "stream fade")) return;
  LiveStreamHost host;
  Report("stream fade",
         SlideStreamVolume(host, CommandWord(params->param1), params->param2),
         params->param1, params->param2);
}

void SetLiveStreamPan(Audio_CommandParams* params) {
  if (!HasParams(params, "stream pan")) return;
  LiveStreamHost host;
  Report("stream pan", SetStreamPan(host, CommandWord(params->param1)),
         params->param1, 0);
}

std::span<std::uint8_t> LiveBoundStreamBytes() {
  if (!g_bound) return {};
  return {static_cast<std::uint8_t*>(g_bound->source), g_bound->byte_count};
}
}  // namespace VS::Audio::Cutscene

namespace VS::Audio {
int BindLiveRoomCutsceneStream(void* data, std::size_t byte_count) {
  if (data == nullptr) {
    VS_LOG_ERROR("audio", "stream binding requires data");
    return 0;
  }
  LiveStreamHost host(data, byte_count);
  const auto result = BindRoomCutsceneStream(
      host, {static_cast<const std::uint8_t*>(data), byte_count});
  switch (result.status) {
    case RoomStreamStatus::kBound:
      break;
    case RoomStreamStatus::kSlotOccupied:
      VS_LOG_DEBUG("audio", "room stream slot is already bound");
      break;
    case RoomStreamStatus::kMissingData:
      VS_LOG_ERROR("audio", "stream binding requires data");
      break;
    case RoomStreamStatus::kInvalidStream:
      VS_LOG_ERROR("audio", "stream binding: {}", result.error);
      break;
    case RoomStreamStatus::kMissingAssets:
      VS_LOG_ERROR("audio", "{}", result.error);
      break;
  }
  return result.handle;
}

int UnbindLiveRoomCutsceneStream() {
  LiveStreamHost host;
  return UnbindRoomCutsceneStream(host);
}
}  // namespace VS::Audio

// Replaces the stream portion of func_8001924C, SLUS_010.40/25AC.c:4024.
void Audio_Native_UpdateCutsceneStream(void) {
  auto& state = Audio_Cutscene_streamState;
  if (state.voiceMask == 0) return;
  LiveStreamHost host;
  const auto voice = static_cast<unsigned>(state.voiceIndex);
  if (Audio_Native_VoiceActive(voice) == 0 &&
      Audio_Native_VoiceActive(voice + 1) == 0) {
    Cutscene::StopStreamVoices(host);
    return;
  }
  if (state.volumeFadeTicksLeft > 0) {
    --state.volumeFadeTicksLeft;
    state.volume =
        state.volumeFadeTicksLeft == 0
            ? g_fade_target
            : std::clamp(static_cast<int>(state.volume) + state.volumeFadeStep,
                         0, kMaximumVolume);
  }
  Cutscene::ApplyStreamGains(host);
}

void Audio_Native_ResetCutsceneStream(void) {
  LiveStreamHost host;
  Cutscene::StopStreamVoices(host);
  Audio_soundState.sfxStreamData[kRoomSlot] = nullptr;
  Audio_Cutscene_streamState = {};
  Audio_Cutscene_streamState.volume = kMaximumVolume;
  g_bound.reset();
  g_gains.reset();
  g_fade_target = kMaximumVolume;
}
