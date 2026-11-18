// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_RENDER_IMAGES_H_TYPES
#define VS_RENDER_IMAGES_H_TYPES
#include "Legacy/Psx.h"

struct Render_TextureRegion;

// decomp: _texture_t (src/include/texture_t.h:12)
struct Render_TextureRegion {
    char x;
    char y;
    char w;
    char h;
    u_short tpage;
    u_short clut;
};
static_assert(sizeof(Render_TextureRegion) == 0x8, "Render_TextureRegion must keep its PS1 layout");

#endif // VS_RENDER_IMAGES_H_TYPES
