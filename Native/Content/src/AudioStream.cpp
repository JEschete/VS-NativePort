#include "Content/AudioStream.hpp"

#include <algorithm>
#include <format>
#include <fstream>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "nlohmann/json.hpp"

namespace VS::Content {
namespace {
constexpr usize kChunkBytes = 0x800;
constexpr u32 kMagic = 0x4F414B41;
constexpr u32 kStereo = 1;
constexpr u32 kRepeat = 2;
constexpr u32 kPitchUnity = 4096;
constexpr u32 kSourceRate = 44100;
constexpr u32 kMaximumPitch = 16384;
constexpr unsigned kBitsPerByte = 8;
constexpr u64 kFnvOffsetBasis = 14695981039346656037ULL;
constexpr u64 kFnvPrime = 1099511628211ULL;

u32 Word(std::span<const u8> bytes, usize offset) {
  u32 result = 0;
  for (usize index = 0; index < sizeof(result); ++index) {
    result |= static_cast<u32>(bytes[offset + index]) << (index * kBitsPerByte);
  }
  return result;
}

usize ChannelOffset(usize byte, usize channel, usize channels) {
  return kAudioStreamHeaderBytes +
         (byte / kChunkBytes * channels + channel) * kChunkBytes +
         byte % kChunkBytes;
}

std::filesystem::path StreamDirectory(const std::filesystem::path& root,
                                      const AudioStreamInfo& info) {
  return root / "audio" / "streams" / "akao" / info.id;
}

nlohmann::json Metadata(const AudioStreamInfo& info) {
  return {{"version", 1},
          {"id", info.id},
          {"source_pitch", info.source_pitch},
          {"sample_rate", info.sample_rate},
          {"channels", info.channels},
          {"frames", info.frames},
          {"repeat", info.repeat},
          {"loop_frame", info.loop_frame},
          {"source_header", info.source_header}};
}
}  // namespace

std::expected<std::vector<EmbeddedAudioStream>, std::string>
FindEffectAudioStreams(std::span<const u8> package) {
  constexpr u32 kStreamBlock = 13;
  std::vector<EmbeddedAudioStream> streams;
  usize offset = 0;
  while (package.size() - offset >= sizeof(u32)) {
    const u32 meta = Word(package, offset);
    const u32 kind = meta >> 16;
    const usize size = meta & 0xFFFF;
    if (kind == 0) return streams;
    if (size < sizeof(u32) || size > package.size() - offset) {
      return std::unexpected("effect block extends outside package");
    }
    if (kind == kStreamBlock) {
      const usize payload = offset + sizeof(u32);
      streams.push_back(
          {payload, package.subspan(payload, size - sizeof(u32))});
    }
    offset += size;
  }
  return std::unexpected("effect package has no terminator");
}

std::expected<AudioStreamInfo, std::string> InspectAudioStream(
    std::span<const u8> bytes) {
  if (bytes.size() < kAudioStreamHeaderBytes || Word(bytes, 0) != kMagic) {
    return std::unexpected("stream requires a complete AKAO header");
  }
  const u32 channel_bytes = Word(bytes, 16);
  const u32 flags = Word(bytes, 24);
  const u32 pitch = Word(bytes, 28);
  if (channel_bytes == 0 || channel_bytes % PsxAdpcmDecoder::kBlockBytes != 0 ||
      (flags & ~(kStereo | kRepeat)) != 0 || pitch == 0 ||
      pitch > kMaximumPitch) {
    return std::unexpected("invalid stream length, format flags or pitch");
  }
  AudioStreamInfo info;
  info.channels = (flags & kStereo) != 0 ? 2 : 1;
  if (ChannelOffset(channel_bytes - 1, info.channels - 1, info.channels) >=
      bytes.size()) {
    return std::unexpected("stream channel payload extends outside its block");
  }
  const u64 frames = static_cast<u64>(channel_bytes) /
                     PsxAdpcmDecoder::kBlockBytes *
                     PsxAdpcmDecoder::kBlockFrames;
  if (frames > std::numeric_limits<u32>::max()) {
    return std::unexpected("stream frame count exceeds native metadata");
  }
  info.frames = static_cast<u32>(frames);
  info.source_pitch = pitch;
  info.sample_rate = (pitch * kSourceRate + kPitchUnity / 2) / kPitchUnity;
  info.repeat = (flags & kRepeat) != 0;
  if (info.repeat) {
    const u32 loop = Word(bytes, 20);
    const usize alignment = info.channels == 2 ? kChunkBytes * info.channels
                                               : PsxAdpcmDecoder::kBlockBytes;
    if (loop % alignment != 0 || loop / info.channels >= channel_bytes) {
      return std::unexpected("stream loop is outside aligned channel data");
    }
    info.loop_frame = loop / info.channels / PsxAdpcmDecoder::kBlockBytes *
                      PsxAdpcmDecoder::kBlockFrames;
  }
  std::copy_n(bytes.begin(), kAudioStreamHeaderBytes,
              info.source_header.begin());
  // Stable content identity, not an authenticity or cryptographic check.
  u64 hash = kFnvOffsetBasis;
  for (const u8 byte : bytes) {
    hash = (hash ^ byte) * kFnvPrime;
  }
  info.id = std::format("{:016x}", hash);
  return info;
}

std::expected<AudioStreamAsset, std::string> DecodeAudioStream(
    std::span<const u8> bytes) {
  auto info = InspectAudioStream(bytes);
  if (!info) return std::unexpected(info.error());
  AudioStreamAsset stream{*info, {}};
  const usize channel_bytes = Word(bytes, 16);
  for (usize channel = 0; channel < info->channels; ++channel) {
    PsxAdpcmDecoder decoder;
    AudioSample sample;
    sample.repeat = info->repeat;
    sample.loop_frame = info->loop_frame;
    sample.pcm.reserve(info->frames);
    for (usize byte = 0; byte < channel_bytes;
         byte += PsxAdpcmDecoder::kBlockBytes) {
      const usize offset = ChannelOffset(byte, channel, info->channels);
      auto block =
          decoder.Decode(std::span<const u8, PsxAdpcmDecoder::kBlockBytes>(
              bytes.data() + offset, PsxAdpcmDecoder::kBlockBytes));
      if (!block) return std::unexpected(block.error());
      // The original 0x800-byte buffer-end flags are not sample loop markers.
      sample.pcm.insert(sample.pcm.end(), block->begin(), block->end());
    }
    stream.channels.push_back(std::move(sample));
  }
  return stream;
}

std::filesystem::path AudioStreamInstrumentPath(
    const std::filesystem::path& data_root, const AudioStreamInfo& info,
    usize channel) {
  return StreamDirectory(data_root, info) / (std::to_string(channel) + ".sfz");
}

std::expected<void, std::string> ExportAudioStream(
    const AudioStreamAsset& stream, const std::filesystem::path& data_root) {
  if (stream.channels.size() != stream.info.channels ||
      stream.channels.empty() || stream.channels.size() > 2) {
    return std::unexpected("stream export requires one or two channels");
  }
  std::error_code error;
  const auto directory = StreamDirectory(data_root, stream.info);
  std::filesystem::create_directories(directory, error);
  if (error) {
    return std::unexpected("creating stream directory: " + error.message());
  }
  for (usize channel = 0; channel < stream.channels.size(); ++channel) {
    const auto& sample = stream.channels[channel];
    if (sample.pcm.size() != stream.info.frames ||
        sample.repeat != stream.info.repeat ||
        sample.loop_frame != stream.info.loop_frame) {
      return std::unexpected("stream channel metadata does not match samples");
    }
    if (auto result = ExportAudioSample(
            sample, AudioStreamInstrumentPath(data_root, stream.info, channel),
            stream.info.sample_rate);
        !result) {
      return result;
    }
  }
  std::ofstream out(directory / "stream.json");
  out << Metadata(stream.info).dump(2) << '\n';
  out.close();
  if (!out) return std::unexpected("writing stream metadata");
  return {};
}

std::expected<void, std::string> ValidateAudioStreamAssets(
    const AudioStreamInfo& info, const std::filesystem::path& data_root) {
  std::ifstream in(StreamDirectory(data_root, info) / "stream.json");
  if (!in) {
    return std::unexpected("stream " + info.id +
                           " is missing; run VS.Extract streams");
  }
  try {
    nlohmann::json metadata;
    in >> metadata;
    const auto expected = Metadata(info);
    for (const auto& [key, value] : expected.items()) {
      if (!metadata.contains(key) || metadata.at(key) != value) {
        return std::unexpected("stream metadata mismatch: " + key);
      }
    }
  } catch (const nlohmann::json::exception& error) {
    return std::unexpected(std::string("invalid stream metadata: ") +
                           error.what());
  }
  for (usize channel = 0; channel < info.channels; ++channel) {
    const auto instrument = AudioStreamInstrumentPath(data_root, info, channel);
    auto wave = instrument;
    wave.replace_extension(".wav");
    std::error_code error;
    if (!std::filesystem::is_regular_file(instrument, error) || error ||
        !std::filesystem::is_regular_file(wave, error) || error) {
      return std::unexpected("stream channel asset is missing or unreadable");
    }
  }
  return {};
}

std::filesystem::path XaAudioPath(const std::filesystem::path& data_root,
                                  std::string_view source_path) {
  return data_root / "audio" / "streams" / "xa" /
         (std::string(source_path) + ".flac");
}

// Original XA filter is file 1/channel 0 at single speed (main.c:9188-9194).
// ENDING.XA repeats each payload four times; only one fits its playback period.
std::expected<std::vector<u8>, std::string> SelectXaAudioSectors(
    std::span<const u8> raw_sectors) {
  constexpr usize kSectorBytes = 2352;
  constexpr usize kPayloadOffset = 24;
  constexpr usize kPayloadBytes = 2304;
  constexpr usize kModeOffset = 15;
  constexpr usize kSubheaderOffset = 16;
  constexpr usize kSubheaderBytes = 4;
  constexpr usize kCodingOffset = 3;
  constexpr u8 kAudioSubmode = 0x64;
  constexpr u8 kReservedCodingBits = 0xAA;
  if (raw_sectors.empty() || raw_sectors.size() % kSectorBytes != 0) {
    return std::unexpected("XA source is not a whole number of raw sectors");
  }
  std::vector<u8> selected;
  usize next_sector = 0;
  u8 format = 0;
  for (usize index = 0; index < raw_sectors.size() / kSectorBytes; ++index) {
    const auto sector = raw_sectors.subspan(index * kSectorBytes, kSectorBytes);
    const auto subheader = sector.subspan(kSubheaderOffset, kSubheaderBytes);
    if (sector[kModeOffset] != 2 ||
        !std::equal(subheader.begin(), subheader.end(),
                    sector.begin() + kSubheaderOffset + kSubheaderBytes)) {
      return std::unexpected("XA mode or duplicated subheader is invalid");
    }
    if (subheader[0] != 1 || subheader[1] != 0 ||
        (subheader[2] & kAudioSubmode) != kAudioSubmode) {
      continue;
    }
    const u8 coding = subheader[kCodingOffset];
    if ((coding & kReservedCodingBits) != 0 ||
        (!selected.empty() && coding != format)) {
      return std::unexpected(
          "XA coding is invalid or changes within the track");
    }
    if (index < next_sector) {
      const auto previous = std::span<const u8>(selected).last(kSectorBytes);
      if (!std::equal(sector.begin() + kPayloadOffset,
                      sector.begin() + kPayloadOffset + kPayloadBytes,
                      previous.begin() + kPayloadOffset)) {
        return std::unexpected("XA redundant sector contains different audio");
      }
      continue;
    }
    if (index != next_sector) {
      return std::unexpected(
          "XA selected channel has an unsupported timing gap");
    }
    format = coding;
    const unsigned channels = (coding & 1) != 0 ? 2 : 1;
    const unsigned sample_rate = (coding & 4) != 0 ? 18900 : 37800;
    const unsigned samples = (coding & 0x10) != 0 ? 2016 : 4032;
    next_sector =
        index + (samples / channels) * kXaSectorsPerSecond / sample_rate;
    selected.insert(selected.end(), sector.begin(), sector.end());
  }
  if (selected.empty() || next_sector != raw_sectors.size() / kSectorBytes) {
    return std::unexpected(
        "XA track is empty or ends inside a playback period");
  }
  return selected;
}

}  // namespace VS::Content
