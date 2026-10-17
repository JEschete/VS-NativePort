// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_FIELD_TRIGGERMARKERS_H_TYPES
#define VS_FIELD_TRIGGERMARKERS_H_TYPES
#include "Legacy/Psx.h"

typedef struct Field_TriggerMarkerVertex Field_TriggerMarkerVertex;

// decomp: D_800E8594_t (src/BATTLE/BATTLE.PRG/146C.c:197)
typedef struct Field_TriggerMarkerVertex {
    short positionX;
    short height;
    short positionZ;
    short reserved06;
    int reserved08;
    char vertexShades[3][4];
    int reserved18;
    int reserved1C;
    int reserved20;
    char lastVertexShade[4];
} Field_TriggerMarkerVertex;

#endif // VS_FIELD_TRIGGERMARKERS_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_FIELD_TRIGGERMARKERS_H_DECLS)
#define VS_FIELD_TRIGGERMARKERS_H_DECLS

extern RECT Field_triggerMarkerTextureSource;
extern RECT Field_triggerMarkerPaletteSource;

#endif // VS_FIELD_TRIGGERMARKERS_H_DECLS
