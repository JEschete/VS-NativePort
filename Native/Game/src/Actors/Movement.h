// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_ACTORS_MOVEMENT_H_TYPES
#define VS_ACTORS_MOVEMENT_H_TYPES
#include "Legacy/Psx.h"

typedef struct Actors_AirborneTickAccumulator Actors_AirborneTickAccumulator;
typedef struct Actors_FallDirection Actors_FallDirection;
typedef struct Actors_MovementRequest Actors_MovementRequest;
typedef struct Actors_TurnRequest Actors_TurnRequest;

// decomp: D_800F4538_unk1848 (src/BATTLE/BATTLE.PRG/3A1A0.h:111)
typedef struct Actors_MovementRequest {
    u_char mode;
    u_char reserved1;
    u_char reserved2;
    u_char reserved3;
    short direction;
    short facing;
    u_int distance;
    int reservedC;
    SVECTOR step;
} Actors_MovementRequest;
_Static_assert(sizeof(Actors_MovementRequest) == 0x18, "Actors_MovementRequest must keep its PS1 layout");

// decomp: func_800A3054_t (src/BATTLE/BATTLE.PRG/3A1A0.c:13)
typedef struct Actors_FallDirection {
    short directionX;
    short reserved2;
    short directionZ;
} Actors_FallDirection;
_Static_assert(sizeof(Actors_FallDirection) == 0x6, "Actors_FallDirection must keep its PS1 layout");

// decomp: func_800A6660_t (src/BATTLE/BATTLE.PRG/3A1A0.c:25)
typedef struct Actors_TurnRequest {
    char reserved0[6];
    short restartTurn;
} Actors_TurnRequest;
_Static_assert(sizeof(Actors_TurnRequest) == 0x8, "Actors_TurnRequest must keep its PS1 layout");

// decomp: func_800AC690_timer_t (src/BATTLE/BATTLE.PRG/40564.c:47)
typedef struct Actors_AirborneTickAccumulator {
    u_int reserved0 : 24;
    u_int tickRemainder : 8;
} Actors_AirborneTickAccumulator;
_Static_assert(sizeof(Actors_AirborneTickAccumulator) == 0x4, "Actors_AirborneTickAccumulator must keep its PS1 layout");

#endif // VS_ACTORS_MOVEMENT_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_ACTORS_MOVEMENT_H_DECLS)
#define VS_ACTORS_MOVEMENT_H_DECLS
#include "Actors/ActorAllocation.h"
#include "Actors/ActorRecord.h"
#include "Actors/Animation.h"
#include "Actors/Location.h"
#include "Actors/ModelInstance.h"
#include "Actors/Shadow.h"
#include "Actors/Skeleton.h"
#include "Actors/TextureAnimation.h"
#include "Battle/Actor.h"
#include "Field/Gimmicks.h"
#include "Items/Inventory.h"
#include "Items/Loadout.h"
#include "Render/Palette.h"

