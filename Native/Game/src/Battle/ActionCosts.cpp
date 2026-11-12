// Hand-maintained (tools/port/owned.csv).
#include "Battle/ActionCosts.hpp"

#include <bit>

#include "Core/Math/ScalarMath.hpp"

namespace VS::Battle {
namespace {
constexpr std::uint8_t kMp = 1, kRisk = 2, kHp = 3, kPp = 4, kBaseAttack = 6;
constexpr int kRiskMaximum = 100;
constexpr std::uint32_t kFirstChain = 22, kChainCount = 18;
constexpr std::uint32_t kFirstDelayExcluded = 22, kDelayExcludedCount = 32;
constexpr std::uint32_t kChainThreshold = 10, kChainDivisor = 10;
constexpr int kAvailableShift = 24, kTypeShift = 16, kRecoveryShift = 8;
constexpr std::uint32_t kAvailableMask = 0xFF000000;
constexpr std::uint8_t kRecoveryBonusMask = 63, kWeaponDelay = 255;
constexpr int kFirstWeaponAction = 141, kWeaponActionEnd = 224;
constexpr std::uint32_t kSilence = 0x1000, kNumbness = 0x2000;
constexpr std::uint32_t kUnavailable = 2, kActionBlocked = 4;

std::int32_t Balance(std::uint8_t type, const ActionResources& resources) {
  switch (type) {
    case kMp:
      return resources.mp;
    case kRisk:
      return kRiskMaximum - resources.risk;
    case kHp:
      return resources.hp;
    case kPp:
      return resources.weapon_pp;
    default:
      return 0;
  }
}
CostResource Resource(std::uint8_t type) {
  switch (type) {
    case kMp:
      return CostResource::kMp;
    case kRisk:
      return CostResource::kRisk;
    case kHp:
      return CostResource::kHp;
    case kPp:
      return CostResource::kWeaponPp;
    default:
      return CostResource::kNone;
  }
}
}  // namespace

std::uint32_t ActionCost::Packed() const {
  return (std::uint32_t{requirements_met} << kAvailableShift) +
         (std::uint32_t{effective_type} << kTypeShift) + cost;
}

// decomp: _getActionCost (src/BATTLE/BATTLE.PRG/146C.c:13075).
std::optional<ActionCost> ActionCostAndAvailability(std::uint32_t action,
                                                    ActionCostActor* actor,
                                                    int update,
                                                    ActionCostWorld& world) {
  auto* definition = world.Definition(action);
  if (definition == nullptr) return std::nullopt;
  auto type = definition->Type();
  std::uint32_t cost = definition->Cost();
  ActionResources resources{};
  if ((type >= kMp && type <= kPp) || type == kBaseAttack) {
    if (actor == nullptr) return std::nullopt;
    resources = actor->Resources();
  }
  if (type == kBaseAttack) {
    type = resources.weapon_type;
    cost = resources.weapon_cost;
    if (type == 0 && cost == 0) {
      return ActionCost{true, 0, 0, CostResource::kNone};
    }
    if (type < kMp || type > kPp) return std::nullopt;
  }
  auto available = Balance(type, resources);
  if (type == kHp) ++cost;
  if (type == kRisk) {
    const auto chain = world.ChainStep();
    if (!chain) return std::nullopt;
    if (chain.value() >= kChainThreshold &&
        action - kFirstChain < kChainCount) {
      cost += (chain.value() * chain.value()) / kChainDivisor;
    }
  }
  const bool enough = available >= std::bit_cast<std::int32_t>(cost);
  if (enough) {
    if (type == kHp) --cost;
    available = Core::Math::WrappingSubtract(available,
                                             std::bit_cast<std::int32_t>(cost));
  } else if (type == kRisk) {
    available = 0;
  }
  const auto resource = Resource(type);
  if (update != 0 && resource != CostResource::kNone) {
    const auto value = type == kRisk ? kRiskMaximum - available : available;
    actor->Pay(resource, static_cast<std::uint16_t>(value));
  }
  return ActionCost{enough || type == kRisk, type, cost, resource};
}

std::optional<ActionCost> ActionCostWorld::Evaluate(std::uint32_t action,
                                                    ActionCostActor* actor,
                                                    int update) {
  return ActionCostAndAvailability(action, actor, update, *this);
}

// decomp: func_80085978 (src/BATTLE/BATTLE.PRG/146C.c:13167).
bool PreviewActorActionCost(int action, int slot, ActionCostWorld& world) {
  const auto actor = world.Actor(slot);
  if (!actor) return false;
  return world.Evaluate(static_cast<std::uint32_t>(action), actor.value(), 0)
      .has_value();
}

// decomp: func_800859B4 (src/BATTLE/BATTLE.PRG/146C.c:13172).
bool AddActionRecoveryDelay(std::uint32_t action, ActionCostActor* actor,
                            int execute, ActionCostWorld& world) {
  if (execute == 0 || action - kFirstDelayExcluded < kDelayExcludedCount) {
    return true;
  }
  auto* definition = world.Definition(action);
  if (definition == nullptr || actor == nullptr) return false;
  const auto delay = definition->RecoveryDelay();
  const auto base =
      delay == kWeaponDelay ? actor->WeaponRecoveryDelay() : delay;
  const std::uint32_t amount =
      base + (actor->RecoveryBonus() & kRecoveryBonusMask);
  actor->SetRecoveryTimer(static_cast<std::uint16_t>(
      actor->RecoveryTimer() + (amount << kRecoveryShift)));
  return true;
}

// decomp: vs_battle_getActionFlags (src/BATTLE/BATTLE.PRG/146C.c:15566).
std::optional<std::uint32_t> ActorActionRestrictionFlags(
    int slot, int action, ActionCostWorld& world) {
  const auto record = world.Actor(slot);
  auto* definition = world.Definition(static_cast<std::uint32_t>(action));
  if (!record || record.value() == nullptr || definition == nullptr) {
    return std::nullopt;
  }
  auto* actor = record.value();
  std::uint32_t flags = actor->RecoveryTimer() != 0 ? 1 : 0;
  const auto cost =
      world.Evaluate(static_cast<std::uint32_t>(action), actor, 0);
  if (!cost) return std::nullopt;
  if ((cost.value().Packed() & kAvailableMask) == 0) flags |= kUnavailable;
  if (slot == 0 && !definition->Unlocked()) flags |= kActionBlocked;
  if (action < kFirstWeaponAction) {
    if ((actor->Statuses() & kSilence) != 0) flags |= kActionBlocked;
  } else if (action < kWeaponActionEnd) {
    const auto drawn = world.WeaponDrawn(slot);
    if (!drawn) return std::nullopt;
    if (drawn.value() == 0) flags |= kActionBlocked;
    if ((actor->Statuses() & kNumbness) != 0) flags |= kActionBlocked;
  }
  return flags;
}
}  // namespace VS::Battle
