#pragma once

#include <array>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <variant>
#include <vector>

#include "Actors/ActorInstances.hpp"
#include "Battle/ActionPreview.hpp"
#include "Battle/ActionResults.hpp"
#include "Battle/DamageCalculations.hpp"
#include "Battle/StatFactors.hpp"
#include "Battle/StatusQueue.hpp"
#include "Content/ActionCatalog.hpp"
#include "Items/EquipmentAffinity.hpp"

namespace VS::Battle {
enum class CombatControl { kPlayer, kAi };

struct CombatActorTarget {
  Actors::SpawnedActor actor = Actors::SpawnedActor::kNone;
  std::int8_t limb{};
  friend bool operator==(const CombatActorTarget&,
                         const CombatActorTarget&) = default;
};

struct CombatWorldTarget {
  std::uint8_t kind{};
  ActionPoint position{};
  friend bool operator==(const CombatWorldTarget&,
                         const CombatWorldTarget&) = default;
};

using CombatTarget = std::variant<CombatActorTarget, CombatWorldTarget>;

// Collision has already resolved these targets. A simulator supplies its own
// query results; the rules do not ask for a camera or an animated skeleton.
struct CombatActionIntent {
  CombatControl control = CombatControl::kPlayer;
  Actors::SpawnedActor source = Actors::SpawnedActor::kNone;
  std::int8_t source_limb{};
  std::uint16_t action{};
  std::vector<CombatTarget> targets;
  bool redirected{};
  ActionPoint focus{};
};

struct CombatLimbState {
  DamageLimb damage{};
  StatFactorVital hp{};
  std::int8_t agility_defense{};
  std::uint8_t name{};
};

struct CombatActorState {
  Actors::SpawnedActor identity = Actors::SpawnedActor::kNone;
  std::uint8_t slot{};
  std::uint32_t weapon_drawn{};
  std::uint8_t spawn{};
  bool cancels_results{};
  bool kill_counted{};
  DamageStats damage{};
  std::int16_t hp{}, max_hp{}, mp{}, max_mp{};
  std::uint16_t agility{};
  std::uint32_t statuses{}, immunities{};
  std::array<CombatLimbState, Items::kLimbCount> limbs{};
  std::array<std::array<std::uint8_t, Items::kLimbCount>, Items::kLimbCount>
      armor_splash{};
  std::array<std::uint8_t, 2> mobility{};
  std::uint8_t default_limb{};
  std::uint16_t recovery_timer{};
  std::uint8_t recovery_bonus{};
};

struct CombatKillProgress {
  std::uint32_t story_progress{};
  std::uint8_t weapon_category{};
  std::uint8_t learned_arts{};
  std::uint16_t weapon_kills{};
  std::optional<int> required_kills;
  std::uint16_t weapon_attacks{};
};

struct CombatEquipmentNotice {
  Actors::SpawnedActor actor = Actors::SpawnedActor::kNone;
  std::int32_t label{};
  std::int32_t stat{};
  Items::StatChange change = Items::StatChange::kIncrease;
  friend bool operator==(const CombatEquipmentNotice&,
                         const CombatEquipmentNotice&) = default;
};

struct CombatHitResult {
  CombatTarget target{CombatActorTarget{}};
  ActorActionResult changes{};
  std::uint8_t threshold{};
  StatusFeedback feedback = StatusFeedback::kNone;
};

struct CombatActionResult {
  CombatActionIntent intent;
  std::uint8_t affinity{}, damage_type{}, category{};
  std::array<std::uint8_t, 2> hit_prerequisites{};
  std::uint32_t availability{};
  CombatHitResult source;
  std::vector<CombatHitResult> hits;
  std::vector<CombatActorState> actors;
  CombatKillProgress progress;
  std::vector<CombatEquipmentNotice> equipment_notices;
  std::vector<std::string> warnings;
};

struct CombatRuleInputs {
  const Content::ActionRule& definition;
  std::span<const CombatActorState> actors;
  Items::ItemRandom& random;
  std::span<const std::uint16_t> blade_pp_bonuses;
  std::span<const StatusRule> status_rules;
  std::span<const std::uint8_t> injury_rules;
  CombatKillProgress progress{};
  std::uint32_t chain_step{};
};

[[nodiscard]] bool IsPhysicalCombatRule(const Content::ActionRule& rule);

// Resolves single-hit physical attacks without live state or presentation
// calls. Actor/equipment changes belong to the returned result; inputs stay
// unchanged. RNG is consumed in the original order, including the roll in a
// preview.
[[nodiscard]] std::expected<CombatActionResult, std::string>
ResolvePhysicalAction(const CombatActionIntent& intent,
                      const CombatRuleInputs& inputs, bool execute);
}  // namespace VS::Battle
