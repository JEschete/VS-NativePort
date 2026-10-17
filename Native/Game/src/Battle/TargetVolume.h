// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_BATTLE_TARGETVOLUME_H_TYPES
#define VS_BATTLE_TARGETVOLUME_H_TYPES
#include "Legacy/Psx.h"

typedef struct Battle_SphereTargetLabel Battle_SphereTargetLabel;
typedef struct Battle_TargetPreview Battle_TargetPreview;
typedef struct Battle_TargetSphere Battle_TargetSphere;
typedef struct Battle_TargetSphereSetupView Battle_TargetSphereSetupView;
typedef struct Battle_TargetSphereSpec Battle_TargetSphereSpec;

#ifndef VS_TYPES_ONLY
#define VS_TYPES_ONLY
#define VS_BATTLE_TARGETVOLUME_H_SET_TYPES_ONLY
#endif
#include "Battle/BattleTypes.h"
#ifdef VS_BATTLE_TARGETVOLUME_H_SET_TYPES_ONLY
#undef VS_TYPES_ONLY
#undef VS_BATTLE_TARGETVOLUME_H_SET_TYPES_ONLY
#endif

// decomp: D_800EB9B8_t2 (src/BATTLE/BATTLE.PRG/573B8.c:31)
typedef struct Battle_TargetSphereSpec {
    u_char kind;
    u_char reserved1;
    u_char reserved2;
    u_char reserved3;
    int reserved4;
    int reserved8;
    int reservedC;
} Battle_TargetSphereSpec;
_Static_assert(sizeof(Battle_TargetSphereSpec) == 0x10, "Battle_TargetSphereSpec must keep its PS1 layout");

// decomp: D_800EB9B8_t (src/BATTLE/BATTLE.PRG/573B8.c:55)
typedef struct Battle_TargetSphereSetupView {
    Battle_TargetSphereSpec primaryVolume;
    Battle_TargetSphereSpec secondaryVolume;
    int depthAndYaw;
    short wirePhaseA;
    short wirePhaseB;
    short wirePhaseC;
    short wireShape;
    char targetCount;
    char selectedTarget;
    u_char sphereMode;
    char reserved2F;
    int reserved30;
    int reserved34;
    int reserved38;
    int reserved3C;
    int targetList0;
    int targetList1;
    Battle_ShapeVertex wireVertices[297];
    u_char labelArea[0x3B24];
    short wireEdges[0x302];
    u_char terrainRowFlags[0x64];
} Battle_TargetSphereSetupView;
_Static_assert(sizeof(Battle_TargetSphereSetupView) == 0x4B1C, "Battle_TargetSphereSetupView must keep its PS1 layout");

// decomp: D_800EB9B8_unk990 (src/BATTLE/BATTLE.PRG/58578.c:24)
typedef struct Battle_SphereTargetLabel {
    char state;
    char depth;
    char gradient;
    char ringSize;
    short screenX;
    char offsetY;
    char fade;
    SVECTOR worldPosition;
    char* text;
} Battle_SphereTargetLabel;

// decomp: D_800EB9B8_t (src/BATTLE/BATTLE.PRG/58578.c:52)
typedef struct Battle_TargetSphere {
    Battle_TargetVolume volumes[2];
    short depth;
    short cameraYaw;
    short wirePhaseA;
    short wirePhaseB;
    short wirePhaseC;
    short wireShape;
    char targetCount;
    char selectedTarget;
    char sphereMode;
    char reserved2F;
    char reserved30;
    char targetMode;
    short vertexCount;
    int reserved34;
    int reserved38;
    short reserved3C;
    short inputLocked;
    Battle_TargetPreview* targetLists[2];
    Battle_ShapeVertex wireVertices[297];
    Battle_SphereTargetLabel labels[24];
    char reservedB70[0x3800];
    int terrainHeights[81];
    char wireEdges[0x604];
    u_char terrainRowFlags[81];
} Battle_TargetSphere;

#endif // VS_BATTLE_TARGETVOLUME_H_TYPES
