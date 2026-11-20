#pragma once
// Hand-maintained (tools/port/owned.csv).

struct Battle_ActionDefinition;
namespace VS::Battle {
struct HitResult;
}  // namespace VS::Battle

namespace VS::Battle {
void QueueLiveWeaponPpRecovery(Battle_ActionDefinition*, VS::Battle::HitResult*,
                               VS::Battle::HitResult*, int, int);
void QueueLiveWeaponDpRecovery(Battle_ActionDefinition*, VS::Battle::HitResult*,
                               VS::Battle::HitResult*, int, int);
void QueueLiveRiskRecovery(Battle_ActionDefinition*, VS::Battle::HitResult*,
                           VS::Battle::HitResult*, int, int);
void QueueLiveHitDamageAndEquipmentWear(Battle_ActionDefinition*,
                                        VS::Battle::HitResult*,
                                        VS::Battle::HitResult*, int, int);
void QueueLiveMpDrain(Battle_ActionDefinition*, VS::Battle::HitResult*,
                      VS::Battle::HitResult*, int, int);
void QueueLiveWeaponPpDrain(Battle_ActionDefinition*, VS::Battle::HitResult*,
                            VS::Battle::HitResult*, int, int);
void QueueLiveShieldPpDrain(Battle_ActionDefinition*, VS::Battle::HitResult*,
                            VS::Battle::HitResult*, int, int);
void QueueLiveHpRecovery(Battle_ActionDefinition*, VS::Battle::HitResult*,
                         VS::Battle::HitResult*, int, int);
void QueueLiveMpRecovery(Battle_ActionDefinition*, VS::Battle::HitResult*,
                         VS::Battle::HitResult*, int, int);
}  // namespace VS::Battle
