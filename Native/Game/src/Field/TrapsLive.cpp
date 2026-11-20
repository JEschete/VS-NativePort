// Hand-maintained (tools/port/owned.csv).
#include "Field/TrapsLive.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <type_traits>

#include "Battle/AbilityState.hpp"
#include "Battle/Action.hpp"
#include "Battle/ActionResolution.hpp"
#include "Battle/Actor.hpp"
#include "Battle/Damage.hpp"
#include "Battle/Hit.hpp"
#include "Core/Log.hpp"
#include "Field/RoomLoadingLive.hpp"
#include "Field/RoomLoadingLiveState.hpp"
#include "Field/Traps.hpp"
#include "Items/ItemRecords.hpp"
#include "Items/ItemRecordsLive.hpp"
#include "Legacy/Psx.hpp"

using VS::Battle::ActionDefinition;
using VS::Battle::LimbStats;

namespace {
using VS::Field::TrapPanelHit;
using VS::Field::TrapPanelTarget;

constexpr std::size_t kActionHitCount = 2;
using LimbTable = decltype(VS::Battle::ActorStats::limbs);
constexpr std::size_t kLimbCount = std::tuple_size_v<LimbTable>;

// Cross-slice calls: retarget when those rows are promoted.
VS::Battle::ActorStats* PlayerStats() {
  return VS::Battle::characterState != nullptr
             ? VS::Battle::characterState->stats
             : nullptr;
}

VS::Battle::Actor* BattleActor(std::size_t index) {
  const std::span actors(VS::Battle::actors);
  return index < actors.size() ? actors[index] : nullptr;
}

std::int32_t RoomTrapLevel() { return VS::Field::LiveRoomTrapLevel(); }
// End of cross-slice calls.

void WarnRefusal(const char* rule, const char* reason) {
  VS::Core::LogWarn("field.traps", "{} refused: {}", rule, reason);
}

template <typename Out, typename In, std::size_t N>
std::array<Out, N> Ratings(std::span<const In, N> values) {
  std::array<Out, N> ratings{};
  std::ranges::transform(values, ratings.begin(),
                         [](In value) { return static_cast<Out>(value); });
  return ratings;
}

template <typename Out, typename In, std::size_t N>
std::array<Out, N> Ratings(const In (&values)[N]) {
  return Ratings<Out>(std::span<const In, N>(values));
}

template <typename Out, typename In, std::size_t N>
std::array<Out, N> Ratings(const std::array<In, N>& values) {
  return Ratings<Out>(std::span<const In, N>(values));
}

TrapPanelTarget ToPanelTarget(const VS::Battle::ActorStats& stats,
                              const LimbStats& limb, bool shield_raised) {
  TrapPanelTarget target;
  target.current_hp = stats.current_hp;
  target.max_hp = stats.max_hp;
  target.risk = stats.risk;
  target.strength = stats.strength;
  target.limb.types = Ratings<std::int16_t>(limb.types);
  target.limb.affinities = Ratings<std::int16_t>(limb.affinities);
  target.limb.armor_strength = limb.armor.attributes.current_strength;
  target.limb.armor_types = Ratings<std::uint16_t>(limb.armor.types);
  target.limb.armor_affinities =
      Ratings<std::int16_t>(limb.armor.class_affinity.affinities[0]);
  target.limb.armor_current_dp = limb.armor.current_dp;
  target.limb.armor_max_dp = limb.armor.max_dp;
  target.shield_raised = shield_raised;
  target.shield.strength = stats.shield.attributes.current_strength;
  // decomp: BATTLE.PRG _damageFromTrapPanel 0x80082794 `lbu $v1, 0x2C4($v0)`:
  // the shield's type bytes are unsigned, though the C struct declares char.
  target.shield.types = Ratings<std::uint8_t>(stats.shield.types);
  target.shield.affinities =
      Ratings<std::int16_t>(stats.shield.class_affinity.affinities[0]);
  target.shield.current_dp = stats.shield.current_dp;
  target.shield.max_dp = stats.shield.max_dp;
  target.shield.current_pp = stats.shield.current_pp;
  target.shield.max_pp = stats.shield.max_pp;
  target.accessory.strength = stats.accessory.current_strength;
  target.accessory.types = Ratings<std::uint16_t>(stats.accessory.types);
  target.accessory.affinities =
      Ratings<std::uint16_t>(stats.accessory.affinities);
  return target;
}

// The panel hit and the struck actor the original reads through
// VS::Battle::actors[hit->target.target_actor] and its limb, or empty (with a
// diagnostic) where the original would read outside a table or through null.
struct PanelInputs {
  TrapPanelHit hit;
  TrapPanelTarget target;
};

std::optional<PanelInputs> ReadPanelInputs(
    const char* rule, const ActionDefinition* action,
    const VS::Battle::HitResult* target_result, int hit_index) {
  if (action == nullptr || target_result == nullptr) {
    WarnRefusal(rule, "null record");
    return std::nullopt;
  }
  if (hit_index < 0 || static_cast<std::size_t>(hit_index) >= kActionHitCount) {
    WarnRefusal(rule, "hit index outside the action's hits");
    return std::nullopt;
  }
  const VS::Battle::Actor* struck =
      BattleActor(target_result->target.target_actor);
  // The original loads the limb signed (lb at 0x800825B8); reading the byte
  // unsigned refuses the same values, since only 0..5 name a limb.
  const auto limb =
      static_cast<std::uint8_t>(target_result->target.target_limb);
  if (struck == nullptr || struck->stats == nullptr || limb >= kLimbCount) {
    WarnRefusal(rule, "target outside the actor table or its limbs");
    return std::nullopt;
  }
  const VS::Battle::ActorStats& stats = *struck->stats;
  const VS::Battle::Actor* owner = BattleActor(stats.actor_id);
  if (owner == nullptr) {
    WarnRefusal(rule, "target's actor id outside the actor table");
    return std::nullopt;
  }
  const auto& hit =
      std::span(action->hits)[static_cast<std::size_t>(hit_index)];
  return PanelInputs{
      .hit = {.stat_factor = static_cast<std::uint8_t>(hit.stat_factor),
              .damage_type = static_cast<std::uint8_t>(hit.damage_type),
              .affinity = static_cast<std::uint8_t>(hit.affinity)},
      .target = ToPanelTarget(
          stats, std::span(stats.limbs)[static_cast<std::size_t>(limb)],
          (owner->weapon_drawn & 1) != 0)};
}

using PanelRule = std::optional<std::int16_t> (*)(const TrapPanelHit&,
                                                  const TrapPanelTarget&,
                                                  std::int32_t, bool,
                                                  VS::Field::TrapRandom&);

std::int16_t RunPanelRule(const char* rule, PanelRule calculate,
                          const ActionDefinition* action,
                          const VS::Battle::HitResult* target_result,
                          int hit_index, int add_random_variation) {
  const auto inputs = ReadPanelInputs(rule, action, target_result, hit_index);
  if (!inputs.has_value()) return 0;
  VS::Items::Live::LiveItemRandom random;
  const auto amount = calculate(inputs->hit, inputs->target, RoomTrapLevel(),
                                add_random_variation != 0, random);
  if (!amount.has_value()) {
    WarnRefusal(rule, "affinity outside 1..7");
    return 0;
  }
  return *amount;
}
}  // namespace

namespace VS::Field {
int RollLiveRiskLockBreak() {
  const VS::Battle::ActorStats* stats = PlayerStats();
  if (stats == nullptr) {
    WarnRefusal("RollRiskLockBreak", "null player record");
    return 0;
  }
  Items::Live::LiveItemRandom random;
  return RollRiskLockBreak(stats->risk, random) ? 1 : 0;
}

std::int16_t CalculateLiveTrapPanelDamage(
    ActionDefinition* action, VS::Battle::HitResult* /*attacker_result*/,
    VS::Battle::HitResult* target_result, int hit_index,
    int add_random_variation) {
  return RunPanelRule("CalculateTrapPanelDamage", &CalculateTrapPanelDamage,
                      action, target_result, hit_index, add_random_variation);
}

std::int16_t CalculateLiveHealingPanelRecovery(
    ActionDefinition* action, VS::Battle::HitResult* /*attacker_result*/,
    VS::Battle::HitResult* target_result, int hit_index,
    int add_random_variation) {
  return RunPanelRule("CalculateHealingPanelRecovery",
                      &CalculateHealingPanelRecovery, action, target_result,
                      hit_index, add_random_variation);
}
}  // namespace VS::Field
