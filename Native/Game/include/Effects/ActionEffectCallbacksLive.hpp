#pragma once
// Hand-maintained (tools/port/owned.csv).

#include "Effects/ActionEffectCallbacks.hpp"

// Live binding for the standalone presentation and actor callbacks
// (ActionEffectCallbacks.hpp) over Effects_tintedActor, Field_controlMode,
// VS::State::tickSpeed, Effects_effectBusyFlags, the actor animation and
// palette services, the HUD, the minimap, the field mode and
// LiveBeginActionEffect. The entries keep the original signatures for the
// generated callers and the module's function pointers.
namespace VS::Effects {
// func_800CF0E8 for a typed request: the action's effect token, 0 when
// refused. Fields the request does not define are zero.
int BeginLivePrimaryEffectRequest(const PrimaryEffectRequest& request,
                                  int action_variant, int variant_argument);

// func_8006CDB8 (146C.c:2149).
void LiveSignalActorEffectAnimation(int actor_index);
// func_8006D028 (146C.c:2211): the second argument is unused.
void LiveTintActionActorRed(int actor_index, int unused);
// func_8006D064 (146C.c:2217).
void LiveRestoreActionActorTint();
// func_800733F8 (146C.c:4613): the effect id's low halfword.
void LiveBeginAshleyStandaloneEffect(int effect_id);
// func_80073484 (146C.c:4638).
void LiveEndAshleyStandaloneEffect();
// func_80074294 (146C.c:5064): the primary and secondary busy bits.
int LiveActionEffectBusyFlags();
}  // namespace VS::Effects
