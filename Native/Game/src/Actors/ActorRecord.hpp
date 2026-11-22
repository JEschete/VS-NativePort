#pragma once
// One loaded actor as the battle program stores it: the model instance and
// bone matrices, the motion and animation state, the texture, palette, sound
// and shadow records, and the actor table that holds the records. The typed
// views live in ActorStateRecord.hpp and the other Actors headers.

#include <array>
#include <cstddef>
#include <cstdint>

#include "Actors/Animation.hpp"
#include "Actors/ModelInstance.hpp"
#include "Actors/Movement.hpp"
#include "Actors/Shadow.hpp"
#include "Actors/TextureAnimationRecords.hpp"
#include "Core/Gte.hpp"
#include "Render/Palette.hpp"

namespace VS::Actors {

// decomp: EdgeHighlight (src/BATTLE/BATTLE.PRG/entity.h:22), the record's
// unk6F4 (formerly D_800F4538_unk58). CompanionOffset (ActorStateRecord.hpp)
// is the typed view.
struct CompanionOffsetRecord {
  std::int8_t offset_x;
  std::int8_t offset_y;
  std::int8_t offset_z;
  std::uint8_t depth_bias;
};
namespace layout {
inline constexpr std::size_t kCompanionOffsetRecordBytes = 0x4;
}  // namespace layout
static_assert(sizeof(CompanionOffsetRecord) ==
                  layout::kCompanionOffsetRecordBytes,
              "CompanionOffsetRecord must keep its PS1 layout");

// decomp: D_800F4538_t (src/BATTLE/BATTLE.PRG/entity.h:303). The native
// record holds 42 bone matrices where the original holds 8 and its scratch.
struct ActorRecord {
  static constexpr std::size_t kBoneMatrices = 42;
  static constexpr std::size_t kSharedSequenceSlots = 4;
  static constexpr std::size_t kTextureOverlays = 8;
  static constexpr std::size_t kTextureCellOrigins = 2;
  static constexpr std::size_t kTextureScrollLayers = 4;
  static constexpr std::size_t kBattleSequences = 12;
  static constexpr std::size_t kSpecialSequences = 4;
  static constexpr std::size_t kPaletteBytes = 0x280;
  static constexpr std::size_t kBasePaletteColors = 0x140;
  static constexpr std::size_t kCurrentPaletteColors = 0xA0;
  static constexpr std::size_t kTextureAnimationChannels = 2;
  static constexpr std::size_t kTextureRowBanks = 3;
  static constexpr std::size_t kJointSlotBytes = 0x10;
  static constexpr std::size_t kLockedBones = 2;
  static constexpr std::size_t kReserved6bcWords = 4;

  ModelInstance instance;
  VS::Core::Gte::MATRIX bone_matrices[kBoneMatrices];
  std::uint8_t route_phase : 2;
  std::uint8_t route_uses_duration : 1;
  std::uint8_t is_carrying : 1;
  std::uint8_t attached_child_count : 2;
  std::uint8_t reserved5ac_bit6 : 1;
  std::uint8_t momentum_stop : 1;
  std::uint8_t custom_tint : 1;
  std::uint8_t bypass_actor_collision : 1;
  std::uint8_t bypass_terrain_collision : 1;
  std::uint8_t ignored_by_collision : 1;
  std::uint8_t wall_contact : 1;
  std::uint8_t on_ledge : 1;
  std::uint8_t was_on_ledge : 1;
  std::uint8_t is_running : 1;
  std::uint8_t animation_event_flag : 1;
  std::uint8_t reserved5ac_bit17 : 1;
  std::uint8_t advance_requested : 1;
  std::uint8_t reserved5ac_bit19 : 1;
  std::uint8_t action_locked : 1;
  std::uint8_t is_swaying : 1;
  std::uint8_t sway_returning : 1;
  std::uint8_t vertices_displaced : 1;
  std::uint8_t animation_locked : 1;
  std::uint8_t stride_parity : 1;
  std::uint8_t jump_speed_raised : 1;
  std::uint8_t is_8bit_texture : 1;
  std::uint8_t support_gimmick : 4;
  std::uint8_t jump_profile : 2;
  std::uint8_t jump_started : 1;
  std::uint8_t fast_animation : 1;

