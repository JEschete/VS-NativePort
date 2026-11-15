#pragma once
// Hand-maintained (tools/port/owned.csv).

struct Effects_ActionEffectRequest;

// Live binding for the effect runtime and the primary effect's action
// lifecycle (EffectRuntime.hpp) over Runtime.cpp's globals, the current
// effect package (Effects_package), the loader state
// (EffectResourcesLiveState.h) and the effect cameras (CameraLiveState.h).
// The entries keep the original signatures for the battle, event and menu
// callers and the module's function pointers. A missing action request,
// package, package block or renderer is refused with a diagnostic, as is a
// track outside the pending mask; a renderer slot without a renderer is
// detached as finished.
namespace VS::Effects {
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
int LiveBeginActionEffect(Effects_ActionEffectRequest* request,
                          int action_variant, int variant_argument);
// func_800CF1A8 (5BF94.c:5462).
void LiveClearPrimaryEffects();
}  // namespace VS::Effects
