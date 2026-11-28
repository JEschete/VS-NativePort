#include "Battle/CombatImpact.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>

#include "Battle/ActionResults.hpp"
#include "Battle/StatusHelpers.hpp"
#include "Items/EquipmentAffinity.hpp"

namespace VS::Battle {
namespace {
class ImpactRefusal final : public std::runtime_error {
 public:
  explicit ImpactRefusal(std::string_view reason)
      : std::runtime_error(std::string(reason)) {}
};

void Require(bool valid, std::string_view reason) {
  if (!valid) throw ImpactRefusal(reason);
}

class ImpactActor final : public ActorVitalsAccess {
 public:
  explicit ImpactActor(ActorVitalsState& state) : state_(state) {}
  ActorVitalsState Read() override { return state_; }
  void Write(const ActorVitalsState& state) override { state_ = state; }

 private:
  ActorVitalsState& state_;
};

class ImpactWorld final : public ActorActionResultWorld,
                          public StatusLifecycleWorld {
 public:
  ImpactWorld(const ActorActionResult& hit, const CombatImpactInputs& inputs,
              CombatImpactResult& result)
      : hit_(hit), inputs_(inputs), result_(result), actor_(result.state) {}

  void CommitEquipmentStats() override {
    result_.events.emplace_back(CombatEquipmentFlush{});
  }
  ActorActionResult ReadHit() override { return hit_; }
  ActorVitalsAccess* TargetStats(std::uint8_t slot) override {
    return slot == inputs_.slot ? &actor_ : nullptr;
  }
  void Number(int slot, int limb, std::uint32_t amount, int label) override {
    Require(slot == inputs_.slot, "Combat number changed target owner");
    result_.events.emplace_back(
        CombatNumberEvent{inputs_.actor, limb, amount, label});
  }
  void StatusLabel(int slot, std::uint32_t mask, bool removing) override {
    Require(slot == inputs_.slot, "Combat status changed target owner");
    result_.events.emplace_back(
        CombatStatusEvent{inputs_.actor, mask, removing});
  }
  void RemoveStatuses(ActorVitalsAccess& stats, std::uint32_t mask) override {
    auto state = stats.Read();
    Require(RemoveActorStatuses(state.status, mask, *this),
            "Combat status removal requires valid mobility limbs");
    stats.Write(state);
  }
  void InflictStatuses(ActorVitalsAccess& stats, std::uint32_t mask) override {
    auto state = stats.Read();
    InflictActorStatuses(state.status, mask, inputs_.durations, *this);
    stats.Write(state);
  }
  std::int32_t RandomBounded(StatusLifecycleState&, int bound) override {
    return inputs_.random.NextBounded(bound);
  }
  std::int32_t RandomAveraged(StatusLifecycleState&, int bound) override {
    return inputs_.random.NextAveraged(bound);
  }
  void UpdateMovement(StatusLifecycleState& state) override {
    result_.events.emplace_back(
        CombatMovementChange{inputs_.actor, state.walk, state.run});
  }
  void EquipmentLabel(StatusLifecycleState&, int label, int stat,
                      std::uint32_t amount) override {
    result_.events.emplace_back(
        CombatEquipmentEvent{inputs_.actor, label, stat, amount});
  }
  void ValueLabel(StatusLifecycleState&, std::uint32_t amount) override {
    result_.events.emplace_back(
        CombatNumberEvent{inputs_.actor, 0, amount, 0, false});
  }
  void StatusLabels(StatusLifecycleState&, std::uint32_t mask) override {
    result_.events.emplace_back(CombatStatusEvent{inputs_.actor, mask, true});
  }
  void RebuildWeapon(StatusLifecycleState& state) override {
    Items::RebuildWeaponClassAffinity(state.weapon);
  }
  void RebuildShield(StatusLifecycleState& state) override {
    Items::RebuildShieldClassAffinity(state.shield);
  }
  void RebuildArmor(StatusLifecycleState& state, std::size_t limb) override {
    Items::RebuildArmorClassAffinity(state.limbs.at(limb).armor);
  }
  void EnchantWeapon(StatusLifecycleState& state,
                     const ElementEnchantment& change) override {
    Require(ApplyWeaponElementEnchantment(state.weapon, change),
            "Combat weapon enchantment requires valid affinities");
  }
  void EnchantArmor(StatusLifecycleState& state,
                    const ElementEnchantment& change) override {
    std::array<Items::ArmorLoadout, Items::kLimbCount> armor{};
    for (std::size_t limb = 0; limb < armor.size(); ++limb) {
      armor.at(limb) = state.limbs.at(limb).armor;
    }
    Require(ApplyArmorElementEnchantment(state.shield, armor, change),
            "Combat armor enchantment requires valid affinities");
    for (std::size_t limb = 0; limb < armor.size(); ++limb) {
      state.limbs.at(limb).armor = armor.at(limb);
    }
  }
  void RefreshEquipmentDisplay(StatusLifecycleState&) override {
    CommitEquipmentStats();
  }

 private:
  const ActorActionResult& hit_;
  const CombatImpactInputs& inputs_;
  CombatImpactResult& result_;
  ImpactActor actor_;
};
}  // namespace

std::expected<CombatImpactResult, std::string> ApplyCombatImpact(
    const CombatHitResult& hit, const CombatImpactInputs& inputs) {
  const auto* target = std::get_if<CombatActorTarget>(&hit.target);
  if (target == nullptr || inputs.actor == Actors::SpawnedActor::kNone ||
      target->actor != inputs.actor || hit.changes.target != inputs.slot) {
    return std::unexpected("Combat impact requires the expected target owner");
  }
  try {
    CombatImpactResult result{
        .actor = inputs.actor, .state = inputs.state, .events = {}};
    ImpactWorld world(hit.changes, inputs, result);
    Require(ApplyActorActionResult(world),
            "Combat impact requires valid target stats and armor limb");
    return result;
  } catch (const ImpactRefusal& error) {
    return std::unexpected(error.what());
  }
}
}  // namespace VS::Battle
