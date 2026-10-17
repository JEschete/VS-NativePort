// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_SYSTEMRESOURCES_H_TYPES
#define VS_SYSTEMRESOURCES_H_TYPES
#include "Legacy/Psx.h"

typedef struct Content_SystemDatHeader Content_SystemDatHeader;

// decomp: System_dat_h (src/BATTLE/INITBTL.PRG/18.c:23)
typedef struct Content_SystemDatHeader {
    u_int image0Offset;
    u_int image1Offset;
    u_int paletteOffset;
    u_int image2Offset;
    u_int image3Offset;
    u_int image4Offset;
    u_int fixedImageOffset;
    u_int interfaceDataOffset;
} Content_SystemDatHeader;
_Static_assert(sizeof(Content_SystemDatHeader) == 0x20, "Content_SystemDatHeader must keep its PS1 layout");

#endif // VS_SYSTEMRESOURCES_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_SYSTEMRESOURCES_H_DECLS)
#define VS_SYSTEMRESOURCES_H_DECLS

void Content_LoadSystemResources(void);

#endif // VS_SYSTEMRESOURCES_H_DECLS
