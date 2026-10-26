// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_RENDER_MODEL_H_TYPES
#define VS_RENDER_MODEL_H_TYPES
#include "Legacy/Psx.h"


#endif // VS_RENDER_MODEL_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_RENDER_MODEL_H_DECLS)
#define VS_RENDER_MODEL_H_DECLS

void Render_PrepareActorCameraMatrices(MATRIX* source);
int Render_DrawActorGroundShadow();

#endif // VS_RENDER_MODEL_H_DECLS
