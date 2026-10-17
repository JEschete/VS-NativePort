// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_FIELD_GIMMICKS_H_TYPES
#define VS_FIELD_GIMMICKS_H_TYPES
#include "Legacy/Psx.h"

typedef struct Actors_ShpModel Actors_ShpModel;
typedef struct Field_GimmickAnimationJob Field_GimmickAnimationJob;
typedef struct Field_GimmickInstance Field_GimmickInstance;
typedef struct Field_GimmickObjectInfo Field_GimmickObjectInfo;
typedef struct Field_GimmickPhysicsView Field_GimmickPhysicsView;
typedef struct Field_GimmickSnapshot Field_GimmickSnapshot;
typedef struct Field_GimmickTemplate Field_GimmickTemplate;
typedef struct Field_MpdLockTrigger Field_MpdLockTrigger;

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
typedef struct Field_GimmickTemplate {
    int instanceFlags;
    int instanceWord4;
    int recolorCount;
    int modelDataSize;
} Field_GimmickTemplate;
_Static_assert(sizeof(Field_GimmickTemplate) == 0x10, "Field_GimmickTemplate must keep its PS1 layout");

// decomp: D_800F1D28_t (src/BATTLE/BATTLE.PRG/146C.c:249)
typedef struct Field_GimmickAnimationJob {
    int jobKind;
    Field_MpdLockTrigger* trigger;
} Field_GimmickAnimationJob;

// decomp: D_800F4538_t2 (src/BATTLE/BATTLE.PRG/3A1A0.h:24)
typedef struct Field_GimmickObjectInfo {
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
} Field_GimmickObjectInfo;
_Static_assert(sizeof(Field_GimmickObjectInfo) == 0xC, "Field_GimmickObjectInfo must keep its PS1 layout");

