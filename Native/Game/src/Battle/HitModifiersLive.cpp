// Hand-maintained (tools/port/owned.csv).
#include "Battle/HitModifiersLive.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iterator>
#include <optional>
#include <type_traits>

#include "Battle/Action.hpp"
#include "Battle/Actor.hpp"
#include "Battle/Damage.hpp"
#include "Battle/Hit.hpp"
#include "Battle/HitChance.hpp"
#include "Battle/HitModifiers.hpp"
#include "Core/Diagnostics.hpp"

using VS::Battle::ActionDefinition;

namespace VS::Battle {
namespace {
HitModifierStats ReadStats(const VS::Battle::ActorStats& stats) {
  HitModifierStats result;
  result.actor_id = stats.actor_id;
  result.risk = stats.risk;
  for (std::size_t i = 0; i < result.weapon_gems.size(); ++i) {
    result.weapon_gems.at(i) = stats.weapon.gems[i].gem_effects;
    result.shield_gems.at(i) = stats.shield.gems[i].gem_effects;
  }
  for (std::size_t i = 0; i < result.chain_evasion.size(); ++i) {
    result.chain_evasion.at(i) =
        static_cast<std::uint8_t>(stats.limbs[i].chain_evasion);
  }
  return result;
}

class LiveHitModifierWorld final : public HitModifierWorld {
 public:
  explicit LiveHitModifierWorld(ActionDefinition* action = nullptr)
      : action_(action) {}
  std::optional<HitModifierStats> Stats(std::uint8_t slot) override {
    if (slot >= std::size(VS::Battle::actors) ||
        VS::Battle::actors[slot] == nullptr ||
        VS::Battle::actors[slot]->stats == nullptr) {
      return std::nullopt;
    }
    return ReadStats(*VS::Battle::actors[slot]->stats);
  }
  std::optional<std::uint32_t> WeaponDrawn(std::uint8_t slot) override {
    if (slot >= std::size(VS::Battle::actors) ||
        VS::Battle::actors[slot] == nullptr) {
      return std::nullopt;
    }
    return std::bit_cast<std::uint32_t>(VS::Battle::actors[slot]->weapon_drawn);
  }
  std::optional<HitModifierAction> Action() override {
    if (action_ == nullptr) return std::nullopt;
    return HitModifierAction{
        action_->id,
        action_->type,
        {static_cast<std::uint8_t>(action_->hits[0].effect_id),
         static_cast<std::uint8_t>(action_->hits[1].effect_id)}};
  }

 private:
  ActionDefinition* action_;
};

std::optional<HitModifierTarget> ReadHit(const VS::Battle::HitResult* hit) {
  if (hit == nullptr) return std::nullopt;
  return HitModifierTarget{hit->target.target_actor,
                           std::bit_cast<std::int8_t>(static_cast<std::uint8_t>(
                               hit->target.target_limb)),
                           hit->targets_position};
}

template <typename T>
const T* Pointer(const std::optional<T>& value) {
  return value ? &value.value() : nullptr;
}

template <typename T>
T Checked(std::optional<T> result) {
  if (!result) {
    VS::Core::Errorf(
        "battle",
        "Hit modifier refused missing actor/stats/action or a used index");
    std::abort();
  }
  return result.value();
}
}  // namespace

// decomp: getAttackGemBuff (src/BATTLE/BATTLE.PRG/146C.c:9702; was
// _getAttackGemBuff).
std::int16_t AttackLiveGemHitBonus(ActionDefinition* action,
                                   VS::Battle::ActorStats* stats) {
  LiveHitModifierWorld world(action);
  const auto state = stats ? std::optional{ReadStats(*stats)} : std::nullopt;
  return Checked(AttackGemHitBonus(Pointer(state), world));
}

// decomp: getDefenseGemBuff (src/BATTLE/BATTLE.PRG/146C.c:9731; was
// _getDefenseGemBuff).
std::int16_t DefenseLiveGemHitBonus(ActionDefinition* action,
                                    VS::Battle::ActorStats* stats, int hit) {
  LiveHitModifierWorld world(action);
  const auto state = stats ? std::optional{ReadStats(*stats)} : std::nullopt;
  return Checked(DefenseGemHitBonus(Pointer(state), hit, world));
}

// decomp: func_8007E97C (src/BATTLE/BATTLE.PRG/146C.c:9855).
int PreviousLiveHitSuccessThreshold(ActionDefinition*, VS::Battle::HitResult*,
                                    VS::Battle::HitResult*, int, int) {
  return PreviousHitSuccessThreshold(
      std::bit_cast<std::int32_t>(VS::Battle::previousHitResult));
}

// decomp: getHitGemModifier (src/BATTLE/BATTLE.PRG/146C.c:9861; was
// _getHitGemModifier).
int LiveGemHitChance(ActionDefinition* action, VS::Battle::HitResult* source,
                     VS::Battle::HitResult* target, int hit, int) {
  LiveHitModifierWorld world(action);
  const auto from = ReadHit(source);
  const auto to = ReadHit(target);
  return Checked(GemHitChance(Pointer(from), Pointer(to), hit, world));
}

// decomp: getRiskModifier (src/BATTLE/BATTLE.PRG/146C.c:10031; was
// _getRiskModifier).
int LiveRiskHitChance(ActionDefinition*, VS::Battle::HitResult* source,
                      VS::Battle::HitResult*, int, int) {
  LiveHitModifierWorld world;
  const auto from = ReadHit(source);
  return Checked(RiskHitChance(Pointer(from), world));
}

// decomp: getChainEvasionModifier (src/BATTLE/BATTLE.PRG/146C.c:10042; was
// _getChainEvasionModifier).
int LiveChainHitChance(ActionDefinition*, VS::Battle::HitResult*,
                       VS::Battle::HitResult* target, int, int) {
  LiveHitModifierWorld world;
  const auto to = ReadHit(target);
  return Checked(ChainHitChance(Pointer(to), world));
}

// Bind against the actual table element type, not an independently guessed ABI.
static_assert(std::is_same_v<decltype(hitFunctions[0]),
                             decltype(&PreviousLiveHitSuccessThreshold)&>);
static_assert(
    std::is_same_v<decltype(hitFunctions[0]), decltype(&LiveGemHitChance)&>);
static_assert(
    std::is_same_v<decltype(hitFunctions[0]), decltype(&LiveRiskHitChance)&>);
static_assert(
    std::is_same_v<decltype(hitFunctions[0]), decltype(&LiveChainHitChance)&>);
}  // namespace VS::Battle
