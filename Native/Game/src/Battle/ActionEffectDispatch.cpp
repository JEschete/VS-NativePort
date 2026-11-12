// Hand-maintained (tools/port/owned.csv).
#include "Battle/ActionEffectDispatch.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>

namespace VS::Battle {
namespace {
constexpr std::uint8_t kCrippleRule = 4;
constexpr std::uint32_t kCrippleStatus = 0x10;
constexpr std::uint8_t kExemptActor = 128;
constexpr std::uint32_t kSpellBlocked = 0x80040000;
constexpr std::uint32_t kSpellRemoved = 0x61000;
constexpr std::uint8_t kWeaponCategories = 10;
constexpr std::uint8_t kArtsPerWeapon = 4;
constexpr std::uint8_t kCriticalDamage = 3;
constexpr std::uint8_t kBreakArt = 3;

bool Healthy(LimbInjuryQueue& queue, std::uint8_t limb) {
  const auto current = queue.CurrentHp(limb);
  const auto pending = queue.PendingHp(limb);
  int predicted = current;
  if (pending.mode == 2) predicted += pending.amount;
  if (pending.mode == 1 || pending.mode == kCriticalDamage) {
    predicted -= pending.amount;
  }
  return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(predicted)) >=
         2;
}

void CountWeaponKill(ActionEffectWorld& world, int execute) {
  if (execute == 0 || world.SourcePosition() || world.SourceSlot() != 0 ||
      world.TargetPosition() || world.TargetActorId() == kExemptActor) {
    return;
  }
  const auto type = world.ActionType();
  constexpr std::uint8_t kBaseAttack = 6;
  if (type != 2 && type != kBreakArt && type != kBaseAttack) return;
  if (world.TargetHp() > world.PendingHp() || world.TargetSlot() == 0 ||
      world.KillCounted()) {
    return;
  }
  world.MarkKillCounted();
  const auto category =
      static_cast<std::uint8_t>(world.WeaponCategory() % kWeaponCategories);
  if (static_cast<std::uint8_t>(world.ProgressFlag()) < kWeaponCategories) {
    return;
  }
  const auto learned = world.LearnedArts(category);
  if (learned >= kArtsPerWeapon) return;
  const auto kills = world.WeaponKills(category);
  if (kills < world.RequiredKills(category, learned)) {
    world.SetWeaponKills(category, static_cast<std::uint16_t>(kills + 1));
  }
}
}  // namespace

// decomp: func_80085008 (src/BATTLE/BATTLE.PRG/146C.c:12930).
void DeriveLimbInjuryStatuses(LimbInjuryQueue& queue) {
  const auto first = queue.MobilityLimb(0);
  const auto second = queue.MobilityLimb(1);
  // Both are actual array indices, including zero, before the logical none
  // gate.
  const bool first_healthy = Healthy(queue, first);
  const bool second_healthy = Healthy(queue, second);
  const bool crippled = (queue.ActiveStatuses() & kCrippleStatus) != 0;
  const bool mobile =
      (first != 0 && first_healthy) || (second != 0 && second_healthy);
  if ((first != 0 || second != 0) && crippled == mobile) {
    queue.QueueStatus(kCrippleRule, crippled);
  }
  if (!queue.PlayerTarget()) return;
  constexpr std::uint8_t kPlayerInjuryLimbs = 4;
  for (std::uint8_t limb = 0; limb < kPlayerInjuryLimbs; ++limb) {
    const bool healthy = Healthy(queue, limb);
    const auto rule = queue.InjuryRule(limb);
    const auto statuses = queue.ActiveStatuses();
    const bool active = (statuses & queue.RuleMask(rule)) != 0;
    if (active == healthy) queue.QueueStatus(rule, active);
  }
}

// decomp: func_80085390 (src/BATTLE/BATTLE.PRG/146C.c:13012).
void ApplyActionHitEffect(ActionEffectWorld& world, int execute, int hit) {
  if (!world.TargetPosition() && world.TargetActorId() == kExemptActor) {
    world.QueueExemptDamage();
  } else if (world.ActionType() == 1) {
    if (!world.TargetPosition() &&
        (world.TargetStatuses() & kSpellBlocked) != 0) {
      world.ClearLastValue();
    } else {
      world.Dispatch(hit, execute);
    }
    if (!world.TargetPosition() && world.TargetActorId() != kExemptActor) {
      world.QueueStatusRemoval(world.TargetStatuses() & kSpellRemoved);
    }
  } else {
    world.Dispatch(hit, execute);
  }
  CountWeaponKill(world, execute);
}
}  // namespace VS::Battle
