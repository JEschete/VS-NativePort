// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_ACTORS_WEAPONMODEL_H_TYPES
#define VS_ACTORS_WEAPONMODEL_H_TYPES
#include "Legacy/Psx.h"

typedef struct Actors_WeaponModel Actors_WeaponModel;
typedef struct Actors_WeaponModelData Actors_WeaponModelData;

// decomp: _wepModelOffsets (src/BATTLE/BATTLE.PRG/3A1A0.h:120)
typedef struct Actors_WeaponModelData {
    u_char header[0x30];
    int textureSection;
    int vertexGroupSection;
    int vertexSection;
    int polygonSection;
    u_char fileBody[0x7C0];
} Actors_WeaponModelData;

// decomp: vs_battle_wepModels_t (src/BATTLE/BATTLE.PRG/3A1A0.h:449)
typedef struct Actors_WeaponModel {
    u_char boneCount;
    u_char vertexGroupCount;
    u_short triangleCount;
    u_short quadCount;
    u_short otherPrimitiveCount;
    u_char actorId : 4;
    u_char reserved08Bit4 : 1;
    u_char isWideTexture : 1;
    u_char reserved08Bit6 : 1;
    u_char isDoubleClut : 1;
    u_char ownerMotionState : 4;
    u_char material : 4;
    u_char reserved0ABits0 : 7;
    u_char reserved0ABit7 : 1;
    u_char paletteColorCount;
    u_char attachmentSlot;
    u_char attachmentBone;
    u_char modelId;
    u_char textureSlot;
    u_char paletteSlotOffset;
    u_char reserved11;
    u_char reserved12;
    u_char reserved13;
    int reserved14;
    short reserved18;
    short reserved1A;
    Actors_WeaponModelData* sectionOffsets;
    MATRIX boneWorldMatrices[4];
    SVECTOR bones[4];
    u_short palettes[7][64];
    u_short workingPalette[96];
    u_short uploadPalette[96];
    u_char reserved5C0;
    u_char reserved5C1;
    u_short reserved5C2;
    int reserved5C4;
    int reserved5C8;
    char reserved5CC;
    char reserved5CD;
    short reserved5CE;
    int reserved5D0;
    int reserved5D4;
    void* reserved5D8;
    int reserved5DC;
    int reserved5E0;
    int reserved5E4;
} Actors_WeaponModel;

#endif // VS_ACTORS_WEAPONMODEL_H_TYPES
