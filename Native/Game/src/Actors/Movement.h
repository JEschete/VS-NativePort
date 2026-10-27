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
#include "Actors/ActorModelSlot.h"
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
void Actors_ConsumeActorMovementOffset(Battle_Actor* actor, SVECTOR* offset_out);
int Actors_ExecuteQueuedActorCommands(void);
void Actors_BeginScriptedActorPath(int actor_id, SVECTOR* target_position);
int Actors_IsScriptedActorPathFinished(int actor_id);
void Actors_ResetActorMovement(int actor_id);
void Actors_StopActorMotion(int actor_id);
void Actors_ClearActorMovementModes(int actor_id);
int Actors_ActorFacingDeltaToPoint(Actors_ActorRecord* actor, SVECTOR* target_point);
void Actors_ResetActorRunMomentum(int actor_id);
void Actors_SetActorWalkRunSpeeds(int actor_id, int walk_speed_fixed, int run_speed_fixed);
void Actors_SetActorTravelStepCounts(int actor_id, int move_steps_per_tile, int turn_steps_per_revolution);
void Actors_ActorTravelStepCounts(int actor_id, int* move_steps_per_tile_out, int* turn_steps_per_revolution_out);
void Actors_BeginActorJumpToTile(Actors_ActorRecord* actor, u_char* target, int speed, short facing);
void Actors_ApplyActorGroundOffset(Actors_ActorRecord* actor);
void Actors_ApplyActorFlightMovement(Actors_ActorRecord* actor);
void Actors_ApplyActorMovementVector(Actors_ActorRecord* actor);
void Actors_UpdateActorTeleportTransition(Actors_ActorRecord* actor);
void Actors_CancelActorTeleportTransition(Actors_ActorRecord* actor);
void Actors_UpdateActorTravelTransition(Actors_ActorRecord* actor);
void Actors_StepActorTransitionDisplacement(Actors_ActorRecord* actor);
void Actors_SetActorFallDestination(Actors_ActorRecord* actor, Actors_FallDirection* fall_direction);
void Actors_BeginActorJumpMovement(int index, int direction, int distance);
void Actors_UpdateActorPlatformContact(Actors_ActorRecord* actor, int mode);
void Actors_UpdateRoomActorMovement(void);
void Actors_UpdateActorMovement(int actorId);
void Actors_ClearActorMovementRequest(Actors_ActorRecord* actor);
int Actors_MoveActorWithCollision(int index, int direction, int distance, SVECTOR* input);
void Actors_UpdateActorSteering(Actors_ActorRecord* actor, int target_facing, Actors_TurnRequest* turn_request);
void Actors_MoveActorToTileAtRate(int actor_id, Actors_TileTarget* tile_target, int move_steps_per_tile, int turn_steps_per_revolution);
void Actors_MoveActorToPositionAtRate(int id, SVECTOR* target, int move_steps_per_tile, int turn_steps_per_revolution);
void Actors_TranslateActorToTileAtRate(int actor_id, Actors_TileTarget* tile_target, int move_steps_per_tile);
void Actors_TurnActorTowardTileAtRate(int actor_id, Actors_TileTarget* tile_target, int turn_steps_per_revolution);
void Actors_TranslateActorByAtRate(int actor_id, SVECTOR* translation, int move_steps_per_tile);
void Actors_TranslateActorToAtRate(int actor_id, SVECTOR* target_position, int move_steps_per_tile);
void Actors_TurnActorByAtRate(int actor_id, short yaw_delta, int turn_steps_per_revolution);
void Actors_TurnActorToAtRate(int actor_id, short target_yaw, int turn_steps_per_revolution);
void Actors_RotateActorByAtRate(int actor_id, SVECTOR* rotation_delta, int turn_steps_per_revolution);
void Actors_RotateActorToAtRate(int actor_id, SVECTOR* target_rotation, int turn_steps_per_revolution);
void Actors_MoveActorToTileOverTime(int actor_id, Actors_TileTarget* tile_target, int move_steps, int turn_steps);
void Actors_TranslateActorToTileOverTime(int actor_id, Actors_TileTarget* tile_target, int move_steps);
void Actors_SetActorTileTranslation(int actor_id, Actors_TileTarget* tile_target, int move_steps, int move_steps_per_tile);
void Actors_TurnActorTowardTileOverTime(int actor_id, Actors_TileTarget* tile_target, int turn_steps);
void Actors_SetActorTileTurn(int actor_id, Actors_TileTarget* tile_target, int turn_steps, int turn_steps_per_revolution);
void Actors_SetActorMotion(int actor_id, SVECTOR* translation, int move_steps);
void Actors_TranslateActorToOverTime(int actor_id, SVECTOR* target_position, int move_steps);
void Actors_SetActorTranslation(int actor_id, SVECTOR* translation, int move_steps);
void Actors_SetActorYawMotion(int actor_id, short yaw_delta, int turn_steps);
void Actors_TurnActorToOverTime(int actor_id, u_short target_yaw, int turn_steps);
void Actors_TurnActorToAtAngularSpeed(int actor_id, short target_yaw, int angular_speed);
void Actors_SetActorYawTurn(int actor_id, short yaw_delta, int turn_steps);
void Actors_SetActorRotationMotion(int actor_id, SVECTOR* rotation_delta, int turn_steps);
void Actors_SetActorRotation(int actor_id, SVECTOR* rotation_delta, int turn_steps);
void Actors_ScaleActorToOverTime(int actor_id, SVECTOR* target_scale, int scale_steps);
void Actors_UpdateActorRoute(int actor_id, Actors_ActorRecord* actor);
int Actors_ActorJumpAnimationProfile(Actors_ActorRecord* actor);
void Actors_UpdateActorJumpAndFall(int actor_id, Actors_ActorRecord* actor);
void Actors_UpdateActorAuxiliaryMotion(Field_GimmickInstance* gimmick);

#endif // VS_ACTORS_MOVEMENT_H_DECLS
