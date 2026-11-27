#include "Movie/MoviePlayer.hpp"

#define DR_FLAC_IMPLEMENTATION

#include <SDL3/SDL.h>
#include <dav1d/dav1d.h>
#include <dr_flac.h>
#include <mkvparser/mkvparser.h>
#include <mkvparser/mkvreader.h>

#include <algorithm>
#include <array>
#include <climits>
#include <cstdio>
#include <filesystem>
#include <format>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "Audio/AudioDevice.hpp"
#include "Content/DataStore.hpp"
#include "Core/Log.hpp"
#include "Platform/Paths.hpp"

namespace {

using VS::i32;
using VS::i64;
using VS::u32;
using VS::u64;
using VS::u8;
using VS::usize;
namespace Audio = VS::Audio;
namespace Content = VS::Content;

constexpr i64 kNanosecondsPerSecond = 1'000'000'000;
constexpr i64 kMovieTicksPerSecond = 60;
constexpr i64 kFallbackFrameNanoseconds = 66'666'667;
constexpr i32 kRgbChannels = 3;
constexpr i32 kMaximumPacketBytes = 8 * 1024 * 1024;
constexpr i32 kAudioBlockFrames = 2048;
constexpr usize kMaximumAudioChannels = 2;
constexpr usize kFlacSeekBufferBytes = 4096;
constexpr i64 kAudioQueueNanoseconds = 100'000'000;
constexpr u64 kAudioStallNanoseconds = 500'000'000;
constexpr u32 kOpaqueBlackRgba = 0xFF000000U;
constexpr int kBlueChannelShift = 16;
constexpr int kGreenChannelShift = 8;
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

struct TrackCursor {
  const mkvparser::Cluster* cluster = nullptr;
  const mkvparser::BlockEntry* entry = nullptr;
  int frame = 0;
};

struct MovieTracks {
  i64 video = -1;
  i64 audio = -1;
  i32 width = 0;
  i32 height = 0;
  i32 sample_rate = 0;
  i32 channels = 0;
  std::vector<u8> flac_header;
};

class MatroskaReader {
 public:
  using File = std::unique_ptr<std::FILE, decltype(&std::fclose)>;

  MatroskaReader() : m_input_(nullptr, &std::fclose) {}

  std::expected<MovieTracks, std::string> Open(
      const std::filesystem::path& path) {
    const auto file = VS::Platform::PathText(path);
    m_input_.reset(_wfopen(path.c_str(), L"rb"));
    if (!m_input_) {
      return std::unexpected("cannot open " + file);
    }
    m_reader_ = std::make_unique<mkvparser::MkvReader>(m_input_.get());
    i64 position = 0;
    mkvparser::EBMLHeader ebml;
    if (ebml.Parse(m_reader_.get(), position) < 0) {
      return std::unexpected(file + ": not a Matroska file");
    }
    mkvparser::Segment* raw_segment = nullptr;
    if (mkvparser::Segment::CreateInstance(m_reader_.get(), position,
                                           raw_segment) != 0 ||
        raw_segment == nullptr) {
      return std::unexpected(file + ": cannot create Matroska segment");
    }
    m_segment_.reset(raw_segment);
    if (m_segment_->Load() < 0) {
      return std::unexpected(file + ": cannot load Matroska segment");
    }

    const mkvparser::Tracks* tracks = m_segment_->GetTracks();
    if (tracks == nullptr) {
      return std::unexpected(file + ": missing Matroska tracks");
    }
    MovieTracks selected;
    for (decltype(tracks->GetTracksCount()) index = 0;
         index < tracks->GetTracksCount(); ++index) {
      const mkvparser::Track* track = tracks->GetTrackByIndex(index);
      if (track == nullptr) continue;
      const char* codec_id = track->GetCodecId();
      const std::string codec = codec_id ? codec_id : "";
      if (track->GetType() == mkvparser::Track::kVideo && codec == "V_AV1") {
        if (selected.video >= 0) {
          return std::unexpected(file + ": multiple AV1 video tracks");
        }
        const auto* video = static_cast<const mkvparser::VideoTrack*>(track);
        if (video->GetWidth() <= 0 || video->GetHeight() <= 0 ||
            video->GetWidth() >
                std::numeric_limits<i32>::max() / kRgbChannels ||
            video->GetHeight() > std::numeric_limits<i32>::max()) {
          return std::unexpected(file + ": invalid AV1 video dimensions");
        }
        selected.video = track->GetNumber();
        selected.width = static_cast<i32>(video->GetWidth());
        selected.height = static_cast<i32>(video->GetHeight());
      } else if (track->GetType() == mkvparser::Track::kAudio &&
                 codec == "A_FLAC") {
        if (selected.audio >= 0) {
          return std::unexpected(file + ": multiple FLAC audio tracks");
        }
        const auto* audio = static_cast<const mkvparser::AudioTrack*>(track);
        selected.audio = track->GetNumber();
        selected.sample_rate = static_cast<i32>(audio->GetSamplingRate());
        selected.channels = static_cast<i32>(audio->GetChannels());
        size_t size = 0;
        const unsigned char* metadata = track->GetCodecPrivate(size);
        if (metadata == nullptr || size == 0 ||
            size > static_cast<size_t>(kMaximumPacketBytes)) {
          return std::unexpected(file + ": missing or invalid FLAC metadata");
        }
        selected.flac_header.assign(metadata, metadata + size);
      }
    }
    if (selected.video < 0 || selected.audio < 0 || selected.sample_rate <= 0 ||
        (selected.channels != 1 && selected.channels != 2)) {
      return std::unexpected(file + ": missing or invalid supported tracks");
    }
    return selected;
  }

