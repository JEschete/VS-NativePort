#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstdint>

#include "Battle/ActionResults.hpp"

namespace VS::Battle {
// Capture retains stats owners, not snapshots. Calculations and limb services
// may change hit identities; subsequent owner reads see their current fields.
class DamageRecoveryQueue {
 public:
  virtual ~DamageRecoveryQueue() = default;
  virtual void CaptureDamageOwners() = 0;
  virtual void CaptureTargetOwner() = 0;
  virtual void ClearCriticalHit() = 0;
  [[nodiscard]] virtual bool CriticalHit() = 0;
  [[nodiscard]] virtual std::int16_t Calculate() = 0;
  [[nodiscard]] virtual ActionResultChange ReadChange(
      ActionResultField field) = 0;
  virtual void WriteChange(ActionResultField field,
                           ActionResultChange change) = 0;
  [[nodiscard]] virtual bool TargetsPosition() = 0;
  virtual void DistributeDamage(std::int16_t amount) = 0;
  virtual void RecoverLimbs(std::int16_t amount) = 0;
  [[nodiscard]] virtual std::uint32_t TargetRemovalStatuses() = 0;
  virtual void QueueRemovalStatuses(std::uint32_t statuses) = 0;
  virtual void ConsumeEquipmentDp(std::int16_t amount) = 0;
  virtual void AccumulateEquipmentPp(std::int16_t amount) = 0;
  [[nodiscard]] virtual std::int16_t TargetMp() = 0;
  virtual void StoreLastValue(std::int16_t amount) = 0;
  [[nodiscard]] virtual bool HasWeapon() = 0;
  [[nodiscard]] virtual bool HasShield() = 0;
  [[nodiscard]] virtual std::uint16_t WeaponPp() = 0;
  [[nodiscard]] virtual std::uint16_t ShieldPp() = 0;
  [[nodiscard]] virtual std::uint8_t SelectedPrimaryHpMode() = 0;
};

// decomp: func_80084B70 (src/BATTLE/BATTLE.PRG/146C.c:12641).
void QueueHitDamageAndEquipmentWear(DamageRecoveryQueue& queue, bool execute);
// decomp: func_80084CAC (src/BATTLE/BATTLE.PRG/146C.c:12671).
void QueueMpDrain(DamageRecoveryQueue& queue);
// decomp: func_80084D44 (src/BATTLE/BATTLE.PRG/146C.c:12685).
void QueueWeaponPpDrain(DamageRecoveryQueue& queue);
// decomp: func_80084DA8 (src/BATTLE/BATTLE.PRG/146C.c:12696).
void QueueShieldPpDrain(DamageRecoveryQueue& queue);
// decomp: func_80084E28 (src/BATTLE/BATTLE.PRG/146C.c:12710).
void QueueHpRecovery(DamageRecoveryQueue& queue);
// decomp: func_80084EA0 (src/BATTLE/BATTLE.PRG/146C.c:12721).
void QueueMpRecovery(DamageRecoveryQueue& queue);
// decomp: func_80084EEC (src/BATTLE/BATTLE.PRG/146C.c:12728).
void QueueWeaponPpRecovery(DamageRecoveryQueue& queue);
// decomp: func_80084F40 (src/BATTLE/BATTLE.PRG/146C.c:12735).
void QueueWeaponDpRecovery(DamageRecoveryQueue& queue);
// decomp: func_80084F94 (src/BATTLE/BATTLE.PRG/146C.c:12742).
void QueueRiskRecovery(DamageRecoveryQueue& queue);
}  // namespace VS::Battle