// layout: ledger (upstream merge) for D_800F45E0_t
typedef struct Field_GimmickInstance {
    /* 0x00 */ char boneCount;
    /* 0x01 */ char reserved1;
    /* 0x02 */ char reserved2;
    /* 0x03 */ char reserved3;
    /* 0x04 */ int reserved4;
    /* 0x08 */ char isSkipped : 1;
    /* 0x08 */ char isVisible : 1;
    /* 0x08 */ char reserved8Bits2 : 1;
    /* 0x08 */ char reserved8Bits3 : 1;
    /* 0x08 */ char reserved8Bits4 : 1;
    /* 0x08 */ char reserved8Bits5 : 1;
    /* 0x08 */ char reserved8Bits6 : 1;
    /* 0x08 */ char transitionFlag : 1;
    /* 0x09 */ u_char motionState : 4;
    /* 0x09 */ u_char reserved9Bits4 : 4;
    /* 0x0A */ u_char transitionMode : 3;
    /* 0x0A */ u_char reservedABits3 : 2;
    /* 0x0A */ u_char reservedABits5 : 2;
    /* 0x0A */ u_char reservedABits7 : 1;
    /* 0x0B */ u_char reservedBBits0 : 4;
    /* 0x0B */ u_char reservedBBits4 : 3;
    /* 0x0B */ u_char reservedBBits7 : 1;
    /* 0x0C */ u_char supportLayer : 4;
    /* 0x0C */ u_char reservedCBits4 : 4;
    /* 0x0D */ u_char reservedD;
    /* 0x0E */ u_char reservedE;
    /* 0x0F */ u_char slot;
    /* 0x10 */ u_char reserved10;
    /* 0x11 */ u_char delay;
    /* 0x12 */ u_char parent;
    /* 0x13 */ u_char linkedGimmick;
    /* 0x14 */ int reserved14;
    /* 0x18 */ short rotationTicks;
    /* 0x1A */ short mode;
    /* 0x1C */ short positionX;
    /* 0x1E */ short positionY;
    /* 0x20 */ short positionZ;
    /* 0x22 */ short reserved22;
    /* 0x24 */ short rotationX;
    /* 0x26 */ u_short rotationY;
    /* 0x28 */ int rotationZ;
    /* 0x2A */ short reserved2A;
    /* 0x2C */ short scaleX;
    /* 0x2E */ short scaleY;
    /* 0x30 */ short scaleZ;
    /* 0x32 */ short reserved32;
    /* 0x34 */ short slideX;
    /* 0x36 */ short velocityY;
    /* 0x38 */ short slideZ;
    /* 0x3A */ short orientation;
    /* 0x3C */ int rotationDeltaX;
    /* 0x3E */ short rotationDeltaY;
    /* 0x40 */ int rotationDeltaZ;
    /* 0x42 */ short reserved42;
    /* 0x44 */ int reserved44;
    /* 0x48 */ int reserved48;
    /* 0x4C */ int savedPosition;
    /* 0x50 */ short reserved50;
    /* 0x52 */ short savedRotationY;
    /* 0x54 */ int baseColor;
    /* 0x58 */ Render_ColorCode tint;
    /* 0x5C */ char tileX;
    /* 0x5D */ volatile char tileLayer;
    /* 0x5E */ char tileZ;
    /* 0x5F */ char reserved5F;
    /* 0x60 */ int savedTile;
    /* 0x64 */ Actors_ModelEffectSlot palettePulse;
    /* 0x68 */ Actors_ShpModel *model;
    /* 0x6C */ MATRIX matrices[5];
    /* 0x10C */ SVECTOR rotationOffset;
    /* 0x114 */ u_char reserved114[24];
    /* 0x12C */ u_short basePalette[16];
    /* 0x14C */ u_char pulsedPalette[32];
    /* 0x16C */ Field_GimmickObjectInfo objectInfo;
    /* 0x178 */ u_char reserved178[1076];
    /* 0x5AC */ u_int reserved5ACBits0 : 3;
    /* 0x5AC */ u_int actionPoseActive : 1;
    /* 0x5AC */ u_int reserved5ACBits4 : 2;
    /* 0x5AC */ u_int reserved5ACBits6 : 2;
    /* 0x5AD */ u_int reserved5ACBits8 : 1;
    /* 0x5AD */ u_int bypassActorCollision : 2;
    /* 0x5AD */ u_int reserved5ACBits11 : 1;
    /* 0x5AD */ u_int reserved5ACBits12 : 1;
    /* 0x5AD */ u_int reserved5ACBits13 : 1;
    /* 0x5AD */ u_int reserved5ACBits14 : 1;
    /* 0x5AD */ u_int reserved5ACBits15 : 1;
    /* 0x5AE */ u_int reserved5ACBits16 : 2;
    /* 0x5AE */ u_int reserved5ACBits18 : 1;
    /* 0x5AE */ u_int reserved5ACBits19 : 1;
    /* 0x5AE */ u_int reserved5ACBits20 : 1;
    /* 0x5AE */ u_int reserved5ACBits21 : 1;
    /* 0x5AE */ u_int reserved5ACBits22 : 1;
    /* 0x5AE */ u_int reserved5ACBits23 : 1;
    /* 0x5AF */ u_int animationLocked : 1;
    /* 0x5AF */ u_int reserved5ACBits25 : 7;
    /* 0x5B0 */ u_char reserved5B0;
    /* 0x5B1 */ u_char reserved5B1;
    /* 0x5B2 */ u_char reserved5B2;
    /* 0x5B3 */ u_char reserved5B3;
    /* 0x5B4 */ u_char reserved5B4;
    /* 0x5B5 */ u_char reserved5B5;
    /* 0x5B6 */ u_char reserved5B6;
    /* 0x5B7 */ u_char reserved5B7;
    /* 0x5B8 */ u_char reserved5B8;
    /* 0x5B9 */ u_char reserved5B9;
    /* 0x5BA */ u_char reserved5BA;
    /* 0x5BB */ u_char reserved5BB;
    /* 0x5BC */ short reserved5BC;
    /* 0x5BE */ short reserved5BE;
    /* 0x5C0 */ u_char reserved5C0;
    /* 0x5C1 */ u_char reserved5C1;
    /* 0x5C2 */ u_short reserved5C2;
    /* 0x5C4 */ int reserved5C4;
    /* 0x5C8 */ int reserved5C8;
    /* 0x5CC */ char reserved5CC;
    /* 0x5CD */ char reserved5CD;
    /* 0x5CE */ short reserved5CE;
    /* 0x5D0 */ int reserved5D0;
    /* 0x5D4 */ int reserved5D4;
    /* 0x5D8 */ void *reserved5D8;
    /* 0x5DC */ int reserved5DC;
    /* 0x5E0 */ int reserved5E0;
    /* 0x5E4 */ int reserved5E4;
    /* 0x5E8 */ int reserved5E8;
    /* 0x5EC */ u_char reserved5EC[80];
    /* 0x63C */ u_short collisionRadius;
    /* 0x63E */ u_short reserved63E;
    /* 0x640 */ u_char reserved640[8];
    /* 0x648 */ int reserved648;
    /* 0x64C */ u_char reserved64C;
    /* 0x64D */ u_char reserved64D;
    /* 0x64E */ u_short reserved64E;
    /* 0x650 */ u_short reserved650;
    /* 0x652 */ u_short reserved652;
    /* 0x654 */ u_short reserved654;
    /* 0x656 */ u_short reserved656;
    /* 0x658 */ u_char reserved658[136];
    /* 0x6E0 */ u_char reserved6E0;
    /* 0x6E1 */ u_char reserved6E1;
    /* 0x6E2 */ u_char reserved6E2;
    /* 0x6E3 */ u_char reserved6E3;
    /* 0x6E4 */ u_char reserved6E4;
    /* 0x6E5 */ u_char reserved6E5;
    /* 0x6E6 */ u_short reserved6E6;
    /* 0x6E8 */ short reserved6E8;
    /* 0x6EA */ short reserved6EA;
    /* 0x6EC */ int reserved6EC;
    /* 0x6F0 */ u_char reserved6F0;
    /* 0x6F1 */ u_char fadeGreen;
    /* 0x6F2 */ u_char fadeRed;
    /* 0x6F3 */ u_char reserved6F3;
    /* 0x6F4 */ Actors_CompanionOffset reserved6F4;
    /* 0x6F8 */ Render_ColorCode reserved6F8;
    /* 0x6FC */ int reserved6FC;
    /* 0x700 */ int reserved700;
    /* 0x704 */ Actors_AnimationPose reserved704;
    /* 0xC54 */ Actors_AnimationPose reservedC54;
    /* 0x11A4 */ u_char reserved11A4[640];
    /* 0x1424 */ u_short basePaletteRows[160];
    /* 0x1564 */ u_short transitionPalette[160];
    /* 0x16A4 */ u_char reserved16A4[344];
    /* 0x17FC */ u_char reserved17FC;
    /* 0x17FD */ u_char reserved17FD;
    /* 0x17FE */ u_char reserved17FE;
    /* 0x17FF */ u_char reserved17FF;
    /* 0x1800 */ u_char reserved1800[60];
    /* 0x183C */ short reserved183C;
    /* 0x183E */ short reserved183E;
    /* 0x1840 */ short reserved1840;
    /* 0x1842 */ short reserved1842;
    /* 0x1844 */ short reserved1844;
    /* 0x1846 */ u_char reserved1846;
    /* 0x1847 */ u_char reserved1847;
    /* 0x1848 */ u_char reserved1848;
    /* 0x1849 */ u_char reserved1849;
    /* 0x184A */ u_char reserved184A;
    /* 0x184B */ u_char reserved184B;
    /* 0x184C */ u_short reserved184C;
    /* 0x184E */ u_short reserved184E;
    /* 0x1850 */ u_int reserved1850;
    /* 0x1854 */ u_char reserved1854[16];
    /* 0x1864 */ Actors_ShadowBlend shadowBlend;
    /* 0x1878 */ int savedPlacement;
    /* 0x187C */ int reserved187C;
} Field_GimmickInstance;

