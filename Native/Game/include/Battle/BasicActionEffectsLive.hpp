#pragma once
// Hand-maintained (tools/port/owned.csv).

struct Battle_ActionDefinition;
struct Battle_HitResult;

namespace VS::Battle {
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
