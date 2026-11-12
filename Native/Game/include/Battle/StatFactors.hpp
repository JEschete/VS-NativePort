#pragma once

#include <cstdint>
#include <optional>

namespace VS::Battle {
struct StatFactorHit {
  std::int16_t hp{};
  std::int16_t mp{};
};
struct StatFactorVital {
  std::int16_t current{};
  std::int16_t maximum{};
};
enum class StatFactorResource { kHp, kMp };

// Reads are deliberately separate: an incoming spell's type/effect gates do
// not require the current action, its hit index, or either hit amount.
class StatFactorWorld {
 public:
  virtual ~StatFactorWorld() = default;
  virtual std::optional<std::uint16_t> QueueIndex() = 0;
  virtual std::optional<std::uint8_t> Factor(int hit) = 0;
  virtual std::optional<StatFactorHit> PreviousTarget() = 0;
  virtual std::optional<StatFactorVital> SourceVital(StatFactorResource) = 0;
  virtual std::optional<StatFactorVital> TargetVital(StatFactorResource) = 0;
  virtual std::optional<std::int16_t> TargetHp() = 0;
  virtual std::optional<std::uint16_t> WeaponPp() = 0;
  virtual std::optional<std::uint16_t> ShieldPp() = 0;
  virtual std::optional<std::uint8_t> SelectedActionType() = 0;
  virtual std::optional<std::uint8_t> SelectedActionCost() = 0;
  virtual std::optional<std::uint8_t> PrimaryHpEffect() = 0;
  virtual std::optional<StatFactorHit> PrimaryHit() = 0;
};

// Original F00412/F00414/F00426 are byte-identical aliases of this kernel.
std::optional<std::int16_t> ScalePreviousTargetHit(int hit,
                                                   StatFactorWorld& world);
std::optional<std::int16_t> ScaleSourceMissingHp(int hit,
                                                 StatFactorWorld& world);
std::optional<std::int16_t> ScaleTargetCurrentHp(int hit,
                                                 StatFactorWorld& world);
std::optional<std::int16_t> WeaponPpWithChainOffset(StatFactorWorld& world);
std::optional<std::int16_t> ScaleIncomingSpellDamage(int hit,
                                                     StatFactorWorld& world);
std::optional<std::int16_t> ScaleSourceMissingMp(int hit,
                                                 StatFactorWorld& world);
// F00417/F00427 share ScaleIncomingSpellDamage; F00418/F00428 share this.
std::optional<std::int16_t> ScaleIncomingPhysicalDamage(int hit,
                                                        StatFactorWorld& world);
std::optional<std::int16_t> TargetMissingHp(StatFactorWorld& world);
std::optional<std::int16_t> TargetMissingMp(StatFactorWorld& world);
std::optional<std::int16_t> ScaleIncomingSpellCost(int hit,
                                                   StatFactorWorld& world);
std::optional<std::int16_t> ScaleIncomingDamage(int hit,
                                                StatFactorWorld& world);
std::optional<std::int16_t> SourceShieldPp(StatFactorWorld& world);
}  // namespace VS::Battle
