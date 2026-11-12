// Hand-maintained (tools/port/owned.csv).
#include "Battle/StatusHelpersLive.hpp"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iterator>
#include <optional>
#include <type_traits>

#include "Battle/StatusHelpers.hpp"
#include "Items/ItemRecordsLive.hpp"
extern "C" {
#include "Battle/AbilityState.h"
#include "Battle/ActionFlow.h"
#include "Battle/Actor.h"
#include "Battle/Damage.h"
#include "Battle/Feedback.h"
#include "Battle/StatusEffects.h"
#include "Core/Diagnostics.h"
#include "Field/Interactions.h"
#include "State/Timers.h"
}

namespace VS::Battle {
namespace {
void Require(bool valid, const char* message) {
  if (!valid) {
    Core_Errorf("battle", "%s", message);
    std::abort();
  }
}

Battle_ActorStats& StatsOf(Battle_ActorStats* stats) {
  Require(stats != nullptr, "Element enchantment requires actor stats");
  return *stats;
}

bool ValidActor(int slot) {
  return slot >= 0 && static_cast<std::size_t>(slot) < std::size(Battle_actors);
}

class LiveStatNumberWorld final : public StatNumberWorld {
 public:
  [[nodiscard]] std::optional<std::uint8_t> RoomSpawnIndex(
      int slot) const override {
    if (!ValidActor(slot) || Battle_actors[slot] == nullptr) {
      return std::nullopt;
    }
    return Battle_actors[slot]->roomSpawnIndex;
  }
  void QueueValueLabel(const StatNumberRequest& request) override {
    Battle_QueueActorValueLabel(request.actor_slot, request.limb_tag,
                                request.encoded_amount, request.label_flags);
  }
};

class LiveTargetingRiskWorld final : public TargetingRiskWorld {
 public:
  [[nodiscard]] std::uint32_t ControlMode() const override {
    return Field_controlMode;
  }
  [[nodiscard]] std::uint32_t ActionFlowMode() const override {
    return Battle_actionFlowMode;
  }
  [[nodiscard]] std::int32_t TickSpeed() const override {
    return State_tickSpeed;
  }
  [[nodiscard]] std::optional<TargetingRiskTarget> SelectedTarget()
      const override {
    if (Battle_actionState == nullptr) return std::nullopt;
    const auto& target = Battle_actionState->selection.primaryTarget;
    const int slot = target.target.targetActor;
    if (!ValidActor(slot)) return std::nullopt;
    const auto* actor = Battle_actors[slot];
    if (actor == nullptr || actor->stats == nullptr) return std::nullopt;
    return TargetingRiskTarget{
        .actor_slot = slot,
        .statuses = std::bit_cast<std::uint32_t>(actor->stats->statuses),
        .targets_position = target.targetsPosition != 0};
  }
  [[nodiscard]] std::optional<std::uint16_t> PlayerRisk() const override {
    if (Battle_characterState == nullptr ||
        Battle_characterState->stats == nullptr) {
      return std::nullopt;
    }
    return Battle_characterState->stats->risk;
  }
  void SetPlayerRisk(std::uint16_t risk) override {
    Require(Battle_characterState != nullptr &&
                Battle_characterState->stats != nullptr,
            "Targeting risk requires the player owner");
    Battle_characterState->stats->risk = risk;
  }
};
}  // namespace

// decomp: func_80086EA4 (src/BATTLE/BATTLE.PRG/146C.c:13801).
void ApplyLiveWeaponElementEnchantment(Battle_ActorStats* pointer,
                                       int increased, int decreased,
                                       int amount) {
  auto& stats = StatsOf(pointer);
  if (stats.weapon.blade.id == 0) return;
  auto weapon = Items::Live::Typed(stats.weapon);
  Require(ApplyWeaponElementEnchantment(weapon, {increased, decreased, amount}),
          "Weapon enchantment refused its affinity indices");
  Items::Live::Commit(stats.weapon, weapon);
}

// decomp: func_80086EF4 (src/BATTLE/BATTLE.PRG/146C.c:13811).
void ApplyLiveArmorElementEnchantment(Battle_ActorStats* pointer, int increased,
                                      int decreased, int amount) {
  auto& stats = StatsOf(pointer);
  Items::ShieldLoadout shield{};
  if (stats.shield.base.id != 0) {
    shield = Items::Live::Typed(stats.shield);
  }
  std::array<Items::ArmorLoadout, Items::kLimbCount> armor{};
  static_assert(std::extent_v<decltype(Battle_ActorStats::limbs)> ==
                Items::kLimbCount);
  for (std::size_t index = 0; index < armor.size(); ++index) {
    if (stats.limbs[index].armor.armor.id != 0) {
      armor.at(index) = Items::Live::Typed(stats.limbs[index].armor);
    }
  }
  Require(ApplyArmorElementEnchantment(shield, armor,
                                       {increased, decreased, amount}),
          "Armor enchantment refused its affinity indices");
  if (stats.shield.base.id != 0) {
    Items::Live::Commit(stats.shield, shield);
  }
  for (std::size_t index = 0; index < armor.size(); ++index) {
    if (stats.limbs[index].armor.armor.id != 0) {
      Items::Live::Commit(stats.limbs[index].armor, armor.at(index));
    }
  }
}

// decomp: func_80087578 (src/BATTLE/BATTLE.PRG/146C.c:14011).
void QueueLiveActorStatChangeNumber(int actor_slot, int limb_tag,
                                    int encoded_amount, int label_flags) {
  LiveStatNumberWorld world;
  const auto queued = QueueActorStatChangeNumber(
      world, {actor_slot, limb_tag,
              std::bit_cast<std::uint32_t>(encoded_amount), label_flags});
  Require(queued.has_value(), "Stat number refused a missing or invalid actor");
}

// decomp: func_80088B8C (src/BATTLE/BATTLE.PRG/146C.c:14745).
void UpdateLiveTargetingRiskPenalty() {
  LiveTargetingRiskWorld world;
  Require(UpdateTargetingRiskPenalty(Battle_targetingRiskTimer, world),
          "Targeting risk refused its required state");
}

// decomp: func_80088CA0 (src/BATTLE/BATTLE.PRG/146C.c:14764).
void ResetLiveTargetingRiskPenaltyTimer() {
  ResetTargetingRiskPenaltyTimer(Battle_targetingRiskTimer);
}
}  // namespace VS::Battle
