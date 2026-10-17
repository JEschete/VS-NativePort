// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_ACTORS_ACTORRECORD_H_TYPES
#define VS_ACTORS_ACTORRECORD_H_TYPES
#include "Legacy/Psx.h"

typedef struct Actors_ActorRecord Actors_ActorRecord;
typedef struct Actors_CompanionOffset Actors_CompanionOffset;

#ifndef VS_TYPES_ONLY
#define VS_TYPES_ONLY
#define VS_ACTORS_ACTORRECORD_H_SET_TYPES_ONLY
#endif
#include "Actors/Animation.h"
#include "Actors/ModelInstance.h"
#include "Actors/Movement.h"
#include "Actors/Shadow.h"
#include "Actors/TextureAnimation.h"
#include "Render/Palette.h"
#ifdef VS_ACTORS_ACTORRECORD_H_SET_TYPES_ONLY
#undef VS_TYPES_ONLY
#undef VS_ACTORS_ACTORRECORD_H_SET_TYPES_ONLY
#endif

// decomp: D_800F4538_unk58 (src/BATTLE/BATTLE.PRG/3A1A0.h:31)
typedef struct Actors_CompanionOffset {
    signed char offsetX;
    signed char offsetY;
    signed char offsetZ;
    u_char depthBias;
} Actors_CompanionOffset;
_Static_assert(sizeof(Actors_CompanionOffset) == 0x4, "Actors_CompanionOffset must keep its PS1 layout");

// decomp: D_800F4538_t (src/BATTLE/BATTLE.PRG/3A1A0.h:400)
typedef struct Actors_ActorRecord {
    Actors_ModelInstance instance;
    MATRIX boneMatrices[42];
    u_char routePhase : 2;
    u_char routeUsesDuration : 1;
    u_char actionPoseActive : 1;
    u_char attachedChildCount : 2;
    u_char reserved5ACBit6 : 1;
    u_char momentumStop : 1;
    u_char customTint : 1;
    u_char bypassActorCollision : 1;
    u_char bypassTerrainCollision : 1;
    u_char ignoredByCollision : 1;
    u_char wallContact : 1;
    u_char onLedge : 1;
    u_char wasOnLedge : 1;
    u_char isRunning : 1;
    u_char animationEventFlag : 1;
    u_char reserved5ACBit17 : 1;
    u_char advanceRequested : 1;
    u_char reserved5ACBit19 : 1;
    u_char weaponPoseActive : 1;
    u_char paletteFadeActive : 1;
    u_char paletteFadeReverse : 1;
    u_char poseOverrideActive : 1;
    u_char animationLocked : 1;
    u_char locomotionAnimationState : 1;
    u_char landingPending : 1;
    u_char textureHeaderBit : 1;
    u_char supportGimmick : 4;
    u_char jumpProfile : 2;
    u_char jumpStarted : 1;
    u_char fastAnimation : 1;

    u_char previewPose : 1;
    u_char holdsTextureSlots : 1;
    u_char reserved5B0Bits6 : 2;
    u_char sequenceModelId;
    u_char framesPerStep;
    u_char locomotionCounter;
    u_char sequenceFlags;
    u_char commonAnimationCount;
    u_char battleAnimationCount;
    u_char jumpLanding;
    u_char animationId;
    u_char runSpeed;
    u_char walkSpeed;
    u_char textureSlot;
    short animationLength;
    short collisionState;
    u_short turnRate;
    u_short moveRate;
    u_short routeMoveRate;
    u_short routeTurnRate;
    u_short scaleFrames;
    u_short transitionFrames;
    char blendFrame;
    char blendFrames;
    short reserved5CE;
    void* ownedSequence;
    void* commonSequence;
    void* battleSequence;
    void* sharedSequences[4];
    SVECTOR routeTarget;
    Actors_TextureOverlayRect textureOverlays[8];
    Actors_TextureCellOrigin textureCellOrigins[2];
    Actors_TextureScrollBounds textureScrollBounds[4];
    u_short collisionRadius;
    u_short collisionHeight;
    u_short occupancyRadius;
    u_short occupancyHeight;
    u_short reserved644;
    u_short targetPointHeight;
    u_int opaque648 : 16;
    u_int previewFlag : 1;
    u_int reserved648Bits17 : 15;
    u_short shadowRadius;
    u_short shadowRadiusExpandRate;
    u_short shadowRadiusShrinkRate;
    u_short reserved652;
    u_short reserved654;
    u_short menuCameraDistance;
    u_short reserved658;
    short anchorHeight;
    int reserved65C;
    int reserved660;
    u_int battleSequenceLbas[12];
    u_short chainAttackAnimationIds[12];
    u_int specialSequenceLbas[4];
    int reserved6BC[4];
    u_short specialSequenceSizes[4];
    int reserved6D4[2];
    int opaque6DC;
    u_char queuedAnimation;
    u_char queuedParameterA;
    u_char queuedParameterB;
    u_char animationSet;
    u_char contactKind;
    u_char travelAnimation;
    u_short modelId;
    short clutA;
    short clutB;
    short ledgeHeight;
    signed char platformOffsetX;
    signed char platformOffsetZ;
    u_char fadeBlue;
    u_char fadeGreen;
    u_char fadeRed;
    u_char reserved6F3;
    Actors_CompanionOffset companionState;
    Render_ColorCode companionColor;
    SVECTOR soundPosition;
    Actors_AnimationPose blendPose;
    Actors_AnimationPose pose;
    u_char palettes[0x280];
    u_short basePalette[0x140];
    u_short currentPalette[0xA0];
    Actors_ModelEffectSlot soundEffect;
    Actors_ModelEffectSlot soundEffectTarget;
    SVECTOR platformAnchor;
    SVECTOR headTracking;
    signed char headTrackTarget;
    u_char companionSlot;
    u_char terrainFacing;
    u_char opaque17FF;
    u_short moveFacing;
    u_char paletteFadeState[2];
    Actors_TextureAnimationFrame textureAnimationFrames[2];
    Actors_TextureAnimationSlot textureAnimationSlots[2];
    short destinationX;
    short jumpFrames;
    short destinationZ;
    short destinationMode;
    Actors_TextureScrollState textureScroll[4];
    short scale;
    u_char textureRowOffsets[2];
    short momentumDistance;
    short momentumDirection;
    short reserved1844;
    u_char runFrames;
    u_char runTurns;
    Actors_MovementRequest turnState;
    u_char opaque1860;
    u_char reserved1861;
    u_char reserved1862;
    u_char reserved1863;
    Actors_ShadowBlend shadowBlend;
    int savedPlacement;
    int animationHeader;
    u_char jointSlots[0x10];
    MATRIX lockedBoneMatrices[2];
    u_char lockedBones[2];
    u_short pendingTargetX;
    u_short pendingTargetZ;
    u_short previousTargetX;
    u_short previousTargetZ;
} Actors_ActorRecord;

#endif // VS_ACTORS_ACTORRECORD_H_TYPES
