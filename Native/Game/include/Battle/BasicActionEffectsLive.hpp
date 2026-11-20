#pragma once
// Hand-maintained (tools/port/owned.csv).

namespace VS::Battle {

struct ActionDefinition;
struct HitResult;
}  // namespace VS::Battle

namespace VS::Battle {
void QueueLiveDarkAffinityRecovery(VS::Battle::ActionDefinition*,
                                   VS::Battle::HitResult*,
                                   VS::Battle::HitResult*, int, int);
void QueueLiveDamageTriggeredRecovery(VS::Battle::ActionDefinition*,
                                      VS::Battle::HitResult*,
                                      VS::Battle::HitResult*, int, int);
void QueueLiveSpellTriggeredRecovery(VS::Battle::ActionDefinition*,
                                     VS::Battle::HitResult*,
                                     VS::Battle::HitResult*, int, int);
void QueueLiveNonSpellTriggeredRecovery(VS::Battle::ActionDefinition*,
                                        VS::Battle::HitResult*,
                                        VS::Battle::HitResult*, int, int);
void QueueLivePhysicalAffinityRecovery(VS::Battle::ActionDefinition*,
                                       VS::Battle::HitResult*,
                                       VS::Battle::HitResult*, int, int);
void QueueLiveAirAffinityRecovery(VS::Battle::ActionDefinition*,
                                  VS::Battle::HitResult*,
                                  VS::Battle::HitResult*, int, int);
void QueueLiveFireAffinityRecovery(VS::Battle::ActionDefinition*,
                                   VS::Battle::HitResult*,
                                   VS::Battle::HitResult*, int, int);
void QueueLiveEarthAffinityRecovery(VS::Battle::ActionDefinition*,
                                    VS::Battle::HitResult*,
                                    VS::Battle::HitResult*, int, int);
void QueueLiveWaterAffinityRecovery(VS::Battle::ActionDefinition*,
                                    VS::Battle::HitResult*,
                                    VS::Battle::HitResult*, int, int);
void QueueLiveLightAffinityRecovery(VS::Battle::ActionDefinition*,
                                    VS::Battle::HitResult*,
                                    VS::Battle::HitResult*, int, int);
void QueueLiveRandomStrengthIncrease(VS::Battle::ActionDefinition*,
                                     VS::Battle::HitResult*,
                                     VS::Battle::HitResult*, int, int);
void QueueLiveRandomIntelligenceIncrease(VS::Battle::ActionDefinition*,
                                         VS::Battle::HitResult*,
                                         VS::Battle::HitResult*, int, int);
void QueueLiveRandomAgilityIncrease(VS::Battle::ActionDefinition*,
                                    VS::Battle::HitResult*,
                                    VS::Battle::HitResult*, int, int);
void QueueLiveRandomMaxHpIncrease(VS::Battle::ActionDefinition*,
                                  VS::Battle::HitResult*,
                                  VS::Battle::HitResult*, int, int);
void QueueLiveRandomMaxMpIncrease(VS::Battle::ActionDefinition*,
                                  VS::Battle::HitResult*,
                                  VS::Battle::HitResult*, int, int);
void QueueLiveRandomStrengthDecrease(VS::Battle::ActionDefinition*,
                                     VS::Battle::HitResult*,
                                     VS::Battle::HitResult*, int, int);
void QueueLiveRandomIntelligenceDecrease(VS::Battle::ActionDefinition*,
                                         VS::Battle::HitResult*,
                                         VS::Battle::HitResult*, int, int);
void QueueLiveRandomAgilityDecrease(VS::Battle::ActionDefinition*,
                                    VS::Battle::HitResult*,
                                    VS::Battle::HitResult*, int, int);
void MarkLiveSourceActionSuccess(VS::Battle::ActionDefinition*,
                                 VS::Battle::HitResult*, VS::Battle::HitResult*,
                                 int, int);
void QueueLiveInstantDeath(VS::Battle::ActionDefinition*,
                           VS::Battle::HitResult*, VS::Battle::HitResult*, int,
                           int);
void QueueLiveDamageBasedArmorWear(VS::Battle::ActionDefinition*,
                                   VS::Battle::HitResult*,
                                   VS::Battle::HitResult*, int, int);
void SetLiveActionFlag64(VS::Battle::ActionDefinition*, VS::Battle::HitResult*,
                         VS::Battle::HitResult*, int, int);
void SetLiveActionFlag32(VS::Battle::ActionDefinition*, VS::Battle::HitResult*,
                         VS::Battle::HitResult*, int, int);
}  // namespace VS::Battle
