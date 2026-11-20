#pragma once
// Battle actions: the action type and spell id values, the action request
// records the targeting and execution code fills, the action state that
// queues them, and the 52-byte action definition records of the module
// image's action table (ActionTable.hpp).

#include <array>
#include <cstddef>
#include <cstdint>

#include "Battle/Hit.hpp"
#include "Battle/TargetingState.hpp"
#include "Core/Gte.h"

namespace VS::Battle {

// ActionDefinition::type values.
// decomp: actionTypes (src/SLUS_010.40/actions.h:6-13)
enum ActionType : std::uint8_t {
  kActionTypeSpell = 1,
  kActionTypeAbility,
  kActionTypeBreakArt,
  kActionTypeItem = 5,
  kActionTypeBaseAttack,
  kActionTypeTrap
};

// decomp: actions_e (src/SLUS_010.40/actions.h:4)
enum ActionId : std::uint8_t { kActionIdDaggerArtOne = 184 };

// The warlock spells' action ids.
// decomp: warlockSpellIds_e (src/BATTLE/BATTLE.PRG/battleUi.h:18-37)
enum WarlockSpell : std::uint8_t {
  kWarlockSpellSolidShock = 0x61,
  kWarlockSpellLightningBolt,
  kWarlockSpellFireball,
  kWarlockSpellVulcanLance,
  kWarlockSpellAquaBlast,
  kWarlockSpellSpiritSurge,
  kWarlockSpellDarkChant,
  kWarlockSpellExorcism,
  kWarlockSpellBanish,
  kWarlockSpellExplosion,
  kWarlockSpellThunderburst = 0x6E,
  kWarlockSpellFlameSphere = 0x72,
  kWarlockSpellGaeaStrike = 0x76,
  kWarlockSpellAvalanche = 0x7A,
  kWarlockSpellRadialSurge = 0x7E,
  kWarlockSpellMeteor = 0x82,
  kWarlockSpellDrainHeart = 0x5E,
  kWarlockSpellDrainMind = 0x5F
};

inline constexpr std::size_t kActionRequestTargetCount = 255;

// One action request: the action, its primary target and up to 255 targets.
// The native record holds 255 targets where the original holds 30, so it
// keeps no PS1 size.
// decomp: D_800F19CC_t2 (src/BATTLE/BATTLE.PRG/hitEntity.h:58-67)
struct ActionRequestRecord {
  std::uint16_t action_id;
  std::int8_t effect_variant;
  std::int8_t damage_type;
  HitResult primary_target;
  std::uint16_t animation_parameter;
  std::uint16_t target_count;
  std::array<HitResult, kActionRequestTargetCount> targets;
  SVECTOR focus_position;
};

inline constexpr std::size_t kActionQueueCount = 4;
inline constexpr std::size_t kActionPreviewCount = 255;
inline constexpr std::size_t kActionStateReservedWords = 6;

// The battle's action state: the selected request, the four queued requests,
// the targeting range and area and the ability-chain input timing.
// decomp: D_800F19CC_t (src/BATTLE/BATTLE.PRG/146C.c:56-89)
struct ActionState {
  std::uint32_t queue_index;
  std::uint32_t chain_step;
  ActionRequestRecord selection;
  std::array<ActionRequestRecord, kActionQueueCount> queue;
  std::int32_t is_player_action;
  std::int32_t source_actor_id;
  std::uint8_t selected_preview;
  std::uint8_t camera_preview;
  std::int8_t selection_frames;
  std::int8_t reserved_298f;
  SVECTOR saved_area_center;
  SVECTOR area_center;
  std::int8_t range_shape;
  std::int8_t range_kind;
  std::int16_t range_facing;
  std::int8_t range_x;
  std::int8_t range_y;
  std::int8_t range_z;
  std::int8_t range_angle;
  SVECTOR range_origin;
  std::int8_t area_shape;
  std::int8_t area_kind;
  std::int16_t area_facing;
  std::int8_t area_x;
  std::int8_t area_y;
  std::int8_t area_z;
  std::int8_t area_angle;
  SVECTOR area_origin;
  std::array<TargetPreview, kActionPreviewCount> previews;
  std::array<std::int32_t, kActionStateReservedWords> reserved_2be8;
  std::uint16_t preview_count;
  std::int8_t previous_chain_buttons;
  std::int8_t chain_ability_slot;
  std::int8_t previous_chain_input;
  std::int8_t ability_input_succeeded;
  std::int8_t ability_input_attempted;
  std::int8_t chain_continued;
  std::uint16_t ability_timer;
  std::uint16_t ability_window;
  std::int16_t preferred_camera_distance;
  std::int16_t camera_zoom_state;
};

// The AI's view of an action: how rarely it is chosen and whether its area
// is shaped or radial.
// decomp: vs_ActionFlags (src/SLUS_010.40/actions.h:17-27)
struct ActionAiFlags {
  std::uint16_t reserved0_bit0 : 4;
  std::uint16_t opaque0_bit4 : 1;
  std::uint16_t rarely_chosen : 1;
  std::uint16_t reserved0_bit6 : 1;
  std::uint16_t shaped_area : 1;
  std::uint16_t reserved1_bit0 : 4;
  std::uint16_t opaque1_bit4 : 1;
  std::uint16_t radial_area : 1;
  std::uint16_t reserved1_bit6 : 2;
};
inline constexpr std::size_t kActionAiFlagsBytes = 0x2;
static_assert(sizeof(ActionAiFlags) == kActionAiFlagsBytes,
              "ActionAiFlags must keep its PS1 layout");

// One hit of an action: its effect, prerequisite, hit function, stat
// calculator and factor, damage type and affinity.
// decomp: vs_Action.hitParams (src/SLUS_010.40/actions.h:52-60)
struct ActionHitParams {
  std::uint32_t effect_id : 7;
  std::uint32_t prerequisite_function_id : 6;
  std::uint32_t hit_function_id : 3;
  std::uint32_t stat_calculator_id : 6;
  std::uint32_t stat_factor : 5;
  std::uint32_t damage_type : 2;
  std::uint32_t affinity : 3;
};
inline constexpr std::size_t kActionHitParamsBytes = 0x4;
static_assert(sizeof(ActionHitParams) == kActionHitParamsBytes,
              "ActionHitParams must keep its PS1 layout");

inline constexpr std::size_t kActionHitCount = 2;
inline constexpr std::size_t kActionNameBytes = 24;

// One action of the module image's action table.
// decomp: vs_Action (src/SLUS_010.40/actions.h:29-62)
struct ActionDefinition {
  std::uint8_t id;
  std::uint8_t effect_id;
  std::uint8_t returns_to_menu_on_cancel : 1;
  std::uint8_t type : 3;
  std::uint8_t category : 4;
  std::uint8_t cost;
  std::uint8_t range_x;
  std::uint8_t range_y;
  std::uint8_t range_z;
  std::uint8_t shape : 3;
  std::uint8_t angle : 5;
  std::uint8_t area_range_x;
  std::uint8_t area_range_y;
  std::uint8_t area_range_z;
  std::uint8_t area_shape;
  std::uint8_t recovery_delay;
  std::uint8_t reserved_d_bit0 : 4;
  std::uint8_t opaque_d_bit4 : 2;
  std::uint8_t resets_hit_results : 1;
  std::uint8_t unlocked : 1;
  ActionAiFlags ai_flags;
  std::int32_t reserved10;
  std::array<ActionHitParams, kActionHitCount> hits;
  std::array<char, kActionNameBytes> name;
};
inline constexpr std::size_t kActionDefinitionBytes = 0x34;
static_assert(sizeof(ActionDefinition) == kActionDefinitionBytes,
              "ActionDefinition must keep its PS1 layout");

}  // namespace VS::Battle
