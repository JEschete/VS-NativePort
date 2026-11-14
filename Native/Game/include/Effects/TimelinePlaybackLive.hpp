#pragma once
// Hand-maintained (tools/port/owned.csv).

struct Effects_EffectInstance;
struct Effects_TimelineTrack;

// Live binding for effect timeline playback (TimelinePlayback.hpp): the load
// reads the package's timeline section into a game heap timeline, and each
// tick reads the track's current key, the instance's timeline, endpoints and
// transforms and the package's action request, and performs the curve,
// particle, audio, renderer, controller, camera or child effect call. The
// entries keep the original signatures for the effect script and module
// pointers. A missing instance, timeline, track, current key, package or
// action request is refused with a diagnostic, as are a transform past the
// instance's sixteen, a vibration pattern outside the package and action
// targets past the request's sixteen.
namespace VS::Effects {
// func_800D55A4 (5BF94.c:7885): replaces the instance's timeline with the
// package's timeline `timeline_index` and starts its tracks. The old timeline
// stays when the package lacks the section (refused); `unused` is ignored, as
// in the original.
void LiveLoadEffectTimeline(Effects_EffectInstance* instance,
                            int timeline_index, int unused);
// The track ticks return 1 while the track runs and 0 once it has no more
// keys, or (refused) without an instance, timeline, track or current key.
// func_800D57FC (5BF94.c:7950).
int LiveTickParticleEmissionTrack(Effects_EffectInstance* instance,
                                  Effects_TimelineTrack* track);
// func_800D5904 (5BF94.c:7974).
int LiveTickEffectSoundTrack(Effects_EffectInstance* instance,
                             Effects_TimelineTrack* track);
// func_800D5A98 (5BF94.c:8005): the track's key parameter holds its delay
// (low half) and next action target (high half); children run the program
// at `child_script_offset`. Refused without the package's action request.
int LiveTickChildEffectTrack(Effects_EffectInstance* instance,
                             Effects_TimelineTrack* track,
                             int child_script_offset);
// func_800D5D74 (5BF94.c:8069).
int LiveTickEffectRendererTrack(Effects_EffectInstance* instance,
                                Effects_TimelineTrack* track);
// func_800D5E00 (5BF94.c:8081): refused without the package's action request.
int LiveTickControllerVibrationTrack(Effects_EffectInstance* instance,
                                     Effects_TimelineTrack* track);
// func_800D6A18 (5BF94.c:8422).
int LiveTickCameraShakeTrack(Effects_EffectInstance* instance,
                             Effects_TimelineTrack* track);
// func_800D6AEC (5BF94.c:8442): ticks the instance's active tracks and
// advances its timeline frame; refused without an instance or timeline.
void LiveTickEffectTimeline(Effects_EffectInstance* instance,
                            int child_script_offset);
}  // namespace VS::Effects