extern u_char Actors_kJumpHangFrames[3204];
extern u_char Actors_kJumpHeightAdjustments[3196];
extern int Actors_motionResultResetWord;
extern u_char Actors_kMotionResultAnimations[3181];
extern u_char Actors_kMotionResultOneAnimationId;
extern u_char Actors_kHangMotionAnimationId;
extern u_char Actors_kSlowQuadrantMoveAnimations[3172];
extern u_char Actors_kFastQuadrantMoveAnimations[3168];
extern u_char Actors_kAirTimerEndAnimations[2632];
extern u_char Actors_kSoftLandingAnimations[2624];
extern u_char Actors_kHardLandingAnimations[2616];
extern u_char Actors_kLandingRecoveryAnimations[2608];
extern int Actors_ashleyMoveInput[12];
extern Actors_ActorRecord* Actors_actorRecords[17];
extern u_short Actors_supportingPlatformLayer;
extern u_char Actors_supportingGimmickIndex;
extern char Actors_isActorUpdateComplete;
extern u_char Actors_pushedGimmickIndex;
int Actors_UpdateAshleyMovementInput(void);
void Actors_ConsumeActorMovementOffset(Battle_Actor* arg0, SVECTOR* arg1);
int Actors_ExecuteQueuedActorCommands(void);
void Actors_BeginScriptedActorPath(int arg0, SVECTOR* arg1);
int Actors_IsScriptedActorPathFinished(int arg0);
void Actors_ResetActorMovement(int arg0);
void Actors_StopActorMotion(int arg0);
void Actors_ClearActorMovementModes(int arg0);
int Actors_ActorFacingDeltaToPoint(Actors_ActorRecord* arg0, SVECTOR* arg1);
void Actors_ResetActorRunMomentum(int arg0);
void Actors_SetActorWalkRunSpeeds(int arg0, int arg1, int arg2);
void Actors_SetActorTravelStepCounts(int arg0, int arg1, int arg2);
void Actors_ActorTravelStepCounts(int arg0, int* arg1, int* arg2);
void Actors_BeginActorJumpToTile(Actors_ActorRecord* actor, u_char* target, int speed, short facing);
void Actors_ApplyActorGroundOffset(Actors_ActorRecord* actor);
void Actors_ApplyActorFlightMovement(Actors_ActorRecord* actor);
void Actors_ApplyActorMovementVector(Actors_ActorRecord* arg0);
void Actors_UpdateActorTeleportTransition(Actors_ActorRecord* arg0);
void Actors_CancelActorTeleportTransition(Actors_ActorRecord* arg0);
void Actors_UpdateActorTravelTransition(Actors_ActorRecord* arg0);
void Actors_StepActorTransitionDisplacement(Actors_ActorRecord* arg0);
void Actors_SetActorFallDestination(Actors_ActorRecord* arg0, Actors_FallDirection* arg1);
void Actors_BeginActorJumpMovement(int index, int direction, int distance);
void Actors_UpdateActorPlatformContact(Actors_ActorRecord* actor, int mode);
void Actors_UpdateRoomActorMovement(void);
void Actors_UpdateActorMovement(int actorId);
void Actors_ClearActorMovementRequest(Actors_ActorRecord* arg0);
int Actors_MoveActorWithCollision(int index, int direction, int distance, SVECTOR* input);
void Actors_UpdateActorSteering(Actors_ActorRecord* arg0, int arg1, Actors_TurnRequest* arg2);
void Actors_MoveActorToTileAtRate(int arg0, Actors_TileTarget* arg1, int arg2, int arg3);
void Actors_MoveActorToPositionAtRate(int id, SVECTOR* target, int arg2, int arg3);
void Actors_TranslateActorToTileAtRate(int arg0, Actors_TileTarget* arg1, int arg2);
void Actors_TurnActorTowardTileAtRate(int arg0, Actors_TileTarget* arg1, int arg2);
void Actors_TranslateActorByAtRate(int arg0, SVECTOR* arg1, int arg2);
void Actors_TranslateActorToAtRate(int arg0, SVECTOR* arg1, int arg2);
void Actors_TurnActorByAtRate(int arg0, short arg1, int arg2);
void Actors_TurnActorToAtRate(int arg0, short arg1, int arg2);
void Actors_RotateActorByAtRate(int arg0, SVECTOR* arg1, int arg2);
void Actors_RotateActorToAtRate(int arg0, SVECTOR* arg1, int arg2);
void Actors_MoveActorToTileOverTime(int arg0, Actors_TileTarget* arg1, int arg2, int arg3);
void Actors_TranslateActorToTileOverTime(int arg0, Actors_TileTarget* arg1, int arg2);
void Actors_SetActorTileTranslation(int arg0, Actors_TileTarget* arg1, int arg2, int arg3);
void Actors_TurnActorTowardTileOverTime(int arg0, Actors_TileTarget* arg1, int arg2);
void Actors_SetActorTileTurn(int arg0, Actors_TileTarget* arg1, int arg2, int arg3);
void Actors_SetActorMotion(int arg0, SVECTOR* arg1, int arg2);
void Actors_TranslateActorToOverTime(int arg0, SVECTOR* arg1, int arg2);
void Actors_SetActorTranslation(int arg0, SVECTOR* arg1, int arg2);
void Actors_SetActorYawMotion(int arg0, short arg1, int arg2);
void Actors_TurnActorToOverTime(int arg0, u_short arg1, int arg2);
void Actors_TurnActorToAtAngularSpeed(int arg0, short arg1, int arg2);
void Actors_SetActorYawTurn(int arg0, short arg1, int arg2);
void Actors_SetActorRotationMotion(int arg0, SVECTOR* arg1, int arg2);
void Actors_SetActorRotation(int arg0, SVECTOR* arg1, int arg2);
void Actors_ScaleActorToOverTime(int arg0, SVECTOR* arg1, int arg2);
void Actors_UpdateActorRoute(int arg0, Actors_ActorRecord* arg1);
int Actors_ActorJumpAnimationProfile(Actors_ActorRecord* actor);
void Actors_UpdateActorJumpAndFall(int arg0, Actors_ActorRecord* actor);
void Actors_SaveActorGroundedTransform(Field_GimmickInstance* arg0);
void Actors_UpdateActorAuxiliaryMotion(Field_GimmickInstance* arg0);
void Actors_BeginTileJump(int, int*, int, int);
void Actors_BeginAirborneTileJump(int, int*, int, int);
void Actors_BeginNavigationJump(int, int*, int, int);
void Actors_BeginTeleport(int, int*, int);
void Actors_BeginTileTravel(int, int*, int);
void Actors_BeginTimedTravel(int, int*, int);

#endif // VS_ACTORS_MOVEMENT_H_DECLS
