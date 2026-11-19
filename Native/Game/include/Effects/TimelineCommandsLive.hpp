#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstdint>

#include "Effects/TimelineCommands.hpp"

// Live binding for the effect timeline action-control commands
// (TimelineCommands.hpp): each reads the instance's endpoints, transforms or
// timeline and performs the selected battle, field, actor, audio or trail
// call. The entries keep the original signatures for the generated timeline;
// a null instance or a transform past the instance's slots is refused with a
// diagnostic.
namespace VS::Effects {
struct LiveActionEffectTarget;
struct LiveEffectInstance;
struct LiveTimelineTrack;

// An action effect endpoint as the typed commands and tracks read it.
[[nodiscard]] CommandEndpoint CommandEndpointOf(
    const LiveActionEffectTarget& endpoint);

// func_800D6388 (5BF94.c:8213): the battle action's target effects.
void LiveApplyTargetActionImpact(LiveEffectInstance* instance);
// func_800D63D0 (5BF94.c:8223): the battle action's source effects.
void LiveApplySourceActionImpact(LiveEffectInstance* instance);
// func_800D6418 (5BF94.c:8233): disables the trap on the target's room tile.
void LiveActivateTargetRoomTile(LiveEffectInstance* instance);
// func_800D6574 (5BF94.c:8259): signals the source actor's effect animation.
void LiveSignalSourceActorFromTimeline(LiveEffectInstance* instance);
// func_800D65D8 (5BF94.c:8272): the emitter's weapon brightness override.
void LiveSetSourceWeaponBrightness(LiveEffectInstance* instance,
                                   std::uint8_t brightness);
// func_800D6628 (5BF94.c:8285): the emitter's reaction sound, drawing random
// variants from the BIOS rand().
void LivePlaySourceActorReactionSound(LiveEffectInstance* instance,
                                      std::uint8_t variant);
// func_800D66CC (5BF94.c:8301): unlocks the target's room object.
void LiveActivateTargetGimmick(LiveEffectInstance* instance);
// func_800D6860 (5BF94.c:8335): opens the receiver's defense ability timing
// window.
void LivePrepareTargetDefenseTiming(LiveEffectInstance* instance);
// func_800D6448 (5BF94.c:8240): spawns a weapon trail from the transform's
// attachment.
void LiveSpawnTransformWeaponTrail(LiveEffectInstance* instance,
                                   std::uint8_t transform_index,
                                   std::uint8_t packed_style);
// func_800D64E4 (5BF94.c:8247): fades every weapon trail.
void LiveFadeActionWeaponTrails();
// func_800D6508 (5BF94.c:8249): the transform's frame advances again.
void LiveResumeEffectTransformMotion(LiveEffectInstance* instance,
                                     std::uint8_t transform_index);
// func_800D6538 (5BF94.c:8255): the timeline stays on its frame.
void LiveHoldEffectTimelineFrame(LiveEffectInstance* instance);
// func_800D6554 (5BF94.c:8257): freezes the room's moving objects briefly.
void LivePauseMovingRoomObjects();
// func_800D65A8 (5BF94.c:8266): the transform resolves its attachment and
// target again.
void LiveInvalidateEffectTransformAttachments(LiveEffectInstance* instance,
                                              std::uint8_t transform_index);
// func_800D6894 (5BF94.c:8342): runs the track's key once it is due; 0 once
// the track has no more keys, or (refused) without an instance, timeline or
// current key.
int LiveTickActionControlTrack(LiveEffectInstance* instance,
                               LiveTimelineTrack* track);
}  // namespace VS::Effects
