#pragma once
// Hand-maintained (tools/port/owned.csv).

// Live binding for the effect runtime and the primary effect's action
// lifecycle (EffectRuntime.hpp) over Runtime.cpp's globals, the current
// effect package (Effects_package), the loader state
// (EffectResourcesLiveState.hpp) and the effect cameras (CameraLiveState.hpp).
// The entries keep the original signatures for the battle, event and menu
// callers and the module's function pointers. A missing action request,
// package, package block or renderer is refused with a diagnostic, as is a
// track outside the pending mask; a renderer slot without a renderer is
// detached as finished. A secondary request with more targets than it holds
// spawns only those it holds, and a refused secondary instance is skipped.
namespace VS::Effects {
struct LiveActionEffectRequest;

// func_800CE64C (5BF94.c:5047).
void LiveInitEffectRuntime();
// func_800CE67C (5BF94.c:5054).
void LiveResetEffectInstances();
// func_800CE9B0 (5BF94.c:5170): 1 while the primary effect is busy.
int LiveUpdatePrimaryEffects();
// func_800CED60 (5BF94.c:5321): the secondary instances' busy bits.
int LiveUpdateSecondaryEffects();
// func_800CEF74 (5BF94.c:5399): 0 when no track can start, else 1.
int LiveStartEffectTrack(int track);
// func_800CF060 (5BF94.c:5427).
int LiveUpdateEffectRuntime();
// func_800CF0E8 (5BF94.c:5444): the action's effect token; 0 when refused.
// The third argument only reached the write-only D_800F54A9 and is unused.
int LiveBeginActionEffect(LiveActionEffectRequest* request, int action_variant,
                          int variant_argument);
// func_800CF1A8 (5BF94.c:5462).
void LiveClearPrimaryEffects();
// func_800CF218 (5BF94.c:5479): the secondary instances' token; 0 when the
// request or package is missing. The second argument is unused.
int LiveSpawnSecondaryEffects(LiveActionEffectRequest* request, int unused,
                              int parameter);
// func_800CF3F8 (5BF94.c:5532): the secondary instances' token.
int LiveBeginSecondaryEffects(LiveActionEffectRequest* request, int parameter);
// func_800CF458 (5BF94.c:5541): LiveSpawnSecondaryEffects under its other
// entry.
int LiveSpawnSecondaryEffectRequest(LiveActionEffectRequest* request,
                                    int unused, int parameter);
// func_800CF8BC (5BF94.c:5697): effect 0x38 on the acting actor.
void LiveSpawnSourceActorEffect();
// func_800CF920 (5BF94.c:5712).
void LiveClearActionEffectMode();
}  // namespace VS::Effects
