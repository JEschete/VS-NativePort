#pragma once
// Hand-maintained (tools/port/owned.csv).

#include "Battle/AttackPrerequisites.hpp"

namespace VS::Battle {
struct ActionDefinition;
}  // namespace VS::Battle

// Binding from the generated hit resolution caller to the typed attack
// prerequisites (AttackPrerequisites.hpp), in the original argument order
// (tools/port/copy_fixups.py OWNED_CALL_BINDINGS). A null action or target,
// a hit past the action's two hits, a rule past the table and an absent
// target actor are refused with a warning on the "battle.targeting" channel.
namespace VS::Battle {
// decomp: canPerformAttack (src/BATTLE/BATTLE.PRG/146C.c:10458; was
// _canPerformAttack): 1 when the hit's prerequisite lets it apply, else 0.
int LiveCheckActionHitPrerequisite(VS::Battle::ActionDefinition* action,
                                   unsigned char* target_result_bytes,
                                   int hit_number);

// The attackPrerequisiteFunctions slots of the typed rules; each runs
// its rule on the target actor named by the hit's target result bytes.
int RejectAttackPrerequisiteSlot(VS::Battle::ActionDefinition* action,
                                 unsigned char* target_result_bytes);
int AcceptAttackPrerequisiteSlot(VS::Battle::ActionDefinition* action,
                                 unsigned char* target_result_bytes);
int IsAttackTargetUndeadSlot(VS::Battle::ActionDefinition* action,
                             unsigned char* target_result_bytes);
int IsAttackTargetLivingSlot(VS::Battle::ActionDefinition* action,
                             unsigned char* target_result_bytes);
int CanApplyStrengthDownSlot(VS::Battle::ActionDefinition* action,
                             unsigned char* target_result_bytes);
int CanApplyIntelligenceDownSlot(VS::Battle::ActionDefinition* action,
                                 unsigned char* target_result_bytes);
int CanApplyAgilityDownSlot(VS::Battle::ActionDefinition* action,
                            unsigned char* target_result_bytes);
int CanApplyEquipmentDownSlot(VS::Battle::ActionDefinition* action,
                              unsigned char* target_result_bytes);
int CanApplyHalfMovementSlot(VS::Battle::ActionDefinition* action,
                             unsigned char* target_result_bytes);
int CanApplyStrengthUpSlot(VS::Battle::ActionDefinition* action,
                           unsigned char* target_result_bytes);
int CanApplyIntelligenceUpSlot(VS::Battle::ActionDefinition* action,
                               unsigned char* target_result_bytes);
int CanApplyAgilityUpSlot(VS::Battle::ActionDefinition* action,
                          unsigned char* target_result_bytes);
int CanApplyEquipmentUpSlot(VS::Battle::ActionDefinition* action,
                            unsigned char* target_result_bytes);
int CanApplyQuickenSlot(VS::Battle::ActionDefinition* action,
                        unsigned char* target_result_bytes);
int CanApplySilenceSlot(VS::Battle::ActionDefinition* action,
                        unsigned char* target_result_bytes);
int CanApplyMagicWardSlot(VS::Battle::ActionDefinition* action,
                          unsigned char* target_result_bytes);
int CanApplyRegenerationSlot(VS::Battle::ActionDefinition* action,
                             unsigned char* target_result_bytes);
int CanApplyParalysisSlot(VS::Battle::ActionDefinition* action,
                          unsigned char* target_result_bytes);
int CanApplyPoisonSlot(VS::Battle::ActionDefinition* action,
                       unsigned char* target_result_bytes);
int CanApplyNumbnessSlot(VS::Battle::ActionDefinition* action,
                         unsigned char* target_result_bytes);
int CanApplyCurseSlot(VS::Battle::ActionDefinition* action,
                      unsigned char* target_result_bytes);
int CanApplyAirAttackUpSlot(VS::Battle::ActionDefinition* action,
                            unsigned char* target_result_bytes);
int CanApplyFireAttackUpSlot(VS::Battle::ActionDefinition* action,
                             unsigned char* target_result_bytes);
int CanApplyEarthAttackUpSlot(VS::Battle::ActionDefinition* action,
                              unsigned char* target_result_bytes);
int CanApplyWaterAttackUpSlot(VS::Battle::ActionDefinition* action,
                              unsigned char* target_result_bytes);
int CanApplyAirDefenseUpSlot(VS::Battle::ActionDefinition* action,
                             unsigned char* target_result_bytes);
int CanApplyFireDefenseUpSlot(VS::Battle::ActionDefinition* action,
                              unsigned char* target_result_bytes);
int CanApplyEarthDefenseUpSlot(VS::Battle::ActionDefinition* action,
                               unsigned char* target_result_bytes);
int CanApplyWaterDefenseUpSlot(VS::Battle::ActionDefinition* action,
                               unsigned char* target_result_bytes);
int CanApplyBanishSlot(VS::Battle::ActionDefinition* action,
                       unsigned char* target_result_bytes);
int CanApplyExorciseSlot(VS::Battle::ActionDefinition* action,
                         unsigned char* target_result_bytes);
int CanApplyDrainSlot(VS::Battle::ActionDefinition* action,
                      unsigned char* target_result_bytes);
int IgnoreAttackPrerequisiteSlot(VS::Battle::ActionDefinition* action,
                                 unsigned char* target_result_bytes);
}  // namespace VS::Battle
