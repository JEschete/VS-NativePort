#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

#include "Items/ItemRecords.hpp"

// Room trap rules: the risk roll that breaks a lock, and the HP a trap panel
// takes or a healing panel restores. Every input is a value, so the rules read
// no battle actor record, room global or PS1 address.
namespace VS::Field {
// The game's random draws (SLUS_010.40 vs_main_getRand*), shared with the
// item rules.
using TrapRandom = Items::ItemRandom;

// Whether a risk roll breaks a lock: a draw in [0, 100) below the risk-derived
// percent (risk + 150) * 100 / 256, capped at 100, plus 20.
[[nodiscard]] bool RollRiskLockBreak(std::uint16_t risk, TrapRandom& random);

inline constexpr std::size_t kTrapDamageTypeCount = 4;
inline constexpr std::size_t kTrapAffinityCount = 8;

template <typename T>
using TrapTypeRatings = std::array<T, kTrapDamageTypeCount>;
template <typename T>
using TrapAffinityRatings = std::array<T, kTrapAffinityCount>;

// The fields of a panel action's hit word (vs_action_t hits[n]).
struct TrapPanelHit {
  std::uint8_t stat_factor = 0;  // 0..31
  std::uint8_t damage_type = 0;  // 0..3, 0 has no type ratings
  std::uint8_t affinity = 0;     // 1..7; 0 names no affinity row
};

// The struck limb's ratings and its armor (vs_battle_uiEquipment_limb).
struct TrapLimbDefense {
  TrapTypeRatings<std::int16_t> types{};
  TrapAffinityRatings<std::int16_t> affinities{};
  std::int16_t armor_strength = 0;
  TrapTypeRatings<std::uint16_t> armor_types{};
  TrapAffinityRatings<std::int16_t> armor_affinities{};
  std::uint16_t armor_current_dp = 0;
  std::uint16_t armor_max_dp = 0;
};

struct TrapShieldDefense {
  std::int16_t strength = 0;
  TrapTypeRatings<std::uint8_t> types{};
  TrapAffinityRatings<std::int16_t> affinities{};
  std::uint16_t current_dp = 0;
  std::uint16_t max_dp = 0;
  std::uint16_t current_pp = 0;
  std::uint16_t max_pp = 0;
};

struct TrapAccessoryDefense {
  std::int16_t strength = 0;
  TrapTypeRatings<std::uint16_t> types{};
  TrapAffinityRatings<std::uint16_t> affinities{};
};

// What a panel reads from the actor standing on it. The shield only counts
// while it is raised (the actor's weapon is drawn).
struct TrapPanelTarget {
  std::int16_t current_hp = 0;
  std::int16_t max_hp = 0;
  std::uint16_t risk = 0;
  std::uint16_t strength = 0;
  TrapLimbDefense limb{};
  bool shield_raised = false;
  TrapShieldDefense shield{};
  TrapAccessoryDefense accessory{};
};

// The panel's attack (a stat-factor share of a tenth of the target's maximum
// HP plus ten per trap level above -1) less the target's defense, never below
// 0, then (when `add_random_variation`) an averaged draw in [-5, 5], never
// below 0; the result is clamped to leave the target at least 1 HP. The
// intermediate values wrap to 16 bits as the original's halfword fields do.
// Empty for a damage type past 3 or an affinity outside 1..7.
[[nodiscard]] std::optional<std::int16_t> CalculateTrapPanelDamage(
    const TrapPanelHit& hit, const TrapPanelTarget& target,
    std::int32_t trap_level, bool add_random_variation, TrapRandom& random);

// The same amount without the HP clamp: the HP a healing panel restores.
[[nodiscard]] std::optional<std::int16_t> CalculateHealingPanelRecovery(
    const TrapPanelHit& hit, const TrapPanelTarget& target,
    std::int32_t trap_level, bool add_random_variation, TrapRandom& random);
}  // namespace VS::Field