  enum class ReadResult : std::uint8_t { kPacket, kEnd, kError };

  ReadResult ReadPacket(i64 track_number, TrackCursor& cursor, Packet& packet,
                        std::string& error) {
    if (cursor.cluster == nullptr) {
      cursor.cluster = m_segment_->GetFirst();
      cursor.entry = nullptr;
      cursor.frame = 0;
      if (cursor.cluster != nullptr && !cursor.cluster->EOS() &&
          cursor.cluster->GetFirst(cursor.entry) < 0) {
        error = "cannot read Matroska cluster";
        return ReadResult::kError;
      }
    }
    while (cursor.cluster != nullptr && !cursor.cluster->EOS()) {
      if (cursor.entry == nullptr || cursor.entry->EOS()) {
        cursor.cluster = m_segment_->GetNext(cursor.cluster);
        cursor.entry = nullptr;
        cursor.frame = 0;
        if (cursor.cluster != nullptr && !cursor.cluster->EOS() &&
            cursor.cluster->GetFirst(cursor.entry) < 0) {
          error = "cannot read Matroska cluster";
          return ReadResult::kError;
        }
        continue;
      }
      const mkvparser::Block* block = cursor.entry->GetBlock();
      if (block == nullptr) {
        error = "Matroska block is missing";
        return ReadResult::kError;
      }
      if (block->GetTrackNumber() != track_number ||
          cursor.frame >= block->GetFrameCount()) {
        if (!AdvanceEntry(cursor, error)) return ReadResult::kError;
        continue;
      }
      const mkvparser::Block::Frame& frame = block->GetFrame(cursor.frame++);
      if (frame.len <= 0 || frame.len > kMaximumPacketBytes) {
        error = "Matroska packet size is outside the supported bound";
        return ReadResult::kError;
      }
      packet.time_ns = block->GetTime(cursor.cluster);
      packet.bytes.resize(static_cast<usize>(frame.len));
      if (frame.Read(m_reader_.get(), packet.bytes.data()) != 0) {
        error = "cannot read Matroska packet";
        return ReadResult::kError;
      }
      if (cursor.frame >= block->GetFrameCount() &&
          !AdvanceEntry(cursor, error)) {
        return ReadResult::kError;
      }
      return ReadResult::kPacket;
    }
    return ReadResult::kEnd;
  }

 private:
  bool AdvanceEntry(TrackCursor& cursor, std::string& error) {
    const mkvparser::BlockEntry* next_entry = nullptr;
    if (cursor.cluster->GetNext(cursor.entry, next_entry) < 0) {
      error = "cannot advance Matroska cluster";
      return false;
    }
    if (next_entry != nullptr) {
      cursor.entry = next_entry;
      cursor.frame = 0;
      return true;
    }

    cursor.cluster = m_segment_->GetNext(cursor.cluster);
    cursor.entry = nullptr;
    cursor.frame = 0;
    if (cursor.cluster != nullptr && !cursor.cluster->EOS() &&
        cursor.cluster->GetFirst(cursor.entry) < 0) {
      error = "cannot read Matroska cluster";
      return false;
    }
    return true;
  }

