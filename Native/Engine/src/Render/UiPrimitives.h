// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_RENDER_UIPRIMITIVES_H_TYPES
#define VS_RENDER_UIPRIMITIVES_H_TYPES
#include "Legacy/Psx.h"


// decomp: vs_scratch (vs_scratch@src/include/scratch.h): the start of scratch memory
#define Render_frameScratch (*(Render_FrameScratch*)Core_ScratchBase())

#endif // VS_RENDER_UIPRIMITIVES_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_RENDER_UIPRIMITIVES_H_DECLS)
#define VS_RENDER_UIPRIMITIVES_H_DECLS

extern int Render_kStatGaugeColors[4];

#endif // VS_RENDER_UIPRIMITIVES_H_DECLS
