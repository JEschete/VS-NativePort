#pragma once
// Hand-maintained (tools/port/owned.csv).

struct Battle_ActionDefinition;
struct Battle_HitResult;

namespace VS::Battle {
int QueueLiveApplicableStatusChanges(int status_id, Battle_HitResult* hit);
int QueueLiveStatusRemovalMask(int status_id, Battle_HitResult* hit);
// Action-effect mediators are void callbacks; incidental MIPS v0 values
// belong only to the differential projection, not this production contract.
void QueueLiveStrengthDown(Battle_ActionDefinition*, Battle_HitResult*,
                           Battle_HitResult* target, int, int);
void QueueLiveIntelligenceDown(Battle_ActionDefinition*, Battle_HitResult*,
                               Battle_HitResult* target, int, int);
void QueueLiveAgilityDown(Battle_ActionDefinition*, Battle_HitResult*,
                          Battle_HitResult* target, int, int);
void QueueLiveEquipmentDown(Battle_ActionDefinition*, Battle_HitResult*,
                            Battle_HitResult* target, int, int);
void QueueLiveHalfMovement(Battle_ActionDefinition*, Battle_HitResult*,
                           Battle_HitResult* target, int, int);
void QueueLiveStrengthUp(Battle_ActionDefinition*, Battle_HitResult*,
                         Battle_HitResult* target, int, int);
void QueueLiveIntelligenceUp(Battle_ActionDefinition*, Battle_HitResult*,
                             Battle_HitResult* target, int, int);
void QueueLiveAgilityUp(Battle_ActionDefinition*, Battle_HitResult*,
                        Battle_HitResult* target, int, int);
void QueueLiveEquipmentUp(Battle_ActionDefinition*, Battle_HitResult*,
                          Battle_HitResult* target, int, int);
void QueueLiveQuicken(Battle_ActionDefinition*, Battle_HitResult*,
                      Battle_HitResult* target, int, int);
void QueueLiveSilence(Battle_ActionDefinition*, Battle_HitResult*,
                      Battle_HitResult* target, int, int);
void QueueLiveMagicWard(Battle_ActionDefinition*, Battle_HitResult*,
                        Battle_HitResult* target, int, int);
void QueueLiveRegeneration(Battle_ActionDefinition*, Battle_HitResult*,
                           Battle_HitResult* target, int, int);
void QueueLiveBanish(Battle_ActionDefinition*, Battle_HitResult*,
                     Battle_HitResult* target, int, int execute_action);
}  // namespace VS::Battle
