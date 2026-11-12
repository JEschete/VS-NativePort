#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace VS::Battle {
inline constexpr std::size_t kHitModifierGemCount = 3;
inline constexpr std::size_t kHitModifierLimbCount = 6;

struct HitModifierStats {
  std::uint8_t actor_id{};
  std::array<std::uint8_t, kHitModifierGemCount> weapon_gems{};
  std::array<std::uint8_t, kHitModifierGemCount> shield_gems{};
  std::uint16_t risk{};
  std::array<std::uint8_t, kHitModifierLimbCount> chain_evasion{};
};

struct HitModifierAction {
  std::uint8_t id{};
  std::uint8_t type{};
  std::array<std::uint8_t, 2> effects{};
};

struct HitModifierTarget {
  std::uint8_t actor{};
  std::int8_t limb{};
  std::int32_t targets_position{};
};

class HitModifierWorld {
 public:
  virtual ~HitModifierWorld() = default;
  virtual std::optional<HitModifierStats> Stats(std::uint8_t slot) = 0;
  virtual std::optional<std::uint32_t> WeaponDrawn(std::uint8_t slot) = 0;
  // Deferred: undrawn actors and inactive hits never require an action.
  virtual std::optional<HitModifierAction> Action() = 0;
};

std::optional<std::int16_t> AttackGemHitBonus(const HitModifierStats* stats,
                                              HitModifierWorld& world);
std::optional<std::int16_t> DefenseGemHitBonus(const HitModifierStats* stats,
                                               int hit,
                                               HitModifierWorld& world);
int PreviousHitSuccessThreshold(std::int32_t previous_result);
std::optional<int> GemHitChance(const HitModifierTarget* source,
                                const HitModifierTarget* target, int hit,
                                HitModifierWorld& world);
std::optional<int> RiskHitChance(const HitModifierTarget* source,
                                 HitModifierWorld& world);
std::optional<int> ChainHitChance(const HitModifierTarget* target,
                                  HitModifierWorld& world);
}  // namespace VS::Battle
