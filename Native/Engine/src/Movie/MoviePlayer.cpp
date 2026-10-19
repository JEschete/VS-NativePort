#include "Movie/MoviePlayer.h"

#include <SDL3/SDL.h>
#include <dav1d/dav1d.h>
#include <mkvparser/mkvparser.h>
#include <mkvparser/mkvreader.h>

#include <utility>

#include "Audio/AudioDevice.hpp"
#include "Content/DataStore.hpp"
#include "Core/Log.hpp"
#include "Render/PageSpace.hpp"

#define DR_FLAC_IMPLEMENTATION
#include <dr_flac.h>

#include <algorithm>
#include <climits>
#include <limits>
#include <memory>
#include <string>
#include <vector>

// Plays a movie VS.Extract converted (Matroska: AV1 video, FLAC audio). Both
// streams are read into memory at open (the title movie is about 10 MB): the
// soundtrack is decoded at once and queued on its own audio source, so it runs
// on in real time while the game decides what to show; the picture is decoded
// frame by frame as the game asks for the frame due at a tick and written as
// 24-bit pixels into the page space, where the MDEC's output went.

namespace {

using VS::i32;
using VS::i64;
using VS::u16;
using VS::u8;
using VS::usize;
namespace Audio = VS::Audio;
namespace Content = VS::Content;
namespace Core = VS::Core;
namespace Render = VS::Render;

constexpr i64 kNanosecondsPerSecond = 1'000'000'000;
constexpr i64 kMovieTicksPerSecond = 60;
constexpr i64 kFallbackFrameNanoseconds = 66'666'667;
constexpr i32 kRgbChannels = 3;
constexpr float kByteMaximum = 255.0f;
constexpr float kRoundToNearestByte = 0.5f;
constexpr float kChromaCenter = 128.0f;
constexpr float kLimitedLumaMinimum = 16.0f;
constexpr float kLimitedLumaRange = 219.0f;
constexpr float kLimitedChromaRange = 224.0f;
constexpr float kRedFromCr = 1.402f;
constexpr float kGreenFromCb = 0.344136f;
constexpr float kGreenFromCr = 0.714136f;
constexpr float kBlueFromCb = 1.772f;

struct Packet {
  i64 time_ns = 0;
  std::vector<u8> bytes;
};

class Player {
 public:
  ~Player() { Close(); }

  bool Open(const std::string& file) {
    mkvparser::MkvReader reader;
    if (reader.Open(file.c_str()) != 0) {
      VS_LOG_ERROR("movie", "cannot open {}", file);
      return false;
    }
    i64 pos = 0;
    mkvparser::EBMLHeader ebml;
    if (ebml.Parse(&reader, pos) < 0) {
      VS_LOG_ERROR("movie", "{}: not a Matroska file", file);
      return false;
    }
    mkvparser::Segment* raw = nullptr;
    if (mkvparser::Segment::CreateInstance(&reader, pos, raw) != 0 ||
        raw == nullptr) {
      return false;
    }
    std::unique_ptr<mkvparser::Segment> segment(raw);
    if (segment->Load() < 0) {
      return false;
    }
    i64 video_track = -1, audio_track = -1;
    std::vector<u8> flac;
    const mkvparser::Tracks* tracks = segment->GetTracks();
    if (tracks == nullptr) return false;
    for (decltype(tracks->GetTracksCount()) index = 0;
         index < tracks->GetTracksCount(); ++index) {
      const mkvparser::Track* t = tracks->GetTrackByIndex(index);
      if (t == nullptr) {
        continue;
      }
      const std::string codec = t->GetCodecId() ? t->GetCodecId() : "";
      if (t->GetType() == mkvparser::Track::kVideo && codec == "V_AV1") {
        video_track = t->GetNumber();
        const auto* v = static_cast<const mkvparser::VideoTrack*>(t);
        if (v->GetWidth() <= 0 || v->GetHeight() <= 0 ||
            v->GetWidth() > std::numeric_limits<i32>::max() / kRgbChannels ||
            v->GetHeight() > std::numeric_limits<i32>::max()) {
          return false;
        }
        m_width_ = static_cast<i32>(v->GetWidth());
        m_height_ = static_cast<i32>(v->GetHeight());
      } else if (t->GetType() == mkvparser::Track::kAudio &&
                 codec == "A_FLAC") {
        audio_track = t->GetNumber();
        const auto* a = static_cast<const mkvparser::AudioTrack*>(t);
        m_sample_rate_ = static_cast<i32>(a->GetSamplingRate());
        m_channels_ = static_cast<i32>(a->GetChannels());
        size_t size = 0;
        const unsigned char* priv = t->GetCodecPrivate(size);
        if (size != 0) {
          if (priv == nullptr) return false;
          flac.assign(priv, priv + size);
        }
      }
    }
    if (video_track < 0) {
      VS_LOG_ERROR("movie", "{}: no AV1 video track", file);
      return false;
    }
    for (const mkvparser::Cluster* c = segment->GetFirst();
         c != nullptr && !c->EOS(); c = segment->GetNext(c)) {
      const mkvparser::BlockEntry* be = nullptr;
      if (c->GetFirst(be) < 0) {
        break;
      }
      while (be != nullptr && !be->EOS()) {
        const mkvparser::Block* b = be->GetBlock();
        for (int k = 0; k < b->GetFrameCount(); ++k) {
          const mkvparser::Block::Frame& f = b->GetFrame(k);
          if (f.len <= 0) return false;
          std::vector<u8> bytes(static_cast<usize>(f.len));
          if (f.Read(&reader, bytes.data()) != 0) {
            continue;
          }
          if (b->GetTrackNumber() == video_track) {
            m_video_.push_back({b->GetTime(c), std::move(bytes)});
          } else if (b->GetTrackNumber() == audio_track) {
            flac.insert(flac.end(), bytes.begin(), bytes.end());
          }
        }
        if (c->GetNext(be, be) < 0) {
          break;
        }
      }
    }
    if (m_video_.empty()) {
      return false;
    }
    Dav1dSettings settings;
    dav1d_default_settings(&settings);
    settings.n_threads = 2;
    settings.max_frame_delay = 1;
    if (dav1d_open(&m_decoder_, &settings) != 0) {
      VS_LOG_ERROR("movie", "dav1d_open failed");
      return false;
    }
    const usize row_bytes = static_cast<usize>(m_width_) * kRgbChannels;
    if (static_cast<usize>(m_height_) > m_rgb_.max_size() / row_bytes) {
      return false;
    }
    m_rgb_.assign(row_bytes * m_height_, 0);
    StartSoundtrack(flac);
    VS_LOG_INFO(
        "movie", "{}: {}x{}, {} frames, {:.1f} s", file, m_width_, m_height_,
        m_video_.size(),
        static_cast<double>(m_video_.back().time_ns) / kNanosecondsPerSecond);
    return true;
  }

