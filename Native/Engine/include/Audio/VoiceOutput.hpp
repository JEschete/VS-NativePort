#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "Audio/VoiceEngine.hpp"
#include "Content/AudioBank.hpp"

// The sound driver's SPU voice writes (SetVoiceKeyOn through
// SetVoiceAdsrReleaseRateAndMode, SLUS_010.40/25AC.c:1102-1310). They reach
// the native voice engine only through VoiceOutputHost; voice indices and
// masks are bounded to the 24 SPU voices.
namespace VS::Audio::Driver {
inline constexpr std::uint32_t kVoiceCount = 24;
inline constexpr std::uint32_t kAllVoices = (1U << kVoiceCount) - 1;
// The pitch register value that plays a sample at its own rate, and the
// largest rate the SPU plays (four times).
inline constexpr std::uint32_t kUnitySampleRate = 0x1000;
inline constexpr std::uint32_t kMaximumSampleRate = 0x4000;
// The widest values that fit the 16-bit pitch register and the ADSR release
// rate (bits 0-4) and release mode (bit 2 lands on bit 5) fields.
inline constexpr int kSampleRateLimit = 0xFFFF;
inline constexpr int kMaximumReleaseRate = 31;
inline constexpr std::uint32_t kMaximumReleaseMode = 7;

enum class VoiceMode : std::uint8_t { kReverb, kNoise, kFrequencyModulation };

enum class VoiceStatus : std::uint8_t {
  kApplied,
  kInvalidVoice,
  kInvalidSampleRate,
  kInvalidRelease,
};

struct VoiceGains {
  float left = 0;
  float right = 0;
};

// The voices a mask selected, and the bits above voice 23 that no SPU voice
// register holds.
struct VoiceMaskResult {
  std::uint32_t voices = 0;
  std::uint32_t ignored = 0;
};

class VoiceOutputHost {
 public:
  virtual ~VoiceOutputHost() = default;
  virtual void KeyOn(std::uint32_t voice) = 0;
  virtual void KeyOff(std::uint32_t voice) = 0;
  // Whether `voice` uses `mode`; the mask setters call it for every voice.
  virtual void SetVoiceMode(std::uint32_t voice, VoiceMode mode,
                            bool enabled) = 0;
  virtual void SetVoiceGains(std::uint32_t voice, const VoiceGains& gains) = 0;
  // The playback rate relative to the sample's own rate.
  virtual void SetVoiceRate(std::uint32_t voice, float rate) = 0;
  virtual void SetVoiceRelease(std::uint32_t voice,
                               const EnvelopeRate& release) = 0;
};

// The 15-bit volume register SetVoiceVolume writes: `value` scaled by
// `scale`/128 when `scale` is nonzero (wrapping as the original's unsigned
// multiply does), then masked.
[[nodiscard]] std::uint16_t VoiceVolumeRegister(std::uint32_t value,
                                                std::uint32_t scale);
// The gain of a volume register value: its signed 15-bit value over 0x4000.
[[nodiscard]] float VoiceVolumeGain(std::uint16_t volume);
// The playback rate of a pitch register value, capped at four times.
[[nodiscard]] float VoiceSampleRateRatio(std::uint16_t sample_rate);
// The release phase the ADSR upper word takes from `rate` and `mode`, or
// nullopt when either would spill into the neighbouring ADSR fields.
[[nodiscard]] std::optional<EnvelopeRate> ReleaseEnvelope(int rate,
                                                          std::uint32_t mode);

// Each mask setter visits the voices in ascending order.
VoiceMaskResult KeyOnVoices(VoiceOutputHost& host, std::uint32_t mask);
VoiceMaskResult KeyOffVoices(VoiceOutputHost& host, std::uint32_t mask);
VoiceMaskResult SetVoiceReverbMask(VoiceOutputHost& host, std::uint32_t mask);
VoiceMaskResult SetVoiceNoiseMask(VoiceOutputHost& host, std::uint32_t mask);
// Voice 0 has no preceding voice to modulate it, so its bit is dropped.
VoiceMaskResult SetVoiceFrequencyModulationMask(VoiceOutputHost& host,
                                                std::uint32_t mask);
VoiceStatus SetVoiceVolume(VoiceOutputHost& host, int voice, std::uint32_t left,
                           std::uint32_t right, std::uint32_t scale);
VoiceStatus SetVoiceSampleRate(VoiceOutputHost& host, int voice,
                               int sample_rate);
VoiceStatus SetVoiceReleaseRateAndMode(VoiceOutputHost& host, int voice,
                                       int rate, std::uint32_t mode);
}  // namespace VS::Audio::Driver

// The sample bank upload (func_800135D8, SLUS_010.40/25AC.c:1033): the bank's
// relocated instrument records fill a run of the 256-entry instrument table.
namespace VS::Audio::Samples {
inline constexpr std::size_t kInstrumentSlotCount = 256;
inline constexpr std::size_t kBankHeaderBytes = 32;
inline constexpr std::uint32_t kAkaoMagic = 0x4F414B41;  // "AKAO"

struct BankHeader {
  std::uint32_t bank_id = 0;
  std::uint32_t instrument_count = 0;
};

// One relocated instrument table entry: the sample and loop addresses are
// offset by the bank's sample memory destination.
struct InstrumentRecord {
  std::uint32_t sample_start = 0;
  std::uint32_t loop_start = 0;
  std::int16_t fine_tune = 0;
  std::int16_t root_note = 0;
  std::uint16_t adsr_lower = 0;
  std::uint16_t adsr_upper = 0;
};

enum class BankUploadStatus : std::uint8_t {
  kUploaded,
  kTruncatedHeader,
  kNotAkao,
  kInvalidDestination,
  kBankUnavailable,
  kCountMismatch,
};

struct BankUploadResult {
  BankUploadStatus status = BankUploadStatus::kTruncatedHeader;
  BankHeader header;
};

class SampleBankHost {
 public:
  virtual ~SampleBankHost() = default;
  // The decoded instruments of bank `bank_id`, or nullopt when it cannot
  // load.
  virtual std::optional<std::span<const Content::AudioInstrument>> LoadBank(
      std::uint32_t bank_id) = 0;
  // Stores instrument `index` of bank `bank_id` in table slot `slot`.
  virtual void BindInstrument(std::size_t slot, const InstrumentRecord& record,
                              std::uint32_t bank_id, std::size_t index,
                              bool available) = 0;
};

// The bank id (word 1) and instrument count (word 7) of an AKAO bank header,
// or nullopt when `bytes` is shorter than the header or is not AKAO.
[[nodiscard]] std::optional<BankHeader> ReadBankHeader(
    std::span<const std::byte> bytes);

// Refuses a destination run outside the table or an empty one before
// loading the bank, and a bank whose decoded count differs from its header.
BankUploadResult UploadBank(SampleBankHost& host,
                            std::span<const std::byte> header,
                            int first_instrument, std::uint32_t sample_address);
}  // namespace VS::Audio::Samples
