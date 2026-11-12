// Hand-maintained (tools/port/owned.csv).
#include "Battle/HitModifiers.hpp"

#include <algorithm>
#include <cstddef>

namespace VS::Battle {
namespace {
constexpr int kGemBonus = 20;
constexpr std::uint8_t kSpell = 1;
constexpr std::uint8_t kPhysicalDefenseGem = 5;
constexpr std::uint8_t kSpellDefenseGem = 6;
constexpr unsigned kRestrictedActionBegin = 40;
constexpr unsigned kAllowedActionResume = 54;
constexpr unsigned kAllowedActionCount = 170;
constexpr int kPercent = 100;
constexpr int kFailureThreshold = 255;
constexpr int kBelowFailureThreshold = 254;
constexpr int kRiskOffset = 150;
constexpr int kRiskDivisor = 256;
constexpr int kMaxChainEvasion = 255;
constexpr std::size_t kEffectGemCount = 18;
constexpr std::array<std::uint8_t, kEffectGemCount> kEffectGems = {
    0, 11, 12, 13, 0, 14, 0, 0, 0, 0, 0, 16, 0, 0, 0, 18, 19, 21,
};

bool HasGem(const std::array<std::uint8_t, kHitModifierGemCount>& gems,
            std::uint8_t effect) {
  return std::ranges::find(gems, effect) != gems.end();
}
}  // namespace

// decomp: _getAttackGemBuff (src/BATTLE/BATTLE.PRG/146C.c:9712).
std::optional<std::int16_t> AttackGemHitBonus(const HitModifierStats* stats,
                                              HitModifierWorld& world) {
  if (stats == nullptr) return std::nullopt;
  const auto drawn = world.WeaponDrawn(stats->actor_id);
  if (!drawn) return std::nullopt;
  if ((drawn.value() & 1U) == 0) return 0;
  const auto action = world.Action();
  if (!action) return std::nullopt;
  const std::uint8_t gem = action.value().type == kSpell ? 2 : 1;
  return HasGem(stats->weapon_gems, gem) ? kGemBonus : 0;
}

// decomp: _getDefenseGemBuff (src/BATTLE/BATTLE.PRG/146C.c:9741).
std::optional<std::int16_t> DefenseGemHitBonus(const HitModifierStats* stats,
                                               int hit,
                                               HitModifierWorld& world) {
  if (stats == nullptr) return std::nullopt;
  const auto drawn = world.WeaponDrawn(stats->actor_id);
  if (!drawn) return std::nullopt;
  if ((drawn.value() & 1U) == 0) return 0;
  const auto action = world.Action();
  if (!action || hit < 0 ||
      static_cast<std::size_t>(hit) >= action.value().effects.size()) {
    return std::nullopt;
  }
  const auto& used = action.value();
  int bonus = 0;
  if (used.type == kSpell) {
    if (HasGem(stats->shield_gems, kSpellDefenseGem)) bonus += kGemBonus;
  } else if (used.id < kRestrictedActionBegin ||
             static_cast<std::uint8_t>(used.id - kAllowedActionResume) <
                 kAllowedActionCount) {
    if (HasGem(stats->shield_gems, kPhysicalDefenseGem)) bonus += kGemBonus;
  }
  const auto effect = used.effects.at(static_cast<std::size_t>(hit));
  if (effect < kEffectGems.size()) {
    const auto gem = kEffectGems.at(effect);
    if (gem != 0 && HasGem(stats->shield_gems, gem)) bonus += kGemBonus;
  }
  return static_cast<std::int16_t>(bonus);
}

// decomp: func_8007E97C (src/BATTLE/BATTLE.PRG/146C.c:9867).
int PreviousHitSuccessThreshold(std::int32_t previous_result) {
  return previous_result != 0 ? kPercent : kFailureThreshold;
}

// decomp: _getHitGemModifier (src/BATTLE/BATTLE.PRG/146C.c:9875).
std::optional<int> GemHitChance(const HitModifierTarget* source,
                                const HitModifierTarget* target, int hit,
                                HitModifierWorld& world) {
  int chance = kPercent;
  if (source != nullptr && source->targets_position == 0) {
    const auto stats = world.Stats(source->actor);
    if (!stats) return std::nullopt;
    const auto bonus = AttackGemHitBonus(&stats.value(), world);
    if (!bonus) return std::nullopt;
    chance += bonus.value();
  }
  if (target != nullptr && target->targets_position == 0) {
    const auto stats = world.Stats(target->actor);
    if (!stats) return std::nullopt;
    const auto bonus = DefenseGemHitBonus(&stats.value(), hit, world);
    if (!bonus) return std::nullopt;
    chance -= bonus.value();
  }
  return chance;
}

// decomp: _getRiskModifier (src/BATTLE/BATTLE.PRG/146C.c:10045).
std::optional<int> RiskHitChance(const HitModifierTarget* source,
                                 HitModifierWorld& world) {
  if (source == nullptr) return std::nullopt;
  const auto stats = world.Stats(source->actor);
  if (!stats) return std::nullopt;
  const int rate =
      ((stats.value().risk + kRiskOffset) * kPercent) / kRiskDivisor;
  return rate == kFailureThreshold ? kBelowFailureThreshold : rate;
}

// decomp: _getChainEvasionModifier (src/BATTLE/BATTLE.PRG/146C.c:10057).
std::optional<int> ChainHitChance(const HitModifierTarget* target,
                                  HitModifierWorld& world) {
  if (target == nullptr) return std::nullopt;
  const auto stats = world.Stats(target->actor);
  if (!stats || target->limb < 0 ||
      static_cast<std::size_t>(target->limb) >=
          stats.value().chain_evasion.size()) {
    return std::nullopt;
  }
  const auto evasion =
      stats.value().chain_evasion.at(static_cast<std::size_t>(target->limb));
  // The original rate==255 correction is unreachable for an unsigned byte.
  return ((kMaxChainEvasion - evasion) * kPercent) / kMaxChainEvasion;
}
}  // namespace VS::Battle
