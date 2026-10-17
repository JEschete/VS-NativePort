// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_ACTORS_MODELINSTANCE_H_TYPES
#define VS_ACTORS_MODELINSTANCE_H_TYPES
#include "Legacy/Psx.h"

typedef struct Actors_ModelEffectSlot Actors_ModelEffectSlot;
typedef struct Actors_ModelInstance Actors_ModelInstance;
typedef struct Actors_ShpModel Actors_ShpModel;

#ifndef VS_TYPES_ONLY
#define VS_TYPES_ONLY
#define VS_ACTORS_MODELINSTANCE_H_SET_TYPES_ONLY
#endif
#include "Render/Palette.h"
#ifdef VS_ACTORS_MODELINSTANCE_H_SET_TYPES_ONLY
#undef VS_TYPES_ONLY
#undef VS_ACTORS_MODELINSTANCE_H_SET_TYPES_ONLY
#endif

// decomp: D_800F4538_unk64 (src/BATTLE/BATTLE.PRG/3A1A0.h:99)
typedef struct Actors_ModelEffectSlot {
    u_char kind;
    u_char level;
    u_char flags;
    u_char parameter;
} Actors_ModelEffectSlot;

// decomp: D_800F4538_unk0 (src/BATTLE/BATTLE.PRG/3A1A0.h:190)
typedef struct Actors_ModelInstance {
    u_char boneCount;
    u_char vertexGroupCount;
    u_short triangleCount;
    u_short quadCount;
    u_short otherPolygonCount;
    char isSkipped : 1;
    char isVisible : 1;
    char isFrozen : 1;
    char uvLayout : 1;
    char textureSlotCountMinusOne : 3;
    char isParsed : 1;
    u_char actionAnimationState : 4;
    u_char isPreviewModel : 1;
    u_char isAttached : 1;
    u_char movementPending : 1;
    u_char soundEnabled : 1;
    u_char movementMode : 3;
    u_char locomotionState : 2;
    u_char motionLocked : 1;
    u_char isJumping : 1;
    u_char weaponDrawn : 1;
    u_char reactionState : 4;
    u_char poseState : 3;
    u_char wasDrawn : 1;
    u_char supportLayer : 4;
    u_char placementLayer : 4;
    u_char reservedD;
    u_char copiedWordCount;
    u_char slot;
    u_char variant;
    u_char jumpCountdown;
    u_char attachParent;
    u_char attachBone;
    u_short lookYaw;
    u_short lookPitch;
    u_short turnFrames;
    u_short currentAnimation;
    SVECTOR position;
    short rotationX;
    short facing;
    short rotationZ;
    short reserved2A;
    short scaleX;
    short scaleY;
    short scaleZ;
    short reserved32;
    SVECTOR rootMotion;
    short rotationStepX;
    short facingDelta;
    short rotationStepZ;
    u_short reserved42;
    SVECTOR scaleDelta;
    int supportOffset;
    short reserved50;
    short previousFacing;
    CVECTOR baseColor;
    Render_ColorCode tint;
    u_char currentTileX;
    u_char currentTileLayer;
    u_char currentTileZ;
    u_char reserved5F;
    u_char previousTileX;
    u_char previousTileLayer;
    u_char previousTileZ;
    u_char reserved63;
    Actors_ModelEffectSlot palettePulse;
    Actors_ShpModel* model;
} Actors_ModelInstance;

#endif // VS_ACTORS_MODELINSTANCE_H_TYPES
