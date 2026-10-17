// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_BATTLE_ACTIONANIMATION_H_TYPES
#define VS_BATTLE_ACTIONANIMATION_H_TYPES
#include "Legacy/Psx.h"


#endif // VS_BATTLE_ACTIONANIMATION_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_BATTLE_ACTIONANIMATION_H_DECLS)
#define VS_BATTLE_ACTIONANIMATION_H_DECLS

extern u_char Battle_chainAbilityAnimationGroups[168];
extern int Battle_chainAbilityAnimationSide;
extern char Battle_specialActionPitch;
void Battle_PlayMeleeAttackAnimation(int arg0, SVECTOR* arg1, int arg2);
void Battle_PlayDefenseAnimation(int arg0, SVECTOR* arg1);
void Battle_SetEnemyCombatPose(int arg0, int arg1);
void Battle_PlaySpellCastAnimation(int arg0, SVECTOR* arg1, int arg2);
void Battle_PlayActorDamageReaction(int arg0, SVECTOR* arg1);
void Battle_PlayChainAbilityAnimation(int arg0, SVECTOR* arg1, int arg2, int arg3);
void Battle_PlayDefenseAbilityAnimation(int arg0, SVECTOR* arg1, int arg2);
void Battle_PlaySpecialActionAnimation(int actorId, SVECTOR* target, int mode);
void Battle_PlayBreakArtAnimation(int arg0, SVECTOR* arg1, int arg2);
void Battle_FinishEnemyCombatPose(int arg0, short arg1);

#endif // VS_BATTLE_ACTIONANIMATION_H_DECLS
