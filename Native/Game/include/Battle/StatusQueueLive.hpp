#pragma once
// Hand-maintained (tools/port/owned.csv).

struct Battle_ActionDefinition;
struct Battle_HitResult;

namespace VS::Battle {
int QueueLiveApplicableStatusChanges(int status_id, Battle_HitResult* hit);
int QueueLiveStatusRemovalMask(int status_id, Battle_HitResult* hit);
int QueueLiveStrengthDown(Battle_ActionDefinition*, Battle_HitResult*,
                          Battle_HitResult* target, int, int);
int QueueLiveIntelligenceDown(Battle_ActionDefinition*, Battle_HitResult*,
                              Battle_HitResult* target, int, int);
int QueueLiveAgilityDown(Battle_ActionDefinition*, Battle_HitResult*,
                         Battle_HitResult* target, int, int);
int QueueLiveEquipmentDown(Battle_ActionDefinition*, Battle_HitResult*,
                           Battle_HitResult* target, int, int);
int QueueLiveHalfMovement(Battle_ActionDefinition*, Battle_HitResult*,
                          Battle_HitResult* target, int, int);
int QueueLiveStrengthUp(Battle_ActionDefinition*, Battle_HitResult*,
                        Battle_HitResult* target, int, int);
}  // namespace VS::Battle