// decomp: func_800A0024_t (src/BATTLE/BATTLE.PRG/30DB0.c:24)
typedef struct Field_GimmickSnapshot {
    u_char placementByte0;
    u_char heightAboveGround;
    u_char placementByte2;
    u_char orientation;
    int positionXY;
    int positionZ;
} Field_GimmickSnapshot;
_Static_assert(sizeof(Field_GimmickSnapshot) == 0xC, "Field_GimmickSnapshot must keep its PS1 layout");

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
    char reserved60[0x10C];
    u_char material, durability, reserved16E, carrier, frame;
};
typedef struct Field_GimmickPhysicsView Field_GimmickPhysicsView;
_Static_assert(sizeof(Field_GimmickPhysicsView) == 0x174, "Field_GimmickPhysicsView must keep its PS1 layout");

#endif // VS_FIELD_GIMMICKS_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_FIELD_GIMMICKS_H_DECLS)
#define VS_FIELD_GIMMICKS_H_DECLS
#include "ActorAssets.h"
#include "Actors/ActorRecord.h"
#include "Actors/Animation.h"
#include "Actors/ModelInstance.h"
#include "Actors/Movement.h"
#include "Actors/Shadow.h"
#include "Actors/Skeleton.h"
#include "Actors/TextureAnimation.h"
#include "Field/Actors.h"
#include "Field/Doors.h"
#include "Field/GimmickMotion.h"
#include "Field/Map.h"
#include "Field/Mpd.h"
#include "Field/Platforms.h"
#include "Field/Room.h"
#include "Field/TriggerMarkers.h"
#include "Field/Zone.h"
#include "Items/Loot.h"
#include "Items/TreasureChests.h"
#include "Render/Palette.h"

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
void Field_DisableTrapAtPosition(SVECTOR* arg0);
void Field_FreezeRoomGimmicksTemporarily(void);
int Field_CurrentSavePointId(void);
void Field_ResetGimmickAnimationQueue(void);
void Field_QueueGimmickAnimation(int arg0, Field_MpdLockTrigger* arg1);
void Field_UpdateGimmickAnimationQueue(void);
int Field_SetMovingPlatformActor(int arg0, int arg1);
void Field_ClearMovingPlatformActor(int arg0);
void Field_InitializeMovingPlatforms(void);
void Field_ResetMovingPlatformWaypoint(int arg0, int arg1);
int Field_UpdateMovingPlatforms(void);
void Field_DrawMovingPlatformShadows(void);
int Field_CollectMovingPlatformPositions(Field_PlatformContactPoint arg0[]);
Field_MpdFloorTrigger* Field_RoomTriggerAtTile(int arg0, int arg1, int arg2);
void Field_SetMovingPlatformsPaused(int arg0);
void Field_FreezeMovingPlatforms(int arg0);
void Field_BeginSwitchToggle(Field_MpdLockTrigger* arg0);
void Field_ToggleRoomSwitch(Field_MpdLockTrigger* arg0);
void Field_SetRoomTriggerEnabled(Field_MpdFloorTrigger* arg0, int arg1);
void Field_DrawRoomTriggerMarkers(void);
void Field_DrawRoomTriggerMarker(int arg0, int arg1, short arg2, u_int arg3);
void Field_SetMovingPlatformVisibility(int arg0, int arg1);
void* Field_MovingPlatformTargetWaypoint(int arg0);
u_int Field_MovingPlatformDwellTicks(int arg0);
Field_MpdFloorTrigger* Field_ActiveRoomTriggers(int* arg0);
void Field_SetPlatformFreezeAppearance(int arg0);
int Field_FindSharedGimmickModel(Content_ActorAssetRequest* arg0);
void Field_SetGimmickPaletteTable(void* arg0);
int Field_CreateGimmickInstance(Content_ActorAssetRequest* arg0);
int Field_ReleaseGimmickInstance(int arg0);
void Field_RefreshAshleyInteractionTile(Actors_ActorRecord* arg0);
void Field_DropUnsupportedGimmickStack(int arg0);
int Field_FindGimmickBelow(Field_GimmickInstance* arg0);
int Field_GimmickSubtype(int arg0);
int Field_GimmickPlacement(int arg0, Field_GimmickSnapshot* arg1);
int Field_SetGimmickOrientation(int index, int material);
int Field_FindInteractableGimmickAhead(int actorId);
Actors_ActorRecord* Field_FindActorAboveGimmick(Field_GimmickInstance* arg0, u_int arg1);
int Field_CanPlaceGimmickAtPoint(int x, int z, int minimumHeight);
int Field_BeginGimmickInteraction(int actorId);
void Field_ApplyGimmickInteractionCommand(Actors_ActorRecord* actor, int mode);
int Field_AreGimmicksMoving(void);
int Field_FindTopGimmickAtTile(int arg0, int arg1, int arg2);
void Field_UpdateGimmickMotion(Field_GimmickPhysicsView* object, int unused);
int Field_UpdateGimmickReactionAnimation(Field_GimmickMotionView* object);
int Field_MeasureTileStack(int arg0, int arg1, int* arg2);
int Field_IsGimmickWithinHeightBand(int arg0, int arg1, int arg2);

#endif // VS_FIELD_GIMMICKS_H_DECLS
