#pragma once

#include <array>
#include <expected>
#include <filesystem>
#include <span>
#include <string>
#include <vector>

#include "Core/Types.hpp"

namespace VS::Content {

inline constexpr u32 kAudioBankSampleRate = 44100;

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

struct AudioInstrument {
  u32 sample_offset = 0;
  u32 loop_offset = 0;
  i16 fine_tune = 0;
  i16 root_note = 0;
  u16 adsr_lower = 0;
  u16 adsr_upper = 0;
  bool available = true;
  std::vector<u8> source_record{};
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
std::expected<AudioBank, std::string> LoadAudioBank(
    const std::filesystem::path& data_root, u32 id);
std::filesystem::path AudioInstrumentPath(
    const std::filesystem::path& data_root, u32 bank_id, usize instrument);

}  // namespace VS::Content
