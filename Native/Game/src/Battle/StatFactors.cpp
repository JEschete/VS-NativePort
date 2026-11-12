// Hand-maintained (tools/port/owned.csv).
#include "Battle/StatFactors.hpp"

#include "Core/Math/ScalarMath.hpp"

namespace VS::Battle {
namespace {
constexpr int kTenths = 10;
constexpr int kPercent = 100;
constexpr std::uint8_t kDrainEffect = 3;
std::int32_t ScaleHit(StatFactorHit amount, std::uint8_t factor) {
  return ((static_cast<std::int32_t>(amount.hp) + amount.mp) * factor) /
         kTenths;
}
std::optional<std::int16_t> WithQueueOffset(std::int32_t amount,
                                            StatFactorWorld& world) {
  const auto queue = world.QueueIndex();
  if (!queue) return std::nullopt;
  return Core::Math::WrapSigned16(static_cast<std::uint32_t>(
      amount + static_cast<std::int32_t>(queue.value()) - 1));
}
std::optional<std::int16_t> MissingVital(int hit, StatFactorResource resource,
                                         StatFactorWorld& world) {
  const auto vital = world.SourceVital(resource);
  if (!vital) return std::nullopt;
  const auto factor = world.Factor(hit);
  if (!factor) return std::nullopt;
  const auto missing =
      static_cast<std::int32_t>(vital.value().maximum) - vital.value().current;
  return WithQueueOffset((missing * factor.value()) / kTenths, world);
}
}  // namespace

// decomp: _previousHitFactor0 (src/BATTLE/BATTLE.PRG/146C.c:11640),
// _previousHitFactor1 (:11659), _previousHitFactor2 (:12146).
// Original entries 0x80082144, 0x80082234, 0x80083524 share all 27
// instructions.
std::optional<std::int16_t> ScalePreviousTargetHit(int hit,
                                                   StatFactorWorld& world) {
  const auto factor = world.Factor(hit);
  if (!factor) return std::nullopt;
  const auto amount = world.PreviousTarget();
  if (!amount) return std::nullopt;
  return WithQueueOffset(ScaleHit(amount.value(), factor.value()), world);
}
// decomp: _hpLostFactor (src/BATTLE/BATTLE.PRG/146C.c:11650).
std::optional<std::int16_t> ScaleSourceMissingHp(int hit,
                                                 StatFactorWorld& world) {
  return MissingVital(hit, StatFactorResource::kHp, world);
}
// decomp: _factorCurrentHp (src/BATTLE/BATTLE.PRG/146C.c:11669).
std::optional<std::int16_t> ScaleTargetCurrentHp(int hit,
                                                 StatFactorWorld& world) {
  const auto hp = world.TargetHp();
  if (!hp) return std::nullopt;
  const auto factor = world.Factor(hit);
  if (!factor) return std::nullopt;
  return WithQueueOffset(
      (static_cast<std::int32_t>(hp.value()) * factor.value()) / kPercent,
      world);
}
// decomp: _getWeaponPp (src/BATTLE/BATTLE.PRG/146C.c:11678).
std::optional<std::int16_t> WeaponPpWithChainOffset(StatFactorWorld& world) {
  const auto pp = world.WeaponPp();
  if (!pp) return std::nullopt;
  return WithQueueOffset(pp.value(), world);
}
// decomp: _factorMagicDamageReceived (src/BATTLE/BATTLE.PRG/146C.c:11686).
std::optional<std::int16_t> ScaleIncomingSpellDamage(int hit,
                                                     StatFactorWorld& world) {
  const auto type = world.SelectedActionType();
  if (!type) return std::nullopt;
  if (type.value() != 1) return 0;
  const auto effect = world.PrimaryHpEffect();
  if (!effect) return std::nullopt;
  if (effect.value() != 1 && effect.value() != kDrainEffect) return 0;
  const auto amount = world.PrimaryHit();
  if (!amount) return std::nullopt;
  const auto factor = world.Factor(hit);
  if (!factor) return std::nullopt;
  return Core::Math::WrapSigned16(
      static_cast<std::uint32_t>(ScaleHit(amount.value(), factor.value())));
}
// decomp: _mpLostFactor (src/BATTLE/BATTLE.PRG/146C.c:11720).
std::optional<std::int16_t> ScaleSourceMissingMp(int hit,
                                                 StatFactorWorld& world) {
  return MissingVital(hit, StatFactorResource::kMp, world);
}
}  // namespace VS::Battle
