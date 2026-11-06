#pragma once
// Hand-maintained (tools/port/owned.csv).
// The attack prerequisite table slots of the typed rules
// (Battle/AttackPrerequisites.hpp). Battle_attackPrerequisiteFunctions and the
// BATTLE.PRG module image hold these bindings; each runs its rule on the
// target actor named by the hit's target result bytes
// (AttackPrerequisitesLive.cpp).
#include "Legacy/Psx.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "Battle/Action.h"
#include "Battle/AttackPrerequisiteTable.h"

int Battle_RejectAttackPrerequisiteSlot(Battle_ActionDefinition* action,
                                        u_char* target_result_bytes);
int Battle_AcceptAttackPrerequisiteSlot(Battle_ActionDefinition* action,
                                        u_char* target_result_bytes);
int Battle_IsAttackTargetUndeadSlot(Battle_ActionDefinition* action,
                                    u_char* target_result_bytes);
int Battle_IsAttackTargetLivingSlot(Battle_ActionDefinition* action,
                                    u_char* target_result_bytes);
int Battle_CanApplyStrengthDownSlot(Battle_ActionDefinition* action,
                                    u_char* target_result_bytes);
int Battle_CanApplyIntelligenceDownSlot(Battle_ActionDefinition* action,
                                        u_char* target_result_bytes);
int Battle_CanApplyAgilityDownSlot(Battle_ActionDefinition* action,
                                   u_char* target_result_bytes);
int Battle_CanApplyEquipmentDownSlot(Battle_ActionDefinition* action,
                                     u_char* target_result_bytes);
int Battle_CanApplyHalfMovementSlot(Battle_ActionDefinition* action,
                                    u_char* target_result_bytes);
int Battle_CanApplyStrengthUpSlot(Battle_ActionDefinition* action,
                                  u_char* target_result_bytes);
int Battle_CanApplyIntelligenceUpSlot(Battle_ActionDefinition* action,
                                      u_char* target_result_bytes);
int Battle_CanApplyAgilityUpSlot(Battle_ActionDefinition* action,
                                 u_char* target_result_bytes);
int Battle_CanApplyEquipmentUpSlot(Battle_ActionDefinition* action,
                                   u_char* target_result_bytes);
int Battle_CanApplyQuickenSlot(Battle_ActionDefinition* action,
                               u_char* target_result_bytes);
int Battle_CanApplySilenceSlot(Battle_ActionDefinition* action,
                               u_char* target_result_bytes);
int Battle_CanApplyMagicWardSlot(Battle_ActionDefinition* action,
                                 u_char* target_result_bytes);
int Battle_CanApplyRegenerationSlot(Battle_ActionDefinition* action,
                                    u_char* target_result_bytes);
int Battle_CanApplyParalysisSlot(Battle_ActionDefinition* action,
                                 u_char* target_result_bytes);
int Battle_CanApplyPoisonSlot(Battle_ActionDefinition* action,
                              u_char* target_result_bytes);
int Battle_CanApplyNumbnessSlot(Battle_ActionDefinition* action,
                                u_char* target_result_bytes);
int Battle_CanApplyCurseSlot(Battle_ActionDefinition* action,
                             u_char* target_result_bytes);
int Battle_CanApplyAirAttackUpSlot(Battle_ActionDefinition* action,
                                   u_char* target_result_bytes);
int Battle_CanApplyFireAttackUpSlot(Battle_ActionDefinition* action,
                                    u_char* target_result_bytes);
int Battle_CanApplyEarthAttackUpSlot(Battle_ActionDefinition* action,
                                     u_char* target_result_bytes);
int Battle_CanApplyWaterAttackUpSlot(Battle_ActionDefinition* action,
                                     u_char* target_result_bytes);
int Battle_CanApplyAirDefenseUpSlot(Battle_ActionDefinition* action,
                                    u_char* target_result_bytes);
int Battle_CanApplyFireDefenseUpSlot(Battle_ActionDefinition* action,
                                     u_char* target_result_bytes);
int Battle_CanApplyEarthDefenseUpSlot(Battle_ActionDefinition* action,
                                      u_char* target_result_bytes);
int Battle_CanApplyWaterDefenseUpSlot(Battle_ActionDefinition* action,
                                      u_char* target_result_bytes);
int Battle_CanApplyBanishSlot(Battle_ActionDefinition* action,
                              u_char* target_result_bytes);
int Battle_CanApplyExorciseSlot(Battle_ActionDefinition* action,
                                u_char* target_result_bytes);
int Battle_CanApplyDrainSlot(Battle_ActionDefinition* action,
                             u_char* target_result_bytes);
int Battle_IgnoreAttackPrerequisiteSlot(Battle_ActionDefinition* action,
                                        u_char* target_result_bytes);

#ifdef __cplusplus
}
#endif
