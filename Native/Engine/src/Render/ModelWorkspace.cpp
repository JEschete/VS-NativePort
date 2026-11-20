#include "Render/ModelWorkspace.hpp"

#include "Render/ModelDraw.hpp"

// decomp: BATTLE 44F14.c and 49268.c model workspace; native storage replaces
// the 0x1F80035C slot.
static VS::Render::ModelScratchpad modelWorkspace;

VS::Render::ModelScratchpad* VS::Render::ModelWorkspace(void) {
  return &modelWorkspace;
}
