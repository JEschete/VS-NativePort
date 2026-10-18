// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_RENDER_MODEL_H_TYPES
#define VS_RENDER_MODEL_H_TYPES
#include "Legacy/Psx.h"


#endif // VS_RENDER_MODEL_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_RENDER_MODEL_H_DECLS)
#define VS_RENDER_MODEL_H_DECLS
#include "Actors/ActorRecord.h"
#include "Actors/Animation.h"
#include "Actors/ModelDraw.h"
#include "Actors/ModelInstance.h"
#include "Actors/Shadow.h"
#include "Actors/Skeleton.h"
#include "Actors/WeaponModel.h"
#include "Field/Gimmicks.h"
#include "Render/Palette.h"

void Render_PrepareActorCameraMatrices(MATRIX* source);
int Render_DrawActorGroundShadow();
void Render_DrawModelFlatPolygonsWithOverrides(Actors_ModelRenderContext*, Actors_WeaponModel*, CVECTOR*, void*);
void Render_DrawModelGouraudPolygonsWithOverrides(Actors_ModelRenderContext*, Field_GimmickInstance*, int*, void*);

#endif // VS_RENDER_MODEL_H_DECLS
