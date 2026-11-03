// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_ACTORS_SKELETON_H_TYPES
#define VS_ACTORS_SKELETON_H_TYPES
#include "Legacy/Psx.h"

typedef struct Actors_ShpBone Actors_ShpBone;
typedef struct Actors_ShpModel Actors_ShpModel;

// decomp: _armature_t (src/BATTLE/BATTLE.PRG/3A1A0.h:83)
typedef struct Actors_ShpBone {
    int length;
    u_char parentIndex;
    u_char reserved5;
    u_char tag;
    u_char limbIds;
    u_char auxiliaryTag;
    u_char reserved9;
    u_short jointInitValue;
    u_char poseLockSlot;
    u_char reservedD;
    u_char reservedE;
    u_char reservedF;
} Actors_ShpBone;
_Static_assert(sizeof(Actors_ShpBone) == 0x10, "Actors_ShpBone must keep its PS1 layout");

// decomp: D_800F4538_unk68 (src/BATTLE/BATTLE.PRG/3A1A0.h:92)
typedef struct Actors_ShpModel {
    u_char header[48];
    u_int soundSection; /* offset from the struct (on-disc pointer) */
    u_int groupSection; /* offset from the struct (on-disc pointer) */
    u_int vertexSection; /* offset from the struct (on-disc pointer) */
    u_int polygonSection; /* offset from the struct (on-disc pointer) */
    Actors_ShpBone bones[0];
} Actors_ShpModel;
_Static_assert(sizeof(Actors_ShpModel) == 0x40, "Actors_ShpModel must keep its PS1 layout");

#endif // VS_ACTORS_SKELETON_H_TYPES
