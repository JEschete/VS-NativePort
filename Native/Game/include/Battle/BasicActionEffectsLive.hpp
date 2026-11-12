#pragma once
// Hand-maintained (tools/port/owned.csv).

struct Battle_ActionDefinition;
struct Battle_HitResult;

namespace VS::Battle {
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
