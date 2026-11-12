#pragma once

#include <cstdint>
#include <optional>

namespace VS::Battle {
enum class CostResource { kNone, kMp, kRisk, kHp, kWeaponPp };

struct ActionResources {
  std::int16_t hp{};
  std::int16_t mp{};
  std::uint16_t risk{};
  std::uint16_t weapon_pp{};
  std::uint8_t weapon_type{};
  std::uint8_t weapon_cost{};
};

struct ActionCost {
  bool requirements_met{};
  std::uint8_t effective_type{};
  std::uint32_t cost{};
  CostResource resource = CostResource::kNone;

  // This is an additive legacy projection, not independent masked fields:
  // large chain costs carry into the availability byte used by callers.
  [[nodiscard]] std::uint32_t Packed() const;
};

class ActionCostActor {
 public:
  virtual ~ActionCostActor() = default;
  virtual ActionResources Resources() = 0;
  virtual void Pay(CostResource resource, std::uint16_t value) = 0;
  virtual std::uint16_t RecoveryTimer() = 0;
  virtual void SetRecoveryTimer(std::uint16_t value) = 0;
  virtual std::uint8_t RecoveryBonus() = 0;
  virtual std::uint8_t WeaponRecoveryDelay() = 0;
  virtual std::uint32_t Statuses() = 0;
};

class ActionCostDefinition {
 public:
  virtual ~ActionCostDefinition() = default;
  virtual std::uint8_t Type() = 0;
  virtual std::uint8_t Cost() = 0;
  virtual std::uint8_t RecoveryDelay() = 0;
  virtual bool Unlocked() = 0;
};

class ActionCostWorld {
 public:
  virtual ~ActionCostWorld() = default;
  virtual ActionCostDefinition* Definition(std::uint32_t action) = 0;
  // An absent record is refused; a present record can carry absent stats
  // when the cost's outer type never uses a resource.
  virtual std::optional<ActionCostActor*> Actor(int slot) = 0;
  virtual std::optional<std::uint32_t> ChainStep() = 0;
  virtual std::optional<std::uint32_t> WeaponDrawn(int slot) = 0;
  virtual std::optional<ActionCost> Evaluate(std::uint32_t action,
                                             ActionCostActor* actor,
                                             int update);
};

// Base attacks support resource types 1..4 and explicit type 0 / cost 0
// (empty or natural weapon: no resource payment). Other weapon combinations
// are refused; the original read an unassigned non-argument register (#352).
[[nodiscard]] std::optional<ActionCost> ActionCostAndAvailability(
    std::uint32_t action, ActionCostActor* actor, int update,
    ActionCostWorld& world);
[[nodiscard]] bool PreviewActorActionCost(int action, int slot,
                                          ActionCostWorld& world);
[[nodiscard]] bool AddActionRecoveryDelay(std::uint32_t action,
                                          ActionCostActor* actor, int execute,
                                          ActionCostWorld& world);
[[nodiscard]] std::optional<std::uint32_t> ActorActionRestrictionFlags(
    int slot, int action, ActionCostWorld& world);
}  // namespace VS::Battle
