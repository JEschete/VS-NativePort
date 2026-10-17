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

void Effects_StartWeaponTrail(int arg0, int arg1);
void Effects_FadeWeaponTrails(int arg0, int arg1);
void Effects_QueueActionHitEffect(int arg0, Battle_HitResult* arg1, int targetLimb);
int Effects_QueueActionImpactEffects(int arg0);
void Effects_SignalActorEffectAnimation(int arg0);
void Effects_TintActionActorRed(int arg0, int unused __attribute__((unused)));
void Effects_RestoreActionActorTint(void);
void Effects_SpawnGroundPlaneImpactEffect(u_short* arg0);
void Effects_SpawnTrapTileEffect(Actors_ActorLocation* arg0);
void Effects_SpawnAbilityLearnedEffect(void);
void Effects_BeginAshleyStandaloneEffect(int arg0);
void Effects_EndAshleyStandaloneEffect(void);
int Effects_ActionEffectBusyFlags(void);
void Effects_StartScriptedActorEffect(int arg0, int arg1);
void Effects_ReleaseScriptedEffectTrigger(int arg0);
int Effects_ScriptedEffectPlaybackState(void);
int Effects_IsScriptedEffectTriggerReady(void);

#endif // VS_EFFECTS_ACTIONEFFECTS_H_DECLS
