// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_EFFECTS_PACKAGE_H_TYPES
#define VS_EFFECTS_PACKAGE_H_TYPES
#include "Legacy/Psx.h"

typedef struct Effects_CurveBlock Effects_CurveBlock;
typedef struct Effects_PackageBlock1 Effects_PackageBlock1;
typedef struct Effects_PackageBlock11 Effects_PackageBlock11;
typedef struct Effects_PackageBlock2 Effects_PackageBlock2;
typedef struct Effects_PackageBlock3 Effects_PackageBlock3;
typedef struct Effects_PackageBlock5 Effects_PackageBlock5;
typedef struct Effects_PackageBlock9 Effects_PackageBlock9;
typedef struct Effects_PackageFile Effects_PackageFile;
typedef struct Effects_PackageTexture Effects_PackageTexture;
typedef struct Effects_PackageTextureInfo Effects_PackageTextureInfo;

#ifndef VS_TYPES_ONLY
#define VS_TYPES_ONLY
#define VS_EFFECTS_PACKAGE_H_SET_TYPES_ONLY
#endif
#include "Effects/Particles.h"
#ifdef VS_EFFECTS_PACKAGE_H_SET_TYPES_ONLY
#undef VS_TYPES_ONLY
#undef VS_EFFECTS_PACKAGE_H_SET_TYPES_ONLY
#endif

// decomp: pFileBlock1 (src/BATTLE/BATTLE.PRG/6F014.c:293)
typedef struct Effects_PackageBlock1 {
    short tableCount;
    u_short dataOffset;
    int tableOffsets[0];
} Effects_PackageBlock1;
_Static_assert(sizeof(Effects_PackageBlock1) == 0x4, "Effects_PackageBlock1 must keep its PS1 layout");

// decomp: pFileBlock11 (src/BATTLE/BATTLE.PRG/5BF94.c:248)
typedef struct Effects_PackageBlock11 {
    int count;
    short offsets[0];
} Effects_PackageBlock11;
_Static_assert(sizeof(Effects_PackageBlock11) == 0x4, "Effects_PackageBlock11 must keep its PS1 layout");

// decomp: pFileBlock2 (src/BATTLE/BATTLE.PRG/6F014.c:298)
typedef struct Effects_PackageBlock2 {
    int animationCount;
    u_char reserved4[0];
} Effects_PackageBlock2;
_Static_assert(sizeof(Effects_PackageBlock2) == 0x4, "Effects_PackageBlock2 must keep its PS1 layout");

// decomp: pFileBlock3 (src/BATTLE/BATTLE.PRG/5BF94.h:334)
typedef struct Effects_PackageBlock3 {
    int count;
    u_short offsets[0];
} Effects_PackageBlock3;
_Static_assert(sizeof(Effects_PackageBlock3) == 0x4, "Effects_PackageBlock3 must keep its PS1 layout");

// decomp: pFileBlock5 (src/BATTLE/BATTLE.PRG/5BF94.h:329)
typedef struct Effects_PackageBlock5 {
    int reserved0;
    Effects_ParticleDefinition definitions[0];
} Effects_PackageBlock5;
_Static_assert(sizeof(Effects_PackageBlock5) == 0x4, "Effects_PackageBlock5 must keep its PS1 layout");

// decomp: pFileBlock9 (src/BATTLE/BATTLE.PRG/5BF94.h:177)
typedef struct Effects_PackageBlock9 {
    short gravityRange[6];
    short dragRange[2];
    u_char reserved10;
    u_char reserved11;
    u_char gravityCurve;
    u_char dragCurve;
    u_char frameCount;
    u_char transformCount;
    u_char timelineIds[4];
} Effects_PackageBlock9;
_Static_assert(sizeof(Effects_PackageBlock9) == 0x1A, "Effects_PackageBlock9 must keep its PS1 layout");

// decomp: pFileCurveBlock (src/BATTLE/BATTLE.PRG/5BF94.h:203)
typedef struct Effects_CurveBlock {
    u_short count;
    u_short dataOffset;
    u_char curveSizes[0];
} Effects_CurveBlock;
_Static_assert(sizeof(Effects_CurveBlock) == 0x4, "Effects_CurveBlock must keep its PS1 layout");

// decomp: pFileTexture (src/BATTLE/BATTLE.PRG/5BF94.h:351)
typedef struct Effects_PackageTexture {
    short clut;
    short tpage;
    u_char u;
    u_char v;
    u_char w;
    u_char h;
    short reserved8;
    short reservedA;
    short reservedC;
    short reservedE;
    short reserved10;
    short reserved12;
    short reserved14;
    short reserved16;
} Effects_PackageTexture;
_Static_assert(sizeof(Effects_PackageTexture) == 0x18, "Effects_PackageTexture must keep its PS1 layout");

// decomp: pFileTextureInfo (src/BATTLE/BATTLE.PRG/5BF94.h:213)
typedef struct Effects_PackageTextureInfo {
    u_short reserved0;
    u_short dataIndex;
} Effects_PackageTextureInfo;
_Static_assert(sizeof(Effects_PackageTextureInfo) == 0x4, "Effects_PackageTextureInfo must keep its PS1 layout");

// decomp: p_file_t (src/BATTLE/BATTLE.PRG/6F014.c:303)
typedef struct Effects_PackageFile {
    int meta;
    u_char blocks[0];
} Effects_PackageFile;
_Static_assert(sizeof(Effects_PackageFile) == 0x4, "Effects_PackageFile must keep its PS1 layout");

#endif // VS_EFFECTS_PACKAGE_H_TYPES
