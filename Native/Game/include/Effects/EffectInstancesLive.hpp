#pragma once
// Hand-maintained (tools/port/owned.csv).

#include "Effects/EffectInstances.hpp"

// Live binding for the effect instance lifecycle (EffectInstances.hpp): the
// chains are Effects_primaryInstances and Effects_secondaryInstances, an
// instance's renderers its context.renderer, and the records live in the game
// heap. The entries keep the original signatures for the effect runtime,
// script and module pointers. A missing spawn request, endpoint, instance,
// renderer or slot is refused with a diagnostic, as are a phase the runtime
// never sets and the detach of a slot the instance does not hold.
namespace VS::Effects {
struct LiveEffectInstance;
struct LiveEffectSpawnRequest;
struct LiveRendererSlot;

// func_800CE714 (5BF94.c:5074).
void LiveInitEffectInstance(LiveEffectSpawnRequest* spawn_request,
                            LiveEffectInstance* instance);
// func_800CE83C (5BF94.c:5114) and func_800CF55C (5BF94.c:5593): a new
// instance starts zeroed before its spawn is applied, where the original kept
// the heap's bytes in its busy bits, effect id and phase. Null when refused
// or the heap has no room.
LiveEffectInstance* LiveCreatePrimaryEffectInstance(
    LiveEffectSpawnRequest* spawn_request);
LiveEffectInstance* LiveCreateSecondaryEffectInstance(
    LiveEffectSpawnRequest* spawn_request);
// func_800CE8F4 (5BF94.c:5139).
void LiveDestroyPrimaryEffectInstance(LiveEffectInstance* instance);
// func_800CF614 (5BF94.c:5617).
void LiveDestroySecondaryEffectInstance(LiveEffectInstance* instance);
// func_800CF484 (5BF94.c:5548).
void LiveSetSecondaryEffectPhase(int phase, LiveEffectInstance* instance);
// func_800CF694 (5BF94.c:5636): null when refused or the heap has no room.
LiveRendererSlot* LiveAttachEffectRenderer(LiveEffectInstance* instance,
                                           EffectRenderer renderer,
                                           int renderer_parameters);
// func_800CF70C (5BF94.c:5647).
void LiveDetachEffectRenderer(LiveEffectInstance* instance,
                              LiveRendererSlot* slot);
// Sends the slot's renderer RendererCommand::kUpdate; false once it has
// finished. A slot without a renderer is refused and counts as finished.
[[nodiscard]] bool LiveUpdateEffectRenderer(LiveRendererSlot& slot);
// func_800D4910 (5BF94.c:7582): UpdateEffectRenderers over the instance's
// renderers.
void LiveUpdateEffectRenderers(LiveEffectInstance* instance);
}  // namespace VS::Effects
