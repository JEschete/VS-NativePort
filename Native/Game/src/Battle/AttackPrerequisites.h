// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_BATTLE_ATTACKPREREQUISITES_H_TYPES
#define VS_BATTLE_ATTACKPREREQUISITES_H_TYPES
#include "Legacy/Psx.h"


#endif // VS_BATTLE_ATTACKPREREQUISITES_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_BATTLE_ATTACKPREREQUISITES_H_DECLS)
#define VS_BATTLE_ATTACKPREREQUISITES_H_DECLS
#include "Battle/Action.h"

extern int (*Battle_attackPrerequisiteFunctions[46])(Battle_ActionDefinition*, u_char*);
int Battle_IsStatusEffectBlocked(int debuff, u_char* arg1);
int Battle_TargetEnemyClass(u_char* arg0);
int Battle_RejectAttackPrerequisite(
    Battle_ActionDefinition* arg0 __attribute__((unused)), u_char* arg1 __attribute__((unused)));
int Battle_AcceptAttackPrerequisite(
    Battle_ActionDefinition* arg0 __attribute__((unused)), u_char* arg1 __attribute__((unused)));
int Battle_IsAttackTargetUndead(Battle_ActionDefinition* arg0 __attribute__((unused)), u_char* arg1);
int Battle_IsAttackTargetLiving(Battle_ActionDefinition* arg0 __attribute__((unused)), u_char* arg1);
int Battle_CanApplyStrengthDown(
    Battle_ActionDefinition* arg0 __attribute__((unused)), u_char* arg1);
int Battle_CanApplyIntelligenceDown(
    Battle_ActionDefinition* arg0 __attribute__((unused)), u_char* arg1);
int Battle_CanApplyAgilityDown(
    Battle_ActionDefinition* arg0 __attribute__((unused)), u_char* arg1);
int Battle_CanApplyEquipmentDown(Battle_ActionDefinition* arg0, u_char* arg1);
int Battle_CanApplyHalfMovement(
    Battle_ActionDefinition* arg0 __attribute__((unused)), u_char* arg1);
int Battle_CanApplyStrengthUp(Battle_ActionDefinition* arg0 __attribute__((unused)), u_char* arg1);
int Battle_CanApplyIntelligenceUp(Battle_ActionDefinition* arg0 __attribute__((unused)), u_char* arg1);
int Battle_CanApplyAgilityUp(Battle_ActionDefinition* arg0 __attribute__((unused)), u_char* arg1);
int Battle_CanApplyEquipmentUp(Battle_ActionDefinition* arg0, u_char* arg1);
int Battle_CanApplyQuicken(
    Battle_ActionDefinition* action __attribute__((unused)), u_char* arg1);
int Battle_CanApplySilence(
    Battle_ActionDefinition* action __attribute__((unused)), u_char* arg1);
int Battle_CanApplyMagicWard(
    Battle_ActionDefinition* action __attribute__((unused)), u_char* arg1);
int Battle_CanApplyRegeneration(
    Battle_ActionDefinition* action __attribute__((unused)), u_char* arg1);
int Battle_CanApplyParalysis(
    Battle_ActionDefinition* action __attribute__((unused)), u_char* arg1);
int Battle_CanApplyPoison(
    Battle_ActionDefinition* action __attribute__((unused)), u_char* arg1);
int Battle_CanApplyNumbness(
    Battle_ActionDefinition* action __attribute__((unused)), u_char* arg1);
int Battle_CanApplyCurse(
    Battle_ActionDefinition* action __attribute__((unused)), u_char* arg1);
int Battle_CanApplyAirAttackUp(
    Battle_ActionDefinition* action __attribute__((unused)), u_char* arg1);
int Battle_CanApplyFireAttackUp(
    Battle_ActionDefinition* action __attribute__((unused)), u_char* arg1);
int Battle_CanApplyEarthAttackUp(
    Battle_ActionDefinition* action __attribute__((unused)), u_char* arg1);
int Battle_CanApplyWaterAttackUp(
    Battle_ActionDefinition* action __attribute__((unused)), u_char* arg1);
int Battle_CanApplyAirDefenseUp(
    Battle_ActionDefinition* action __attribute__((unused)), u_char* arg1);
int Battle_CanApplyFireDefenseUp(
    Battle_ActionDefinition* action __attribute__((unused)), u_char* arg1);
int Battle_CanApplyEarthDefenseUp(
    Battle_ActionDefinition* action __attribute__((unused)), u_char* arg1);
int Battle_CanApplyWaterDefenseUp(
    Battle_ActionDefinition* action __attribute__((unused)), u_char* arg1);
int Battle_CanApplyBanish(
    Battle_ActionDefinition* action __attribute__((unused)), u_char* arg1);
int Battle_CanApplyExorcise(
    Battle_ActionDefinition* action __attribute__((unused)), u_char* arg1);
int Battle_CanApplyDrain(
    Battle_ActionDefinition* action __attribute__((unused)), u_char* arg1);
int Battle_IgnoreAttackPrerequisite(Battle_ActionDefinition* action __attribute__((unused)), u_char* arg1);
int Battle_CheckActionHitPrerequisite(Battle_ActionDefinition* action, u_char* arg1, int hitNumber);

#endif // VS_BATTLE_ATTACKPREREQUISITES_H_DECLS
