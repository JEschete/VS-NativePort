// Hand-maintained (tools/port/owned.csv).
#include "Battle/ActionCostsLive.hpp"

#include <array>
#include <bit>
#include <cstdlib>
#include <iterator>
#include <optional>

#include "Battle/Action.hpp"
#include "Battle/ActionCosts.hpp"
#include "Battle/ActionFlow.hpp"
#include "Battle/ActionTable.hpp"
#include "Battle/Actor.hpp"
#include "Battle/Damage.hpp"
#include "Core/Diagnostics.hpp"

using VS::Battle::ActionDefinition;

namespace VS::Battle {
namespace {
[[noreturn]] void Refuse() {
  VS::Core::Errorf(
      "battle",
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
  explicit LiveCostActor(VS::Battle::ActorStats* actor) : actor_(actor) {}
  ActionResources Resources() override {
    return {actor_->current_hp,
            actor_->current_mp,
            actor_->risk,
            actor_->weapon.current_pp,
            static_cast<std::uint8_t>(actor_->weapon.action_type),
            static_cast<std::uint8_t>(actor_->weapon.risk)};
  }
  void Pay(CostResource resource, std::uint16_t value) override {
    switch (resource) {
      case CostResource::kMp:
        actor_->current_mp = std::bit_cast<std::int16_t>(value);
        break;
      case CostResource::kRisk:
        actor_->risk = value;
        break;
      case CostResource::kHp:
        actor_->current_hp = std::bit_cast<std::int16_t>(value);
        break;
      case CostResource::kWeaponPp:
        actor_->weapon.current_pp = value;
        break;
      case CostResource::kNone:
        break;
    }
  }
  std::uint16_t RecoveryTimer() override {
    return actor_->action_recovery_timer;
  }
  void SetRecoveryTimer(std::uint16_t value) override {
    actor_->action_recovery_timer = value;
  }
  std::uint8_t RecoveryBonus() override { return actor_->recovery_delay_bonus; }
  std::uint8_t WeaponRecoveryDelay() override {
    return static_cast<std::uint8_t>(actor_->weapon.recovery_delay);
  }
  std::uint32_t Statuses() override {
    return static_cast<std::uint32_t>(actor_->statuses);
  }

 private:
  VS::Battle::ActorStats* actor_;
};
class LiveCostDefinition final : public ActionCostDefinition {
 public:
  explicit LiveCostDefinition(ActionDefinition& definition)
      : definition_(definition) {}
  std::uint8_t Type() override { return definition_.type; }
  std::uint8_t Cost() override { return definition_.cost; }
  std::uint8_t RecoveryDelay() override { return definition_.recovery_delay; }
  bool Unlocked() override { return definition_.unlocked != 0; }

 private:
  ActionDefinition& definition_;
};
VS::Battle::Actor* ActorAt(int slot) {
  if (slot < 0 ||
      static_cast<std::size_t>(slot) >= std::size(VS::Battle::actors)) {
    return nullptr;
  }
  return VS::Battle::actors[slot];
}
class LiveCostWorld final : public ActionCostWorld {
 public:
  ActionCostDefinition* Definition(std::uint32_t action) override {
    if (action >= definitions_.size()) return nullptr;
    auto& entry = definitions_.at(action);
    if (!entry) entry.emplace(VS::Battle::actionDefinitions[action]);
    return &entry.value();
  }
  std::optional<ActionCostActor*> Actor(int slot) override {
    auto* actor = ActorAt(slot);
    if (actor == nullptr) return std::nullopt;
    if (actor->stats == nullptr) return nullptr;
    return &actor_.emplace(actor->stats);
  }
  std::optional<std::uint32_t> ChainStep() override {
    return VS::Battle::actionState
               ? std::optional{VS::Battle::actionState->chain_step}
               : std::nullopt;
  }
  std::optional<std::uint32_t> WeaponDrawn(int slot) override {
    auto* actor = ActorAt(slot);
    return actor
               ? std::optional{static_cast<std::uint32_t>(actor->weapon_drawn)}
               : std::nullopt;
  }

 private:
  std::array<std::optional<LiveCostDefinition>,
             std::size(VS::Battle::actionDefinitions)>
      definitions_{};
  std::optional<LiveCostActor> actor_;
};
}  // namespace

// decomp: _getActionCost (src/BATTLE/BATTLE.PRG/146C.c:13075), WORD3.
std::uint32_t LiveActionCostAndAvailability(std::uint32_t action,
                                            VS::Battle::ActorStats* actor,
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
void LiveAddActionRecoveryDelay(std::uint32_t action,
                                VS::Battle::ActorStats* actor, int execute) {
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
