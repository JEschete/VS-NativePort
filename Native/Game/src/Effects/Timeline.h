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

void Effects_LoadEffectTimeline(Effects_EffectInstance* arg0, int arg1, int arg2);
void Effects_InitializeParticleTimelineKey(Effects_TimelineTrack* arg0);
void Effects_InitializeShakeTimelineKey(Effects_TimelineTrack* arg0);
int Effects_AdvanceEffectTimelineKey(Effects_TimelineTrack* arg0);
int Effects_TickParticleEmissionTrack(Effects_EffectInstance* arg0, Effects_TimelineTrack* arg1);
int Effects_TickEffectSoundTrack(Effects_EffectInstance* arg0, Effects_TimelineTrack* arg1);
int Effects_TickChildEffectTrack(Effects_EffectInstance* arg0, Effects_ChildTrackView* arg1, int arg2);
int Effects_TickEffectRendererTrack(Effects_EffectInstance* arg0, Effects_TimelineTrack* arg1);
int Effects_TickControllerVibrationTrack(Effects_EffectInstance* actor, Effects_TimelineTrack* event);
int Effects_TickSourceActorColorTrack(Effects_EffectInstance* arg0, Effects_TimelineTrack* arg1);
int Effects_TickTargetActorColorTrack(Effects_EffectInstance* arg0, Effects_TimelineTrack* arg1, int arg2);
int Effects_TickOtherActorColorTrack(Effects_EffectInstance* arg0, Effects_TimelineTrack* arg1);
int Effects_TickSceneColorTrack(Effects_EffectInstance* arg0, Effects_TimelineTrack* arg1);
int Effects_TickBackgroundColorTrack(Effects_EffectInstance* arg0, Effects_TimelineTrack* arg1);
void Effects_ApplyTargetActionImpact(Effects_EffectInstance* arg0);
void Effects_ApplySourceActionImpact(Effects_EffectInstance* arg0);
void Effects_ActivateTargetRoomTile(Effects_EffectInstance* arg0);
void Effects_SpawnTransformWeaponTrail(Effects_EffectInstance* arg0, u_char arg1, u_char arg2);
void Effects_FadeActionWeaponTrails(void);
void Effects_ResumeEffectTransformMotion(Effects_TransformControlView* arg0, char arg1);
void Effects_HoldEffectTimelineFrame(Effects_EffectInstance* arg0);
void Effects_PauseMovingRoomObjects(void);
void Effects_SignalSourceActorFromTimeline(Effects_EffectInstance* arg0);
void Effects_InvalidateEffectTransformAttachments(Effects_TransformControlView* arg0, char arg1);
void Effects_SetSourceWeaponBrightness(Effects_EffectInstance* arg0, u_char arg1);
void Effects_PlaySourceActorReactionSound(Effects_EffectInstance* arg0, u_char arg1);
void Effects_ActivateTargetGimmick(Effects_EffectInstance* arg0);
void Effects_PrepareAshleyTargetImpact(Effects_EffectInstance* arg0);
int Effects_TickActionControlTrack(Effects_EffectInstance* arg0, Effects_TimelineTrack* arg1);
int Effects_TickCameraShakeTrack(Effects_EffectInstance* arg0, Effects_TimelineTrack* arg1);
void Effects_TickEffectTimeline(Effects_EffectInstance* arg0, int arg1);

#endif // VS_EFFECTS_TIMELINE_H_DECLS