  void Close() {
    if (m_decoder_ != nullptr) {
      dav1d_data_unref(&m_pending_);
      dav1d_close(&m_decoder_);
    }
    Audio::AudioDevice::CloseSource(m_audio_);
    m_audio_ = nullptr;
  }

  // Decodes up to the frame due at `timeNs`. False once past the last frame.
  bool Advance(i64 time_ns) {
    while (m_shown_ + 1 < static_cast<i64>(m_video_.size()) &&
           m_video_[m_shown_ + 1].time_ns <= time_ns) {
      if (!DecodeNext()) {
        return false;
      }
      ++m_shown_;
    }
    const i64 frame_ns = m_video_.size() > 1
                             ? m_video_[1].time_ns - m_video_[0].time_ns
                             : kFallbackFrameNanoseconds;
    return time_ns < m_video_.back().time_ns + frame_ns;
  }

  void Draw(i32 x, i32 y) const {
    // 24-bit pixels are stored as consecutive R, G, B bytes: a row of w pixels
    // covers w * 3 / 2 halfwords
    Render::GamePages().Upload(
        {x, y, m_width_ * kRgbChannels / static_cast<i32>(sizeof(u16)),
         m_height_},
        reinterpret_cast<const u16*>(m_rgb_.data()));
  }

 private:
  bool DecodeNext() {
    Dav1dPicture pic{};
    while (true) {
      if (m_pending_.sz == 0 && m_sent_ < m_video_.size()) {
        const Packet& p = m_video_[m_sent_++];
        u8* dst = dav1d_data_create(&m_pending_, p.bytes.size());
        if (dst == nullptr) return false;
        std::ranges::copy(p.bytes, dst);
      }
      if (m_pending_.sz != 0) {
        const int r = dav1d_send_data(m_decoder_, &m_pending_);
        if (r < 0 && r != DAV1D_ERR(EAGAIN)) {
          VS_LOG_ERROR("movie", "dav1d_send_data: {}", r);
          return false;
        }
      }
      const int r = dav1d_get_picture(m_decoder_, &pic);
      if (r == 0) {
        break;
      }
      if (r != DAV1D_ERR(EAGAIN) ||
          (m_pending_.sz == 0 && m_sent_ >= m_video_.size())) {
        return false;
      }
    }
    const bool converted = ToRgb(pic);
    dav1d_picture_unref(&pic);
    return converted;
  }