  File m_input_;
  std::unique_ptr<mkvparser::MkvReader> m_reader_;
  std::unique_ptr<mkvparser::Segment> m_segment_;
};

struct FlacSource {
  MatroskaReader* demux = nullptr;
  i64 track_number = -1;
  TrackCursor cursor;
  std::vector<u8> header;
  usize header_offset = 0;
  Packet packet;
  usize packet_offset = 0;
  drflac_int64 stream_offset = 0;
  bool ended = false;
  std::string error;

  size_t Read(void* output, size_t requested) {
    auto* destination = static_cast<u8*>(output);
    size_t copied = 0;
    while (copied < requested) {
      if (header_offset < header.size()) {
        const size_t count =
            std::min(requested - copied, header.size() - header_offset);
        std::copy_n(header.data() + header_offset, count, destination + copied);
        header_offset += count;
        copied += count;
        continue;
      }
      if (packet_offset == packet.bytes.size()) {
        if (ended) break;
        const auto result =
            demux->ReadPacket(track_number, cursor, packet, error);
        if (result == MatroskaReader::ReadResult::kError) {
          ended = true;
          break;
        }
        if (result == MatroskaReader::ReadResult::kEnd) {
          ended = true;
          break;
        }
        packet_offset = 0;
      }
      const size_t count =
          std::min(requested - copied, packet.bytes.size() - packet_offset);
      std::copy_n(packet.bytes.data() + packet_offset, count,
                  destination + copied);
      packet_offset += count;
      copied += count;
    }
    stream_offset += static_cast<drflac_int64>(copied);
    return copied;
  }

  drflac_bool32 Seek(int offset, drflac_seek_origin origin) {
    if (offset < 0 ||
        (origin != DRFLAC_SEEK_SET && origin != DRFLAC_SEEK_CUR)) {
      return DRFLAC_FALSE;
    }
    const drflac_int64 target =
        origin == DRFLAC_SEEK_SET
            ? offset
            : stream_offset + static_cast<drflac_int64>(offset);
    if (target < 0) return DRFLAC_FALSE;
    if (target < stream_offset) {
      cursor = {};
      header_offset = 0;
      packet_offset = packet.bytes.size();
      ended = false;
      error.clear();
      stream_offset = 0;
    }
    std::array<u8, kFlacSeekBufferBytes> discard{};
    while (stream_offset < target) {
      const auto remaining = static_cast<size_t>(target - stream_offset);
      const size_t count =
          Read(discard.data(), std::min(discard.size(), remaining));
      if (count == 0) return DRFLAC_FALSE;
    }
    return DRFLAC_TRUE;
  }

  static size_t ReadCallback(void* user_data, void* output, size_t requested) {
    return static_cast<FlacSource*>(user_data)->Read(output, requested);
  }

  static drflac_bool32 SeekCallback(void* user_data, int offset,
                                    drflac_seek_origin origin) {
    return static_cast<FlacSource*>(user_data)->Seek(offset, origin);
  }

  static drflac_bool32 TellCallback(void* user_data,
                                    drflac_int64* stream_offset) {
    *stream_offset = static_cast<FlacSource*>(user_data)->stream_offset;
    return DRFLAC_TRUE;
  }
};

}  // namespace

struct VS::Movie::PlaybackSession::Impl {
  ~Impl() { Close(); }

