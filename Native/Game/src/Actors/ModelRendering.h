// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_ACTORS_MODELRENDERING_H_TYPES
#define VS_ACTORS_MODELRENDERING_H_TYPES
#include "Legacy/Psx.h"


#endif // VS_ACTORS_MODELRENDERING_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_ACTORS_MODELRENDERING_H_DECLS)
#define VS_ACTORS_MODELRENDERING_H_DECLS
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
#include "Render/Palette.h"

extern int Actors_renderPassCounter;
extern Actors_ModelRenderContext Actors_modelRenderContext;
void Actors_PrepareActorRootTransform(Actors_ActorRecord* actor);
void Actors_UpdateAndDrawRoomActors(MATRIX* camera);
void Actors_TransformActorRootOffset(Actors_ActorRecord* actor);
void Actors_DrawGimmickModel(Field_GimmickInstance* object, MATRIX* camera);
void Actors_DrawActorWeaponModel(Actors_WeaponModel* weapon, MATRIX* source);
void Actors_DrawActorModel(void*, MATRIX*, char);
void Actors_InitializeActorModelResources(void);

#endif // VS_ACTORS_MODELRENDERING_H_DECLS
