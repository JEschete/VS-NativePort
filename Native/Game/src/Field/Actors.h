// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_FIELD_ACTORS_H_TYPES
#define VS_FIELD_ACTORS_H_TYPES
#include "Legacy/Psx.h"

typedef struct Field_ActorPlacement Field_ActorPlacement;

// decomp: func_80089888_t (src/BATTLE/BATTLE.PRG/2842C.h:23)
typedef struct Field_ActorPlacement {
    u_int tileX : 8;
    u_int direction : 8;
    u_int tileZ : 8;
    u_int height : 8;
} Field_ActorPlacement;

#endif // VS_FIELD_ACTORS_H_TYPES
