// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_BATTLE_AI_MOVEMENT_H_TYPES
#define VS_BATTLE_AI_MOVEMENT_H_TYPES
#include "Legacy/Psx.h"

typedef struct Battle_Ai_ArenaOrbitState Battle_Ai_ArenaOrbitState;

// layout: ledger (upstream merge) for D_800F5920_t
typedef struct Battle_Ai_ArenaOrbitState {
    /* 0x00 */ u_short heading;
    /* 0x02 */ u_char phase;
    /* 0x03 */ u_char distanceCached;
    /* 0x04 */ int distanceSquared;
    /* 0x08 */ char reserved8[2];
    /* 0x0A */ u_char isOrbiting;
    /* 0x0B */ u_char pathStep;
    /* 0x0C */ short cooldown;
    /* 0x0E */ short requested;
    /* 0x10 */ short orbitAngle;
    /* 0x12 */ short savedHeight;
    /* 0x14 */ u_char healthPhase;
    /* 0x15 */ char reserved15;
    /* 0x16 */ u_char planSaved;
    /* 0x17 */ u_char savedPlanCount;
    /* 0x18 */ short startX;
    /* 0x1A */ short startZ;
    /* 0x1C */ u_char started;
} Battle_Ai_ArenaOrbitState;
_Static_assert(sizeof(Battle_Ai_ArenaOrbitState) == 0x20, "Battle_Ai_ArenaOrbitState must keep its PS1 layout");

#endif // VS_BATTLE_AI_MOVEMENT_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_BATTLE_AI_MOVEMENT_H_DECLS)
#define VS_BATTLE_AI_MOVEMENT_H_DECLS
#include "Actors/ActorAllocation.h"
#include "Actors/ActorRecord.h"
#include "Actors/Animation.h"
#include "Actors/ModelInstance.h"
#include "Actors/Movement.h"
#include "Actors/Shadow.h"
#include "Actors/TextureAnimation.h"
#include "Battle/Actor.h"
#include "Battle/Ai/AiState.h"
#include "Battle/Ai/AiViews.h"
#include "Battle/Ai/Navigation.h"
#include "Battle/Ai/Perception.h"
#include "Items/Inventory.h"
#include "Items/Loadout.h"
#include "Render/Palette.h"

extern int Battle_Ai_savedMovementRandomState;
extern unsigned short (*Battle_Ai_patrolRoutes)[8];
int Battle_Ai_HasRepeatingMovementHistory(Battle_Ai_PerceptionUpdateView* state);
int Battle_Ai_IsMovementHistoryStalled(Battle_Ai_PerceptionUpdateView* state);
void Battle_Ai_SaveMovementRandomState(void);
int Battle_Ai_TryRecoverBlockedMovement(Battle_Ai_BlockedRecoveryView* state);
int Battle_Ai_IsFlightDirectionObstructed(Battle_Ai_ObstructionView* state, int direction, int ahead);
int Battle_Ai_CanFlyInDirection(Battle_Ai_DirectionCacheView* state, int direction);
int Battle_Ai_UpdateFlightGroundTransition(Battle_Ai_MovementCheckView* state, int action, int mode);
void Battle_Ai_UpdateFlightMovement(Battle_Ai_MovementStepView* state, int action);
void Battle_Ai_UpdateFlightDirectionalMovement(Battle_Ai_ActorAiState* arg0, int arg1);
void Battle_Ai_ProjectClearRadialDestination(Battle_Ai_MoveTargetView* state, Battle_Ai_TilePosition source, int angle,
    unsigned int distance);
void Battle_Ai_MoveToRadialDestination(Battle_Ai_MovementDestinationView* state, int unused);
void Battle_Ai_MoveAwayFromReference(Battle_Ai_MovementDestinationView* state, int angle);
u_char Battle_Ai_FilterDirectionsByGroundHeight(u_char arg0, u_int arg1, int arg2);
void Battle_Ai_MoveWithHeightFilteredDirections(Battle_Ai_TerrainMoveView* state);
Battle_Ai_DirectionDelta* Battle_Ai_WanderDirectionOffset(int index);
int Battle_Ai_ChooseNextWanderTile(Battle_Ai_WanderView* state, unsigned int tile, unsigned int maxCount, int arg3);
int Battle_Ai_RandomWanderHeading(int direction);
void Battle_Ai_StartWanderWalk(Battle_Ai_WanderView* s);
void Battle_Ai_UpdateWanderMovement(Battle_Ai_WanderView* state, unsigned int tile, unsigned int range);
void Battle_Ai_UpdateWaypointPatrol(Battle_Ai_PatrolView* state);
int Battle_Ai_UpdateTargetApproachMovement(Battle_Ai_TargetApproachView* s, int option);
int Battle_Ai_UpdateTargetFacing(Battle_Ai_MovementDecisionView* context, int targetId);
void Battle_Ai_UpdateMovementRecovery(Battle_Ai_MovementRecoveryView* state);

#endif // VS_BATTLE_AI_MOVEMENT_H_DECLS
