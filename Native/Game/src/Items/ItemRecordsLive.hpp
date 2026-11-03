#pragma once
// Hand-maintained (tools/port/owned.csv).
// The Items Live bindings' view of the legacy records: which typed record
// (ItemRecords.hpp) each Items_* struct holds, the layout pins that make the
// byte copy between them exact, and the battle action, actor and diagnostic
// helpers the bindings share. Only the *Live.cpp files and the tests include
// it.

#include <bit>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <type_traits>
#include <utility>

#include "Items/EquipmentAffinity.hpp"
#include "Items/EquipmentStats.hpp"
#include "Items/ItemRecords.hpp"

extern "C" {
#include "Battle/Abilities.h"
#include "Battle/Action.h"
#include "Battle/Actor.h"
#include "Battle/Damage.h"
#include "Battle/Feedback.h"
#include "Battle/StatusEffects.h"
#include "Core/Diagnostics.h"
#include "Items/Equipment.h"
#include "Items/EquipmentInit.h"
#include "Items/Inventory.h"
#include "Items/Loadout.h"
#include "Items/Loot.h"
#include "Items/TreasureChests.h"
#include "Legacy/Psx.h"
}

namespace VS::Items::Live {
template <typename Legacy>
struct TypedRecord;

template <>
struct TypedRecord<Items_EquipmentPartStats> {
  using type = EquipmentPart;
};
template <>
struct TypedRecord<Items_WeaponLoadout> {
  using type = WeaponLoadout;
};
template <>
struct TypedRecord<Items_ShieldLoadout> {
  using type = ShieldLoadout;
};
template <>
struct TypedRecord<Items_ArmorLoadout> {
  using type = ArmorLoadout;
};
template <>
struct TypedRecord<Items_AccessorySpawnLoadout> {
  using type = AccessorySpawnLoadout;
};
template <>
struct TypedRecord<Items_WeaponSpawnLoadout> {
  using type = WeaponSpawnLoadout;
};
template <>
struct TypedRecord<Items_ShieldSpawnLoadout> {
  using type = ShieldSpawnLoadout;
};
template <>
struct TypedRecord<Items_ArmorInit> {
  using type = ArmorInit;
};
template <>
struct TypedRecord<Items_AccessoryInit> {
  using type = AccessoryInit;
};
template <>
struct TypedRecord<Items_InventoryArmor> {
  using type = InventoryArmor;
};
template <>
struct TypedRecord<Items_InventoryBlade> {
  using type = InventoryBlade;
};
template <>
struct TypedRecord<Items_InventoryGem> {
  using type = InventoryGem;
};
template <>
struct TypedRecord<Items_InventoryGrip> {
  using type = InventoryGrip;
};
template <>
struct TypedRecord<Items_InventoryMisc> {
  using type = InventoryMisc;
};
template <>
struct TypedRecord<Items_InventoryShield> {
  using type = InventoryShield;
};
template <>
struct TypedRecord<Items_InventoryWeapon> {
  using type = InventoryWeapon;
};
template <>
struct TypedRecord<Items_Inventory> {
  using type = InventoryContents;
};
template <>
struct TypedRecord<Items_InventoryIndices> {
  using type = InventoryIndices;
};
template <>
struct TypedRecord<Items_MiscItemStack> {
  using type = MiscItemStack;
};
template <>
struct TypedRecord<Items_MiscItemDrop> {
  using type = MiscItemDrop;
};
template <>
struct TypedRecord<Items_LootedWeapon> {
  using type = LootedWeapon;
};
template <>
struct TypedRecord<Items_LootedShield> {
  using type = LootedShield;
};
template <>
struct TypedRecord<Items_LootedArmor> {
  using type = LootedArmor;
};
// The accessory slot of the loot holds the looted-armour record.
template <>
struct TypedRecord<Items_LootedAccessory> {
  using type = LootedArmor;
};
template <>
struct TypedRecord<Items_BattleLoot> {
  using type = BattleLoot;
};
template <>
struct TypedRecord<Items_TreasureChest> {
  using type = TreasureChest;
};

template <typename Legacy>
using TypedRecordOf = typename TypedRecord<Legacy>::type;

// Every pair has one size and a typed side without padding, so the byte copy
// both ways is the identity.
template <typename Legacy>
consteval bool SameLayout() {
  return sizeof(Legacy) == sizeof(TypedRecordOf<Legacy>) &&
         std::has_unique_object_representations_v<TypedRecordOf<Legacy>> &&
         std::is_trivially_copyable_v<Legacy>;
}

template <typename Legacy>
[[nodiscard]] TypedRecordOf<Legacy> Typed(const Legacy& legacy) {
  static_assert(SameLayout<Legacy>());
  return std::bit_cast<TypedRecordOf<Legacy>>(legacy);
}

template <typename Legacy>
void Commit(Legacy& legacy, const TypedRecordOf<Legacy>& typed) {
  static_assert(SameLayout<Legacy>());
  legacy = std::bit_cast<Legacy>(typed);
}

// The fields the typed records name, at the offsets the legacy structs keep.
static_assert(offsetof(Items_EquipmentPartStats, currentDp) ==
              offsetof(EquipmentPart, current_dp));
static_assert(offsetof(Items_EquipmentPartStats, damageType) ==
              offsetof(EquipmentPart, damage_type));
static_assert(offsetof(Items_EquipmentPartStats, gemEffects) ==
              offsetof(EquipmentPart, gem_effects));
static_assert(offsetof(Items_EquipmentPartStats, range) ==
              offsetof(EquipmentPart, range));
static_assert(offsetof(Items_EquipmentPartStats, classes) ==
              offsetof(EquipmentPart, classes));
static_assert(offsetof(Items_EquipmentPartStats, affinities) ==
              offsetof(EquipmentPart, affinities));
static_assert(offsetof(Items_WeaponLoadout, blade) ==
              offsetof(WeaponLoadout, blade));
static_assert(offsetof(Items_WeaponLoadout, gems) ==
              offsetof(WeaponLoadout, gems));
static_assert(offsetof(Items_WeaponLoadout, wepId) ==
              offsetof(WeaponLoadout, weapon_model_id));
static_assert(offsetof(Items_WeaponLoadout, damageType) ==
              offsetof(WeaponLoadout, damage_type));
static_assert(offsetof(Items_WeaponLoadout, currentPp) ==
              offsetof(WeaponLoadout, current_pp));
static_assert(offsetof(Items_WeaponLoadout, currentDp) ==
              offsetof(WeaponLoadout, current_dp));
static_assert(offsetof(Items_WeaponLoadout, currentStr) ==
              offsetof(WeaponLoadout, attributes));
static_assert(offsetof(Items_WeaponLoadout, range) ==
              offsetof(WeaponLoadout, range));
static_assert(offsetof(Items_WeaponLoadout, classAffinityCurrent) ==
              offsetof(WeaponLoadout, class_affinity));
static_assert(offsetof(Items_WeaponLoadout, affinityModifiers) ==
              offsetof(WeaponLoadout, affinity_modifiers));
static_assert(offsetof(Items_WeaponLoadout, classAffinityBaseline) ==
              offsetof(WeaponLoadout, baseline));
static_assert(offsetof(Items_ShieldLoadout, base) ==
              offsetof(ShieldLoadout, base));
static_assert(offsetof(Items_ShieldLoadout, index) ==
              offsetof(ShieldLoadout, index));
static_assert(offsetof(Items_ShieldLoadout, currentPp) ==
              offsetof(ShieldLoadout, current_pp));
static_assert(offsetof(Items_ShieldLoadout, currentDp) ==
              offsetof(ShieldLoadout, current_dp));
static_assert(offsetof(Items_ShieldLoadout, types) ==
              offsetof(ShieldLoadout, types));
static_assert(offsetof(Items_ShieldLoadout, classAffinityBaseline) ==
              offsetof(ShieldLoadout, baseline));
static_assert(offsetof(Items_ArmorLoadout, currentDp) ==
              offsetof(ArmorLoadout, current_dp));
static_assert(offsetof(Items_ArmorLoadout, classAffinityCurrent) ==
              offsetof(ArmorLoadout, class_affinity));
static_assert(offsetof(Items_ArmorLoadout, index) ==
              offsetof(ArmorLoadout, index));
static_assert(offsetof(Items_ArmorLoadout, classAffinityBaseline) ==
              offsetof(ArmorLoadout, baseline));
static_assert(offsetof(Items_AccessorySpawnLoadout, dropRate) ==
              offsetof(AccessorySpawnLoadout, drop_rate));
static_assert(offsetof(Items_AccessorySpawnLoadout, classes) ==
              offsetof(AccessorySpawnLoadout, classes));
static_assert(offsetof(Items_WeaponSpawnLoadout, material) ==
              offsetof(WeaponSpawnLoadout, material));
static_assert(offsetof(Items_WeaponSpawnLoadout, name) ==
              offsetof(WeaponSpawnLoadout, name));
static_assert(offsetof(Items_ShieldSpawnLoadout, index) ==
              offsetof(ShieldSpawnLoadout, index));
static_assert(offsetof(Items_ArmorInit, index) == offsetof(ArmorInit, index));
static_assert(offsetof(Items_AccessoryInit, index) ==
              offsetof(AccessoryInit, index));
static_assert(offsetof(Items_InventoryArmor, types) ==
              offsetof(InventoryArmor, types));
static_assert(offsetof(Items_InventoryArmor, material) ==
              offsetof(InventoryArmor, material));
static_assert(offsetof(Items_InventoryBlade, cost) ==
              offsetof(InventoryBlade, cost));
static_assert(offsetof(Items_InventoryBlade, range) ==
              offsetof(InventoryBlade, range));
static_assert(offsetof(Items_InventoryBlade, material) ==
              offsetof(InventoryBlade, material));
static_assert(offsetof(Items_InventoryGem, gemEffects) ==
              offsetof(InventoryGem, gem_effects));
static_assert(offsetof(Items_InventoryGem, setItemIndex) ==
              offsetof(InventoryGem, set_item_index));
static_assert(offsetof(Items_InventoryGrip, types) ==
              offsetof(InventoryGrip, types));
static_assert(offsetof(Items_InventoryShield, base) ==
              offsetof(InventoryShield, base));
static_assert(offsetof(Items_InventoryShield, gems) ==
              offsetof(InventoryShield, gems));
static_assert(offsetof(Items_InventoryWeapon, gems) ==
              offsetof(InventoryWeapon, gems));
static_assert(offsetof(Items_InventoryWeapon, name) ==
              offsetof(InventoryWeapon, name));
static_assert(offsetof(Items_Inventory, shields) ==
              offsetof(InventoryContents, shields));
static_assert(offsetof(Items_Inventory, gems) ==
              offsetof(InventoryContents, gems));
static_assert(offsetof(Items_Inventory, misc) ==
              offsetof(InventoryContents, misc));
static_assert(offsetof(Items_InventoryIndices, gems) ==
              offsetof(InventoryIndices, gems));
static_assert(offsetof(Items_InventoryIndices, misc) ==
              offsetof(InventoryIndices, misc));
static_assert(offsetof(Items_BattleLoot, shield) ==
              offsetof(BattleLoot, shield));
static_assert(offsetof(Items_BattleLoot, armor) == offsetof(BattleLoot, armor));
static_assert(offsetof(Items_BattleLoot, accessory) ==
              offsetof(BattleLoot, accessory));
static_assert(offsetof(Items_BattleLoot, misc) == offsetof(BattleLoot, misc));
static_assert(offsetof(Items_TreasureChest, openedFlag) ==
              offsetof(TreasureChest, opened_flag));
static_assert(offsetof(Items_TreasureChest, loot) ==
              offsetof(TreasureChest, loot));
// The battle actor records the bindings read and write by name.
inline constexpr std::size_t kActorStatsWeaponOffset = 0x3C;
static_assert(offsetof(Battle_ActorStats, weapon) == kActorStatsWeaponOffset);
inline constexpr std::size_t kActorStatsShieldOffset = 0x1D4;
static_assert(offsetof(Battle_ActorStats, shield) == kActorStatsShieldOffset);
inline constexpr std::size_t kActorStatsAccessoryOffset = 0x338;
static_assert(offsetof(Battle_ActorStats, accessory) ==
              kActorStatsAccessoryOffset);
inline constexpr std::size_t kActorStatsLimbsOffset = 0x398;
static_assert(offsetof(Battle_ActorStats, limbs) == kActorStatsLimbsOffset);
inline constexpr std::size_t kActorStatsStatusesOffset = 0x948;
static_assert(offsetof(Battle_ActorStats, statuses) ==
              kActorStatsStatusesOffset);
inline constexpr std::size_t kActorStatsActorIdOffset = 0x957;
static_assert(offsetof(Battle_ActorStats, actorId) == kActorStatsActorIdOffset);
inline constexpr std::size_t kActorStatsDropItemOffset = 0x958;
static_assert(offsetof(Battle_ActorStats, dropItem) ==
              kActorStatsDropItemOffset);
static_assert(offsetof(Battle_ActorStats, secondDropItem) ==
              offsetof(Battle_ActorStats, dropItem) + sizeof(MiscItemStack));
inline constexpr std::size_t kActorStatsRandomDropItemOffset = 0x960;
static_assert(offsetof(Battle_ActorStats, randomDropItem) ==
              kActorStatsRandomDropItemOffset);
inline constexpr std::size_t kLimbStatsBytes = 0xDC;
static_assert(sizeof(Battle_LimbStats) == kLimbStatsBytes);
inline constexpr std::size_t kLimbStatsMaxHpOffset = 0x02;
static_assert(offsetof(Battle_LimbStats, maxHp) == kLimbStatsMaxHpOffset);
inline constexpr std::size_t kLimbStatsNameIndexOffset = 0x06;
static_assert(offsetof(Battle_LimbStats, nameIndex) ==
              kLimbStatsNameIndexOffset);
inline constexpr std::size_t kLimbStatsArmorOffset = 0x20;
static_assert(offsetof(Battle_LimbStats, armor) == kLimbStatsArmorOffset);
static_assert(std::extent_v<decltype(Battle_ActorStats::limbs)> == kLimbCount);
static_assert(std::extent_v<decltype(Battle_ActionDefinition::hits)> ==
              kActionHitCount);
// The battle actor table's length (decomp: vs_battle_actors).
inline constexpr std::size_t kBattleActorSlots = 42;
static_assert(std::extent_v<decltype(Battle_actors)> == kBattleActorSlots);
static_assert(std::extent_v<decltype(Battle_limbArmorDpLoss)> >= kLimbCount);

// The typed copy of a record that may be absent.
template <typename Legacy>
[[nodiscard]] std::optional<TypedRecordOf<Legacy>> TypedOrNone(
    const Legacy* legacy) {
  if (legacy == nullptr) return std::nullopt;
  return Typed(*legacy);
}

template <typename Record>
[[nodiscard]] const Record* OrNull(const std::optional<Record>& record) {
  return record.has_value() ? &*record : nullptr;
}

// Reports a refusal the typed core returned; kOk reports nothing.
void WarnRefusal(const char* operation, ItemsStatus status);
// Reports a binding given a null record it needs, which it leaves alone.
void WarnNullRecord(const char* operation);

// What the item rules read of a battle action.
[[nodiscard]] ItemAction ToItemAction(const Battle_ActionDefinition& action);

// The battle actor `actor_id` names; null for an id past the table.
[[nodiscard]] Battle_Actor* BattleActor(std::int32_t actor_id);

// The defender's shield and limb armour, whether its weapon is drawn and
// whether its notices are shown; CommitDefender writes the equipment back.
[[nodiscard]] DefenderEquipment ToDefender(const Battle_ActorStats& stats);
void CommitDefender(Battle_ActorStats& stats,
                    const DefenderEquipment& defender);

// Ashley's battle actor statistics; null (reported) before she is loaded.
[[nodiscard]] Battle_ActorStats* AshleyStats(const char* operation);

// Ashley's weapon with the reach and attack shape it sets on her actor;
// CommitAshleyWeapon writes back what equipping changes.
[[nodiscard]] AshleyWeaponState ToAshleyWeapon(const Battle_ActorStats& stats,
                                               const Battle_Actor& actor);
void CommitAshleyWeapon(Battle_ActorStats& stats, Battle_Actor& actor,
                        const AshleyWeaponState& ashley);

// Ashley's statuses on her battle actor. The equip functions call the host
// after writing the equipment; the first call commits that equipment, so the
// labels and status removal see it as the original did.
template <typename CommitEquipment>
class LiveEquipStatusHost final : public EquipStatusHost {
 public:
  LiveEquipStatusHost(Battle_ActorStats& ashley, CommitEquipment commit)
      : ashley_(ashley), commit_(std::move(commit)) {}

  std::int32_t AshleyStatuses() override {
    CommitOnce();
    return ashley_.statuses;
  }
  void QueueStatusLabels(std::int32_t statuses) override {
    CommitOnce();
    Battle_QueueActorStatusLabels(ashley_.actorId, 0, statuses, 1);
  }
  void RemoveStatuses(std::int32_t statuses) override {
    CommitOnce();
    Battle_RemoveActorStatuses(statuses, &ashley_);
  }
  // Commits the equipment when no status call did.
  void Finish() { CommitOnce(); }

 private:
  void CommitOnce() {
    if (committed_) return;
    committed_ = true;
    commit_();
  }

  Battle_ActorStats& ashley_;
  CommitEquipment commit_;
  bool committed_ = false;
};

// The game's random draws (SLUS_010.40 vs_main_getRand*).
class LiveItemRandom final : public ItemRandom {
 public:
  [[nodiscard]] std::int32_t NextBounded(std::int32_t bound) override;
  [[nodiscard]] std::int32_t NextAveraged(std::int32_t bound) override;
};
}  // namespace VS::Items::Live
