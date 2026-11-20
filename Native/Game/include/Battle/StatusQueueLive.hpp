#pragma once
// Hand-maintained (tools/port/owned.csv).

struct Battle_ActionDefinition;
namespace VS::Battle {
struct HitResult;
}  // namespace VS::Battle

namespace VS::Battle {
int QueueLiveApplicableStatusChanges(int status_id, VS::Battle::HitResult* hit);
int QueueLiveStatusRemovalMask(int status_id, VS::Battle::HitResult* hit);
// Action-effect mediators are void callbacks; incidental MIPS v0 values
// belong only to the differential projection, not this production contract.
void QueueLiveStrengthDown(Battle_ActionDefinition*, VS::Battle::HitResult*,
                           VS::Battle::HitResult* target, int, int);
void QueueLiveIntelligenceDown(Battle_ActionDefinition*, VS::Battle::HitResult*,
                               VS::Battle::HitResult* target, int, int);
void QueueLiveAgilityDown(Battle_ActionDefinition*, VS::Battle::HitResult*,
                          VS::Battle::HitResult* target, int, int);
void QueueLiveEquipmentDown(Battle_ActionDefinition*, VS::Battle::HitResult*,
                            VS::Battle::HitResult* target, int, int);
void QueueLiveHalfMovement(Battle_ActionDefinition*, VS::Battle::HitResult*,
                           VS::Battle::HitResult* target, int, int);
void QueueLiveStrengthUp(Battle_ActionDefinition*, VS::Battle::HitResult*,
                         VS::Battle::HitResult* target, int, int);
void QueueLiveIntelligenceUp(Battle_ActionDefinition*, VS::Battle::HitResult*,
                             VS::Battle::HitResult* target, int, int);
void QueueLiveAgilityUp(Battle_ActionDefinition*, VS::Battle::HitResult*,
                        VS::Battle::HitResult* target, int, int);
void QueueLiveEquipmentUp(Battle_ActionDefinition*, VS::Battle::HitResult*,
                          VS::Battle::HitResult* target, int, int);
void QueueLiveQuicken(Battle_ActionDefinition*, VS::Battle::HitResult*,
                      VS::Battle::HitResult* target, int, int);
void QueueLiveSilence(Battle_ActionDefinition*, VS::Battle::HitResult*,
                      VS::Battle::HitResult* target, int, int);
void QueueLiveMagicWard(Battle_ActionDefinition*, VS::Battle::HitResult*,
                        VS::Battle::HitResult* target, int, int);
void QueueLiveRegeneration(Battle_ActionDefinition*, VS::Battle::HitResult*,
                           VS::Battle::HitResult* target, int, int);
void QueueLiveBanish(Battle_ActionDefinition*, VS::Battle::HitResult*,
                     VS::Battle::HitResult* target, int, int execute_action);
void QueueLiveParalysis(Battle_ActionDefinition*, VS::Battle::HitResult*,
                        VS::Battle::HitResult* target, int, int);
void QueueLivePoison(Battle_ActionDefinition*, VS::Battle::HitResult*,
                     VS::Battle::HitResult* target, int, int);
void QueueLiveNumbness(Battle_ActionDefinition*, VS::Battle::HitResult*,
                       VS::Battle::HitResult* target, int, int);
void QueueLiveCurse(Battle_ActionDefinition*, VS::Battle::HitResult*,
                    VS::Battle::HitResult* target, int, int);
void QueueLiveAirAttackUp(Battle_ActionDefinition*, VS::Battle::HitResult*,
                          VS::Battle::HitResult* target, int, int);
void QueueLiveFireAttackUp(Battle_ActionDefinition*, VS::Battle::HitResult*,
                           VS::Battle::HitResult* target, int, int);
void QueueLiveEarthAttackUp(Battle_ActionDefinition*, VS::Battle::HitResult*,
                            VS::Battle::HitResult* target, int, int);
void QueueLiveWaterAttackUp(Battle_ActionDefinition*, VS::Battle::HitResult*,
                            VS::Battle::HitResult* target, int, int);
void QueueLiveAirDefenseUp(Battle_ActionDefinition*, VS::Battle::HitResult*,
                           VS::Battle::HitResult* target, int, int);
void QueueLiveFireDefenseUp(Battle_ActionDefinition*, VS::Battle::HitResult*,
                            VS::Battle::HitResult* target, int, int);
void QueueLiveEarthDefenseUp(Battle_ActionDefinition*, VS::Battle::HitResult*,
                             VS::Battle::HitResult* target, int, int);
void QueueLiveWaterDefenseUp(Battle_ActionDefinition*, VS::Battle::HitResult*,
                             VS::Battle::HitResult* target, int, int);
void QueueLiveParalysisCure(Battle_ActionDefinition*, VS::Battle::HitResult*,
                            VS::Battle::HitResult* target, int, int);
void QueueLivePoisonCure(Battle_ActionDefinition*, VS::Battle::HitResult*,
                         VS::Battle::HitResult* target, int, int);
void QueueLiveCurseCure(Battle_ActionDefinition*, VS::Battle::HitResult*,
                        VS::Battle::HitResult* target, int, int);
void QueueLiveNumbnessCure(Battle_ActionDefinition*, VS::Battle::HitResult*,
                           VS::Battle::HitResult* target, int, int);
void QueueLiveDispel(Battle_ActionDefinition*, VS::Battle::HitResult*,
                     VS::Battle::HitResult* target, int, int);
void QueueLiveNegativeStatusCure(Battle_ActionDefinition*,
                                 VS::Battle::HitResult*,
                                 VS::Battle::HitResult* target, int, int);
}  // namespace VS::Battle
