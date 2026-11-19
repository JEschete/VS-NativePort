#pragma once
// Hand-maintained (tools/port/owned.csv).
// What the Items Live bindings share: the battle actor layout pins they read
// and write by name, and the battle action, actor and diagnostic helpers.
// Only the *Live.cpp files and the tests include it.

#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <utility>

#include "Battle/AbilityState.h"
#include "Battle/Action.h"
#include "Battle/Actor.h"
#include "Battle/Damage.h"
#include "Battle/Feedback.h"
#include "Battle/FloatingLabelProducersLive.hpp"
#include "Battle/StatusEffects.h"
#include "Battle/StatusLifecycleLive.hpp"
#include "Core/Diagnostics.hpp"
#include "Items/EquipmentAffinity.hpp"
#include "Items/EquipmentStats.hpp"
#include "Items/ItemRecords.hpp"
#include "Legacy/Psx.h"

namespace VS::Items::Live {
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
    VS::Battle::LiveQueueActorStatusLabels(ashley_.actorId, 0, statuses, 1);
  }
  void RemoveStatuses(std::int32_t statuses) override {
    CommitOnce();
    VS::Battle::RemoveLiveActorStatuses(statuses, &ashley_);
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
