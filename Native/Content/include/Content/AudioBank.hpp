#pragma once

#include <array>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "Core/Types.hpp"

namespace VS::Content {

inline constexpr u32 kAudioBankSampleRate = 44100;
// The sound driver's instrument table (g_InstrumentInfo) slots a bank's
// first_instrument and count must fit.
inline constexpr u32 kAudioInstrumentSlots = 256;

// The wave-bank converter (ASSET_PIPELINE.md §6.5, #504): each nonempty
// SOUND\WAVEnnnn.DAT becomes audio\banks\<bank id>\ (bank.json in schema
// vs.audio-bank, AudioBankJson.hpp, and a WAV and an SFZ per playable
// instrument).
inline constexpr std::string_view kWaveBankConverterName = "wave-bank";
inline constexpr std::uint32_t kWaveBankConverterVersion = 2;
inline constexpr std::string_view kWaveBankPrefix = "SOUND/WAVE";
inline constexpr std::string_view kWaveBankExtension = ".DAT";

// Import-only block decoder; callers own end flags and channel interleave.
class PsxAdpcmDecoder {
 public:
  static constexpr usize kBlockBytes = 16;
  static constexpr usize kBlockFrames = 28;
  std::expected<std::array<i16, kBlockFrames>, std::string> Decode(
      std::span<const u8, kBlockBytes> block);

 private:
  int m_previous_ = 0;
  int m_older_ = 0;
};

inline constexpr usize kAudioLegacyInstrumentBytes = 64;
inline constexpr usize kAudioLegacyPitchCount = 12;

// What an AKAO 3.0 instrument record (WAVE0032's 64-byte form) holds beyond
// the 16-byte record's members: the twelve chromatic pitches (Q20, 1 << 20
// playing the sample at its recorded rate), from which root_note and fine_tune
// are read, and the libspu ADSR modes (SPU_VOICE_LINEARIncN and so on, 0-7),
// whose rate bits the packed envelope words keep.
struct AudioLegacyArticulation {
  std::array<u32, kAudioLegacyPitchCount> pitch_table{};
  u8 attack_mode = 0;
  u8 sustain_mode = 0;
  u8 release_mode = 0;

  bool operator==(const AudioLegacyArticulation&) const = default;
};

// The articulation an AKAO 3.0 instrument record holds.
[[nodiscard]] AudioLegacyArticulation AudioLegacyArticulationOf(
    std::span<const u8, kAudioLegacyInstrumentBytes> record) noexcept;

struct AudioInstrument {
  u32 sample_offset = 0;
  u32 loop_offset = 0;
  i16 fine_tune = 0;
  i16 root_note = 0;
  u16 adsr_lower = 0;
  u16 adsr_upper = 0;
  bool available = true;
  // Only for an AKAO 3.0 record; playback reads the members above.
  std::optional<AudioLegacyArticulation> legacy_articulation{};

  bool operator==(const AudioInstrument&) const = default;
};

struct AudioSample {
  std::vector<i16> pcm;
  u32 loop_frame = 0;
  bool repeat = false;
};

struct AudioBank {
  u32 id = 0;
  u32 first_instrument = 0;
  u32 source_destination = 0;
  std::vector<AudioInstrument> instruments;
  std::vector<AudioSample> samples;
};

std::expected<void, std::string> ExportAudioSample(
    const AudioSample& sample, const std::filesystem::path& instrument_path,
    u32 sample_rate = kAudioBankSampleRate);

// WAVE header/instruments: SLUS_010.40/25AC.c:822-887, 970-1054.
// Offsets remain import provenance, never native sample-memory addresses.
std::expected<AudioBank, std::string> DecodeAudioBank(
    std::span<const u8> bytes);
std::expected<void, std::string> ExportAudioBank(
    const AudioBank& bank, const std::filesystem::path& data_root);
// The bank audio\banks\<id>\bank.json holds, without samples; a version 1
// file names `VS.Extract authoring`, which upgrades it in place.
std::expected<AudioBank, std::string> LoadAudioBank(
    const std::filesystem::path& data_root, u32 id);
std::filesystem::path AudioInstrumentPath(
    const std::filesystem::path& data_root, u32 bank_id, usize instrument);
// The files ExportAudioBank writes for `bank`, '/'-separated under the data
// root and sorted.
std::vector<std::string> AudioBankFiles(const AudioBank& bank);

}  // namespace VS::Content