  std::uint8_t root_key_pinned : 1;
  std::uint8_t holds_texture_slots : 1;
  std::uint8_t reserved5b0_bits6 : 2;
  std::uint8_t sequence_model_id;
  std::uint8_t frames_per_step;
  std::uint8_t idle_ticks;
  std::uint8_t common_entry_count;
  std::uint8_t common_animation_count;
  std::uint8_t battle_animation_count;
  std::uint8_t tick_remainder;
  std::uint8_t animation_id;
  std::uint8_t run_speed;
  std::uint8_t walk_speed;
  std::uint8_t texture_slot;
  std::int16_t animation_length;
  std::int16_t speed_quarters;
  std::uint16_t turn_rate;
  std::uint16_t move_rate;
  std::uint16_t route_move_rate;
  std::uint16_t route_turn_rate;
  std::uint16_t scale_frames;
  std::uint16_t transition_frames;
  std::int8_t blend_frame;
  std::int8_t blend_frames;
  std::int16_t reserved5ce;
  void* owned_sequence;
  void* common_sequence;
  void* battle_sequence;
  void* shared_sequences[kSharedSequenceSlots];
  VS::Core::Gte::SVECTOR route_target;
  TextureOverlayRect texture_overlays[kTextureOverlays];
  TextureCellOriginRecord texture_cell_origins[kTextureCellOrigins];
  TextureScrollBoundsRecord texture_scroll_bounds[kTextureScrollLayers];
  std::uint16_t collision_radius;
  std::uint16_t collision_height;
  std::uint16_t occupancy_radius;
  std::uint16_t occupancy_height;
  std::uint16_t reserved644;
  std::uint16_t target_point_height;
  std::uint32_t reserved648 : 16;
  std::uint32_t floor_lit : 1;
  std::uint32_t reserved648_bits17 : 15;
  std::uint16_t shadow_radius;
  std::uint16_t shadow_radius_expand_rate;
  std::uint16_t shadow_radius_shrink_rate;
  std::uint16_t reserved652;
  std::uint16_t reserved654;
  std::uint16_t menu_camera_distance;
  std::uint16_t reserved658;
  std::int16_t anchor_height;
  std::int32_t reserved65c;
  std::int32_t reserved660;
  std::uint32_t battle_sequence_lbas[kBattleSequences];
  // Byte sizes of the battle sequences: entity.h calls them
  // chainAttackAnimationIds, ShpHeader btSeqSizes (#509).
  std::uint16_t battle_sequence_sizes[kBattleSequences];
  std::uint32_t special_sequence_lbas[kSpecialSequences];
  std::int32_t reserved6bc[kReserved6bcWords];
  std::uint16_t special_sequence_sizes[kSpecialSequences];
  std::int32_t reserved6d4[2];
  std::int32_t opaque6dc;
  std::uint8_t queued_animation;
  std::uint8_t queued_blend_frames;
  std::uint8_t queued_start_frame;
  std::uint8_t animation_set;
  std::uint8_t contact_kind;
  std::uint8_t travel_animation;
  std::uint16_t model_id;
  std::int16_t clut_a;
  std::int16_t clut_b;
  std::int16_t ledge_height;
  std::int8_t platform_offset_x;
  std::int8_t platform_offset_z;
  std::uint8_t fade_blue;
  std::uint8_t fade_green;
  std::uint8_t fade_red;
  std::uint8_t reserved6f3;
  CompanionOffsetRecord companion_state;
  Render::ColorCode companion_color;
  VS::Core::Gte::SVECTOR sound_position;
  AnimationPose blend_pose;
  AnimationPose pose;
  std::uint8_t palettes[kPaletteBytes];
  std::uint16_t base_palette[kBasePaletteColors];
  std::uint16_t current_palette[kCurrentPaletteColors];
  ModelEffectSlot sound_effect;
  ModelEffectSlot sound_effect_target;
  VS::Core::Gte::SVECTOR platform_anchor;
  VS::Core::Gte::SVECTOR head_tracking;
  std::int8_t head_track_target;
  std::uint8_t companion_slot;
  std::uint8_t terrain_facing;
  std::uint8_t ledge_attempts;
  std::uint16_t move_facing;
  std::uint8_t sway_angles[kTextureAnimationChannels];
  TextureAnimationFrame texture_animation_frames[kTextureAnimationChannels];
  TextureAnimationSlotRecord texture_animation_slots[kTextureAnimationChannels];
  std::int16_t destination_x;
  std::int16_t jump_frames;
  std::int16_t destination_z;
  std::int16_t destination_mode;
  TextureScrollStateRecord texture_scroll[kTextureScrollLayers];
  std::int16_t scale;
  union {
    std::uint8_t texture_row_offsets[kTextureRowBanks];
    struct {
      std::uint8_t reserved_texture_rows[2];
      std::int16_t momentum_distance;
    };
  };
  std::int16_t momentum_direction;
  std::int16_t reserved1844;
  std::uint8_t run_frames;
  std::uint8_t slow_ticks;
  MovementRequest turn_state;
  std::uint8_t opaque1860;
  std::uint8_t reserved1861;
  std::uint8_t reserved1862;
  std::uint8_t reserved1863;
  ShadowBlendRecord shadow_blend;
  std::int32_t saved_placement;
  std::intptr_t animation_header;
  std::uint8_t joint_slots[kJointSlotBytes];
  VS::Core::Gte::MATRIX locked_bone_matrices[kLockedBones];
  std::uint8_t locked_bones[kLockedBones];
  std::uint16_t pending_target_x;
  std::uint16_t pending_target_z;
  std::uint16_t previous_target_x;
  std::uint16_t previous_target_z;
  std::uint32_t model_data_size;
};

inline constexpr int kActorTableSlots = 17;

// decomp: D_800F4538 (src/BATTLE/BATTLE.PRG/entity.h:524). Ashley, the room
// actors and their companion models; ActorRecord.cpp defines it.
extern std::array<ActorRecord*, kActorTableSlots> actorRecords;

}  // namespace VS::Actors
