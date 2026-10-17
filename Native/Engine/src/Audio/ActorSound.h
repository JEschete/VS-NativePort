// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_AUDIO_ACTORSOUND_H_TYPES
#define VS_AUDIO_ACTORSOUND_H_TYPES
#include "Legacy/Psx.h"


#endif // VS_AUDIO_ACTORSOUND_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_AUDIO_ACTORSOUND_H_DECLS)
#define VS_AUDIO_ACTORSOUND_H_DECLS
#include "Actors/ActorRecord.h"
#include "Actors/Animation.h"
#include "Actors/ModelInstance.h"
#include "Actors/Movement.h"
#include "Actors/Shadow.h"
#include "Actors/Skeleton.h"
#include "Actors/TextureAnimation.h"
#include "Render/Palette.h"

extern u_char Audio_objectSurfaceSoundIds[2956];
void Audio_PlayRandomActorImpactSound(u_int* arg0, int arg1, int arg2);
void Audio_ActorSoundPanAndVolume(int arg0, int* arg1, int* arg2);
int Audio_UpdateActorSoundTransition();
void Audio_PlayActorModelSound(Actors_ModelInstance* arg0, int arg1);
void Audio_PlayActorModelSoundVariant(int arg0, int arg1);
void Audio_PlayActorSoundPair(Actors_ActorRecord* arg0, int arg1, int arg2);
void Audio_PlayGimmickSoundPair(void* arg0, int arg1, int arg2);
void Audio_PlayActorSurfaceContactSound(int arg0, Actors_ActorRecord* arg1, int arg2);

#endif // VS_AUDIO_ACTORSOUND_H_DECLS
