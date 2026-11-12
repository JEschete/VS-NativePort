#pragma once
// Hand-maintained (tools/port/owned.csv).

struct Battle_ActionDefinition;
struct Battle_HitResult;

namespace VS::Battle {
void QueueLiveDarkAffinityRecovery(Battle_ActionDefinition*, Battle_HitResult*,
                                   Battle_HitResult*, int, int);
void QueueLiveDamageTriggeredRecovery(Battle_ActionDefinition*,
                                      Battle_HitResult*, Battle_HitResult*, int,
                                      int);
void QueueLiveSpellTriggeredRecovery(Battle_ActionDefinition*,
                                     Battle_HitResult*, Battle_HitResult*, int,
                                     int);
void QueueLiveNonSpellTriggeredRecovery(Battle_ActionDefinition*,
                                        Battle_HitResult*, Battle_HitResult*,
                                        int, int);
void QueueLivePhysicalAffinityRecovery(Battle_ActionDefinition*,
                                       Battle_HitResult*, Battle_HitResult*,
                                       int, int);
void QueueLiveAirAffinityRecovery(Battle_ActionDefinition*, Battle_HitResult*,
                                  Battle_HitResult*, int, int);
void QueueLiveFireAffinityRecovery(Battle_ActionDefinition*, Battle_HitResult*,
                                   Battle_HitResult*, int, int);
void QueueLiveEarthAffinityRecovery(Battle_ActionDefinition*, Battle_HitResult*,
                                    Battle_HitResult*, int, int);
void QueueLiveWaterAffinityRecovery(Battle_ActionDefinition*, Battle_HitResult*,
                                    Battle_HitResult*, int, int);
void QueueLiveLightAffinityRecovery(Battle_ActionDefinition*, Battle_HitResult*,
                                    Battle_HitResult*, int, int);
void QueueLiveRandomStrengthIncrease(Battle_ActionDefinition*,
                                     Battle_HitResult*, Battle_HitResult*, int,
                                     int);
void QueueLiveRandomIntelligenceIncrease(Battle_ActionDefinition*,
                                         Battle_HitResult*, Battle_HitResult*,
                                         int, int);
void QueueLiveRandomAgilityIncrease(Battle_ActionDefinition*, Battle_HitResult*,
                                    Battle_HitResult*, int, int);
void QueueLiveRandomMaxHpIncrease(Battle_ActionDefinition*, Battle_HitResult*,
                                  Battle_HitResult*, int, int);
void QueueLiveRandomMaxMpIncrease(Battle_ActionDefinition*, Battle_HitResult*,
                                  Battle_HitResult*, int, int);
void QueueLiveRandomStrengthDecrease(Battle_ActionDefinition*,
                                     Battle_HitResult*, Battle_HitResult*, int,
                                     int);
void QueueLiveRandomIntelligenceDecrease(Battle_ActionDefinition*,
                                         Battle_HitResult*, Battle_HitResult*,
                                         int, int);
void QueueLiveRandomAgilityDecrease(Battle_ActionDefinition*, Battle_HitResult*,
                                    Battle_HitResult*, int, int);
void MarkLiveSourceActionSuccess(Battle_ActionDefinition*, Battle_HitResult*,
                                 Battle_HitResult*, int, int);
void QueueLiveInstantDeath(Battle_ActionDefinition*, Battle_HitResult*,
                           Battle_HitResult*, int, int);
void QueueLiveDamageBasedArmorWear(Battle_ActionDefinition*, Battle_HitResult*,
                                   Battle_HitResult*, int, int);
void SetLiveActionFlag64(Battle_ActionDefinition*, Battle_HitResult*,
                         Battle_HitResult*, int, int);
void SetLiveActionFlag32(Battle_ActionDefinition*, Battle_HitResult*,
                         Battle_HitResult*, int, int);
}  // namespace VS::Battle