  std::expected<void, std::string> Open(const std::filesystem::path& path) {
    auto opened = demux.Open(path);
    if (!opened) return std::unexpected(opened.error());
    tracks = std::move(*opened);

    flac_source.demux = &demux;
    flac_source.track_number = tracks.audio;
    flac_source.header = tracks.flac_header;
    audio_decoder =
        drflac_open(&FlacSource::ReadCallback, &FlacSource::SeekCallback,
                    &FlacSource::TellCallback, &flac_source, nullptr);
    if (audio_decoder == nullptr) {
      return std::unexpected("cannot initialize Matroska FLAC track");
    }
    if (audio_decoder->sampleRate !=
            static_cast<unsigned>(tracks.sample_rate) ||
        audio_decoder->channels != static_cast<unsigned>(tracks.channels) ||
        (audio_decoder->channels != 1 && audio_decoder->channels != 2)) {
      return std::unexpected("FLAC metadata does not match the Matroska track");
    }
    audio_output.sample_rate = static_cast<i32>(audio_decoder->sampleRate);
    audio_output.channels = static_cast<i32>(audio_decoder->channels);
    audio_output.total_frames = audio_decoder->totalPCMFrameCount;

    Dav1dSettings settings;
    dav1d_default_settings(&settings);
    settings.n_threads = 2;
    settings.max_frame_delay = 1;
    if (dav1d_open(&video_decoder, &settings) != 0) {
      return std::unexpected("dav1d_open failed");
    }
    const usize pixels =
        static_cast<usize>(tracks.width) * static_cast<usize>(tracks.height);
    if (tracks.height != 0 && pixels / static_cast<usize>(tracks.height) !=
                                  static_cast<usize>(tracks.width)) {
      return std::unexpected("movie image dimensions overflow");
    }
    rgba.assign(pixels, kOpaqueBlackRgba);

    Audio::AudioDevice* device = Audio::AudioDevice::Primary();
    if (device != nullptr && device->IsOpen() && device->IsRealtime()) {
      audio_device = device;
      audio_stream =
          device->OpenSource(audio_output.sample_rate, audio_output.channels);
      if (audio_stream == nullptr) {
        audio_device = nullptr;
        VS::Core::LogWarn("movie",
                          "soundtrack output unavailable; using logical clock");
      } else {
        audio_device_id = SDL_GetAudioStreamDevice(audio_stream);
        if (audio_device_id == 0) {
          Audio::AudioDevice::CloseSource(audio_stream);
          audio_stream = nullptr;
          audio_device = nullptr;
          VS::Core::LogWarn(
              "movie", "soundtrack source is unbound; using logical clock");
        } else {
          clock_source = VS::Movie::ClockSource::kAudioSamples;
          last_audio_progress_ns = SDL_GetTicksNS();
          QueueAudio();
        }
      }
    } else {
      VS::Core::LogWarn("movie",
                        "soundtrack output unavailable; using logical clock");
    }

    const auto result =
        demux.ReadPacket(tracks.video, video_cursor, next_video, demux_error);
    if (result == MatroskaReader::ReadResult::kError) {
      return std::unexpected(demux_error);
    }
    if (result == MatroskaReader::ReadResult::kEnd) {
      return std::unexpected("movie has no video frames");
    }
    has_next_video = true;
    return {};
  }

  void Close() {
    if (audio_stream != nullptr) {
      Audio::AudioDevice::CloseSource(audio_stream);
      audio_stream = nullptr;
    }
    audio_device = nullptr;
    audio_device_id = 0;
    if (audio_decoder != nullptr) {
      drflac_close(audio_decoder);
      audio_decoder = nullptr;
    }
    if (video_decoder != nullptr) {
      dav1d_data_unref(&pending_data);
      dav1d_close(&video_decoder);
    }
    audio_output.audible = false;
    has_video_frame = false;
  }

  bool QueueAudio() {
    if (audio_stream == nullptr || audio_decoder == nullptr) return true;
    const int queued_bytes = SDL_GetAudioStreamQueued(audio_stream);
    if (queued_bytes < 0) {
      DisableAudio("cannot query soundtrack queue");
      return false;
    }
    const auto bytes_per_frame =
        static_cast<u64>(audio_output.channels) * sizeof(drflac_int16);
    u64 queued_frames = static_cast<u64>(queued_bytes) / bytes_per_frame;
    const u64 target_frames = static_cast<u64>(audio_output.sample_rate) *
                              static_cast<u64>(kAudioQueueNanoseconds) /
                              static_cast<u64>(kNanosecondsPerSecond);
    std::array<drflac_int16,
               static_cast<usize>(kAudioBlockFrames) * kMaximumAudioChannels>
        samples{};
    while (queued_frames < target_frames && !audio_ended) {
      const u64 wanted =
          std::min<u64>(target_frames - queued_frames, kAudioBlockFrames);
      const drflac_uint64 decoded =
          drflac_read_pcm_frames_s16(audio_decoder, wanted, samples.data());
      if (decoded == 0) {
        if (!flac_source.error.empty() ||
            (audio_output.total_frames != 0 &&
             audio_output.submitted_frames < audio_output.total_frames)) {
          DisableAudio(flac_source.error.empty()
                           ? "FLAC ended before its declared frame count"
                           : flac_source.error);
          return false;
        }
        audio_ended = true;
        if (!SDL_FlushAudioStream(audio_stream)) {
          DisableAudio("cannot flush soundtrack output");
          return false;
        }
        break;
      }
      const auto bytes = static_cast<int>(decoded * bytes_per_frame);
      if (!SDL_PutAudioStreamData(audio_stream, samples.data(), bytes)) {
        DisableAudio("cannot queue soundtrack samples");
        return false;
      }
      audio_output.submitted_frames += decoded;
      queued_frames += decoded;
    }
    audio_output.queued_frames = queued_frames;
    return true;
  }

