#pragma once
// Hand-maintained (tools/port/owned.csv).

#include "Effects/ScriptedEffectTriggers.hpp"

// Live binding for the scripted actor effect trigger lifecycle
// (ScriptedEffectTriggers.hpp) over Battle_actors, LiveBeginActionEffect and
// the effect runtime's track and phase services. The entries keep the
// original signatures for the event script's effect commands and the
// module's function pointers.
namespace VS::Effects {
// func_8007BE20 (146C.c:8581): the effect id's low halfword.
void LiveStartScriptedActorEffect(int effect_id, int target_actor_index);
// func_8007BEC4 (146C.c:8611).
void LiveReleaseScriptedEffectTrigger(int track_index);
// func_8007BEE4 (146C.c:8613).
int LiveScriptedEffectPlaybackState();
// func_8007BF0C (146C.c:8617): 1 when a trigger track can start, else 0.
int LiveIsScriptedEffectTriggerReady();
}  // namespace VS::Effects
