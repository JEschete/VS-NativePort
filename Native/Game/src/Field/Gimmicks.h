// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_FIELD_GIMMICKS_H_TYPES
#define VS_FIELD_GIMMICKS_H_TYPES
#include "Legacy/Psx.h"

struct Actors_ShpModel;
struct Field_GimmickAnimationJob;
struct Field_GimmickInstance;
struct Field_GimmickObjectInfo;
struct Field_GimmickPhysicsView;
struct Field_GimmickSnapshot;
struct Field_GimmickTemplate;
struct Field_MpdLockTrigger;

#ifndef VS_TYPES_ONLY
#define VS_TYPES_ONLY
#define VS_FIELD_GIMMICKS_H_SET_TYPES_ONLY
#endif
#include "Actors/ActorRecord.h"
#include "Actors/Animation.h"
#include "Actors/ModelInstance.h"
#include "Actors/Shadow.h"
#include "Render/Palette.h"
#ifdef VS_FIELD_GIMMICKS_H_SET_TYPES_ONLY
#undef VS_TYPES_ONLY
#undef VS_FIELD_GIMMICKS_H_SET_TYPES_ONLY
#endif

// decomp: D_800E8FB4_t (src/BATTLE/BATTLE.PRG/30DB0.c:36)
struct Field_GimmickTemplate {
    int instanceFlags;
    int instanceWord4;
    int recolorCount;
    int modelDataSize;
};
static_assert(sizeof(Field_GimmickTemplate) == 0x10, "Field_GimmickTemplate must keep its PS1 layout");

// decomp: D_800F1D28_t (src/BATTLE/BATTLE.PRG/146C.c:249)
struct Field_GimmickAnimationJob {
    int jobKind;
    Field_MpdLockTrigger* trigger;
};

// decomp: D_800F4538_t2 (src/BATTLE/BATTLE.PRG/3A1A0.h:24)
struct Field_GimmickObjectInfo {
    u_char objectId : 3;
    u_char isFalling : 1;
    u_char interaction : 2;
    u_char reservedBits7 : 2;
    u_char subtype;
    u_char material;
    u_char carrier;
    u_char supportIndex;
    u_char reserved5;
    u_char reserved6;
    u_char reserved7;
    int reserved8;
};
static_assert(sizeof(Field_GimmickObjectInfo) == 0xC, "Field_GimmickObjectInfo must keep its PS1 layout");

