// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#pragma once
#include "Legacy/Psx.h"

struct Actors_AirborneTickAccumulator;
struct Actors_FallDirection;
struct Actors_MovementRequest;
struct Actors_TurnRequest;

// decomp: D_800F4538_unk1848 (src/BATTLE/BATTLE.PRG/3A1A0.h:111)
struct Actors_MovementRequest {
    u_char mode;
    u_char reserved1;
    u_char reserved2;
    u_char reserved3;
    short direction;
    short facing;
    u_int distance;
    int reservedC;
    SVECTOR step;
};
static_assert(sizeof(Actors_MovementRequest) == 0x18, "Actors_MovementRequest must keep its PS1 layout");

// decomp: func_800A3054_t (src/BATTLE/BATTLE.PRG/3A1A0.c:13)
struct Actors_FallDirection {
    short directionX;
    short reserved2;
    short directionZ;
};
static_assert(sizeof(Actors_FallDirection) == 0x6, "Actors_FallDirection must keep its PS1 layout");

// decomp: func_800A6660_t (src/BATTLE/BATTLE.PRG/3A1A0.c:25)
struct Actors_TurnRequest {
    char reserved0[6];
    short restartTurn;
};
static_assert(sizeof(Actors_TurnRequest) == 0x8, "Actors_TurnRequest must keep its PS1 layout");

// decomp: func_800AC690_timer_t (src/BATTLE/BATTLE.PRG/40564.c:47)
struct Actors_AirborneTickAccumulator {
    u_int reserved0 : 24;
    u_int tickRemainder : 8;
};
static_assert(sizeof(Actors_AirborneTickAccumulator) == 0x4, "Actors_AirborneTickAccumulator must keep its PS1 layout");

#include "Actors/ActorRecord.h"
#include "Actors/Animation.h"
#include "Actors/ModelInstance.h"
#include "Actors/Shadow.hpp"
#include "Actors/Skeleton.h"
#include "Actors/TextureAnimation.h"
#include "Render/Palette.hpp"

extern u_char Actors_kJumpHangFrames[3204];
extern u_char Actors_kJumpHeightAdjustments[3196];
extern u_char Actors_kMotionResultAnimations[3181];
extern u_char Actors_kMotionResultOneAnimationId;
extern u_char Actors_kHangMotionAnimationId;
extern u_char Actors_kSlowQuadrantMoveAnimations[3172];
extern u_char Actors_kFastQuadrantMoveAnimations[3168];
extern u_char Actors_kAirTimerEndAnimations[2632];
extern u_char Actors_kSoftLandingAnimations[2624];
extern u_char Actors_kHardLandingAnimations[2616];
extern u_char Actors_kLandingRecoveryAnimations[2608];
extern Actors_ActorRecord* Actors_actorRecords[17];
extern u_short Actors_supportingPlatformLayer;
extern u_char Actors_supportingGimmickIndex;
extern char Actors_isActorUpdateComplete;
extern u_char Actors_pushedGimmickIndex;
