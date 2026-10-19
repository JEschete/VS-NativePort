#pragma once
// Hand-maintained (tools/port/owned.csv).
#ifndef VS_OWNED_ACTORS_SEQUENCEPLAYBACK_H
#define VS_OWNED_ACTORS_SEQUENCEPLAYBACK_H
#include "Core/Gte.h"

typedef struct Actors_ActorRecord Actors_ActorRecord;
typedef struct Actors_AnimationPose Actors_AnimationPose;

typedef struct Actors_AnimationTrackContext {
  const unsigned char* data;
  int firstFrame;
  int lastFrame;
} Actors_AnimationTrackContext;

#ifdef __cplusplus
extern "C" {
#endif

void Actors_AdvanceSkeletalAnimation(Actors_ActorRecord* actor,
                                     Actors_AnimationPose* state, int mode);

void Actors_DecodeAnimationTrack(const Actors_AnimationTrackContext* context,
                                 int offset, SVECTOR* value, SVECTOR* delta,
                                 int rotation);

void Actors_BlendActorPose(Actors_AnimationPose* dst, Actors_AnimationPose* src,
                           int count, int frame, int frames);

#ifdef __cplusplus
}
#endif

#endif  // VS_OWNED_ACTORS_SEQUENCEPLAYBACK_H
