#pragma once
// Hand-maintained (tools/port/owned.csv).
#ifndef VS_OWNED_ACTORS_MODELDRAWING_H
#define VS_OWNED_ACTORS_MODELDRAWING_H
#include "Actors/ActorRecord.h"
#include "Actors/Animation.h"
#include "Actors/ModelDraw.h"
#include "Actors/ModelInstance.h"
#include "Actors/Movement.h"
#include "Actors/Shadow.h"
#include "Actors/Skeleton.h"
#include "Actors/TextureAnimation.h"
#include "Actors/WeaponModel.h"
#include "Field/Gimmicks.h"
#include "Legacy/Psx.h"
#include "Render/Palette.h"

#ifdef __cplusplus
extern "C" {
#endif

int Actors_BuildActorModelPose(Actors_ActorRecord* actor, MATRIX* camera);
MATRIX* Actors_ResolveAttachmentParentMatrix(const Actors_ActorRecord* actor);
void Actors_DrawActorModel(void* actor_data, MATRIX* camera, char companion);
int Actors_ProjectActorBoneEndpoint(int actor_id, int bone, int* output,
                                    int endpoint);

#ifdef __cplusplus
}
#endif

#endif  // VS_OWNED_ACTORS_MODELDRAWING_H
