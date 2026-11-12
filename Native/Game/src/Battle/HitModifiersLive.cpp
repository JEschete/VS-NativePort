// Hand-maintained (tools/port/owned.csv).
#include "Battle/HitModifiersLive.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iterator>
#include <optional>
#include <type_traits>

#include "Battle/HitModifiers.hpp"
extern "C" {
#include "Battle/Action.h"
#include "Battle/Actor.h"
#include "Battle/Damage.h"
#include "Battle/Hit.h"
#include "Battle/HitChance.h"
#include "Core/Diagnostics.h"
}

namespace VS::Battle {
namespace {
HitModifierStats ReadStats(const Battle_ActorStats& stats) {
  HitModifierStats result;
  result.actor_id = stats.actorId;
  result.risk = stats.risk;
  for (std::size_t i = 0; i < result.weapon_gems.size(); ++i) {
    result.weapon_gems.at(i) =
        static_cast<std::uint8_t>(stats.weapon.gems[i].gemEffects);
    result.shield_gems.at(i) =
        static_cast<std::uint8_t>(stats.shield.gems[i].gemEffects);
  }
  for (std::size_t i = 0; i < result.chain_evasion.size(); ++i) {
    result.chain_evasion.at(i) =
        static_cast<std::uint8_t>(stats.limbs[i].chainEvasion);
  }
  return result;
}

class LiveHitModifierWorld final : public HitModifierWorld {
 public:
  explicit LiveHitModifierWorld(Battle_ActionDefinition* action = nullptr)
      : action_(action) {}
  std::optional<HitModifierStats> Stats(std::uint8_t slot) override {
    if (slot >= std::size(Battle_actors) || Battle_actors[slot] == nullptr ||
        Battle_actors[slot]->stats == nullptr) {
      return std::nullopt;
    }
    return ReadStats(*Battle_actors[slot]->stats);
  }
  std::optional<std::uint32_t> WeaponDrawn(std::uint8_t slot) override {
    if (slot >= std::size(Battle_actors) || Battle_actors[slot] == nullptr) {
      return std::nullopt;
    }
    return std::bit_cast<std::uint32_t>(Battle_actors[slot]->weaponDrawn);
  }
  std::optional<HitModifierAction> Action() override {
    if (action_ == nullptr) return std::nullopt;
    return HitModifierAction{
        action_->id,
        action_->type,
        {static_cast<std::uint8_t>(action_->hits[0].effectId),
         static_cast<std::uint8_t>(action_->hits[1].effectId)}};
  }

 private:
  Battle_ActionDefinition* action_;
};

std::optional<HitModifierTarget> ReadHit(const Battle_HitResult* hit) {
  if (hit == nullptr) return std::nullopt;
  return HitModifierTarget{hit->target.targetActor,
                           std::bit_cast<std::int8_t>(static_cast<std::uint8_t>(
                               hit->target.targetLimb)),
                           hit->targetsPosition};
}

template <typename T>
const T* Pointer(const std::optional<T>& value) {
  return value ? &value.value() : nullptr;
}

template <typename T>
T Checked(std::optional<T> result) {
  if (!result) {
    Core_Errorf(
        "battle",
        "Hit modifier refused missing actor/stats/action or a used index");
    std::abort();
  }
  return result.value();
}
}  // namespace

// decomp: _getAttackGemBuff (src/BATTLE/BATTLE.PRG/146C.c:9712).
std::int16_t AttackLiveGemHitBonus(Battle_ActionDefinition* action,
                                   Battle_ActorStats* stats) {
  LiveHitModifierWorld world(action);
  const auto state = stats ? std::optional{ReadStats(*stats)} : std::nullopt;
  return Checked(AttackGemHitBonus(Pointer(state), world));
}

// decomp: _getDefenseGemBuff (src/BATTLE/BATTLE.PRG/146C.c:9741).
std::int16_t DefenseLiveGemHitBonus(Battle_ActionDefinition* action,
                                    Battle_ActorStats* stats, int hit) {
  LiveHitModifierWorld world(action);
  const auto state = stats ? std::optional{ReadStats(*stats)} : std::nullopt;
  return Checked(DefenseGemHitBonus(Pointer(state), hit, world));
}

// decomp: func_8007E97C (src/BATTLE/BATTLE.PRG/146C.c:9867).
int PreviousLiveHitSuccessThreshold(Battle_ActionDefinition*, Battle_HitResult*,
                                    Battle_HitResult*, int, int) {
  return PreviousHitSuccessThreshold(
      std::bit_cast<std::int32_t>(Battle_previousHitResult));
}

// decomp: _getHitGemModifier (src/BATTLE/BATTLE.PRG/146C.c:9875).
int LiveGemHitChance(Battle_ActionDefinition* action, Battle_HitResult* source,
                     Battle_HitResult* target, int hit, int) {
  LiveHitModifierWorld world(action);
  const auto from = ReadHit(source);
  const auto to = ReadHit(target);
  return Checked(GemHitChance(Pointer(from), Pointer(to), hit, world));
}

// decomp: _getRiskModifier (src/BATTLE/BATTLE.PRG/146C.c:10045).
int LiveRiskHitChance(Battle_ActionDefinition*, Battle_HitResult* source,
                      Battle_HitResult*, int, int) {
  LiveHitModifierWorld world;
  const auto from = ReadHit(source);
  return Checked(RiskHitChance(Pointer(from), world));
}

// decomp: _getChainEvasionModifier (src/BATTLE/BATTLE.PRG/146C.c:10057).
int LiveChainHitChance(Battle_ActionDefinition*, Battle_HitResult*,
                       Battle_HitResult* target, int, int) {
  LiveHitModifierWorld world;
  const auto to = ReadHit(target);
  return Checked(ChainHitChance(Pointer(to), world));
}

// Bind against the actual table element type, not an independently guessed ABI.
static_assert(std::is_same_v<decltype(Battle_hitFunctions[0]),
                             decltype(&PreviousLiveHitSuccessThreshold)&>);
static_assert(std::is_same_v<decltype(Battle_hitFunctions[0]),
                             decltype(&LiveGemHitChance)&>);
static_assert(std::is_same_v<decltype(Battle_hitFunctions[0]),
                             decltype(&LiveRiskHitChance)&>);
static_assert(std::is_same_v<decltype(Battle_hitFunctions[0]),
                             decltype(&LiveChainHitChance)&>);
}  // namespace VS::Battle
