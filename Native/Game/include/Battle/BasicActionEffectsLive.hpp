#pragma once
// Hand-maintained (tools/port/owned.csv).

struct Battle_ActionDefinition;
namespace VS::Battle {
struct HitResult;
}  // namespace VS::Battle

namespace VS::Battle {
void QueueLiveDarkAffinityRecovery(Battle_ActionDefinition*,
                                   VS::Battle::HitResult*,
                                   VS::Battle::HitResult*, int, int);
void QueueLiveDamageTriggeredRecovery(Battle_ActionDefinition*,
                                      VS::Battle::HitResult*,
                                      VS::Battle::HitResult*, int, int);
void QueueLiveSpellTriggeredRecovery(Battle_ActionDefinition*,
                                     VS::Battle::HitResult*,
                                     VS::Battle::HitResult*, int, int);
void QueueLiveNonSpellTriggeredRecovery(Battle_ActionDefinition*,
                                        VS::Battle::HitResult*,
                                        VS::Battle::HitResult*, int, int);
void QueueLivePhysicalAffinityRecovery(Battle_ActionDefinition*,
                                       VS::Battle::HitResult*,
                                       VS::Battle::HitResult*, int, int);
void QueueLiveAirAffinityRecovery(Battle_ActionDefinition*,
                                  VS::Battle::HitResult*,
                                  VS::Battle::HitResult*, int, int);
void QueueLiveFireAffinityRecovery(Battle_ActionDefinition*,
                                   VS::Battle::HitResult*,
                                   VS::Battle::HitResult*, int, int);
void QueueLiveEarthAffinityRecovery(Battle_ActionDefinition*,
                                    VS::Battle::HitResult*,
                                    VS::Battle::HitResult*, int, int);
void QueueLiveWaterAffinityRecovery(Battle_ActionDefinition*,
                                    VS::Battle::HitResult*,
                                    VS::Battle::HitResult*, int, int);
void QueueLiveLightAffinityRecovery(Battle_ActionDefinition*,
                                    VS::Battle::HitResult*,
                                    VS::Battle::HitResult*, int, int);
void QueueLiveRandomStrengthIncrease(Battle_ActionDefinition*,
                                     VS::Battle::HitResult*,
                                     VS::Battle::HitResult*, int, int);
void QueueLiveRandomIntelligenceIncrease(Battle_ActionDefinition*,
                                         VS::Battle::HitResult*,
                                         VS::Battle::HitResult*, int, int);
void QueueLiveRandomAgilityIncrease(Battle_ActionDefinition*,
                                    VS::Battle::HitResult*,
                                    VS::Battle::HitResult*, int, int);
void QueueLiveRandomMaxHpIncrease(Battle_ActionDefinition*,
                                  VS::Battle::HitResult*,
                                  VS::Battle::HitResult*, int, int);
void QueueLiveRandomMaxMpIncrease(Battle_ActionDefinition*,
                                  VS::Battle::HitResult*,
                                  VS::Battle::HitResult*, int, int);
void QueueLiveRandomStrengthDecrease(Battle_ActionDefinition*,
                                     VS::Battle::HitResult*,
                                     VS::Battle::HitResult*, int, int);
void QueueLiveRandomIntelligenceDecrease(Battle_ActionDefinition*,
                                         VS::Battle::HitResult*,
                                         VS::Battle::HitResult*, int, int);
void QueueLiveRandomAgilityDecrease(Battle_ActionDefinition*,
                                    VS::Battle::HitResult*,
                                    VS::Battle::HitResult*, int, int);
void MarkLiveSourceActionSuccess(Battle_ActionDefinition*,
                                 VS::Battle::HitResult*, VS::Battle::HitResult*,
                                 int, int);
void QueueLiveInstantDeath(Battle_ActionDefinition*, VS::Battle::HitResult*,
                           VS::Battle::HitResult*, int, int);
void QueueLiveDamageBasedArmorWear(Battle_ActionDefinition*,
                                   VS::Battle::HitResult*,
                                   VS::Battle::HitResult*, int, int);
void SetLiveActionFlag64(Battle_ActionDefinition*, VS::Battle::HitResult*,
                         VS::Battle::HitResult*, int, int);
void SetLiveActionFlag32(Battle_ActionDefinition*, VS::Battle::HitResult*,
                         VS::Battle::HitResult*, int, int);
}  // namespace VS::Battle
