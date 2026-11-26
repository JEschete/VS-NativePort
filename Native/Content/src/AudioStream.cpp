#include "Content/AudioStream.hpp"

#include <algorithm>
#include <cstddef>
#include <format>
#include <fstream>
#include <iterator>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "Content/AudioStreamJson.hpp"
#include "Content/JsonText.hpp"
#include "Platform/Paths.hpp"

namespace VS::Content {
namespace {
constexpr usize kChunkBytes = kAudioStreamChunkBytes;
constexpr u32 kMagic = 0x4F414B41;
constexpr u32 kStereo = 1;
constexpr u32 kRepeat = 2;
constexpr u32 kPitchUnity = 4096;
constexpr u32 kSourceRate = 44100;
constexpr u32 kMaximumPitch = kAudioStreamMaximumPitch;
constexpr unsigned kBitsPerByte = 8;
constexpr u64 kFnvOffsetBasis = 14695981039346656037ULL;
constexpr u64 kFnvPrime = 1099511628211ULL;
// The header words Sound_Cutscene_StartStream reads (25AC.c:6445-6476).
constexpr usize kChannelBytesWord = 16;
constexpr usize kLoopWord = 20;
constexpr usize kFormatWord = 24;
constexpr usize kPitchWord = 28;
constexpr usize kStartPageWord = 32;

u32 Word(std::span<const u8> bytes, usize offset) {
  u32 result = 0;
  for (usize index = 0; index < sizeof(result); ++index) {
    result |= static_cast<u32>(bytes[offset + index]) << (index * kBitsPerByte);
  }
  return result;
}

void PutWord(std::span<u8> bytes, usize offset, u32 value) {
  for (usize index = 0; index < sizeof(value); ++index) {
    bytes[offset + index] = static_cast<u8>(value >> (index * kBitsPerByte));
  }
}

// Whether the header word at `offset` holds a value the stream describes: the
// magic, or a word the player reads (the loop only when it repeats).
bool IsNamedWord(usize offset, bool repeat) {
  switch (offset) {
    case 0:
    case kChannelBytesWord:
    case kFormatWord:
    case kPitchWord:
    case kStartPageWord:
      return true;
    case kLoopWord:
      return repeat;
    default:
      return false;
  }
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

std::expected<AudioStreamInfo, std::string> ReadAudioStreamHeader(
    std::span<const u8> header) {
  if (header.size() < kAudioStreamHeaderBytes || Word(header, 0) != kMagic) {
    return std::unexpected("stream requires a complete AKAO header");
  }
  const u32 channel_bytes = Word(header, kChannelBytesWord);
  const u32 flags = Word(header, kFormatWord);
  const u32 pitch = Word(header, kPitchWord);
  if (channel_bytes == 0 || channel_bytes % PsxAdpcmDecoder::kBlockBytes != 0 ||
      (flags & ~(kStereo | kRepeat)) != 0 || pitch == 0 ||
      pitch > kMaximumPitch) {
    return std::unexpected("invalid stream length, format flags or pitch");
  }
  AudioStreamInfo info;
  info.channels = (flags & kStereo) != 0 ? 2 : 1;
  info.repeat = (flags & kRepeat) != 0;
  for (usize offset = 0; offset < kAudioStreamHeaderBytes;
       offset += sizeof(u32)) {
    if (!IsNamedWord(offset, info.repeat) && Word(header, offset) != 0) {
      return std::unexpected(std::format(
          "stream header word +{} is not zero; the player does not read it",
          offset));
    }
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
  if (info.repeat) {
    const u32 loop = Word(header, kLoopWord);
    const usize alignment = info.channels == 2 ? kChunkBytes * info.channels
                                               : PsxAdpcmDecoder::kBlockBytes;
    if (loop % alignment != 0 || loop / info.channels >= channel_bytes) {
      return std::unexpected("stream loop is outside aligned channel data");
    }
    info.loop_frame = loop / info.channels / PsxAdpcmDecoder::kBlockBytes *
                      PsxAdpcmDecoder::kBlockFrames;
  }
  info.start_page = Word(header, kStartPageWord);
  return info;
}

std::array<u8, kAudioStreamHeaderBytes> AudioStreamHeaderOf(
    const AudioStreamInfo& info) {
  std::array<u8, kAudioStreamHeaderBytes> header{};
  PutWord(header, 0, kMagic);
  PutWord(header, kChannelBytesWord,
          info.frames / PsxAdpcmDecoder::kBlockFrames *
              PsxAdpcmDecoder::kBlockBytes);
  if (info.repeat) {
    PutWord(header, kLoopWord,
            static_cast<u32>(static_cast<u64>(info.loop_frame) /
                             PsxAdpcmDecoder::kBlockFrames *
                             PsxAdpcmDecoder::kBlockBytes * info.channels));
  }
  PutWord(header, kFormatWord,
          (info.channels == 2 ? kStereo : 0) | (info.repeat ? kRepeat : 0));
  PutWord(header, kPitchWord, info.source_pitch);
  PutWord(header, kStartPageWord, info.start_page);
  return header;
}

std::expected<AudioStreamInfo, std::string> InspectAudioStream(
    std::span<const u8> bytes) {
  auto info = ReadAudioStreamHeader(bytes);
  if (!info) return info;
  const u32 channel_bytes = Word(bytes, kChannelBytesWord);
  if (ChannelOffset(channel_bytes - 1, info->channels - 1, info->channels) >=
      bytes.size()) {
    return std::unexpected("stream channel payload extends outside its block");
  }
  // Stable content identity, not an authenticity or cryptographic check.
  u64 hash = kFnvOffsetBasis;
  for (const u8 byte : bytes) {
    hash = (hash ^ byte) * kFnvPrime;
  }
  info->id = std::format("{:016x}", hash);
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

std::string AudioStreamFile(std::string_view id) {
  return std::format("audio/streams/akao/{}/stream.json", id);
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
  std::ofstream out(directory / "stream.json", std::ios::binary);
  out << FormatJsonText(AudioStreamJson(stream.info)) << '\n';
  out.close();
  if (!out) return std::unexpected("writing stream metadata");
  return {};
}

std::expected<void, std::string> ValidateAudioStreamAssets(
    const AudioStreamInfo& info, const std::filesystem::path& data_root) {
  std::ifstream in(StreamDirectory(data_root, info) / "stream.json",
                   std::ios::binary);
  if (!in) {
    return std::unexpected("stream " + info.id +
                           " is missing; run VS.Extract streams");
  }
  const std::string text{std::istreambuf_iterator<char>(in), {}};
  const std::string name = AudioStreamFile(info.id);
  auto json = ParseJsonText(std::as_bytes(std::span(text)), name);
  if (!json) return std::unexpected(json.error());
  auto described = AudioStreamInfoOf(*json, name);
  if (!described) return std::unexpected(described.error());
  if (*described != info) {
    return std::unexpected(
        std::format("{} differs from the stream its effect package holds; run "
                    "VS.Extract streams to write it again",
                    name));
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
  return data_root / Platform::Utf8Path(XaAudioFile(source_path));
}

std::string XaAudioFile(std::string_view source_path) {
  return "audio/streams/xa/" + std::string(source_path) + ".flac";
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
