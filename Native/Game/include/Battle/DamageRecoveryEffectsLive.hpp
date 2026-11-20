#pragma once
// Hand-maintained (tools/port/owned.csv).

namespace VS::Battle {

struct ActionDefinition;
struct HitResult;
}  // namespace VS::Battle

namespace VS::Battle {
void QueueLiveWeaponPpRecovery(VS::Battle::ActionDefinition*,
                               VS::Battle::HitResult*, VS::Battle::HitResult*,
                               int, int);
void QueueLiveWeaponDpRecovery(VS::Battle::ActionDefinition*,
                               VS::Battle::HitResult*, VS::Battle::HitResult*,
                               int, int);
void QueueLiveRiskRecovery(VS::Battle::ActionDefinition*,
                           VS::Battle::HitResult*, VS::Battle::HitResult*, int,
                           int);
void QueueLiveHitDamageAndEquipmentWear(VS::Battle::ActionDefinition*,
                                        VS::Battle::HitResult*,
                                        VS::Battle::HitResult*, int, int);
void QueueLiveMpDrain(VS::Battle::ActionDefinition*, VS::Battle::HitResult*,
                      VS::Battle::HitResult*, int, int);
void QueueLiveWeaponPpDrain(VS::Battle::ActionDefinition*,
                            VS::Battle::HitResult*, VS::Battle::HitResult*, int,
                            int);
void QueueLiveShieldPpDrain(VS::Battle::ActionDefinition*,
                            VS::Battle::HitResult*, VS::Battle::HitResult*, int,
                            int);
void QueueLiveHpRecovery(VS::Battle::ActionDefinition*, VS::Battle::HitResult*,
                         VS::Battle::HitResult*, int, int);
void QueueLiveMpRecovery(VS::Battle::ActionDefinition*, VS::Battle::HitResult*,
                         VS::Battle::HitResult*, int, int);
}  // namespace VS::Battle
