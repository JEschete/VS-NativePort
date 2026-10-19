#include "Render/ModelWorkspace.h"

#include "Render/ModelDraw.h"

// decomp: BATTLE 44F14.c and 49268.c model workspace; native storage replaces
// the 0x1F80035C slot.
static Render_ModelScratchpad modelWorkspace;

Render_ModelScratchpad* Render_ModelWorkspace(void) { return &modelWorkspace; }
