#pragma once
// Hand-maintained (tools/port/owned.csv).

// Live binding for the effect timeline keys and colour tracks
// (TimelineTracks.hpp): each reads the track's current key and the instance's
// timeline and endpoints, and performs the actor, curve or palette call. The
// entries keep the original signatures for the generated timeline. A missing
// instance, track or timeline, or a key outside the track, is refused with a
// diagnostic; action targets and other actors past the request's sixteen are
// not read.
namespace VS::Effects {
struct LiveEffectInstance;
struct LiveTimelineTrack;

// func_800D5700 (5BF94.c:7920): the particle key's timer and parameter.
void LiveInitializeParticleTimelineKey(LiveTimelineTrack* track);
// func_800D5738 (5BF94.c:7927): the shake key's timer.
void LiveInitializeShakeTimelineKey(LiveTimelineTrack* track);
// func_800D5780 (5BF94.c:7933): 1 when the track moved to its next key; 0
// once it has no more, or (refused) without a track.
int LiveAdvanceEffectTimelineKey(LiveTimelineTrack* track);
// The colour tracks return 1 while the track runs and 0 once it has no more
// keys, or (refused) without an instance, track, timeline or current key.
// func_800D5F8C (5BF94.c:8128).
int LiveTickSourceActorColorTrack(LiveEffectInstance* instance,
                                  LiveTimelineTrack* track);
// func_800D6048 (5BF94.c:8144).
int LiveTickTargetActorColorTrack(LiveEffectInstance* instance,
                                  LiveTimelineTrack* track,
                                  int apply_to_all_targets);
// func_800D61AC (5BF94.c:8172).
int LiveTickOtherActorColorTrack(LiveEffectInstance* instance,
                                 LiveTimelineTrack* track);
// func_800D6298 (5BF94.c:8189).
int LiveTickSceneColorTrack(LiveEffectInstance* instance,
                            LiveTimelineTrack* track);
// func_800D6310 (5BF94.c:8201).
int LiveTickBackgroundColorTrack(LiveEffectInstance* instance,
                                 LiveTimelineTrack* track);
}  // namespace VS::Effects
