// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_FIELD_PLATFORMS_H_TYPES
#define VS_FIELD_PLATFORMS_H_TYPES
#include "Legacy/Psx.h"

struct Field_PlatformContactPoint;

// decomp: func_8008D2C0_t (src/BATTLE/BATTLE.PRG/146C.c:256)
struct Field_PlatformContactPoint {
    short x;
    short height;
    short z;
    short platformIndex;
};
static_assert(sizeof(Field_PlatformContactPoint) == 0x8, "Field_PlatformContactPoint must keep its PS1 layout");

#endif // VS_FIELD_PLATFORMS_H_TYPES
