#pragma once
// The 0x14-byte shadow-blend record an actor and a gimmick embed: the
// shadow's colour and the level and radius ramps the typed
// VS::Actors::ShadowBlend (SequencePlayback.hpp) loads and stores.
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace VS::Actors {
// decomp: D_800F4538_unk1864 (src/BATTLE/BATTLE.PRG/entity.h:28-41)
struct ShadowBlendRecord {
  // RGB in the low three bytes (49EB8.c writes bytes 0-2 of unk0).
  /* 0x00 */ std::int32_t color;
  /* 0x04 */ std::uint8_t level_frames;
  /* 0x05 */ std::uint8_t radius_frames;
  /* 0x06 */ std::uint8_t reserved6;
  /* 0x07 */ std::uint8_t reserved7;
  /* 0x08 */ std::uint16_t level;
  /* 0x0A */ std::int16_t level_delta;
  /* 0x0C */ std::uint16_t radius_expand_rate;
  /* 0x0E */ std::uint16_t radius_shrink_rate;
  /* 0x10 */ std::int16_t radius_expand_delta;
  /* 0x12 */ std::int16_t radius_shrink_delta;
};

namespace layout {
inline constexpr std::size_t kShadowBlendRecordBytes = 0x14;
static_assert(sizeof(ShadowBlendRecord) == kShadowBlendRecordBytes);
static_assert(std::is_trivially_copyable_v<ShadowBlendRecord>);
inline constexpr std::size_t kShadowLevelFramesAt = 0x04;
static_assert(offsetof(ShadowBlendRecord, level_frames) ==
              kShadowLevelFramesAt);
inline constexpr std::size_t kShadowLevelOffset = 0x08;
static_assert(offsetof(ShadowBlendRecord, level) == kShadowLevelOffset);
inline constexpr std::size_t kShadowRatesAt = 0x0C;
static_assert(offsetof(ShadowBlendRecord, radius_expand_rate) ==
              kShadowRatesAt);
inline constexpr std::size_t kShadowDeltasAt = 0x10;
static_assert(offsetof(ShadowBlendRecord, radius_expand_delta) ==
              kShadowDeltasAt);
}  // namespace layout
}  // namespace VS::Actors
