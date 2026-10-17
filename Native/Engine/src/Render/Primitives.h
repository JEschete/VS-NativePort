// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_RENDER_PRIMITIVES_H_TYPES
#define VS_RENDER_PRIMITIVES_H_TYPES
#include "Legacy/Psx.h"

typedef struct Render_PolyFt4 Render_PolyFt4;

// decomp: VS_POLY_FT4 (src/include/gpu.h:93)
typedef struct Render_PolyFt4 {
    u_long colorCode;
    u_long xy0;
    u_long uv0Palette;
    u_long xy1;
    u_long uv1TexturePage;
    u_long xy2;
    u_long uv2;
    u_long xy3;
    u_long uv3;
} Render_PolyFt4;
_Static_assert(sizeof(Render_PolyFt4) == 0x24, "Render_PolyFt4 must keep its PS1 layout");

#endif // VS_RENDER_PRIMITIVES_H_TYPES
