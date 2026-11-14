// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_EFFECTS_TIMELINE_H_TYPES
#define VS_EFFECTS_TIMELINE_H_TYPES
#include "Legacy/Psx.h"


#endif // VS_EFFECTS_TIMELINE_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_EFFECTS_TIMELINE_H_DECLS)
#define VS_EFFECTS_TIMELINE_H_DECLS
#include "Effects/Particles.h"
#include "Effects/Runtime.h"
#include "Effects/Transforms.h"

void Effects_LoadEffectTimeline(Effects_EffectInstance* effect_instance, int timeline_index, int arg2);
int Effects_TickParticleEmissionTrack(Effects_EffectInstance* effect_instance, Effects_TimelineTrack* timeline_track);
int Effects_TickEffectSoundTrack(Effects_EffectInstance* effect_instance, Effects_TimelineTrack* timeline_track);
int Effects_TickChildEffectTrack(Effects_EffectInstance* effect_instance, Effects_ChildTrackView* child_track, int child_script_offset);
int Effects_TickEffectRendererTrack(Effects_EffectInstance* effect_instance, Effects_TimelineTrack* timeline_track);
int Effects_TickControllerVibrationTrack(Effects_EffectInstance* actor, Effects_TimelineTrack* event);
int Effects_TickCameraShakeTrack(Effects_EffectInstance* effect_instance, Effects_TimelineTrack* timeline_track);
void Effects_TickEffectTimeline(Effects_EffectInstance* effect_instance, int child_script_offset);

#endif // VS_EFFECTS_TIMELINE_H_DECLS
