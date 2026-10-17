// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_FIELD_GIMMICKMOTION_H_TYPES
#define VS_FIELD_GIMMICKMOTION_H_TYPES
#include "Legacy/Psx.h"

typedef struct Field_GimmickMotionView Field_GimmickMotionView;

// decomp: objectMotionContext (src/BATTLE/BATTLE.PRG/44F14.c:106)
typedef struct Field_GimmickMotionView {
    char reserved00[8];
    u_int flags;
    char reserved0C[3];
    u_char objectId;
    char reserved10[0xA];
    short mode;
    short x, y, z, pad22;
    short rotationX, rotationY, rotationZ, pad2A;
    short scaleX, scaleY, scaleZ, pad32, reserved34;
    short deltaY, reserved38, orientation;
    char reserved3C[0xF0];
    char savedState[32];
    char state[32];
    char reserved16C[2];
    u_char material, reserved16F, frame;
} Field_GimmickMotionView;
_Static_assert(sizeof(Field_GimmickMotionView) == 0x174, "Field_GimmickMotionView must keep its PS1 layout");

#endif // VS_FIELD_GIMMICKMOTION_H_TYPES
