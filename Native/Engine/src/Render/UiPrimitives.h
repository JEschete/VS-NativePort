// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#pragma once
#include "Legacy/Psx.h"


// decomp: vs_scratch (vs_scratch@src/include/scratch.h): the start of scratch memory
#define Render_frameScratch (*(Render_FrameScratch*)Core_ScratchBase())

extern int Render_kStatGaugeColors[4];
