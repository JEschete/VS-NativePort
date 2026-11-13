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
void Battle_SetEnemyCombatPose(int actor_slot, int pose_mode);
void Battle_PlayActorDamageReaction(int actor_slot, SVECTOR* source_position);
void Battle_PlayDefenseAbilityAnimation(int actor_slot, SVECTOR* target_position, int arg2);
void Battle_FinishEnemyCombatPose(int actor_slot, short facing);

#endif // VS_BATTLE_ACTIONANIMATION_H_DECLS
