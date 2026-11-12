#pragma once

#include <array>
#include <cstdint>
#include <optional>

#include "Battle/HitModifiers.hpp"

namespace VS::Battle {
struct HitAttributes {
  std::uint16_t base{};
  std::int16_t accessory{};
  std::int16_t weapon{};
  std::int16_t shield{};
  std::array<std::int16_t, kHitModifierLimbCount> armor{};
};
struct HitAttributeStats {
  std::uint8_t actor_id{};
  std::uint16_t risk{};
  std::int16_t hp{};
  std::int16_t max_hp{};
  HitAttributes agility{};
  HitAttributes intelligence{};
  std::array<std::int8_t, kHitModifierLimbCount> defense{};
};

class HitAttributeAccess {
 public:
  virtual ~HitAttributeAccess() = default;
  virtual HitAttributeStats Read() = 0;
  virtual std::optional<std::int16_t> AttackBonus() = 0;
  virtual std::optional<std::int16_t> DefenseBonus(int hit) = 0;
};

class HitAttributeWorld {
 public:
  virtual ~HitAttributeWorld() = default;
  // Access objects retain the original stats owner across the random callback.
  virtual HitAttributeAccess* Stats(std::uint8_t slot) = 0;
  virtual std::optional<std::uint32_t> WeaponDrawn(std::uint8_t slot) = 0;
  virtual std::optional<std::uint8_t> Cost() = 0;
  virtual std::int32_t RandomAveraged(int bound) = 0;
};

std::optional<int> AgilityHitChance(const HitModifierTarget* source,
                                    const HitModifierTarget* target, int hit,
                                    int variance, HitAttributeWorld& world);
std::optional<int> IntelligenceHitChance(const HitModifierTarget* source,
                                         const HitModifierTarget* target,
                                         int hit, int variance,
                                         HitAttributeWorld& world);
std::optional<int> CostAdjustedAgilityHitChance(const HitModifierTarget* source,
                                                const HitModifierTarget* target,
                                                int hit, int variance,
                                                HitAttributeWorld& world);

struct CalculationTarget {
  std::uint8_t actor{};
  std::int32_t position{};
  std::uint8_t threshold{};
};
struct CalculationActor {
  std::uint8_t id{};
  std::int32_t statuses{};
};
class ActionCalculationWorld {
 public:
  virtual ~ActionCalculationWorld() = default;
  virtual std::optional<CalculationTarget> Target() = 0;
  virtual std::optional<CalculationTarget> Source() = 0;
  virtual std::optional<CalculationActor> Actor(std::uint8_t slot) = 0;
  virtual std::optional<std::uint8_t> ActionType() = 0;
  // Refuse a missing action, used hit index, sentinel or missing callback.
  virtual std::optional<int> HitCalculator(int hit, int execute) = 0;
  virtual std::optional<std::int16_t> StatCalculator(int hit, int execute) = 0;
  virtual std::uint32_t Prerequisite() = 0;
  virtual void StoreThreshold(std::uint8_t threshold) = 0;
  virtual void StoreLastValue(std::uint32_t value) = 0;
  virtual std::int32_t RandomBounded(int bound) = 0;
};
std::optional<int> RollActionHit(int hit, int execute,
                                 ActionCalculationWorld& world);
std::optional<std::int16_t> CalculateActionStatChange(
    int hit, int execute, ActionCalculationWorld& world);
}  // namespace VS::Battle
