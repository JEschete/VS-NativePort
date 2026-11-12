// Hand-maintained (tools/port/owned.csv).
#include "Battle/DamageRecoveryEffects.hpp"

#include <bit>
#include <cstdint>

namespace VS::Battle {
namespace {
constexpr std::uint8_t kCriticalHpDamage = 3;
ActionResultChange AddedChange(ActionResultChange change, std::int16_t amount,
                               std::uint8_t mode) {
  change.amount = std::bit_cast<std::int16_t>(
      static_cast<std::uint16_t>(change.amount + amount));
  change.mode = mode;
  return change;
}
void AddChange(DamageRecoveryQueue& queue, ActionResultField field,
               std::int16_t amount, std::uint8_t mode) {
  queue.WriteChange(field, AddedChange(queue.ReadChange(field), amount, mode));
}
}  // namespace

// decomp: func_80084B70 (src/BATTLE/BATTLE.PRG/146C.c:12789).
void QueueHitDamageAndEquipmentWear(DamageRecoveryQueue& queue, bool execute) {
  queue.CaptureDamageOwners();
  queue.ClearCriticalHit();
  const auto amount = queue.Calculate();
  const auto pending = queue.ReadChange(ActionResultField::kHp);
  queue.WriteChange(ActionResultField::kHp,
                    AddedChange(pending, amount,
                                queue.CriticalHit() ? kCriticalHpDamage : 1));
  if (queue.TargetsPosition()) return;
  queue.DistributeDamage(amount);
  queue.QueueRemovalStatuses(queue.TargetRemovalStatuses());
  if (execute) {
    queue.ConsumeEquipmentDp(amount);
    queue.AccumulateEquipmentPp(amount);
  }
}

// decomp: func_80084CAC (src/BATTLE/BATTLE.PRG/146C.c:12820).
void QueueMpDrain(DamageRecoveryQueue& queue) {
  queue.CaptureTargetOwner();
  auto amount = queue.Calculate();
  const auto current = queue.TargetMp();
  if (current < amount) {
    amount = current;
    queue.StoreLastValue(current);
  }
  AddChange(queue, ActionResultField::kMp, amount, 1);
}

// decomp: func_80084D44 (src/BATTLE/BATTLE.PRG/146C.c:12835).
void QueueWeaponPpDrain(DamageRecoveryQueue& queue) {
  queue.CaptureTargetOwner();
  if (queue.HasWeapon()) {
    AddChange(queue, ActionResultField::kWeaponPp,
              std::bit_cast<std::int16_t>(queue.WeaponPp()), 1);
  }
}

// decomp: func_80084DA8 (src/BATTLE/BATTLE.PRG/146C.c:12847).
void QueueShieldPpDrain(DamageRecoveryQueue& queue) {
  queue.CaptureTargetOwner();
  const auto mode = queue.SelectedPrimaryHpMode();
  if ((mode == 1 || mode == kCriticalHpDamage) && queue.HasShield()) {
    AddChange(queue, ActionResultField::kShieldPp,
              std::bit_cast<std::int16_t>(queue.ShieldPp()), 1);
  }
}

// decomp: func_80084E28 (src/BATTLE/BATTLE.PRG/146C.c:12862).
void QueueHpRecovery(DamageRecoveryQueue& queue) {
  const auto amount = queue.Calculate();
  AddChange(queue, ActionResultField::kHp, amount, 2);
  if (!queue.TargetsPosition()) queue.RecoverLimbs(amount);
}

// decomp: func_80084EA0 (src/BATTLE/BATTLE.PRG/146C.c:12874).
void QueueMpRecovery(DamageRecoveryQueue& queue) {
  const auto amount = queue.Calculate();
  AddChange(queue, ActionResultField::kMp, amount, 2);
}

// decomp: func_80084EEC (src/BATTLE/BATTLE.PRG/146C.c:12882).
void QueueWeaponPpRecovery(DamageRecoveryQueue& queue) {
  const auto amount = queue.Calculate();
  AddChange(queue, ActionResultField::kWeaponPp, amount, 2);
}

// decomp: func_80084F40 (src/BATTLE/BATTLE.PRG/146C.c:12890).
void QueueWeaponDpRecovery(DamageRecoveryQueue& queue) {
  const auto amount = queue.Calculate();
  AddChange(queue, ActionResultField::kWeaponDp, amount, 2);
}

// decomp: func_80084F94 (src/BATTLE/BATTLE.PRG/146C.c:12898).
void QueueRiskRecovery(DamageRecoveryQueue& queue) {
  const auto amount = queue.Calculate();
  if (amount != 0) AddChange(queue, ActionResultField::kRisk, amount, 2);
}
}  // namespace VS::Battle
