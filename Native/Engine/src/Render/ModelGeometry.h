#pragma once
// Hand-maintained (tools/port/owned.csv).
#ifndef VS_OWNED_RENDER_MODELGEOMETRY_H
#define VS_OWNED_RENDER_MODELGEOMETRY_H
#include "Actors/ActorRecord.h"
#include "Actors/Animation.h"
#include "Actors/ModelDraw.h"
#include "Actors/ModelInstance.h"
#include "Actors/Shadow.h"
#include "Actors/Skeleton.h"
#include "Actors/WeaponModel.h"
#include "Field/Gimmicks.h"
#include "Legacy/Psx.h"
#include "Render/Palette.h"

#ifdef __cplusplus
extern "C" {
#endif

int Render_ProjectModelVertices(void* instance, MATRIX* matrices,
                                int near_offset, void* model_data);
int Render_ValidateModelData(void* instance, const void* data, size_t size);
void Render_DrawModelFlatPolygons(Actors_ModelRenderContext* context,
                                  void* instance, CVECTOR* tint,
                                  void* model_data);
void Render_DrawModelGouraudPolygons(Actors_ModelRenderContext* context,
                                     void* instance, void* tint,
                                     void* model_data);
void Render_DrawModelGouraudPolygonsWithOverrides(
    Actors_ModelRenderContext* context, Field_GimmickInstance* object,
    int* tint, void* model_data);
void Render_DrawActorPolygons(Actors_ModelRenderContext* context,
                              Actors_ModelInstance* instance,
                              const CVECTOR* tint, Actors_ShpModel* model);

#ifdef __cplusplus
}
#endif

#endif  // VS_OWNED_RENDER_MODELGEOMETRY_H
