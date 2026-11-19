#pragma once
// The AKAO stream header and the cutscene stream data block it heads.
#include <cstddef>
#include <cstdint>

namespace VS::Audio {
namespace layout {
inline constexpr std::size_t kAkaoStreamHeaderReserved08Bytes = 0x8;
inline constexpr std::size_t kAkaoStreamHeaderReserved24Bytes = 0x4;
inline constexpr std::size_t kCutsceneStreamLeadBytes = 0x80;
inline constexpr std::size_t kCutsceneStreamGapBytes = 0x24;
}  // namespace layout

// decomp: FAkaoHeader (src/include/sound.h:559)
struct AkaoStreamHeader {
  /* 0x00 */ std::uint32_t magic;
  /* 0x04 */ std::int32_t reserved04;  // -> StreamState.field8_0x20
  /* 0x08 */ std::uint8_t reserved08[layout::kAkaoStreamHeaderReserved08Bytes];
  /* 0x10 */ std::uint32_t total_pages;
  /* 0x14 */ std::int32_t reserved14;
  /* 0x18 */ std::int32_t reserved18;  // -> StreamState.field2_0x8
  /* 0x1C */ std::int32_t sample_rate;
  /* 0x20 */ std::uint32_t current_page;
  /* 0x24 */ std::uint8_t reserved24[layout::kAkaoStreamHeaderReserved24Bytes];
  // Outgoing voice reference for the volume handoff.
  /* 0x28 */ std::int32_t reserved28;
};

// decomp: FSoundCutsceneStreamData (src/include/sound.h:566)
struct CutsceneStreamData {
  // Probably some pointers to chunks.
  /* 0x00 */ std::uint8_t reserved00[layout::kCutsceneStreamLeadBytes];
  /* 0x80 */ AkaoStreamHeader stream_header;
  /* 0xAC */ std::uint8_t reservedac[layout::kCutsceneStreamGapBytes];
  /* 0xD0 */ std::uint8_t audio_data[1];
};

namespace layout {
inline constexpr std::size_t kAkaoStreamHeaderBytes = 0x2C;
static_assert(sizeof(AkaoStreamHeader) == kAkaoStreamHeaderBytes);
inline constexpr std::size_t kCutsceneStreamDataBytes = 0xD4;
static_assert(sizeof(CutsceneStreamData) == kCutsceneStreamDataBytes);
}  // namespace layout
}  // namespace VS::Audio
