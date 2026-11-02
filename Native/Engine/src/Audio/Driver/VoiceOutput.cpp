// Hand-maintained (tools/port/owned.csv).
#include "Audio/VoiceOutput.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace VS::Audio::Driver {
namespace {
constexpr std::uint32_t kVolumeMask = 0x7FFF;
constexpr std::uint32_t kVolumeSign = 0x4000;
constexpr int kVolumeModulus = 0x8000;
constexpr unsigned kVolumeScaleBits = 7;
constexpr unsigned kReleaseModeBit = 5;
constexpr unsigned kReleaseModeShift = 2;

bool ValidVoice(int voice) {
  return voice >= 0 && static_cast<std::uint32_t>(voice) < kVoiceCount;
}

template <typename Visit>
VoiceMaskResult VisitVoices(std::uint32_t mask, Visit visit) {
  for (std::uint32_t voice = 0; voice < kVoiceCount; ++voice) {
    visit(voice, (mask & (1U << voice)) != 0);
  }
  return {mask & kAllVoices, mask & ~kAllVoices};
}

VoiceMaskResult SetModeMask(VoiceOutputHost& host, std::uint32_t mask,
                            VoiceMode mode) {
  return VisitVoices(mask, [&](std::uint32_t voice, bool enabled) {
    host.SetVoiceMode(voice, mode, enabled);
  });
}
}  // namespace

std::uint16_t VoiceVolumeRegister(std::uint32_t value, std::uint32_t scale) {
  if (scale != 0) value = (value * scale) >> kVolumeScaleBits;
  return static_cast<std::uint16_t>(value & kVolumeMask);
}

float VoiceVolumeGain(std::uint16_t volume) {
  const std::uint32_t value = volume & kVolumeMask;
  const int signed_value = (value & kVolumeSign) != 0
                               ? static_cast<int>(value) - kVolumeModulus
                               : static_cast<int>(value);
  return static_cast<float>(signed_value) / kVolumeSign;
}

float VoiceSampleRateRatio(std::uint16_t sample_rate) {
  return static_cast<float>(
             std::min<std::uint32_t>(sample_rate, kMaximumSampleRate)) /
         kUnitySampleRate;
}

std::optional<EnvelopeRate> ReleaseEnvelope(int rate, std::uint32_t mode) {
  if (rate < 0 || rate > kMaximumReleaseRate || mode > kMaximumReleaseMode) {
    return std::nullopt;
  }
  const auto upper = static_cast<std::uint16_t>(
      static_cast<std::uint32_t>(rate) |
      ((mode >> kReleaseModeShift) << kReleaseModeBit));
  return DecodeAkaoEnvelope(0, upper).release;
}

// decomp: SetVoiceKeyOn (src/SLUS_010.40/25AC.c:1102).
VoiceMaskResult KeyOnVoices(VoiceOutputHost& host, std::uint32_t mask) {
  return VisitVoices(mask, [&](std::uint32_t voice, bool selected) {
    if (selected) host.KeyOn(voice);
  });
}

// decomp: SetVoiceKeyOff (src/SLUS_010.40/25AC.c:1108).
VoiceMaskResult KeyOffVoices(VoiceOutputHost& host, std::uint32_t mask) {
  return VisitVoices(mask, [&](std::uint32_t voice, bool selected) {
    if (selected) host.KeyOff(voice);
  });
}

// decomp: SetVoiceReverbMode (src/SLUS_010.40/25AC.c:1114).
VoiceMaskResult SetVoiceReverbMask(VoiceOutputHost& host, std::uint32_t mask) {
  return SetModeMask(host, mask, VoiceMode::kReverb);
}

// decomp: SetVoiceNoiseMode (src/SLUS_010.40/25AC.c:1120).
VoiceMaskResult SetVoiceNoiseMask(VoiceOutputHost& host, std::uint32_t mask) {
  return SetModeMask(host, mask, VoiceMode::kNoise);
}

// decomp: SetVoiceFmMode (src/SLUS_010.40/25AC.c:1126).
VoiceMaskResult SetVoiceFrequencyModulationMask(VoiceOutputHost& host,
                                                std::uint32_t mask) {
  constexpr std::uint32_t kUnmodulatedVoice = 1;
  return SetModeMask(host, mask & ~kUnmodulatedVoice,
                     VoiceMode::kFrequencyModulation);
}

