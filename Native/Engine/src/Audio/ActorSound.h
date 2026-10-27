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
void Audio_PlayRandomActorImpactSound(u_int* sound_bank, int pan, int volume);
void Audio_ActorSoundPanAndVolume(int actor_index, int* output_pan, int* output_volume);
void Audio_PlayActorModelSound(Actors_ModelInstance* model_instance, int sound_index);
void Audio_PlayActorModelSoundVariant(int actor_index, int variant_index);
void Audio_PlayActorSoundPair(Actors_ActorRecord* source_actor, int first_effect_id, int second_effect_id);
void Audio_PlayGimmickSoundPair(void* gimmick, int first_effect_id, int second_effect_id);
void Audio_PlayActorSurfaceContactSound(int actor_index, Actors_ActorRecord* actor, int contact_sound_code);

#endif // VS_AUDIO_ACTORSOUND_H_DECLS