// decomp: D_800F45E0_t (src/BATTLE/BATTLE.PRG/3A1A0.h:633)
struct Field_GimmickInstance {
    char boneCount;
    char reserved1;
    char reserved2;
    char reserved3;
    int reserved4;
    char isSkipped : 1;
    char isVisible : 1;
    char reserved8Bits2 : 1;
    char reserved8Bits3 : 1;
    char reserved8Bits4 : 1;
    char reserved8Bits5 : 1;
    char reserved8Bits6 : 1;
    char transitionFlag : 1;
    u_char motionState : 4;
    u_char reserved9Bits4 : 4;
    u_char transitionMode : 3;
    u_char reservedABits3 : 2;
    u_char reservedABits5 : 2;
    u_char reservedABits7 : 1;
    u_char reservedBBits0 : 4;
    u_char reservedBBits4 : 3;
    u_char reservedBBits7 : 1;
    u_char supportLayer : 4;
    u_char reservedCBits4 : 4;
    u_char reservedD;
    u_char reservedE;
    u_char slot;
    u_char reserved10;
    u_char delay;
    u_char parent;
    u_char linkedGimmick;
    int reserved14;
    short rotationTicks;
    short mode;
    short positionX;
    short positionY;
    short positionZ;
    short reserved22;
    short rotationX;
    u_short rotationY;
    u_short rotationZ;
    short reserved2A;
    short scaleX;
    short scaleY;
    short scaleZ;
    short reserved32;
    short slideX;
    short velocityY;
    short slideZ;
    short orientation;
    short rotationDeltaX;
    short rotationDeltaY;
    short rotationDeltaZ;
    short reserved42;
    int reserved44;
    int reserved48;
    int savedPosition;
    short reserved50;
    short savedRotationY;
    int baseColor;
    Render_ColorCode tint;
    char tileX;
    volatile char tileLayer; // dear god why
    char tileZ;
    char reserved5F;
    int savedTile;
    Actors_ModelEffectSlot palettePulse;
    Actors_ShpModel* model;
    MATRIX matrices[5];
    SVECTOR rotationOffset;
    u_char reserved114[0x18];
    u_short basePalette[16];
    u_short pulsedPalette[16];
    Field_GimmickObjectInfo objectInfo;
    u_char reserved178[0x434];
    u_int reserved5ACBits0 : 3;
    u_int actionPoseActive : 1;
    u_int reserved5ACBits4 : 2;
    u_int reserved5ACBits6 : 2;
    u_int reserved5ACBits8 : 1;
    u_int bypassActorCollision : 2;
    u_int reserved5ACBits11 : 1;
    u_int reserved5ACBits12 : 1;
    u_int reserved5ACBits13 : 1;
    u_int reserved5ACBits14 : 1;
    u_int reserved5ACBits15 : 1;
    u_int reserved5ACBits16 : 2;
    u_int reserved5ACBits18 : 1;
    u_int reserved5ACBits19 : 1;
    u_int reserved5ACBits20 : 1;
    u_int reserved5ACBits21 : 1;
    u_int reserved5ACBits22 : 1;
    u_int reserved5ACBits23 : 1;
    u_int animationLocked : 1;
    u_int reserved5ACBits25 : 7;
    u_char reserved5B0;
    u_char reserved5B1;
    u_char reserved5B2;
    u_char reserved5B3;
    u_char reserved5B4;
    u_char reserved5B5;
    u_char reserved5B6;
    u_char reserved5B7;
    u_char reserved5B8;
    u_char reserved5B9;
    u_char reserved5BA;
    u_char reserved5BB;
    short reserved5BC;
    short reserved5BE;
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
    int reserved5E8;
    u_char reserved5EC[0x50];
    u_short collisionRadius;
    u_short reserved63E;
    u_char reserved640[8];
    int reserved648;
    u_char reserved64C;
    u_char reserved64D;
    u_short reserved64E;
    u_short reserved650;
    u_short reserved652;
    u_short reserved654;
    u_short reserved656;
    u_char reserved658[0x88];
    u_char reserved6E0;
    u_char reserved6E1;
    u_char reserved6E2;
    u_char reserved6E3;
    u_char reserved6E4;
    u_char reserved6E5;
    u_short reserved6E6;
    short reserved6E8;
    short reserved6EA;
    int reserved6EC;
    u_char reserved6F0;
    u_char fadeGreen;
    u_char fadeRed;
    u_char reserved6F3;
    Actors_CompanionOffset reserved6F4;
    Render_ColorCode reserved6F8;
    int reserved6FC;
    int reserved700;
    Actors_AnimationPose reserved704;
    Actors_AnimationPose reservedC54;
    u_char reserved11A4[0x280];
    u_short basePaletteRows[0xA0];
    u_short transitionPalette[0xA0];
    u_char reserved16A4[0x158];
    u_char reserved17FC;
    u_char reserved17FD;
    u_char reserved17FE;
    u_char reserved17FF;
    u_char reserved1800[0x3C];
    short reserved183C;
    short reserved183E;
    short reserved1840;
    short reserved1842;
    short reserved1844;
    u_char reserved1846;
    u_char reserved1847;
    u_char reserved1848;
    u_char reserved1849;
    u_char reserved184A;
    u_char reserved184B;
    u_short reserved184C;
    u_short reserved184E;
    u_int reserved1850;
    u_char reserved1854[0x10];
    Actors_ShadowBlend shadowBlend;
    int savedPlacement;
    int reserved187C;
};