// decomp: SetVoiceVolume (src/SLUS_010.40/25AC.c:1132).
VoiceStatus SetVoiceVolume(VoiceOutputHost& host, int voice, std::uint32_t left,
                           std::uint32_t right, std::uint32_t scale) {
  if (!ValidVoice(voice)) return VoiceStatus::kInvalidVoice;
  host.SetVoiceGains(static_cast<std::uint32_t>(voice),
                     {VoiceVolumeGain(VoiceVolumeRegister(left, scale)),
                      VoiceVolumeGain(VoiceVolumeRegister(right, scale))});
  return VoiceStatus::kApplied;
}

// decomp: SetVoiceSampleRate (src/SLUS_010.40/25AC.c:1148).
VoiceStatus SetVoiceSampleRate(VoiceOutputHost& host, int voice,
                               int sample_rate) {
  if (!ValidVoice(voice)) return VoiceStatus::kInvalidVoice;
  if (sample_rate < 0 || sample_rate > kSampleRateLimit) {
    return VoiceStatus::kInvalidSampleRate;
  }
  host.SetVoiceRate(
      static_cast<std::uint32_t>(voice),
      VoiceSampleRateRatio(static_cast<std::uint16_t>(sample_rate)));
  return VoiceStatus::kApplied;
}

// decomp: SetVoiceAdsrReleaseRateAndMode (src/SLUS_010.40/25AC.c:1301).
VoiceStatus SetVoiceReleaseRateAndMode(VoiceOutputHost& host, int voice,
                                       int rate, std::uint32_t mode) {
  if (!ValidVoice(voice)) return VoiceStatus::kInvalidVoice;
  const auto release = ReleaseEnvelope(rate, mode);
  if (!release) return VoiceStatus::kInvalidRelease;
  host.SetVoiceRelease(static_cast<std::uint32_t>(voice), *release);
  return VoiceStatus::kApplied;
}
}  // namespace VS::Audio::Driver

namespace VS::Audio::Samples {
namespace {
constexpr std::size_t kBankIdWord = 1;
constexpr std::size_t kInstrumentCountWord = 7;

std::uint32_t Word(std::span<const std::byte> bytes, std::size_t index) {
  constexpr std::size_t kWordBytes = 4;
  constexpr unsigned kByteBits = 8;
  std::uint32_t value = 0;
  for (std::size_t byte = kWordBytes; byte-- > 0;) {
    value = (value << kByteBits) |
            std::to_integer<std::uint32_t>(bytes[index * kWordBytes + byte]);
  }
  return value;
}

InstrumentRecord Relocate(const Content::AudioInstrument& source,
                          std::uint32_t sample_address) {
  return {source.sample_offset + sample_address,
          source.loop_offset + sample_address,
          source.fine_tune,
          source.root_note,
          source.adsr_lower,
          source.adsr_upper};
}
}  // namespace

std::optional<BankHeader> ReadBankHeader(std::span<const std::byte> bytes) {
  if (bytes.size() < kBankHeaderBytes || Word(bytes, 0) != kAkaoMagic) {
    return std::nullopt;
  }
  return BankHeader{Word(bytes, kBankIdWord),
                    Word(bytes, kInstrumentCountWord)};
}

// decomp: func_800135D8 (src/SLUS_010.40/25AC.c:1033), with
// Sound_CopyAndRelocateInstruments (25AC.c:970) copying the records. Decoded
// bank assets replace its SPU sample transfer.
BankUploadResult UploadBank(SampleBankHost& host,
                            std::span<const std::byte> header,
                            int first_instrument,
                            std::uint32_t sample_address) {
  if (header.size() < kBankHeaderBytes) {
    return {BankUploadStatus::kTruncatedHeader, {}};
  }
  const auto read = ReadBankHeader(header);
  if (!read) return {BankUploadStatus::kNotAkao, {}};
  const BankUploadResult refused{BankUploadStatus::kInvalidDestination, *read};
  if (first_instrument < 0 ||
      static_cast<std::size_t>(first_instrument) >= kInstrumentSlotCount ||
      read->instrument_count == 0 ||
      read->instrument_count >
          kInstrumentSlotCount - static_cast<std::size_t>(first_instrument)) {
    return refused;
  }
  const auto instruments = host.LoadBank(read->bank_id);
  if (!instruments) return {BankUploadStatus::kBankUnavailable, *read};
  if (instruments->size() != read->instrument_count) {
    return {BankUploadStatus::kCountMismatch, *read};
  }
  const auto first = static_cast<std::size_t>(first_instrument);
  for (std::size_t index = 0; index < instruments->size(); ++index) {
    const auto& source = (*instruments)[index];
    host.BindInstrument(first + index, Relocate(source, sample_address),
                        read->bank_id, index, source.available);
  }
  return {BankUploadStatus::kUploaded, *read};
}
}  // namespace VS::Audio::Samples
