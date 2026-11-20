#pragma once
// Hand-maintained (tools/port/owned.csv).

namespace VS::Battle {

struct ActionDefinition;
struct HitResult;
}  // namespace VS::Battle

namespace VS::Battle {
int QueueLiveApplicableStatusChanges(int status_id, VS::Battle::HitResult* hit);
int QueueLiveStatusRemovalMask(int status_id, VS::Battle::HitResult* hit);
// Action-effect mediators are void callbacks; incidental MIPS v0 values
// belong only to the differential projection, not this production contract.
void QueueLiveStrengthDown(VS::Battle::ActionDefinition*,
                           VS::Battle::HitResult*,
                           VS::Battle::HitResult* target, int, int);
void QueueLiveIntelligenceDown(VS::Battle::ActionDefinition*,
                               VS::Battle::HitResult*,
                               VS::Battle::HitResult* target, int, int);
void QueueLiveAgilityDown(VS::Battle::ActionDefinition*, VS::Battle::HitResult*,
                          VS::Battle::HitResult* target, int, int);
void QueueLiveEquipmentDown(VS::Battle::ActionDefinition*,
                            VS::Battle::HitResult*,
                            VS::Battle::HitResult* target, int, int);
void QueueLiveHalfMovement(VS::Battle::ActionDefinition*,
                           VS::Battle::HitResult*,
                           VS::Battle::HitResult* target, int, int);
void QueueLiveStrengthUp(VS::Battle::ActionDefinition*, VS::Battle::HitResult*,
                         VS::Battle::HitResult* target, int, int);
void QueueLiveIntelligenceUp(VS::Battle::ActionDefinition*,
                             VS::Battle::HitResult*,
                             VS::Battle::HitResult* target, int, int);
void QueueLiveAgilityUp(VS::Battle::ActionDefinition*, VS::Battle::HitResult*,
                        VS::Battle::HitResult* target, int, int);
void QueueLiveEquipmentUp(VS::Battle::ActionDefinition*, VS::Battle::HitResult*,
                          VS::Battle::HitResult* target, int, int);
void QueueLiveQuicken(VS::Battle::ActionDefinition*, VS::Battle::HitResult*,
                      VS::Battle::HitResult* target, int, int);
void QueueLiveSilence(VS::Battle::ActionDefinition*, VS::Battle::HitResult*,
                      VS::Battle::HitResult* target, int, int);
void QueueLiveMagicWard(VS::Battle::ActionDefinition*, VS::Battle::HitResult*,
                        VS::Battle::HitResult* target, int, int);
void QueueLiveRegeneration(VS::Battle::ActionDefinition*,
                           VS::Battle::HitResult*,
                           VS::Battle::HitResult* target, int, int);
void QueueLiveBanish(VS::Battle::ActionDefinition*, VS::Battle::HitResult*,
                     VS::Battle::HitResult* target, int, int execute_action);
void QueueLiveParalysis(VS::Battle::ActionDefinition*, VS::Battle::HitResult*,
                        VS::Battle::HitResult* target, int, int);
void QueueLivePoison(VS::Battle::ActionDefinition*, VS::Battle::HitResult*,
                     VS::Battle::HitResult* target, int, int);
void QueueLiveNumbness(VS::Battle::ActionDefinition*, VS::Battle::HitResult*,
                       VS::Battle::HitResult* target, int, int);
void QueueLiveCurse(VS::Battle::ActionDefinition*, VS::Battle::HitResult*,
                    VS::Battle::HitResult* target, int, int);
void QueueLiveAirAttackUp(VS::Battle::ActionDefinition*, VS::Battle::HitResult*,
                          VS::Battle::HitResult* target, int, int);
void QueueLiveFireAttackUp(VS::Battle::ActionDefinition*,
                           VS::Battle::HitResult*,
                           VS::Battle::HitResult* target, int, int);
void QueueLiveEarthAttackUp(VS::Battle::ActionDefinition*,
                            VS::Battle::HitResult*,
                            VS::Battle::HitResult* target, int, int);
void QueueLiveWaterAttackUp(VS::Battle::ActionDefinition*,
                            VS::Battle::HitResult*,
                            VS::Battle::HitResult* target, int, int);
void QueueLiveAirDefenseUp(VS::Battle::ActionDefinition*,
                           VS::Battle::HitResult*,
                           VS::Battle::HitResult* target, int, int);
void QueueLiveFireDefenseUp(VS::Battle::ActionDefinition*,
                            VS::Battle::HitResult*,
                            VS::Battle::HitResult* target, int, int);
void QueueLiveEarthDefenseUp(VS::Battle::ActionDefinition*,
                             VS::Battle::HitResult*,
                             VS::Battle::HitResult* target, int, int);
void QueueLiveWaterDefenseUp(VS::Battle::ActionDefinition*,
                             VS::Battle::HitResult*,
                             VS::Battle::HitResult* target, int, int);
void QueueLiveParalysisCure(VS::Battle::ActionDefinition*,
                            VS::Battle::HitResult*,
                            VS::Battle::HitResult* target, int, int);
void QueueLivePoisonCure(VS::Battle::ActionDefinition*, VS::Battle::HitResult*,
                         VS::Battle::HitResult* target, int, int);
void QueueLiveCurseCure(VS::Battle::ActionDefinition*, VS::Battle::HitResult*,
                        VS::Battle::HitResult* target, int, int);
void QueueLiveNumbnessCure(VS::Battle::ActionDefinition*,
                           VS::Battle::HitResult*,
                           VS::Battle::HitResult* target, int, int);
void QueueLiveDispel(VS::Battle::ActionDefinition*, VS::Battle::HitResult*,
                     VS::Battle::HitResult* target, int, int);
void QueueLiveNegativeStatusCure(VS::Battle::ActionDefinition*,
                                 VS::Battle::HitResult*,
                                 VS::Battle::HitResult* target, int, int);
}  // namespace VS::Battle
