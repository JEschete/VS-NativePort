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
    Battle_TargetPreview* targetList0;
    Battle_TargetPreview* targetList1;
    Battle_ShapeVertex wireVertices[297];
    char reservedLabelArea_990[16];
    void* nativePointer9A0; /* PS1 pointer slot 0x9A0 */
    char reservedLabelArea_9A4[16];
    void* nativePointer9B4; /* PS1 pointer slot 0x9B4 */
    char reservedLabelArea_9B8[16];
    void* nativePointer9C8; /* PS1 pointer slot 0x9C8 */
    char reservedLabelArea_9CC[16];
    void* nativePointer9DC; /* PS1 pointer slot 0x9DC */
    char reservedLabelArea_9E0[16];
    void* nativePointer9F0; /* PS1 pointer slot 0x9F0 */
    char reservedLabelArea_9F4[16];
    void* nativePointerA04; /* PS1 pointer slot 0xA04 */
    char reservedLabelArea_A08[16];
    void* nativePointerA18; /* PS1 pointer slot 0xA18 */
    char reservedLabelArea_A1C[16];
    void* nativePointerA2C; /* PS1 pointer slot 0xA2C */
    char reservedLabelArea_A30[16];
    void* nativePointerA40; /* PS1 pointer slot 0xA40 */
    char reservedLabelArea_A44[16];
    void* nativePointerA54; /* PS1 pointer slot 0xA54 */
    char reservedLabelArea_A58[16];
    void* nativePointerA68; /* PS1 pointer slot 0xA68 */
    char reservedLabelArea_A6C[16];
    void* nativePointerA7C; /* PS1 pointer slot 0xA7C */
    char reservedLabelArea_A80[16];
    void* nativePointerA90; /* PS1 pointer slot 0xA90 */
    char reservedLabelArea_A94[16];
    void* nativePointerAA4; /* PS1 pointer slot 0xAA4 */
    char reservedLabelArea_AA8[16];
    void* nativePointerAB8; /* PS1 pointer slot 0xAB8 */
    char reservedLabelArea_ABC[16];
    void* nativePointerACC; /* PS1 pointer slot 0xACC */
    char reservedLabelArea_AD0[16];
    void* nativePointerAE0; /* PS1 pointer slot 0xAE0 */
    char reservedLabelArea_AE4[16];
    void* nativePointerAF4; /* PS1 pointer slot 0xAF4 */
    char reservedLabelArea_AF8[16];
    void* nativePointerB08; /* PS1 pointer slot 0xB08 */
    char reservedLabelArea_B0C[16];
    void* nativePointerB1C; /* PS1 pointer slot 0xB1C */
    char reservedLabelArea_B20[16];
    void* nativePointerB30; /* PS1 pointer slot 0xB30 */
    char reservedLabelArea_B34[16];
    void* nativePointerB44; /* PS1 pointer slot 0xB44 */
    char reservedLabelArea_B48[16];
    void* nativePointerB58; /* PS1 pointer slot 0xB58 */
    char reservedLabelArea_B5C[16];
    void* nativePointerB6C; /* PS1 pointer slot 0xB6C */
    char reservedLabelArea_B70[14660];
    short wireEdges[0x302];
    u_char terrainRowFlags[0x64];
} Battle_TargetSphereSetupView;

// decomp: D_800EB9B8_unk990 (src/BATTLE/BATTLE.PRG/58578.c:24)
typedef struct Battle_SphereTargetLabel {
    char state;
    char depth;
    char gradient;
    char ringSize;
    short screenX;
    u_char offsetY;
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
