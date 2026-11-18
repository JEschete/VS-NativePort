#pragma once
// Hand-maintained (tools/port/owned.csv).

#include "Battle/AttackPrerequisites.hpp"

struct Battle_ActionDefinition;

// Binding from the generated hit resolution caller to the typed attack
// prerequisites (AttackPrerequisites.hpp), in the original argument order
// (tools/port/copy_fixups.py OWNED_CALL_BINDINGS). A null action or target,
// a hit past the action's two hits, a rule past the table and an absent
// target actor are refused with a warning on the "battle.targeting" channel.
namespace VS::Battle {
// decomp: _canPerformAttack (src/BATTLE/BATTLE.PRG/146C.c:10523): 1 when the
// hit's prerequisite lets it apply, else 0.
int LiveCheckActionHitPrerequisite(Battle_ActionDefinition* action,
                                   unsigned char* target_result_bytes,
                                   int hit_number);

// The Battle_attackPrerequisiteFunctions slots of the typed rules; each runs
// its rule on the target actor named by the hit's target result bytes.
int RejectAttackPrerequisiteSlot(Battle_ActionDefinition* action,
                                 unsigned char* target_result_bytes);
int AcceptAttackPrerequisiteSlot(Battle_ActionDefinition* action,
                                 unsigned char* target_result_bytes);
int IsAttackTargetUndeadSlot(Battle_ActionDefinition* action,
                             unsigned char* target_result_bytes);
int IsAttackTargetLivingSlot(Battle_ActionDefinition* action,
                             unsigned char* target_result_bytes);
int CanApplyStrengthDownSlot(Battle_ActionDefinition* action,
                             unsigned char* target_result_bytes);
int CanApplyIntelligenceDownSlot(Battle_ActionDefinition* action,
                                 unsigned char* target_result_bytes);
int CanApplyAgilityDownSlot(Battle_ActionDefinition* action,
                            unsigned char* target_result_bytes);
int CanApplyEquipmentDownSlot(Battle_ActionDefinition* action,
                              unsigned char* target_result_bytes);
int CanApplyHalfMovementSlot(Battle_ActionDefinition* action,
                             unsigned char* target_result_bytes);
int CanApplyStrengthUpSlot(Battle_ActionDefinition* action,
                           unsigned char* target_result_bytes);
int CanApplyIntelligenceUpSlot(Battle_ActionDefinition* action,
                               unsigned char* target_result_bytes);
int CanApplyAgilityUpSlot(Battle_ActionDefinition* action,
                          unsigned char* target_result_bytes);
int CanApplyEquipmentUpSlot(Battle_ActionDefinition* action,
                            unsigned char* target_result_bytes);
int CanApplyQuickenSlot(Battle_ActionDefinition* action,
                        unsigned char* target_result_bytes);
int CanApplySilenceSlot(Battle_ActionDefinition* action,
                        unsigned char* target_result_bytes);
int CanApplyMagicWardSlot(Battle_ActionDefinition* action,
                          unsigned char* target_result_bytes);
int CanApplyRegenerationSlot(Battle_ActionDefinition* action,
                             unsigned char* target_result_bytes);
int CanApplyParalysisSlot(Battle_ActionDefinition* action,
                          unsigned char* target_result_bytes);
int CanApplyPoisonSlot(Battle_ActionDefinition* action,
                       unsigned char* target_result_bytes);
int CanApplyNumbnessSlot(Battle_ActionDefinition* action,
                         unsigned char* target_result_bytes);
int CanApplyCurseSlot(Battle_ActionDefinition* action,
                      unsigned char* target_result_bytes);
int CanApplyAirAttackUpSlot(Battle_ActionDefinition* action,
                            unsigned char* target_result_bytes);
int CanApplyFireAttackUpSlot(Battle_ActionDefinition* action,
                             unsigned char* target_result_bytes);
int CanApplyEarthAttackUpSlot(Battle_ActionDefinition* action,
                              unsigned char* target_result_bytes);
int CanApplyWaterAttackUpSlot(Battle_ActionDefinition* action,
                              unsigned char* target_result_bytes);
int CanApplyAirDefenseUpSlot(Battle_ActionDefinition* action,
                             unsigned char* target_result_bytes);
int CanApplyFireDefenseUpSlot(Battle_ActionDefinition* action,
                              unsigned char* target_result_bytes);
int CanApplyEarthDefenseUpSlot(Battle_ActionDefinition* action,
                               unsigned char* target_result_bytes);
int CanApplyWaterDefenseUpSlot(Battle_ActionDefinition* action,
                               unsigned char* target_result_bytes);
int CanApplyBanishSlot(Battle_ActionDefinition* action,
                       unsigned char* target_result_bytes);
int CanApplyExorciseSlot(Battle_ActionDefinition* action,
                         unsigned char* target_result_bytes);
int CanApplyDrainSlot(Battle_ActionDefinition* action,
                      unsigned char* target_result_bytes);
int IgnoreAttackPrerequisiteSlot(Battle_ActionDefinition* action,
                                 unsigned char* target_result_bytes);
}  // namespace VS::Battle