// decomp: func_800A0024_t (src/BATTLE/BATTLE.PRG/30DB0.c:24)
struct Field_GimmickSnapshot {
    u_char placementByte0;
    u_char heightAboveGround;
    u_char placementByte2;
    u_char orientation;
    int positionXY;
    int positionZ;
};
static_assert(sizeof(Field_GimmickSnapshot) == 0xC, "Field_GimmickSnapshot must keep its PS1 layout");

// decomp: objectAnimationState (src/BATTLE/BATTLE.PRG/44F14.c:148)
struct Field_GimmickPhysicsView {
    char reserved0[8];
    int flags;
    char reservedC[3];
    u_char id, reserved10;
    signed char delay;
    u_char parent, reserved13;
    int reserved14;
    short rotationTicks, mode;
    SVECTOR position;
    short rotationX, rotationY, rotationZ, reserved2A;
    short scaleX, scaleY, scaleZ, reserved32;
    short slideX, velocityY, slideZ, orientation;
    short rotationRemainingX, rotationRemainingY, rotationRemainingZ;
    char reserved42[0x1A];
    u_char tileX, reserved5D, tileZ, reserved5F;
    char reserved60_60[8];
    void* nativePointer68; /* PS1 pointer slot 0x68 */
    char reserved60_6C[256];
    u_char material, durability, reserved16E, carrier, frame;
};
struct Field_GimmickPhysicsView;

#endif // VS_FIELD_GIMMICKS_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_FIELD_GIMMICKS_H_DECLS)
#define VS_FIELD_GIMMICKS_H_DECLS
#include "Field/Doors.h"
#include "Field/Map.h"
#include "Field/Mpd.h"
#include "Field/Room.h"
#include "Field/TriggerMarkers.h"
#include "Field/Zone.h"
#include "Items/Loot.h"
#include "Items/TreasureChests.h"

extern MATRIX Field_triggerMarkerMatrix;
extern Field_TriggerMarkerVertex Field_triggerMarkerVertex;
extern char Field_triggerMarkerRecordTail[192];
extern u_char Field_gimmickPaletteParameters[32];
extern void* Field_gimmickModelTemplates[5];
extern u_char Field_kGimmickRollOrientations[3424];
extern u_char Field_kGimmickOrientationRotations[3328];
extern int Field_interactingGimmickId;
extern u_char Field_gimmickTemplateIndices[2964];
extern u_char Field_kGimmickSpinHeightSteps[2948];
extern u_char Field_kGimmickSpinPitchSteps[2932];
extern u_char Field_kGimmickSpinYawSteps[2916];
extern u_char Field_kGimmickSpinRollSteps[2900];
extern u_char Field_kGimmickBounceHeightSteps[2884];
extern u_char Field_kGimmickTiltDepthSteps[2880];
extern u_char Field_kGimmickTiltAngleSteps[2868];
extern u_char Field_kGimmickRollHeightSteps[2856];
extern u_char Field_kGimmickRollDisplacementSteps[2840];
extern u_char Field_kGimmickRollAngleSteps[2824];
extern u_char Field_kGimmickSquashHeightSteps[2808];
extern u_char Field_kGimmickSquashScaleX[2792];
extern u_char Field_kGimmickSquashScaleY[2776];
extern u_char Field_kGimmickSquashScaleZ[2760];
extern Field_RoomRecord Field_roomData;
extern u_short Field_roomAnimationTicks;
extern Field_GimmickAnimationJob Field_gimmickAnimationQueue[8];
extern short Field_platformFreezeFrames;
extern short Field_pendingGimmickAnimationCount;
extern u_short Field_triggerMarkerFrameTicks;
extern u_char Field_areMovingPlatformsPaused;
extern char Field_isPlatformFreezeTintActive;
extern u_char Field_platformBounceFrames[1304];
extern char Field_gimmickPaletteColors[240];
extern Field_GimmickInstance* Field_gimmickInstances[16];
extern int Field_gimmickInteractionDirection;
extern short const Field_kPlatformBounceOffsets[];

#endif // VS_FIELD_GIMMICKS_H_DECLS