  // YCbCr 4:2:0 to RGB; the MDEC's output range is full (the conversion keeps
  // it), BT.601 coefficients.
  bool ToRgb(const Dav1dPicture& pic) {
    if (pic.p.bpc != CHAR_BIT || pic.p.layout != DAV1D_PIXEL_LAYOUT_I420 ||
        pic.data[0] == nullptr || pic.data[1] == nullptr ||
        pic.data[2] == nullptr) {
      VS_LOG_ERROR("movie", "unsupported decoded picture layout");
      return false;
    }
    const bool full = pic.seq_hdr == nullptr || pic.seq_hdr->color_range != 0;
    const auto* py = static_cast<const u8*>(pic.data[0]);
    const auto* pu = static_cast<const u8*>(pic.data[1]);
    const auto* pv = static_cast<const u8*>(pic.data[2]);
    const i32 w = std::min(m_width_, pic.p.w), h = std::min(m_height_, pic.p.h);
    for (i32 row = 0; row < h; ++row) {
      u8* out =
          m_rgb_.data() + static_cast<usize>(row) * m_width_ * kRgbChannels;
      for (i32 col = 0; col < w; ++col) {
        float y = py[row * pic.stride[0] + col];
        const auto chroma_offset = (row / 2) * pic.stride[1] + col / 2;
        const float u = static_cast<float>(pu[chroma_offset]) - kChromaCenter;
        const float v = static_cast<float>(pv[chroma_offset]) - kChromaCenter;
        float s = 1.0f;
        if (!full) {
          y = (y - kLimitedLumaMinimum) * (kByteMaximum / kLimitedLumaRange);
          s = kByteMaximum / kLimitedChromaRange;
        }
        const auto clamp = [](float v) {
          return static_cast<u8>(
              std::clamp(v + kRoundToNearestByte, 0.0f, kByteMaximum));
        };
        u8* pixel = out + static_cast<usize>(col) * kRgbChannels;
        pixel[0] = clamp(y + kRedFromCr * s * v);
        pixel[1] = clamp(y - kGreenFromCb * s * u - kGreenFromCr * s * v);
        pixel[2] = clamp(y + kBlueFromCb * s * u);
      }
    }
    return true;
  }

  void StartSoundtrack(const std::vector<u8>& flac) {
    constexpr usize kFlacMarkerBytes = 4;
    Audio::AudioDevice* device = Audio::AudioDevice::Primary();
    if (device == nullptr || flac.size() < kFlacMarkerBytes ||
        m_sample_rate_ <= 0) {
      VS_LOG_WARN("movie",
                  "soundtrack not played (device {}, {} bytes of FLAC, {} Hz)",
                  device != nullptr, flac.size(), m_sample_rate_);
      return;
    }
    unsigned int channels = 0, rate = 0;
    drflac_uint64 frames = 0;
    drflac_int16* pcm = drflac_open_memory_and_read_pcm_frames_s16(
        flac.data(), flac.size(), &channels, &rate, &frames, nullptr);
    if (pcm == nullptr) {
      VS_LOG_ERROR("movie", "soundtrack: FLAC decode failed");
      return;
    }
    m_audio_ =
        device->OpenSource(static_cast<i32>(rate), static_cast<i32>(channels));
    if (m_audio_ != nullptr) {
      SDL_PutAudioStreamData(
          m_audio_, pcm,
          static_cast<int>(frames * channels * sizeof(drflac_int16)));
      VS_LOG_INFO("movie", "soundtrack: {:.1f} s, {} Hz, {} ch",
                  static_cast<double>(frames) / rate, rate, channels);
    }
    drflac_free(pcm, nullptr);
  }

  std::vector<Packet> m_video_;
  std::vector<u8> m_rgb_;
  Dav1dContext* m_decoder_ = nullptr;
  Dav1dData m_pending_{};
  usize m_sent_ = 0;
  i64 m_shown_ = -1;
  i32 m_width_ = 0, m_height_ = 0, m_sample_rate_ = 0, m_channels_ = 0;
  SDL_AudioStream* m_audio_ = nullptr;
};

std::unique_ptr<Player> g_player;

}  // namespace

extern "C" int Movie_PlayerOpen(const char* disc_path) {
  g_player.reset();
  const Content::DataStore* store = Content::DataStore::Get();
  const Content::DataStore::Entry* e = store ? store->Find(disc_path) : nullptr;
  if (e == nullptr || e->form == "raw" || e->form == "stream") {
    VS_LOG_WARN(
        "movie",
        "{}: no converted movie (run VS.Extract extract with FFmpeg available)",
        disc_path);
    return 0;
  }
  auto player = std::make_unique<Player>();
  if (!player->Open(store->FileOf(*e).string())) {
    return 0;
  }
  g_player = std::move(player);
  return 1;
}

extern "C" int Movie_PlayerDrawFrame(int ticks, int x, int y) {
  if (!g_player) {
    return 0;
  }
  const bool more = g_player->Advance(
      static_cast<i64>(ticks) * kNanosecondsPerSecond / kMovieTicksPerSecond);
  g_player->Draw(x, y);
  return more ? 1 : 0;
}

extern "C" void Movie_PlayerClose(void) { g_player.reset(); }
