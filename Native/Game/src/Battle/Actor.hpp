#pragma once
// Battle actors: the actor list node, its stats block (limbs, equipment,
// limb actions and statuses) and the per-limb records inside it.

#include <array>
#include <cstddef>
#include <cstdint>

#include "Items/ItemRecords.hpp"

namespace VS::Actors {
enum class SpawnedActor : std::uint64_t;
struct ActorRecord;
struct ActorModelSlot;
struct ModelInstance;
}  // namespace VS::Actors

namespace VS::Battle {

struct ActorStats;

inline constexpr std::size_t kActorArgumentBytes = sizeof(std::int32_t);

// Actor::command_arg1, read as bytes, halves or one word.
union ActorCommandArgument {
  std::array<std::uint8_t, kActorArgumentBytes> bytes;
  std::array<std::uint16_t, 2> halves;
  std::int32_t word;
};

// Actor::last_trigger_tile, read as bytes or one word.
union ActorTriggerTile {
  std::array<std::uint8_t, kActorArgumentBytes> bytes;
  std::int32_t word;
};

inline constexpr std::size_t kActorEquipmentModelCount = 2;

// One battle actor: the list link, the scripted command, the spawn and
// equipment state, the AI groups and the stats and model it owns.
// decomp: Actor (src/BATTLE/BATTLE.PRG/actor.h:104-150; formerly
// vs_battle_actor)
struct Actor {
  VS::Actors::SpawnedActor next;
  std::int32_t id;
  std::uint8_t command_state;
  std::uint8_t command_opcode;
  std::uint8_t command_target;
  std::uint8_t command_flags;
  std::int32_t command_arg0;
  ActorCommandArgument command_arg1;
  std::int32_t command_arg2;
  std::uint16_t model_id;
  std::uint16_t sub_type;
  std::uint16_t kind;
  std::uint8_t facing;
  std::uint8_t combat_pose;
  std::int32_t weapon_drawn;
  std::uint8_t interaction_flags;
  std::uint8_t equipped_weapon_category;
  std::uint8_t retain_after_defeat;
  std::uint8_t room_spawn_index;
  std::uint8_t defeated;
  std::uint8_t kill_counted;
  std::uint8_t blade_id;
  std::uint8_t shield_id;
  ActorTriggerTile last_trigger_tile;
  std::uint32_t ai_group : 4;
  std::uint32_t reserved30_bit4 : 5;
  std::uint32_t ai_linked_group : 5;
  std::uint32_t reserved31_bit6 : 18;
  std::int32_t spawn_ai_settings;
  std::int32_t ai_settings;
  ActorStats* stats;
  std::int32_t allocation_kind;
  VS::Actors::ModelInstance* model;
  std::array<VS::Actors::ActorModelSlot*, kActorEquipmentModelCount>
      equipment_models;
  VS::Actors::ActorRecord* second_body;
};

// One limb action slot: the action and its effect model.
// decomp: Actor_unk8C0 (src/BATTLE/BATTLE.PRG/actor.h:12-17; formerly
// vs_battle_actor_unk8C0)
struct ActionSlot {
  std::uint16_t action_id;
  std::int8_t index : 4;
  std::int8_t effect_model_id : 4;
  std::int8_t opaque3;
};
inline constexpr std::size_t kActionSlotBytes = 0x4;
static_assert(sizeof(ActionSlot) == kActionSlotBytes,
              "ActionSlot must keep its PS1 layout");

inline constexpr std::size_t kLimbTypeCount = 4;
inline constexpr std::size_t kLimbAffinityCount = 8;

// One limb of an actor: its HP, bone, name, types, affinities and armor.
// decomp: vs_UiEquipment_limb (src/BATTLE/BATTLE.PRG/uiEquipment.h:121-132;
// formerly vs_battle_uiEquipment_limb)
struct LimbStats {
  std::int16_t hp;
  std::int16_t max_hp;
  std::int8_t agility_defense_bonus;
  std::int8_t anchor_bone;
  std::uint8_t name_index;
  std::int8_t chain_evasion;
  std::array<std::int16_t, kLimbTypeCount> types;
  std::array<std::int16_t, kLimbAffinityCount> affinities;
  VS::Items::ArmorLoadout armor;
};
inline constexpr std::size_t kLimbStatsArmorOffset = 0x20;
inline constexpr std::size_t kLimbStatsBytes = 0xDC;
static_assert(offsetof(LimbStats, armor) == kLimbStatsArmorOffset,
              "LimbStats::armor must keep its PS1 offset");
static_assert(sizeof(LimbStats) == kLimbStatsBytes,
              "LimbStats must keep its PS1 layout");

inline constexpr std::size_t kActorNameBytes = 24;
inline constexpr std::size_t kActorLimbCount = 6;
inline constexpr std::size_t kLimbActionCount = 4;
inline constexpr std::size_t kDpSplashCount = 6;
inline constexpr std::size_t kActorStatusTimerCount = 8;

// An actor's stats: name, HP/MP and attributes, movement, reach, equipment,
// limbs and their actions, statuses and drops.
// decomp: vs_ActorStats (src/BATTLE/BATTLE.PRG/actor.h:56-102; formerly
// vs_battle_actor2)
struct ActorStats {
  std::array<char, kActorNameBytes> name;
  std::int16_t current_hp;
  std::int16_t max_hp;
  std::int16_t current_mp;
  std::int16_t max_mp;
  std::uint16_t risk;
  std::uint16_t strength;
  std::uint16_t total_strength;
  std::uint16_t intelligence;
  std::uint16_t total_intelligence;
  std::uint16_t agility;
  std::uint16_t total_agility;
  std::int8_t base_walk_speed;
  std::int8_t base_run_speed;
  std::int8_t movement_class;
  std::int8_t walk_speed;
  std::int8_t gait_class;
  std::int8_t run_speed;
  std::uint32_t mobility_limb0 : 8;
  std::uint32_t mobility_limb1 : 8;
  std::uint32_t default_target_limb : 8;
  std::uint32_t enemy_class : 3;
  std::uint32_t reach : 5;
  std::uint32_t current_range : 8;
  std::uint32_t current_range_y : 8;
  std::uint32_t current_range_z : 8;
  std::uint32_t current_attack_shape : 3;
  std::uint32_t current_attack_shape_angle : 5;
  VS::Items::WeaponLoadout weapon;
  VS::Items::ShieldLoadout shield;
  VS::Items::AccessorySpawnLoadout accessory;
  std::array<LimbStats, kActorLimbCount> limbs;
  std::array<std::array<ActionSlot, kLimbActionCount>, kActorLimbCount>
      limb_actions;
  std::array<std::array<std::int8_t, kDpSplashCount>, kActorLimbCount>
      dp_splash;
  std::int32_t status_immunities;
  std::int32_t statuses;
  std::array<std::uint8_t, kActorStatusTimerCount> status_timers;
  std::uint16_t action_recovery_timer;
  std::uint8_t cancels_hit_results : 1;
  std::uint8_t is_analyzed : 1;
  std::uint8_t recovery_delay_bonus : 6;
  std::uint8_t actor_id;
  VS::Items::MiscItemStack drop_item;
  VS::Items::MiscItemStack second_drop_item;
  VS::Items::MiscItemDrop random_drop_item;
};
inline constexpr std::size_t kActorStatsWeaponOffset = 0x3C;
inline constexpr std::size_t kActorStatsShieldOffset = 0x1D4;
inline constexpr std::size_t kActorStatsAccessoryOffset = 0x338;
inline constexpr std::size_t kActorStatsLimbsOffset = 0x398;
inline constexpr std::size_t kActorStatsLimbActionsOffset = 0x8C0;
inline constexpr std::size_t kActorStatsDpSplashOffset = 0x920;
inline constexpr std::size_t kActorStatsStatusImmunitiesOffset = 0x944;
inline constexpr std::size_t kActorStatsStatusTimersOffset = 0x94C;
inline constexpr std::size_t kActorStatsActionRecoveryTimerOffset = 0x954;
inline constexpr std::size_t kActorStatsActorIdOffset = 0x957;
inline constexpr std::size_t kActorStatsDropItemOffset = 0x958;
inline constexpr std::size_t kActorStatsRandomDropItemOffset = 0x960;
inline constexpr std::size_t kActorStatsBytes = 0x964;
static_assert(offsetof(ActorStats, weapon) == kActorStatsWeaponOffset);
static_assert(offsetof(ActorStats, shield) == kActorStatsShieldOffset);
static_assert(offsetof(ActorStats, accessory) == kActorStatsAccessoryOffset);
static_assert(offsetof(ActorStats, limbs) == kActorStatsLimbsOffset);
static_assert(offsetof(ActorStats, limb_actions) ==
              kActorStatsLimbActionsOffset);
static_assert(offsetof(ActorStats, dp_splash) == kActorStatsDpSplashOffset);
static_assert(offsetof(ActorStats, status_immunities) ==
              kActorStatsStatusImmunitiesOffset);
static_assert(offsetof(ActorStats, status_timers) ==
              kActorStatsStatusTimersOffset);
static_assert(offsetof(ActorStats, action_recovery_timer) ==
              kActorStatsActionRecoveryTimerOffset);
static_assert(offsetof(ActorStats, actor_id) == kActorStatsActorIdOffset);
static_assert(offsetof(ActorStats, drop_item) == kActorStatsDropItemOffset);
static_assert(offsetof(ActorStats, random_drop_item) ==
              kActorStatsRandomDropItemOffset);
static_assert(sizeof(ActorStats) == kActorStatsBytes,
              "ActorStats must keep its PS1 layout");

}  // namespace VS::Battle
