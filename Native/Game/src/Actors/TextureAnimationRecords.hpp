#pragma once
// An actor's texture animation records as the battle program stores them: the
// per-channel animation slots, texture overlays, cell origins and scrolling
// layers inside VS::Actors::ActorRecord, and the module image's timeline keys.
// The typed views live in Actors/TextureAnimation.hpp.

#include <cstddef>
#include <cstdint>

namespace VS::Actors {

// decomp: D_800F4538_unk180C (src/BATTLE/BATTLE.PRG/entity.h:251)
struct TextureAnimationSlotRecord {
  std::uint8_t timeline : 4;
  std::uint8_t mode : 4;
  std::uint8_t current_frame;
  std::uint8_t key_index;
  std::uint8_t ticks;
};
inline constexpr std::size_t kTextureAnimationSlotRecordBytes = 0x4;
static_assert(sizeof(TextureAnimationSlotRecord) ==
                  kTextureAnimationSlotRecordBytes,
              "TextureAnimationSlotRecord must keep its PS1 layout");

// decomp: D_800F4538_unk181C (src/BATTLE/BATTLE.PRG/entity.h:292)
struct TextureScrollStateRecord {
  std::uint8_t x;
  std::uint8_t y;
  std::int8_t step_x;
  std::int8_t step_y;
  std::uint8_t interval;
  std::uint8_t timer;
  std::uint8_t pad6;
  std::uint8_t pad7;
};
inline constexpr std::size_t kTextureScrollStateRecordBytes = 0x8;
static_assert(sizeof(TextureScrollStateRecord) ==
                  kTextureScrollStateRecordBytes,
              "TextureScrollStateRecord must keep its PS1 layout");

// decomp: D_800F4538_unk614 (src/BATTLE/BATTLE.PRG/entity.h:266)
struct TextureCellOriginRecord {
  std::uint8_t x;
  std::uint8_t y;
  std::uint8_t columns;
  std::uint8_t reserved3;
};
inline constexpr std::size_t kTextureCellOriginRecordBytes = 0x4;
static_assert(sizeof(TextureCellOriginRecord) == kTextureCellOriginRecordBytes,
              "TextureCellOriginRecord must keep its PS1 layout");

// decomp: D_800F4538_unk61C (src/BATTLE/BATTLE.PRG/entity.h:280)
struct TextureScrollBoundsRecord {
  std::uint8_t x;
  std::uint8_t y;
  std::uint8_t width;
  std::uint8_t height;
  std::uint8_t initial_step_x;
  std::uint8_t initial_step_y;
  std::uint8_t bounces : 1;
  std::uint8_t reserved_bits1 : 7;
  std::uint8_t interval;
};
inline constexpr std::size_t kTextureScrollBoundsRecordBytes = 0x8;
static_assert(sizeof(TextureScrollBoundsRecord) ==
                  kTextureScrollBoundsRecordBytes,
              "TextureScrollBoundsRecord must keep its PS1 layout");

// decomp: func_800AAE9C_t (src/BATTLE/BATTLE.PRG/40564.c:19)
struct TextureAnimationKeyRecord {
  std::uint8_t frame;
  std::uint8_t duration;
};
inline constexpr std::size_t kTextureAnimationKeyRecordBytes = 0x2;
static_assert(sizeof(TextureAnimationKeyRecord) ==
                  kTextureAnimationKeyRecordBytes,
              "TextureAnimationKeyRecord must keep its PS1 layout");

// decomp: TexMapAnimation (src/BATTLE/BATTLE.PRG/entity.h:273)
struct TextureAnimationFrame {
  std::uint8_t overlay_index;
  std::uint8_t source_x;
  std::uint8_t source_y;
  std::uint8_t columns;
};
inline constexpr std::size_t kTextureAnimationFrameBytes = 0x4;
static_assert(sizeof(TextureAnimationFrame) == kTextureAnimationFrameBytes,
              "TextureAnimationFrame must keep its PS1 layout");

// decomp: TexMapOverlay (src/BATTLE/BATTLE.PRG/entity.h:259)
struct TextureOverlayRect {
  std::uint8_t x;
  std::uint8_t y;
  std::uint8_t width;
  std::uint8_t height;
};
inline constexpr std::size_t kTextureOverlayRectBytes = 0x4;
static_assert(sizeof(TextureOverlayRect) == kTextureOverlayRectBytes,
              "TextureOverlayRect must keep its PS1 layout");

}  // namespace VS::Actors
