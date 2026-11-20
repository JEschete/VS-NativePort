#pragma once
// A loaded weapon model: the WEP file header with its section offsets, and the
// battle program's per-weapon record that draws it with its owner.

#include <cstddef>
#include <cstdint>

#include "Core/Gte.h"

namespace VS::Actors {

// decomp: WepModelOffsets (src/BATTLE/BATTLE.PRG/entity.h:118). The section
// fields are byte offsets from the start of the file.
inline constexpr std::size_t kWepHeaderPrefixBytes = 0x30;
inline constexpr std::size_t kWepFileBodyBytes = 0x7C0;
struct WeaponModelData {
  std::uint8_t header[kWepHeaderPrefixBytes];
  std::int32_t texture_section;
  std::int32_t vertex_group_section;
  std::int32_t vertex_section;
  std::int32_t polygon_section;
  std::uint8_t file_body[kWepFileBodyBytes];
};
inline constexpr std::size_t kWeaponModelDataBytes = 0x800;
static_assert(sizeof(WeaponModelData) == kWeaponModelDataBytes,
              "WeaponModelData must keep its PS1 layout");

inline constexpr std::size_t kWeaponModelBones = 4;
inline constexpr std::size_t kWeaponModelPalettes = 7;
inline constexpr std::size_t kWeaponPaletteColors = 64;
inline constexpr std::size_t kWeaponWorkPaletteColors = 96;

// decomp: vs_WepModels (src/BATTLE/BATTLE.PRG/entity.h:465)
struct WeaponModel {
  std::uint8_t bone_count;
  std::uint8_t vertex_group_count;
  std::uint16_t triangle_count;
  std::uint16_t quad_count;
  std::uint16_t other_primitive_count;
  std::uint8_t actor_id : 4;
  // The weapon is drawn with its owner (WeaponAppearance::shown).
  std::uint8_t shown : 1;
  std::uint8_t is_wide_texture : 1;
  // brightness replaces the lit colour (WeaponAppearance::brightness_override).
  std::uint8_t brightness_override : 1;
  std::uint8_t is_double_clut : 1;
  std::uint8_t owner_motion_state : 4;
  std::uint8_t material : 4;
  std::uint8_t reserved0a_bits0 : 7;
  std::uint8_t reserved0a_bit7 : 1;
  std::uint8_t palette_color_count;
  std::uint8_t attachment_slot;
  std::uint8_t attachment_bone;
  std::uint8_t model_id;
  std::uint8_t texture_slot;
  std::uint8_t palette_slot_offset;
  // The palette blend's current level, target and remaining frames
  // (WeaponAppearance::blend_level, blend_target and blend_frames).
  std::uint8_t blend_level;
  std::uint8_t blend_target;
  std::uint8_t blend_frames;
  std::int32_t reserved14;
  std::int16_t reserved18;
  std::int16_t reserved1a;
  WeaponModelData* section_offsets;
  MATRIX bone_world_matrices[kWeaponModelBones];
  SVECTOR bones[kWeaponModelBones];
  std::uint16_t palettes[kWeaponModelPalettes][kWeaponPaletteColors];
  std::uint16_t working_palette[kWeaponWorkPaletteColors];
  std::uint16_t upload_palette[kWeaponWorkPaletteColors];
  // The grey level drawn while brightness_override is set.
  std::uint8_t brightness;
  std::uint8_t reserved5c1;
  std::uint16_t reserved5c2;
  std::int32_t reserved5c4;
  std::int32_t reserved5c8;
  std::int8_t reserved5cc;
  std::int8_t reserved5cd;
  std::int16_t reserved5ce;
  std::int32_t reserved5d0;
  std::int32_t reserved5d4;
  void* reserved5d8;
  std::int32_t reserved5dc;
  std::int32_t reserved5e0;
  std::int32_t reserved5e4;
};

}  // namespace VS::Actors
