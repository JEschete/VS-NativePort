#pragma once
// AKAO sequence records: the file header, the sequence file the sequencer
// plays and the keymap entry an instrument note maps through.
#include <cstddef>
#include <cstdint>

#include "Audio/Akao.hpp"

namespace VS::Audio {
inline constexpr std::size_t kAkaoMagicBytes = 4;

// decomp: AkaoSeqHeader (src/SLUS_010.40/25AC.c:41)
struct AkaoFileHeader {
  std::uint8_t magic[kAkaoMagicBytes];
  std::uint16_t sequence_id;
  std::uint16_t length;
  std::uint16_t reverb_mode;
  AkaoTimestamp timestamp;
};

// decomp: FAkaoSequence (src/include/sound.h:444)
struct AkaoSequenceFile {
  /* 0x00 */ std::uint32_t reserved00;
  /* 0x04 */ std::uint32_t reserved04;
  /* 0x08 */ std::uint32_t reserved08;
  /* 0x0C */ std::uint32_t reserved0c;
  /* 0x10 */ std::uint32_t reserved10;
  /* 0x14 */ std::uint32_t sample_bank_id;
  /* 0x18 */ std::uint32_t reserved18;
  /* 0x1C */ std::uint32_t instrument_data_offset;
  /* 0x20 */ std::uint32_t channel_enable_mask;
  /* 0x24 */ std::uint32_t keyed_mask;
  /* 0x28 */ std::uint32_t allocated_voice_mask;
  /* 0x2C */ std::uint32_t reserved2c;
  /* 0x30 */ std::int32_t patch_region_offset;   // field-relative
  /* 0x34 */ std::int32_t keymap_region_offset;  // field-relative
  /* 0x38 */ std::uint32_t resume_key_on_mask;
  /* 0x3C */ std::uint32_t push_release_mask;
  /* 0x40 */ std::uint8_t payload[1];  // starts at 0x40 (variable length)
};

// decomp: FSoundKeymapEntry8 (src/include/sound.h:421)
struct KeymapEntry {
  std::int8_t instrument_index;
  std::uint8_t note;
  std::uint8_t attack_rate;
  std::uint8_t sustain_rate;
  std::uint8_t sustain_mode;  // values 3/5/7 map to 0x4000/0x8000/0xC000
  std::uint8_t release_rate;
  std::uint8_t volume_scale;
  std::uint8_t pan_and_reverb;  // low 7 bits pan, high bit reverb-enable
};

namespace layout {
inline constexpr std::size_t kAkaoFileHeaderBytes = 0x10;
static_assert(sizeof(AkaoFileHeader) == kAkaoFileHeaderBytes);
inline constexpr std::size_t kAkaoSequenceFileBytes = 0x44;
static_assert(sizeof(AkaoSequenceFile) == kAkaoSequenceFileBytes);
inline constexpr std::size_t kKeymapEntryBytes = 0x8;
static_assert(sizeof(KeymapEntry) == kKeymapEntryBytes);
}  // namespace layout
}  // namespace VS::Audio
