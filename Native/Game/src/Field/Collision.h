// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_FIELD_COLLISION_H_TYPES
#define VS_FIELD_COLLISION_H_TYPES
#include "Legacy/Psx.h"

typedef struct Field_SavedPositionWords Field_SavedPositionWords;

// decomp: D_800F4B0A (D_800F4B0A@BATTLE/BATTLE.PRG): a view into another global
#define Field_collisionProbeHeight (*(short*)((u_char*)&Field_collisionProbePosition + 2))

// decomp: func_800A6798_t (src/BATTLE/BATTLE.PRG/3A1A0.c:29)
typedef struct Field_SavedPositionWords {
    int words[2];
} Field_SavedPositionWords;
_Static_assert(sizeof(Field_SavedPositionWords) == 0x8, "Field_SavedPositionWords must keep its PS1 layout");

#endif // VS_FIELD_COLLISION_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_FIELD_COLLISION_H_DECLS)
#define VS_FIELD_COLLISION_H_DECLS

extern u_char Field_kTileSurfaceIds[2744];
extern u_char Field_collisionProbeDirection;
extern short Field_platformContactSearchDepth;
extern SVECTOR Field_collisionProbePosition;

#endif // VS_FIELD_COLLISION_H_DECLS
