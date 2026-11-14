#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstdint>

struct Effects_EffectInstance;

// Live binding for the effect timeline action-control commands
// (TimelineCommands.hpp): each reads the instance's endpoints and performs the
// selected battle, field, actor or audio call. The entries keep the original
// signatures for the generated action-control track; a null instance is
// refused with a diagnostic.
namespace VS::Effects {
// func_800D6388 (5BF94.c:8213): the battle action's target effects.
void LiveApplyTargetActionImpact(Effects_EffectInstance* instance);
// func_800D63D0 (5BF94.c:8223): the battle action's source effects.
void LiveApplySourceActionImpact(Effects_EffectInstance* instance);
// func_800D6418 (5BF94.c:8233): disables the trap on the target's room tile.
void LiveActivateTargetRoomTile(Effects_EffectInstance* instance);
// func_800D6574 (5BF94.c:8259): signals the source actor's effect animation.
void LiveSignalSourceActorFromTimeline(Effects_EffectInstance* instance);
// func_800D65D8 (5BF94.c:8272): the emitter's weapon brightness override.
void LiveSetSourceWeaponBrightness(Effects_EffectInstance* instance,
                                   std::uint8_t brightness);
// func_800D6628 (5BF94.c:8285): the emitter's reaction sound, drawing random
// variants from the BIOS rand().
void LivePlaySourceActorReactionSound(Effects_EffectInstance* instance,
                                      std::uint8_t variant);
// func_800D66CC (5BF94.c:8301): unlocks the target's room object.
void LiveActivateTargetGimmick(Effects_EffectInstance* instance);
// func_800D6860 (5BF94.c:8335): opens the receiver's defense ability timing
// window.
void LivePrepareTargetDefenseTiming(Effects_EffectInstance* instance);
}  // namespace VS::Effects
