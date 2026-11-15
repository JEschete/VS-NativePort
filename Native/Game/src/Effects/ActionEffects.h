// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_EFFECTS_ACTIONEFFECTS_H_TYPES
#define VS_EFFECTS_ACTIONEFFECTS_H_TYPES
#include "Legacy/Psx.h"


#endif // VS_EFFECTS_ACTIONEFFECTS_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_EFFECTS_ACTIONEFFECTS_H_DECLS)
#define VS_EFFECTS_ACTIONEFFECTS_H_DECLS

void Effects_StartScriptedActorEffect(int effect_id, int target_actor_index);
void Effects_ReleaseScriptedEffectTrigger(int track_index);
int Effects_ScriptedEffectPlaybackState(void);
int Effects_IsScriptedEffectTriggerReady(void);

#endif // VS_EFFECTS_ACTIONEFFECTS_H_DECLS
