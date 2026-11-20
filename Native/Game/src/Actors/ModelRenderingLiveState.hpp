#pragma once
// Hand-maintained (tools/port/owned.csv).
// The model render context as the copied code reads it. ModelRenderingLive.cpp
// defines it.

#include "Actors/ModelDraw.hpp"
#include "Legacy/Psx.hpp"

namespace VS::Actors {

// decomp: D_800F2458 (BATTLE/BATTLE.PRG 30DB0.c): the projected vertices of
// the model being drawn and the model scale.
extern ModelRenderContext modelRenderContext;

}  // namespace VS::Actors
