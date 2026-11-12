// Hand-maintained (tools/port/owned.csv).
#include "Battle/ActionCostsLive.hpp"

#include <array>
#include <bit>
#include <cstdlib>
#include <iterator>
#include <optional>

#include "Battle/ActionCosts.hpp"
extern "C" {
#include "Battle/Action.h"
#include "Battle/ActionFlow.h"
#include "Battle/ActionTable.h"
#include "Battle/Actor.h"
#include "Battle/Damage.h"
#include "Core/Diagnostics.h"
}

namespace VS::Battle {
namespace {
[[noreturn]] void Refuse() {
  Core_Errorf("battle",
              "Action cost refused missing used state/index or unsupported "
              "weapon resource type/cost (#352; only 1..4 or 0/0 supported)");
  std::abort();
}
template <typename T>
T Checked(std::optional<T> value) {
  if (!value) Refuse();
  return value.value();
}
class LiveCostActor final : public ActionCostActor {
 public:
  explicit LiveCostActor(Battle_ActorStats* actor) : actor_(actor) {}
  ActionResources Resources() override {
    return {actor_->currentHp,
            actor_->currentMp,
            actor_->risk,
            actor_->weapon.currentPp,
            static_cast<std::uint8_t>(actor_->weapon.actionType),
            static_cast<std::uint8_t>(actor_->weapon.risk)};
  }
  void Pay(CostResource resource, std::uint16_t value) override {
    switch (resource) {
      case CostResource::kMp:
        actor_->currentMp = std::bit_cast<std::int16_t>(value);
        break;
      case CostResource::kRisk:
        actor_->risk = value;
        break;
      case CostResource::kHp:
        actor_->currentHp = std::bit_cast<std::int16_t>(value);
        break;
      case CostResource::kWeaponPp:
        actor_->weapon.currentPp = value;
        break;
      case CostResource::kNone:
        break;
    }
  }
  std::uint16_t RecoveryTimer() override { return actor_->actionRecoveryTimer; }
  void SetRecoveryTimer(std::uint16_t value) override {
    actor_->actionRecoveryTimer = value;
  }
  std::uint8_t RecoveryBonus() override { return actor_->recoveryDelayBonus; }
  std::uint8_t WeaponRecoveryDelay() override {
    return static_cast<std::uint8_t>(actor_->weapon.recoveryDelay);
  }
  std::uint32_t Statuses() override {
    return static_cast<std::uint32_t>(actor_->statuses);
  }

 private:
  Battle_ActorStats* actor_;
};
class LiveCostDefinition final : public ActionCostDefinition {
 public:
  explicit LiveCostDefinition(Battle_ActionDefinition& definition)
      : definition_(definition) {}
  std::uint8_t Type() override { return definition_.type; }
  std::uint8_t Cost() override { return definition_.cost; }
  std::uint8_t RecoveryDelay() override { return definition_.recoveryDelay; }
  bool Unlocked() override { return definition_.unlocked != 0; }

 private:
  Battle_ActionDefinition& definition_;
};
Battle_Actor* ActorAt(int slot) {
  if (slot < 0 || static_cast<std::size_t>(slot) >= std::size(Battle_actors)) {
    return nullptr;
  }
  return Battle_actors[slot];
}
class LiveCostWorld final : public ActionCostWorld {
 public:
  ActionCostDefinition* Definition(std::uint32_t action) override {
    if (action >= definitions_.size()) return nullptr;
    auto& entry = definitions_.at(action);
    if (!entry) entry.emplace(Battle_actionDefinitions[action]);
    return &entry.value();
  }
  std::optional<ActionCostActor*> Actor(int slot) override {
    auto* actor = ActorAt(slot);
    if (actor == nullptr) return std::nullopt;
    if (actor->stats == nullptr) return nullptr;
    return &actor_.emplace(actor->stats);
  }
  std::optional<std::uint32_t> ChainStep() override {
    return Battle_actionState ? std::optional{Battle_actionState->chainStep}
                              : std::nullopt;
  }
  std::optional<std::uint32_t> WeaponDrawn(int slot) override {
    auto* actor = ActorAt(slot);
    return actor ? std::optional{static_cast<std::uint32_t>(actor->weaponDrawn)}
                 : std::nullopt;
  }

 private:
  std::array<std::optional<LiveCostDefinition>,
             std::size(Battle_actionDefinitions)>
      definitions_{};
  std::optional<LiveCostActor> actor_;
};
}  // namespace

// decomp: _getActionCost (src/BATTLE/BATTLE.PRG/146C.c:13075), WORD3.
std::uint32_t LiveActionCostAndAvailability(std::uint32_t action,
                                            Battle_ActorStats* actor,
                                            int update) {
  LiveCostWorld world;
  std::optional<LiveCostActor> owner;
  if (actor != nullptr) owner.emplace(actor);
  return Checked(ActionCostAndAvailability(
                     action, owner ? &owner.value() : nullptr, update, world))
      .Packed();
}
// decomp: func_80085978 (src/BATTLE/BATTLE.PRG/146C.c:13167), VOID2.
void LivePreviewActorActionCost(int action, int slot) {
  LiveCostWorld world;
  if (!PreviewActorActionCost(action, slot, world)) Refuse();
}
// decomp: func_800859B4 (src/BATTLE/BATTLE.PRG/146C.c:13172), VOID3.
void LiveAddActionRecoveryDelay(std::uint32_t action, Battle_ActorStats* actor,
                                int execute) {
  LiveCostWorld world;
  std::optional<LiveCostActor> owner;
  if (actor != nullptr) owner.emplace(actor);
  if (!AddActionRecoveryDelay(action, owner ? &owner.value() : nullptr, execute,
                              world)) {
    Refuse();
  }
}
// decomp: vs_battle_getActionFlags (src/BATTLE/BATTLE.PRG/146C.c:15566), INT2.
int LiveActorActionRestrictionFlags(int slot, int action) {
  LiveCostWorld world;
  return static_cast<int>(
      Checked(ActorActionRestrictionFlags(slot, action, world)));
}
}  // namespace VS::Battle
