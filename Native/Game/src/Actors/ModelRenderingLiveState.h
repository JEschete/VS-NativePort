#pragma once
// Hand-maintained (tools/port/owned.csv).
// The room pass counter and the model render context as the copied code reads
// them. ModelRenderingLive.cpp defines both.

#include "Actors/ModelDraw.h"
#include "Legacy/Psx.h"

#ifdef __cplusplus
extern "C" {
#endif

// decomp: D_800E9308 (BATTLE/BATTLE.PRG 44F14.c): the room passes drawn; the
// pass is its only writer.
extern int Actors_renderPassCounter;
// decomp: D_800F2458 (BATTLE/BATTLE.PRG 30DB0.c): the projected vertices of
// the model being drawn and the model scale.
extern Actors_ModelRenderContext Actors_modelRenderContext;

#ifdef __cplusplus
}
#endif
