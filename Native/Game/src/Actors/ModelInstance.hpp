#pragma once
// The model instance an actor record and a gimmick embed: the SHP's counts,
// the draw and motion state bits, the transform, the colours, the tiles it
// stands on and the SHP it draws. The typed views live in ActorMotion.hpp and
// ActorStateRecord.hpp.

#include <cstddef>
#include <cstdint>

#include "Core/Gte.hpp"
#include "Render/Palette.hpp"

namespace VS::Actors {

struct ShpModel;

// decomp: D_800F4538_unk64 (src/BATTLE/BATTLE.PRG/entity.h:86)
struct ModelEffectSlot {
  std::uint8_t kind;
  std::uint8_t level;
  std::uint8_t flags;
  std::uint8_t parameter;
};
namespace layout {
inline constexpr std::size_t kModelEffectSlotBytes = 0x4;
}  // namespace layout
static_assert(sizeof(ModelEffectSlot) == layout::kModelEffectSlotBytes,
              "ModelEffectSlot must keep its PS1 layout");

// decomp: D_800F4538_unk0 (src/BATTLE/BATTLE.PRG/entity.h:127)
struct ModelInstance {
  /* 0x00 */ std::uint8_t bone_count;
  /* 0x01 */ std::uint8_t vertex_group_count;
  /* 0x02 */ std::uint16_t triangle_count;
  /* 0x04 */ std::uint16_t quad_count;
  /* 0x06 */ std::uint16_t other_polygon_count;
  /* 0x08 */ std::int8_t is_skipped : 1;
  /* 0x08 */ std::int8_t is_visible : 1;
  /* 0x08 */ std::int8_t is_frozen : 1;
  /* 0x08 */ std::int8_t uv_layout : 1;
  /* 0x08 */ std::int8_t texture_slot_count_minus_one : 3;
  /* 0x08 */ std::int8_t is_parsed : 1;
  /* 0x09 */ std::uint8_t action_animation_state : 4;
  /* 0x09 */ std::uint8_t is_preview_model : 1;
  /* 0x09 */ std::uint8_t is_attached : 1;
  /* 0x09 */ std::uint8_t movement_pending : 1;
  /* 0x09 */ std::uint8_t sound_enabled : 1;
  /* 0x0A */ std::uint8_t movement_mode : 3;
  /* 0x0A */ std::uint8_t locomotion_state : 2;
  /* 0x0A */ std::uint8_t motion_locked : 1;
  /* 0x0A */ std::uint8_t is_jumping : 1;
  /* 0x0A */ std::uint8_t weapon_drawn : 1;
  /* 0x0B */ std::uint8_t reaction_state : 4;
  /* 0x0B */ std::uint8_t pose_state : 3;
  /* 0x0B */ std::uint8_t was_drawn : 1;
  /* 0x0C */ std::uint8_t support_layer : 4;
  /* 0x0C */ std::uint8_t placement_layer : 4;
  /* 0x0D */ std::uint8_t reserved0d;
  /* 0x0E */ std::uint8_t copied_word_count;
  /* 0x0F */ std::uint8_t slot;
  /* 0x10 */ std::uint8_t variant;
  /* 0x11 */ std::uint8_t jump_countdown;
  /* 0x12 */ std::uint8_t attach_parent;
  /* 0x13 */ std::uint8_t attach_bone;
  /* 0x14 */ std::uint16_t look_yaw;
  /* 0x16 */ std::uint16_t look_pitch;
  /* 0x18 */ std::uint16_t turn_frames;
  /* 0x1A */ std::uint16_t current_animation;
  /* 0x1C */ VS::Core::Gte::SVECTOR position;
  /* 0x24 */ std::int16_t rotation_x;
  /* 0x26 */ std::int16_t facing;
  /* 0x28 */ std::int16_t rotation_z;
  /* 0x2A */ std::int16_t reserved2a;
  /* 0x2C */ std::int16_t scale_x;
  /* 0x2E */ std::int16_t scale_y;
  /* 0x30 */ std::int16_t scale_z;
  /* 0x32 */ std::int16_t reserved32;
  /* 0x34 */ VS::Core::Gte::SVECTOR root_motion;
  /* 0x3C */ std::int16_t rotation_step_x;
  /* 0x3E */ std::int16_t facing_delta;
  /* 0x40 */ std::int16_t rotation_step_z;
  /* 0x42 */ std::uint16_t reserved42;
  /* 0x44 */ VS::Core::Gte::SVECTOR scale_delta;
  /* 0x4C */ std::int32_t support_offset;
  /* 0x50 */ std::int16_t reserved50;
  /* 0x52 */ std::int16_t previous_facing;
  /* 0x54 */ VS::Core::Gte::CVECTOR base_color;
  /* 0x58 */ Render::ColorCode tint;
  /* 0x5C */ std::uint8_t current_tile_x;
  /* 0x5D */ std::uint8_t current_tile_layer;
  /* 0x5E */ std::uint8_t current_tile_z;
  /* 0x5F */ std::uint8_t reserved5f;
  /* 0x60 */ std::uint8_t previous_tile_x;
  /* 0x61 */ std::uint8_t previous_tile_layer;
  /* 0x62 */ std::uint8_t previous_tile_z;
  /* 0x63 */ std::uint8_t reserved63;
  /* 0x64 */ ModelEffectSlot palette_pulse;
  // A native pointer, so the record grows past the PS1's 0x6C bytes here.
  /* 0x68 */ ShpModel* model;
};

namespace layout {
inline constexpr std::size_t kModelInstanceCopiedWordCountAt = 0x0E;
static_assert(offsetof(ModelInstance, copied_word_count) ==
              kModelInstanceCopiedWordCountAt);
inline constexpr std::size_t kModelInstancePositionAt = 0x1C;
static_assert(offsetof(ModelInstance, position) == kModelInstancePositionAt);
inline constexpr std::size_t kModelInstanceBaseColorAt = 0x54;
static_assert(offsetof(ModelInstance, base_color) == kModelInstanceBaseColorAt);
inline constexpr std::size_t kModelInstanceCurrentTileAt = 0x5C;
static_assert(offsetof(ModelInstance, current_tile_x) ==
              kModelInstanceCurrentTileAt);
inline constexpr std::size_t kModelInstancePalettePulseAt = 0x64;
static_assert(offsetof(ModelInstance, palette_pulse) ==
              kModelInstancePalettePulseAt);
inline constexpr std::size_t kModelInstanceModelAt = 0x68;
static_assert(offsetof(ModelInstance, model) == kModelInstanceModelAt);
}  // namespace layout

}  // namespace VS::Actors
