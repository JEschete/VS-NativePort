#pragma once
// Hand-maintained (tools/port/owned.csv).
// What the Items Live bindings share: the battle actor layout pins they read
// and write by name, and the battle action, actor and diagnostic helpers.
// Only the *Live.cpp files and the tests include it.

#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <utility>

#include "Battle/AbilityState.hpp"
#include "Battle/Action.hpp"
#include "Battle/Actor.hpp"
#include "Battle/Damage.hpp"
#include "Battle/Feedback.hpp"
#include "Battle/FloatingLabelProducersLive.hpp"
#include "Battle/StatusEffects.hpp"
#include "Battle/StatusLifecycleLive.hpp"
#include "Core/Diagnostics.hpp"
#include "Items/EquipmentAffinity.hpp"
#include "Items/EquipmentStats.hpp"
#include "Items/ItemRandomLive.hpp"
#include "Items/ItemRecords.hpp"
#include "Legacy/Psx.hpp"

namespace VS::Items::Live {
// The battle actor records the bindings read and write by name.
inline constexpr std::size_t kActorStatsWeaponOffset = 0x3C;
static_assert(offsetof(VS::Battle::ActorStats, weapon) ==
              kActorStatsWeaponOffset);
inline constexpr std::size_t kActorStatsShieldOffset = 0x1D4;
static_assert(offsetof(VS::Battle::ActorStats, shield) ==
              kActorStatsShieldOffset);
inline constexpr std::size_t kActorStatsAccessoryOffset = 0x338;
static_assert(offsetof(VS::Battle::ActorStats, accessory) ==
              kActorStatsAccessoryOffset);
inline constexpr std::size_t kActorStatsLimbsOffset = 0x398;
static_assert(offsetof(VS::Battle::ActorStats, limbs) ==
              kActorStatsLimbsOffset);
inline constexpr std::size_t kActorStatsStatusesOffset = 0x948;
static_assert(offsetof(VS::Battle::ActorStats, statuses) ==
              kActorStatsStatusesOffset);
inline constexpr std::size_t kActorStatsActorIdOffset = 0x957;
static_assert(offsetof(VS::Battle::ActorStats, actor_id) ==
              kActorStatsActorIdOffset);
inline constexpr std::size_t kActorStatsDropItemOffset = 0x958;
static_assert(offsetof(VS::Battle::ActorStats, drop_item) ==
              kActorStatsDropItemOffset);
static_assert(offsetof(VS::Battle::ActorStats, second_drop_item) ==
              offsetof(VS::Battle::ActorStats, drop_item) +
                  sizeof(MiscItemStack));
inline constexpr std::size_t kActorStatsRandomDropItemOffset = 0x960;
static_assert(offsetof(VS::Battle::ActorStats, random_drop_item) ==
              kActorStatsRandomDropItemOffset);
inline constexpr std::size_t kLimbStatsBytes = 0xDC;
static_assert(sizeof(VS::Battle::LimbStats) == kLimbStatsBytes);
inline constexpr std::size_t kLimbStatsMaxHpOffset = 0x02;
static_assert(offsetof(VS::Battle::LimbStats, max_hp) == kLimbStatsMaxHpOffset);
inline constexpr std::size_t kLimbStatsNameIndexOffset = 0x06;
static_assert(offsetof(VS::Battle::LimbStats, name_index) ==
              kLimbStatsNameIndexOffset);
inline constexpr std::size_t kLimbStatsArmorOffset = 0x20;
static_assert(offsetof(VS::Battle::LimbStats, armor) == kLimbStatsArmorOffset);
static_assert(std::tuple_size_v<decltype(VS::Battle::ActorStats::limbs)> ==
              kLimbCount);
static_assert(std::tuple_size_v<decltype(VS::Battle::ActionDefinition::hits)> ==
              kActionHitCount);
// The battle actor table's length (decomp: vs_battle_actors).
inline constexpr std::size_t kBattleActorSlots = 42;
static_assert(std::tuple_size_v<decltype(VS::Battle::actors)> ==
              kBattleActorSlots);
static_assert(std::tuple_size_v<decltype(VS::Battle::limbArmorDpLoss)> >=
              kLimbCount);

// Reports a refusal the typed core returned; kOk reports nothing.
void WarnRefusal(const char* operation, ItemsStatus status);
// Reports a binding given a null record it needs, which it leaves alone.
void WarnNullRecord(const char* operation);

// What the item rules read of a battle action.
[[nodiscard]] ItemAction ToItemAction(
    const VS::Battle::ActionDefinition& action);

// The battle actor `actor_id` names; null for an id past the table.
[[nodiscard]] VS::Battle::Actor* BattleActor(std::int32_t actor_id);

// The defender's shield and limb armour, whether its weapon is drawn and
// whether its notices are shown; CommitDefender writes the equipment back.
[[nodiscard]] DefenderEquipment ToDefender(const VS::Battle::ActorStats& stats);
void CommitDefender(VS::Battle::ActorStats& stats,
                    const DefenderEquipment& defender);

// Ashley's battle actor statistics; null (reported) before she is loaded.
[[nodiscard]] VS::Battle::ActorStats* AshleyStats(const char* operation);

// Ashley's weapon with the reach and attack shape it sets on her actor;
// CommitAshleyWeapon writes back what equipping changes.
[[nodiscard]] AshleyWeaponState ToAshleyWeapon(
    const VS::Battle::ActorStats& stats, const VS::Battle::Actor& actor);
void CommitAshleyWeapon(VS::Battle::ActorStats& stats, VS::Battle::Actor& actor,
                        const AshleyWeaponState& ashley);

// Ashley's statuses on her battle actor. The equip functions call the host
// after writing the equipment; the first call commits that equipment, so the
// labels and status removal see it as the original did.
template <typename CommitEquipment>
class LiveEquipStatusHost final : public EquipStatusHost {
 public:
  LiveEquipStatusHost(VS::Battle::ActorStats& ashley, CommitEquipment commit)
      : ashley_(ashley), commit_(std::move(commit)) {}

  std::int32_t AshleyStatuses() override {
    CommitOnce();
    return ashley_.statuses;
  }
  void QueueStatusLabels(std::int32_t statuses) override {
    CommitOnce();
    VS::Battle::LiveQueueActorStatusLabels(ashley_.actor_id, 0, statuses, 1);
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

  VS::Battle::ActorStats& ashley_;
  CommitEquipment commit_;
  bool committed_ = false;
};

}  // namespace VS::Items::Live
