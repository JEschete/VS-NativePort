#include "Content/AudioBank.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <fstream>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "nlohmann/json.hpp"
namespace VS::Content {
namespace {
constexpr usize kHeaderBytes = 64;
constexpr usize kInstrumentBytes = 16;
constexpr usize kLegacyInstrumentBytes = 64;
constexpr usize kLegacyEnvelopeOffset = 56;
constexpr u64 kLegacyUnityPitch = 1U << 20;
constexpr usize kLegacyPitchOffset = 8;
constexpr i16 kChromaticNotes = 12;
constexpr i16 kLegacyRootNote = 72;
constexpr unsigned kPositivePitchDivisor = 32;
constexpr unsigned kNegativePitchDivisor = 16;
constexpr usize kBlockBytes = 16;
constexpr usize kBlockSamples = 28;
constexpr usize kTransferAlignment = 64;
constexpr u32 kSampleRate = 44100;
constexpr u32 kMaxInstruments = 256;
constexpr usize kBankIdOffset = 4;
constexpr usize kDestinationOffset = 16;
constexpr usize kSampleSizeOffset = 20;
constexpr usize kFirstInstrumentOffset = 24;
constexpr usize kInstrumentCountOffset = 28;
constexpr u8 kEndFlag = 1;
constexpr u8 kRepeatFlag = 2;
constexpr u8 kLoopFlag = 4;
constexpr u32 kVersion = 1;
constexpr int kByteBits = 8;
constexpr int kHalfWordBits = 16;
constexpr u16 kByteMask = 255;
constexpr int kMaximumAdpcmShift = 12;
constexpr usize kLoopOffset = 4;
constexpr usize kFineTuneOffset = 8;
constexpr usize kRootNoteOffset = 10;
constexpr usize kAdsrLowerOffset = 12;
constexpr usize kAdsrUpperOffset = 14;
constexpr u32 kRiffHeaderSize = 36;
constexpr std::streamsize kFourCcSize = 4;
constexpr auto kPredictors = std::to_array<std::array<int, 2>>(
    {{0, 0}, {60, 0}, {115, -52}, {98, -55}, {122, -60}});

u16 Read16(std::span<const u8> bytes, usize offset) {
  return static_cast<u16>(bytes[offset] | (bytes[offset + 1] << kByteBits));
}

u32 Read32(std::span<const u8> bytes, usize offset) {
  return Read16(bytes, offset) |
         (static_cast<u32>(Read16(bytes, offset + 2)) << kHalfWordBits);
}

template <class T>
bool ReadNumber(const nlohmann::json& value, T* result) {
  if (value.is_number_unsigned()) {
    const auto number = value.get<u64>();
    if (!std::in_range<T>(number)) return false;
    *result = static_cast<T>(number);
    return true;
  }
  if (value.is_number_integer()) {
    const auto number = value.get<i64>();
    if (!std::in_range<T>(number)) return false;
    *result = static_cast<T>(number);
    return true;
  }
  return false;
}

template <class T>
bool ReadInteger(const nlohmann::json& entry, const char* key, T* result) {
  return ReadNumber(entry.at(key), result);
}

bool LegacyRecords(std::span<const u8> bytes, u32 count, u32 size) {
  const auto end = kHeaderBytes + count * kLegacyInstrumentBytes;
  if (end > bytes.size() || bytes.size() - end != size) return false;
  for (usize index = 0; index < count; ++index) {
    const auto offset = kHeaderBytes + index * kLegacyInstrumentBytes;
    const auto record = bytes.subspan(offset, kLegacyInstrumentBytes);
    if (Read32(record, 0) > size || Read32(record, kLoopOffset) > size) {
      return false;
    }
    const auto base = Read32(record, 8);
    for (usize note = 1; note < kChromaticNotes; ++note) {
      const auto pitch = Read32(record, 8 + note * 4);
      if (base == 0 ? pitch != 0
                    : pitch <= base || pitch >= static_cast<u64>(base) * 2) {
        return false;
      }
    }
    const auto envelope = record.subspan(kLegacyEnvelopeOffset);
    constexpr std::array<u8, 8> kLimits{127, 15, 15, 127, 31, 7, 7, 7};
    for (usize field = 0; field < kLimits.size(); ++field) {
      if (envelope[field] > kLimits[field]) return false;
    }
  }
  return true;
}

AudioInstrument ReadInstrument(std::span<const u8> record) {
  AudioInstrument instrument;
  instrument.sample_offset = Read32(record, 0);
  instrument.loop_offset = Read32(record, kLoopOffset);
  instrument.source_record.assign(record.begin(), record.end());
  if (record.size() == kInstrumentBytes) {
    instrument.fine_tune = std::bit_cast<i16>(Read16(record, kFineTuneOffset));
    instrument.root_note = std::bit_cast<i16>(Read16(record, kRootNoteOffset));
    instrument.adsr_lower = Read16(record, kAdsrLowerOffset);
    instrument.adsr_upper = Read16(record, kAdsrUpperOffset);
    return instrument;
  }
  // AKAO 3.0 articulation: Q20 chromatic pitches at +8, unpacked ADSR at +56.
  // WAVE0032 retains this older record; normal VS records are 16 bytes.
  u64 pitch = Read32(record, kLegacyPitchOffset);
  instrument.available = pitch != 0;
  instrument.root_note = kLegacyRootNote;
  while (pitch >= kLegacyUnityPitch * 2) {
    pitch /= 2;
    instrument.root_note -= kChromaticNotes;
  }
  while (pitch != 0 && pitch < kLegacyUnityPitch / 2) {
    pitch *= 2;
    instrument.root_note += kChromaticNotes;
  }
  instrument.fine_tune =
      pitch >= kLegacyUnityPitch
          ? static_cast<i16>((pitch - kLegacyUnityPitch) /
                             kPositivePitchDivisor)
          : std::bit_cast<i16>(static_cast<u16>(pitch / kNegativePitchDivisor));
  const auto envelope = record.subspan(kLegacyEnvelopeOffset);
  const auto lower =
      static_cast<u16>(((envelope[5] & 4) << 13) | (envelope[0] << 8) |
                       (envelope[1] << 4) | envelope[2]);
  const auto upper = static_cast<u16>(
      ((envelope[6] & 4) << 13) | ((envelope[6] & 2) << 13) |
      (envelope[3] << 6) | ((envelope[7] & 4) << 3) | envelope[4]);
  instrument.adsr_lower = lower;
  instrument.adsr_upper = upper;
  return instrument;
}

std::expected<std::vector<u8>, std::string> ReadSourceRecord(
    const nlohmann::json& entry) {
  std::vector<u8> bytes;
  if (!entry.contains("source_record")) return bytes;
  const auto& record = entry.at("source_record");
  if (!record.is_array() ||
      (!record.empty() && record.size() != kInstrumentBytes &&
       record.size() != kLegacyInstrumentBytes)) {
    return std::unexpected("invalid original instrument record");
  }
  for (const auto& value : record) {
    u8 byte = 0;
    if (!ReadNumber(value, &byte)) {
      return std::unexpected("invalid original instrument byte");
    }
    bytes.push_back(byte);
  }
  return bytes;
}

std::filesystem::path BankDirectory(const std::filesystem::path& root, u32 id) {
  return root / "audio" / "banks" / std::to_string(id);
}

std::expected<AudioSample, std::string> DecodeSample(
    std::span<const u8> bytes, const AudioInstrument& instrument) {
  if (instrument.sample_offset % kBlockBytes != 0 ||
      instrument.sample_offset >= bytes.size()) {
    return std::unexpected("sample offset outside aligned ADPCM data");
  }
  AudioSample sample;
  PsxAdpcmDecoder decoder;
  u32 loop_offset = instrument.loop_offset;
  for (usize offset = instrument.sample_offset;
       offset + kBlockBytes <= bytes.size(); offset += kBlockBytes) {
    auto decoded = decoder.Decode(
        std::span<const u8, kBlockBytes>(bytes.data() + offset, kBlockBytes));
    if (!decoded) return std::unexpected(decoded.error());
    const u8 flags = bytes[offset + 1];
    if ((flags & kLoopFlag) != 0) {
      loop_offset = static_cast<u32>(offset);
    }
    sample.pcm.insert(sample.pcm.end(), decoded->begin(), decoded->end());
    if ((flags & kEndFlag) != 0) {
      sample.repeat = (flags & kRepeatFlag) != 0;
      if (sample.repeat) {
        if (loop_offset < instrument.sample_offset || loop_offset > offset ||
            loop_offset % kBlockBytes != 0) {
          return std::unexpected("repeat offset outside decoded sample");
        }
        sample.loop_frame =
            static_cast<u32>((loop_offset - instrument.sample_offset) /
                             kBlockBytes * kBlockSamples);
      }
      return sample;
    }
  }
  return std::unexpected("sample has no end block");
}

void Write16(std::ostream& out, u16 value) {
  out.put(static_cast<char>(value & kByteMask));
  out.put(static_cast<char>(value >> kByteBits));
}

void Write32(std::ostream& out, u32 value) {
  Write16(out, static_cast<u16>(value));
  Write16(out, static_cast<u16>(value >> kHalfWordBits));
}

bool WriteWave(const std::filesystem::path& path, std::span<const i16> pcm,
               u32 sample_rate) {
  constexpr u32 kFormatChunkSize = 16;
  constexpr u16 kPcmFormat = 1;
  constexpr u16 kBitsPerSample = 16;
  const auto data_bytes = static_cast<u32>(pcm.size_bytes());
  std::ofstream out(path, std::ios::binary);
  out.write("RIFF", kFourCcSize);
  Write32(out, kRiffHeaderSize + data_bytes);
  out.write("WAVEfmt ", kFourCcSize * 2);
  Write32(out, kFormatChunkSize);
  Write16(out, kPcmFormat);
  Write16(out, 1);
  Write32(out, sample_rate);
  Write32(out, sample_rate * sizeof(i16));
  Write16(out, sizeof(i16));
  Write16(out, kBitsPerSample);
  out.write("data", kFourCcSize);
  Write32(out, data_bytes);
  for (const auto value : pcm) {
    Write16(out, std::bit_cast<u16>(value));
  }
  out.close();
  return out.good();
}
}  // namespace

std::expected<std::array<i16, PsxAdpcmDecoder::kBlockFrames>, std::string>
PsxAdpcmDecoder::Decode(std::span<const u8, kBlockBytes> block) {
  const auto filter = static_cast<usize>(block[0] >> 4);
  const int shift = block[0] & 15;
  if (filter >= kPredictors.size() || shift > kMaximumAdpcmShift) {
    return std::unexpected("invalid ADPCM predictor or shift");
  }
  std::array<i16, kBlockFrames> output{};
  for (usize index = 0; index < kBlockFrames; ++index) {
    const int packed = block[2 + index / 2];
    const int nibble = (packed >> ((index % 2) * 4)) & 15;
    const int signed_nibble = nibble < 8 ? nibble : nibble - 16;
    const int prediction = (m_previous_ * kPredictors[filter][0] +
                            m_older_ * kPredictors[filter][1] + 32) >>
                           6;
    const int value = std::clamp(((signed_nibble * 4096) >> shift) + prediction,
                                 -32768, 32767);
    output[index] = static_cast<i16>(value);
    m_older_ = m_previous_;
    m_previous_ = value;
  }
  return output;
}

std::expected<void, std::string> ExportAudioSample(
    const AudioSample& sample, const std::filesystem::path& instrument_path,
    u32 sample_rate) {
  if (sample.pcm.empty() || sample_rate == 0 ||
      sample_rate > std::numeric_limits<u32>::max() / sizeof(i16) ||
      sample.pcm.size() >
          (std::numeric_limits<u32>::max() - kRiffHeaderSize) / sizeof(i16) ||
      (sample.repeat && sample.loop_frame >= sample.pcm.size())) {
    return std::unexpected("invalid audio sample size, rate or loop");
  }
  auto wave_path = instrument_path;
  wave_path.replace_extension(".wav");
  if (!WriteWave(wave_path, sample.pcm, sample_rate)) {
    return std::unexpected("writing audio sample " + wave_path.string());
  }
  std::ofstream sfz(instrument_path);
  sfz << "<region> sample=" << wave_path.filename().string()
      << " key=60 pitch_keytrack=0"
      << " amp_veltrack=0 ampeg_attack=0 ampeg_decay=0 ampeg_sustain=100"
      << " ampeg_release=0 bend_up=14400 bend_down=-14400 bend_smooth=0 "
         "loop_mode="
      << (sample.repeat ? "loop_continuous" : "no_loop");
  if (sample.repeat) {
    sfz << " loop_start=" << sample.loop_frame
        << " loop_end=" << sample.pcm.size() - 1;
  }
  sfz << '\n';
  sfz.close();
  if (!sfz) {
    return std::unexpected("writing SFZ instrument " +
                           instrument_path.string());
  }
  return {};
}

std::expected<AudioBank, std::string> DecodeAudioBank(
    std::span<const u8> bytes) {
  constexpr std::array<u8, 4> kMagic{'A', 'K', 'A', 'O'};
  if (bytes.size() < kHeaderBytes ||
      !std::equal(kMagic.begin(), kMagic.end(), bytes.begin())) {
    return std::unexpected("missing AKAO wave-bank header");
  }
  const auto count = Read32(bytes, kInstrumentCountOffset);
  const auto first = Read32(bytes, kFirstInstrumentOffset);
  const auto size = Read32(bytes, kSampleSizeOffset);
  if (count == 0 || first >= kMaxInstruments ||
      count > kMaxInstruments - first) {
    return std::unexpected("instrument range exceeds native bank slots");
  }
  const usize stride = LegacyRecords(bytes, count, size)
                           ? kLegacyInstrumentBytes
                           : kInstrumentBytes;
  const usize data_offset = kHeaderBytes + count * stride;
  if (bytes.size() < data_offset || size == 0 || size % kBlockBytes != 0) {
    return std::unexpected("truncated or unaligned wave-bank data");
  }
  const usize available = std::min<usize>(size, bytes.size() - data_offset);
  // WAVE banks can omit the final DMA alignment padding. Every referenced
  // sample must still terminate within the bytes actually present.
  if (available % kBlockBytes != 0 ||
      (available < size && (available + kTransferAlignment - 1) /
                                   kTransferAlignment * kTransferAlignment !=
                               size)) {
    return std::unexpected("truncated or unaligned wave-bank data");
  }
  AudioBank bank;
  bank.id = Read32(bytes, kBankIdOffset);
  bank.first_instrument = first;
  bank.source_destination = Read32(bytes, kDestinationOffset);
  for (u32 index = 0; index < count; ++index) {
    const usize offset = kHeaderBytes + index * stride;
    auto instrument = ReadInstrument(bytes.subspan(offset, stride));
    const auto start = instrument.sample_offset;
    const bool erased_padding =
        start < available && start % kBlockBytes == 0 &&
        size % kTransferAlignment == 0 && size - start < kTransferAlignment &&
        std::ranges::all_of(
            bytes.subspan(data_offset + start, available - start),
            [](u8 byte) { return byte == kByteMask; });
    if (!instrument.available || (start == instrument.loop_offset &&
                                  (start == available || erased_padding))) {
      instrument.available = false;
      bank.instruments.push_back(instrument);
      bank.samples.emplace_back();
      continue;
    }
    auto sample =
        DecodeSample(bytes.subspan(data_offset, available), instrument);
    if (!sample) {
      return std::unexpected("instrument " + std::to_string(index) + ": " +
                             sample.error());
    }
    bank.instruments.push_back(instrument);
    bank.samples.push_back(std::move(*sample));
  }
  return bank;
}

std::filesystem::path AudioInstrumentPath(
    const std::filesystem::path& data_root, u32 bank_id, usize instrument) {
  return BankDirectory(data_root, bank_id) /
         (std::to_string(instrument) + ".sfz");
}

std::expected<void, std::string> ExportAudioBank(
    const AudioBank& bank, const std::filesystem::path& data_root) {
  if (bank.instruments.empty() ||
      bank.instruments.size() != bank.samples.size()) {
    return std::unexpected("bank export requires decoded samples");
  }
  const auto directory = BankDirectory(data_root, bank.id);
  std::error_code error;
  std::filesystem::create_directories(directory, error);
  if (error) {
    return std::unexpected("creating audio bank directory: " + error.message());
  }
  nlohmann::json metadata{{"version", kVersion},
                          {"bank_id", bank.id},
                          {"first_instrument", bank.first_instrument},
                          {"source_destination", bank.source_destination},
                          {"instruments", nlohmann::json::array()}};
  for (usize index = 0; index < bank.instruments.size(); ++index) {
    const auto& instrument = bank.instruments[index];
    const auto& sample = bank.samples[index];
    metadata["instruments"].push_back(
        {{"sample_offset", instrument.sample_offset},
         {"loop_offset", instrument.loop_offset},
         {"fine_tune", instrument.fine_tune},
         {"root_note", instrument.root_note},
         {"adsr_lower", instrument.adsr_lower},
         {"adsr_upper", instrument.adsr_upper},
         {"available", instrument.available},
         {"source_record", instrument.source_record}});
    if (!instrument.available) {
      continue;
    }
    if (auto result = ExportAudioSample(
            sample, AudioInstrumentPath(data_root, bank.id, index),
            kSampleRate);
        !result) {
      return result;
    }
  }
  std::ofstream out(directory / "bank.json");
  out << metadata.dump(2) << '\n';
  out.close();
  if (!out) {
    return std::unexpected("writing audio bank metadata");
  }
  return {};
}

std::expected<AudioBank, std::string> LoadAudioBank(
    const std::filesystem::path& data_root, u32 id) {
  std::ifstream in(BankDirectory(data_root, id) / "bank.json");
  if (!in) {
    return std::unexpected("audio bank " + std::to_string(id) +
                           " is missing; run VS.Extract audio");
  }
  try {
    nlohmann::json metadata;
    in >> metadata;
    AudioBank bank;
    u32 version = 0;
    if (!ReadInteger(metadata, "version", &version) || version != kVersion ||
        !ReadInteger(metadata, "bank_id", &bank.id) || bank.id != id) {
      return std::unexpected("unsupported audio bank version or mismatched ID");
    }
    if (!ReadInteger(metadata, "first_instrument", &bank.first_instrument) ||
        !ReadInteger(metadata, "source_destination",
                     &bank.source_destination) ||
        !metadata.at("instruments").is_array()) {
      return std::unexpected("invalid native bank metadata");
    }
    for (const auto& entry : metadata.at("instruments")) {
      AudioInstrument instrument;
      if (!ReadInteger(entry, "sample_offset", &instrument.sample_offset) ||
          !ReadInteger(entry, "loop_offset", &instrument.loop_offset) ||
          !ReadInteger(entry, "fine_tune", &instrument.fine_tune) ||
          !ReadInteger(entry, "root_note", &instrument.root_note) ||
          !ReadInteger(entry, "adsr_lower", &instrument.adsr_lower) ||
          !ReadInteger(entry, "adsr_upper", &instrument.adsr_upper)) {
        return std::unexpected("out-of-range native instrument metadata");
      }
      instrument.available = entry.at("available").get<bool>();
      auto record = ReadSourceRecord(entry);
      if (!record) return std::unexpected(record.error());
      instrument.source_record = std::move(*record);
      bank.instruments.push_back(instrument);
    }
    if (bank.instruments.empty() || bank.first_instrument >= kMaxInstruments ||
        bank.instruments.size() > kMaxInstruments - bank.first_instrument) {
      return std::unexpected("invalid audio bank instrument range");
    }
    return bank;
  } catch (const nlohmann::json::exception& error) {
    return std::unexpected("invalid audio bank metadata: " +
                           std::string(error.what()));
  }
}
}  // namespace VS::Content