  void DisableAudio(const std::string& reason) {
    if (audio_stream == nullptr) return;
    VS::Core::LogError("movie", "soundtrack stopped: {}; continuing video",
                       reason);
    const i64 now = static_cast<i64>(SDL_GetTicksNS());
    realtime_fallback_time_ns = last_presentation_time_ns;
    realtime_fallback_start_ns = now;
    clock_source = VS::Movie::ClockSource::kRealtimeFallback;
    Audio::AudioDevice::CloseSource(audio_stream);
    audio_stream = nullptr;
    audio_device = nullptr;
    audio_device_id = 0;
    audio_output.audible = false;
    audio_output.queued_frames = 0;
    if (audio_decoder != nullptr) {
      drflac_close(audio_decoder);
      audio_decoder = nullptr;
    }
  }

  i64 PresentationTime(std::uint32_t elapsed_ticks) {
    if (clock_source == VS::Movie::ClockSource::kAudioSamples) {
      if (Audio::AudioDevice::Primary() != audio_device ||
          audio_device == nullptr || !audio_device->IsOpen() ||
          SDL_GetAudioStreamDevice(audio_stream) != audio_device_id) {
        DisableAudio("audio device was closed or replaced");
        return PresentationTime(elapsed_ticks);
      }
      if (!QueueAudio()) {
        if (clock_source == VS::Movie::ClockSource::kRealtimeFallback) {
          return PresentationTime(elapsed_ticks);
        }
      }
      if (audio_stream == nullptr) {
        return PresentationTime(elapsed_ticks);
      }
      const int queued_bytes = SDL_GetAudioStreamQueued(audio_stream);
      if (queued_bytes < 0) {
        DisableAudio("cannot query soundtrack position");
        return PresentationTime(elapsed_ticks);
      }
      const auto bytes_per_frame =
          static_cast<u64>(audio_output.channels) * sizeof(drflac_int16);
      const u64 queued_frames =
          static_cast<u64>(queued_bytes) / bytes_per_frame;
      const u64 consumed_frames =
          audio_output.submitted_frames -
          std::min(audio_output.submitted_frames, queued_frames);
      audio_output.queued_frames = queued_frames;
      audio_output.consumed_frames = consumed_frames;
      const u64 now = SDL_GetTicksNS();
      if (queued_frames < last_queued_frames ||
          consumed_frames > last_consumed_frames) {
        last_audio_progress_ns = now;
      } else if (queued_frames != 0 &&
                 now - last_audio_progress_ns > kAudioStallNanoseconds) {
        DisableAudio("soundtrack queue stopped advancing");
        return PresentationTime(elapsed_ticks);
      }
      last_queued_frames = queued_frames;
      last_consumed_frames = consumed_frames;
      const u64 elapsed_ns = consumed_frames *
                             static_cast<u64>(kNanosecondsPerSecond) /
                             static_cast<u64>(audio_output.sample_rate);
      last_presentation_time_ns = static_cast<i64>(std::min<u64>(
          elapsed_ns, static_cast<u64>(std::numeric_limits<i64>::max())));
      return last_presentation_time_ns;
    }
    if (clock_source == VS::Movie::ClockSource::kRealtimeFallback) {
      const i64 now = static_cast<i64>(SDL_GetTicksNS());
      last_presentation_time_ns =
          realtime_fallback_time_ns +
          std::max<i64>(0, now - realtime_fallback_start_ns);
      return last_presentation_time_ns;
    }
    last_presentation_time_ns = static_cast<i64>(elapsed_ticks) *
                                kNanosecondsPerSecond / kMovieTicksPerSecond;
    return last_presentation_time_ns;
  }

