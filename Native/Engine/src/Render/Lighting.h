// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_RENDER_LIGHTING_H_TYPES
#define VS_RENDER_LIGHTING_H_TYPES
#include "Legacy/Psx.h"


// decomp: SLUS_010.40 DpqColor (src/SLUS_010.40/32154.s)
// Native replacement: shared SDK implementation, not a second legacy body.
#define Render_ApplyDepthCueColor Core_Gte_DpqColor

// decomp: SLUS_010.40 SetFogNear (src/SLUS_010.40/32154.s)
// Native replacement: shared SDK implementation, not a second legacy body.
#define Render_SetFogNearDistance Core_Gte_SetFogNear

#endif // VS_RENDER_LIGHTING_H_TYPES
