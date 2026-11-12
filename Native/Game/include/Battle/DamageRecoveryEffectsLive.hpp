#pragma once
// Hand-maintained (tools/port/owned.csv).

struct Battle_ActionDefinition;
struct Battle_HitResult;

namespace VS::Battle {
void QueueLiveWeaponPpRecovery(Battle_ActionDefinition*, Battle_HitResult*,
                               Battle_HitResult*, int, int);
void QueueLiveWeaponDpRecovery(Battle_ActionDefinition*, Battle_HitResult*,
                               Battle_HitResult*, int, int);
void QueueLiveRiskRecovery(Battle_ActionDefinition*, Battle_HitResult*,
                           Battle_HitResult*, int, int);
void QueueLiveHitDamageAndEquipmentWear(Battle_ActionDefinition*,
                                        Battle_HitResult*, Battle_HitResult*,
                                        int, int);
void QueueLiveMpDrain(Battle_ActionDefinition*, Battle_HitResult*,
                      Battle_HitResult*, int, int);
void QueueLiveWeaponPpDrain(Battle_ActionDefinition*, Battle_HitResult*,
                            Battle_HitResult*, int, int);
void QueueLiveShieldPpDrain(Battle_ActionDefinition*, Battle_HitResult*,
                            Battle_HitResult*, int, int);
void QueueLiveHpRecovery(Battle_ActionDefinition*, Battle_HitResult*,
                         Battle_HitResult*, int, int);
void QueueLiveMpRecovery(Battle_ActionDefinition*, Battle_HitResult*,
                         Battle_HitResult*, int, int);
}  // namespace VS::Battle
