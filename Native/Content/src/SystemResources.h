// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_SYSTEMRESOURCES_H_TYPES
#define VS_SYSTEMRESOURCES_H_TYPES
#include "Legacy/Psx.h"

typedef struct Content_SystemDatHeader Content_SystemDatHeader;

// decomp: System_dat_h (src/BATTLE/INITBTL.PRG/18.c:23)
typedef struct Content_SystemDatHeader {
    size_t image0Offset;
    size_t image1Offset;
    size_t paletteOffset;
    size_t image2Offset;
    size_t image3Offset;
    size_t image4Offset;
    size_t fixedImageOffset;
    size_t interfaceDataOffset;
} Content_SystemDatHeader;

#endif // VS_SYSTEMRESOURCES_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_SYSTEMRESOURCES_H_DECLS)
#define VS_SYSTEMRESOURCES_H_DECLS

void Content_LoadSystemResources(void);

#endif // VS_SYSTEMRESOURCES_H_DECLS
