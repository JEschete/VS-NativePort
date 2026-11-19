// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#pragma once
#include "Legacy/Psx.h"

struct Actors_ModelEffectSlot;
struct Actors_ModelInstance;
struct Actors_ShpModel;

#include "Render/Palette.hpp"

// decomp: D_800F4538_unk64 (src/BATTLE/BATTLE.PRG/3A1A0.h:99)
struct Actors_ModelEffectSlot {
    u_char kind;
    u_char level;
    u_char flags;
    u_char parameter;
};
static_assert(sizeof(Actors_ModelEffectSlot) == 0x4, "Actors_ModelEffectSlot must keep its PS1 layout");

// decomp: D_800F4538_unk0 (src/BATTLE/BATTLE.PRG/3A1A0.h:190)
struct Actors_ModelInstance {
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
    VS::Render::ColorCode tint;
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
};