  bool DecodeVideoPacket(const Packet& packet, std::string& error) {
    u8* destination = dav1d_data_create(&pending_data, packet.bytes.size());
    if (destination == nullptr) {
      error = "cannot allocate AV1 input packet";
      return false;
    }
    std::copy(packet.bytes.begin(), packet.bytes.end(), destination);
    while (true) {
      if (pending_data.sz != 0) {
        const int sent = dav1d_send_data(video_decoder, &pending_data);
        if (sent < 0 && sent != DAV1D_ERR(EAGAIN)) {
          error = std::format("dav1d_send_data failed: {}", sent);
          return false;
        }
      }
      Dav1dPicture picture{};
      const int received = dav1d_get_picture(video_decoder, &picture);
      if (received == 0) {
        const bool converted = ConvertPicture(picture, error);
        dav1d_picture_unref(&picture);
        return converted;
      }
      if (received != DAV1D_ERR(EAGAIN) || pending_data.sz == 0) {
        error = std::format("dav1d_get_picture failed: {}", received);
        return false;
      }
    }
  }

  bool ConvertPicture(const Dav1dPicture& picture, std::string& error) {
    if (picture.p.bpc != CHAR_BIT ||
        picture.p.layout != DAV1D_PIXEL_LAYOUT_I420 ||
        picture.data[0] == nullptr || picture.data[1] == nullptr ||
        picture.data[2] == nullptr || picture.p.w != tracks.width ||
        picture.p.h != tracks.height) {
      error = "unsupported decoded AV1 picture layout or dimensions";
      return false;
    }
    const bool full =
        picture.seq_hdr == nullptr || picture.seq_hdr->color_range != 0;
    const auto* y_plane = static_cast<const u8*>(picture.data[0]);
    const auto* u_plane = static_cast<const u8*>(picture.data[1]);
    const auto* v_plane = static_cast<const u8*>(picture.data[2]);
    for (i32 row = 0; row < tracks.height; ++row) {
      for (i32 column = 0; column < tracks.width; ++column) {
        float y = y_plane[row * picture.stride[0] + column];
        const auto chroma_offset = (row / 2) * picture.stride[1] + column / 2;
        const float u =
            static_cast<float>(u_plane[chroma_offset]) - kChromaCenter;
        const float v =
            static_cast<float>(v_plane[chroma_offset]) - kChromaCenter;
        float scale = 1.0f;
        if (!full) {
          y = (y - kLimitedLumaMinimum) * (kByteMaximum / kLimitedLumaRange);
          scale = kByteMaximum / kLimitedChromaRange;
        }
        const auto clamp = [](float value) {
          return static_cast<u32>(static_cast<u8>(
              std::clamp(value + kRoundToNearestByte, 0.0f, kByteMaximum)));
        };
        const u32 red = clamp(y + kRedFromCr * scale * v);
        const u32 green =
            clamp(y - kGreenFromCb * scale * u - kGreenFromCr * scale * v);
        const u32 blue = clamp(y + kBlueFromCb * scale * u);
        rgba[static_cast<usize>(row) * tracks.width + column] =
            kOpaqueBlackRgba | (blue << kBlueChannelShift) |
            (green << kGreenChannelShift) | red;
      }
    }
    return true;
  }

  bool AdvanceVideo(i64 presentation_time_ns, std::string& error) {
    while (has_next_video && next_video.time_ns <= presentation_time_ns) {
      const i64 packet_time_ns = next_video.time_ns;
      if (!DecodeVideoPacket(next_video, error)) return false;
      if (has_video_frame && packet_time_ns > current_video_time_ns) {
        frame_duration_ns = packet_time_ns - current_video_time_ns;
      }
      current_video_time_ns = packet_time_ns;
      has_video_frame = true;

      const auto result =
          demux.ReadPacket(tracks.video, video_cursor, next_video, demux_error);
      if (result == MatroskaReader::ReadResult::kError) {
        error = demux_error;
        return false;
      }
      has_next_video = result == MatroskaReader::ReadResult::kPacket;
      if (!has_next_video) video_eof = true;
    }
    return true;
  }

