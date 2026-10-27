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
void Effects_InitializeParticleTimelineKey(Effects_TimelineTrack* timeline_track);
void Effects_InitializeShakeTimelineKey(Effects_TimelineTrack* timeline_track);
int Effects_AdvanceEffectTimelineKey(Effects_TimelineTrack* timeline_track);
int Effects_TickParticleEmissionTrack(Effects_EffectInstance* effect_instance, Effects_TimelineTrack* timeline_track);
int Effects_TickEffectSoundTrack(Effects_EffectInstance* effect_instance, Effects_TimelineTrack* timeline_track);
int Effects_TickChildEffectTrack(Effects_EffectInstance* effect_instance, Effects_ChildTrackView* child_track, int child_script_offset);
int Effects_TickEffectRendererTrack(Effects_EffectInstance* effect_instance, Effects_TimelineTrack* timeline_track);
int Effects_TickControllerVibrationTrack(Effects_EffectInstance* actor, Effects_TimelineTrack* event);
int Effects_TickSourceActorColorTrack(Effects_EffectInstance* effect_instance, Effects_TimelineTrack* timeline_track);
int Effects_TickTargetActorColorTrack(Effects_EffectInstance* effect_instance, Effects_TimelineTrack* timeline_track, int apply_to_all_targets);
int Effects_TickOtherActorColorTrack(Effects_EffectInstance* effect_instance, Effects_TimelineTrack* timeline_track);
int Effects_TickSceneColorTrack(Effects_EffectInstance* effect_instance, Effects_TimelineTrack* timeline_track);
int Effects_TickBackgroundColorTrack(Effects_EffectInstance* effect_instance, Effects_TimelineTrack* timeline_track);
void Effects_ApplyTargetActionImpact(Effects_EffectInstance* effect_instance);
void Effects_ApplySourceActionImpact(Effects_EffectInstance* effect_instance);
void Effects_ActivateTargetRoomTile(Effects_EffectInstance* effect_instance);
void Effects_SpawnTransformWeaponTrail(Effects_EffectInstance* effect_instance, u_char transform_index, u_char packed_trail_style);
void Effects_FadeActionWeaponTrails(void);
void Effects_ResumeEffectTransformMotion(Effects_TransformControlView* transform_controls, char transform_index);
void Effects_HoldEffectTimelineFrame(Effects_EffectInstance* effect_instance);
void Effects_PauseMovingRoomObjects(void);
void Effects_SignalSourceActorFromTimeline(Effects_EffectInstance* effect_instance);
void Effects_InvalidateEffectTransformAttachments(Effects_TransformControlView* transform_controls, char transform_index);
void Effects_SetSourceWeaponBrightness(Effects_EffectInstance* effect_instance, u_char brightness_override);
void Effects_PlaySourceActorReactionSound(Effects_EffectInstance* effect_instance, u_char reaction_sound_variant);
void Effects_ActivateTargetGimmick(Effects_EffectInstance* effect_instance);
void Effects_PrepareAshleyTargetImpact(Effects_EffectInstance* effect_instance);
int Effects_TickActionControlTrack(Effects_EffectInstance* effect_instance, Effects_TimelineTrack* timeline_track);
int Effects_TickCameraShakeTrack(Effects_EffectInstance* effect_instance, Effects_TimelineTrack* timeline_track);
void Effects_TickEffectTimeline(Effects_EffectInstance* effect_instance, int child_script_offset);

#endif // VS_EFFECTS_TIMELINE_H_DECLS
