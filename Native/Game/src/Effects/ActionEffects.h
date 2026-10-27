// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_EFFECTS_ACTIONEFFECTS_H_TYPES
#define VS_EFFECTS_ACTIONEFFECTS_H_TYPES
#include "Legacy/Psx.h"


#endif // VS_EFFECTS_ACTIONEFFECTS_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_EFFECTS_ACTIONEFFECTS_H_DECLS)
#define VS_EFFECTS_ACTIONEFFECTS_H_DECLS
#include "Actors/Location.h"
#include "Battle/ActionResolution.h"
#include "Battle/BattleTypes.h"
#include "Battle/Hit.h"

void Effects_StartWeaponTrail(int actor_index, int bone_index);
void Effects_FadeWeaponTrails(int arg0, int arg1);
void Effects_QueueActionHitEffect(int effect_id, Battle_HitResult* hit_result, int targetLimb);
int Effects_QueueActionImpactEffects(int attacker_index);
void Effects_SignalActorEffectAnimation(int actor_index);
void Effects_TintActionActorRed(int actor_index, int unused __attribute__((unused)));
void Effects_RestoreActionActorTint(void);
void Effects_SpawnGroundPlaneImpactEffect(u_short* world_position_components);
void Effects_SpawnTrapTileEffect(Actors_ActorLocation* actor_location);
void Effects_SpawnAbilityLearnedEffect(void);
void Effects_BeginAshleyStandaloneEffect(int effect_id);
void Effects_EndAshleyStandaloneEffect(void);
int Effects_ActionEffectBusyFlags(void);
void Effects_StartScriptedActorEffect(int effect_id, int target_actor_index);
void Effects_ReleaseScriptedEffectTrigger(int track_index);
int Effects_ScriptedEffectPlaybackState(void);
int Effects_IsScriptedEffectTriggerReady(void);

#endif // VS_EFFECTS_ACTIONEFFECTS_H_DECLS