  PlaybackFrame Snapshot(i64 presentation_time_ns) const {
    PlaybackFrame frame;
    frame.video = {
        has_video_frame ? std::span<const u32>(rgba) : std::span<const u32>{},
        tracks.width, tracks.height, tracks.width, current_video_time_ns};
    frame.audio = audio_output;
    frame.audio.audible = audio_stream != nullptr;
    frame.clock_source = clock_source;
    frame.presentation_time_ns = presentation_time_ns;
    frame.video_finished =
        video_eof && has_video_frame &&
        presentation_time_ns >= current_video_time_ns + frame_duration_ns;
    return frame;
  }

  MatroskaReader demux;
  MovieTracks tracks;
  TrackCursor video_cursor;
  std::string demux_error;
  Packet next_video;
  bool has_next_video = false;
  bool video_eof = false;
  bool has_video_frame = false;
  i64 current_video_time_ns = 0;
  i64 frame_duration_ns = kFallbackFrameNanoseconds;
  std::vector<u32> rgba;

  FlacSource flac_source;
  drflac* audio_decoder = nullptr;
  SDL_AudioStream* audio_stream = nullptr;
  Audio::AudioDevice* audio_device = nullptr;
  SDL_AudioDeviceID audio_device_id = 0;
  AudioOutput audio_output;
  bool audio_ended = false;
  u64 last_queued_frames = 0;
  u64 last_consumed_frames = 0;
  u64 last_audio_progress_ns = 0;
  ClockSource clock_source = ClockSource::kLogicalTicks;
  i64 last_presentation_time_ns = 0;
  i64 realtime_fallback_time_ns = 0;
  i64 realtime_fallback_start_ns = 0;

  Dav1dContext* video_decoder = nullptr;
  Dav1dData pending_data{};
};

VS::Movie::PlaybackSession::PlaybackSession(std::unique_ptr<Impl> impl)
    : m_impl_(std::move(impl)) {}

VS::Movie::PlaybackSession::~PlaybackSession() = default;

VS::Movie::PlaybackSession::PlaybackSession(PlaybackSession&&) noexcept =
    default;

VS::Movie::PlaybackSession& VS::Movie::PlaybackSession::operator=(
    PlaybackSession&&) noexcept = default;

std::expected<VS::Movie::PlaybackSession, std::string>
VS::Movie::PlaybackSession::Open(std::string_view disc_path) {
  const Content::DataStore* store = Content::DataStore::Get();
  const Content::DataStore::Entry* entry =
      store != nullptr ? store->Find(disc_path) : nullptr;
  if (entry == nullptr || entry->form == "raw" || entry->form == "stream") {
    return std::unexpected(
        std::format("{}: no converted movie (run VS.Extract extract with "
                    "FFmpeg available)",
                    disc_path));
  }
  auto impl = std::make_unique<Impl>();
  auto opened = impl->Open(store->FileOf(*entry));
  if (!opened) return std::unexpected(opened.error());
  VS::Core::LogInfo("movie", "{}: {}x{}, audio {} Hz, {} ch, {} frames",
                    VS::Platform::PathText(store->FileOf(*entry)),
                    impl->tracks.width, impl->tracks.height,
                    impl->audio_output.sample_rate, impl->audio_output.channels,
                    impl->audio_output.total_frames);
  return PlaybackSession(std::move(impl));
}

std::expected<VS::Movie::PlaybackFrame, std::string>
VS::Movie::PlaybackSession::Update(std::uint32_t elapsed_ticks) {
  if (m_impl_ == nullptr || m_impl_->video_decoder == nullptr) {
    return std::unexpected("movie session is closed");
  }
  const i64 presentation_time_ns = m_impl_->PresentationTime(elapsed_ticks);
  std::string error;
  if (!m_impl_->AdvanceVideo(presentation_time_ns, error)) {
    return std::unexpected(std::move(error));
  }
  return m_impl_->Snapshot(presentation_time_ns);
}

std::expected<VS::Movie::AudioOutput, std::string>
VS::Movie::PlaybackSession::PumpAudio(std::uint32_t elapsed_ticks) {
  if (m_impl_ == nullptr || m_impl_->video_decoder == nullptr) {
    return std::unexpected("movie session is closed");
  }
  static_cast<void>(m_impl_->PresentationTime(elapsed_ticks));
  auto output = m_impl_->audio_output;
  output.audible = m_impl_->audio_stream != nullptr;
  return output;
}

void VS::Movie::PlaybackSession::Close() {
  if (m_impl_ != nullptr) m_impl_->Close();
  m_impl_.reset();
}
