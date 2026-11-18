// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#pragma once
#include "Legacy/Psx.h"

struct Render_LegacyImageSize;

// decomp: imgHeader_t (src/SLUS_010.40/main.c:134)
struct Render_LegacyImageSize {
    char widthLow;
    char widthHigh;
    char heightLow;
    char heightHigh;
};
static_assert(sizeof(Render_LegacyImageSize) == 0x4, "Render_LegacyImageSize must keep its PS1 layout");

extern u_long Render_loadingScreenPixels[1260];
extern Render_LegacyImageSize Render_legacyLoadingImageHeader;
