// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_RENDER_LOADING_H_TYPES
#define VS_RENDER_LOADING_H_TYPES
#include "Legacy/Psx.h"

typedef struct Render_LegacyImageSize Render_LegacyImageSize;

// decomp: imgHeader_t (src/SLUS_010.40/main.c:134)
typedef struct Render_LegacyImageSize {
    char widthLow;
    char widthHigh;
    char heightLow;
    char heightHigh;
} Render_LegacyImageSize;

#endif // VS_RENDER_LOADING_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_RENDER_LOADING_H_DECLS)
#define VS_RENDER_LOADING_H_DECLS

extern u_long Render_loadingScreenPixels[];
extern Render_LegacyImageSize Render_legacyLoadingImageHeader;

#endif // VS_RENDER_LOADING_H_DECLS
