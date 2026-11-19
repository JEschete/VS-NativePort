#pragma once
// AKAO file records: the BCD timestamp a sequence header carries, and the
// sound-effect program file (the magic, each effect's program offsets and
// metadata, then the bytecode).
#include <cstddef>
#include <cstdint>

namespace VS::Audio {
inline constexpr std::size_t kAkaoProgramSlots = 0x100;
inline constexpr std::size_t kAkaoProgramOffsetsPerSlot = 2;

namespace layout {
inline constexpr std::size_t kAkaoProgramFileReservedBytes = 0xC;
}  // namespace layout

// decomp: AkaoTimeStamp (src/SLUS_010.40/25AC.c:33)
struct AkaoTimestamp {
  std::uint8_t year_bcd;
  std::uint8_t month_bcd;
  std::uint8_t day_bcd;
  std::uint8_t hours_bcd;
  std::uint8_t minutes_bcd;
  std::uint8_t seconds_bcd;
};

// decomp: FAkaoFileBlob (src/include/sound.h:197)
struct AkaoProgramFile {
  /* 0x000 */ std::int32_t magic;  // AKAO
  // Padding? Version? Counts? Music AKAO blobs have a different flag here.
  /* 0x004 */ std::uint8_t reserved04[layout::kAkaoProgramFileReservedBytes];
  // Offsets into the bytecode, indexed by sound-effect id.
  /* 0x010 */ std::uint16_t program_offsets[kAkaoProgramSlots]
                                           [kAkaoProgramOffsetsPerSlot];
  // Two per-effect tables whose meaning is not established.
  /* 0x410 */ std::uint16_t metadata_table_a[kAkaoProgramSlots];
  /* 0x610 */ std::uint16_t metadata_table_b[kAkaoProgramSlots];
  /* 0x810 */ std::uint8_t program_data[1];  // Sound-effect bytecode
};

namespace layout {
inline constexpr std::size_t kAkaoTimestampBytes = 0x6;
static_assert(sizeof(AkaoTimestamp) == kAkaoTimestampBytes);
inline constexpr std::size_t kAkaoProgramFileBytes = 0x814;
static_assert(sizeof(AkaoProgramFile) == kAkaoProgramFileBytes);
}  // namespace layout
}  // namespace VS::Audio
