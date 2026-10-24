// Hand-maintained (tools/port/owned.csv).
#include "Battle/Ai/FlightTerrain.h"

#include "Actors/ActorAllocation.h"
#include "Actors/ActorRecord.h"
#include "Actors/Animation.h"
#include "Actors/ModelInstance.h"
#include "Actors/Movement.h"
#include "Actors/Shadow.h"
#include "Actors/Skeleton.h"
#include "Actors/TextureAnimation.h"
#include "Battle/Actor.h"
#include "Battle/Ai/ActionSelection.h"
#include "Battle/Ai/ActorCleanup.h"
#include "Battle/Ai/ActorCommands.h"
#include "Battle/Ai/ActorState.h"
#include "Battle/Ai/AiState.h"
#include "Battle/Ai/AiViews.h"
#include "Battle/Ai/DecisionPrimitives.h"
#include "Battle/Ai/FrameUpdate.h"
#include "Battle/Ai/GroundNavigation.h"
#include "Battle/Ai/Movement.h"
#include "Battle/Ai/MovementBehavior.h"
#include "Battle/Ai/Navigation.h"
#include "Battle/Ai/NavigationPrimitives.h"
#include "Battle/Ai/OrbitingBoss.h"
#include "Battle/Ai/Perception.h"
#include "Battle/Ai/RoomState.h"
#include "Battle/Damage.h"
#include "Core/Math/ScalarMath.h"
#include "Core/Math/TileDistance.h"
#include "Field/Collision.h"
#include "Field/Gimmicks.h"
#include "Field/Platforms.h"
#include "Field/TerrainHeight.h"
#include "Items/Inventory.h"
#include "Items/Loadout.h"
#include "Legacy/Psx.h"
#include "Render/Palette.h"

enum {
  kFlightAngleFractionMask = 0x3FF,
  kFlightDirectionShift = 9,
  kFlightCardinalDirectionMask = 6,
  kFlightDiagonalDirectionCount = 3,
  kFlightDirectionMask = 7,
  kNavigationHeightMask = 0x1FFFF,
  kNavigationHeightSign = 0x10000,
  kNavigationHeightModulus = 0x20000,
  kSegmentSampleShift = 4,
  kSegmentSampleCount = 16,
};

// decomp: BATTLE.PRG func_800E0850 (src/BATTLE/BATTLE.PRG/771A8.c:942).
int Battle_Ai_LowestReachableGroundHeight(Battle_Ai_ActorAiState* state,
                                          u_int movement_angle) {
  int minimum_height = 0;
  int remaining_directions = 1;
  int direction;

  if (movement_angle & kFlightAngleFractionMask) {
    remaining_directions = kFlightDiagonalDirectionCount;
  }
  direction = (int)((movement_angle >> kFlightDirectionShift) &
                    kFlightCardinalDirectionMask);
  while (--remaining_directions != -1) {
    if (Battle_Ai_CanFlyInDirection((void*)state, direction) != 0) {
      Battle_Ai_DirectionDelta* direction_deltas =
          (Battle_Ai_DirectionDelta*)Battle_Ai_FrameState()->directions;
      int tile_x = state->tile.tile.tileX + direction_deltas[direction].dx;
      int tile_z = state->tile.tile.tileZ + direction_deltas[direction].dz;
      int ground_height = Field_GroundHeightAtTile(tile_x, tile_z);
      if (ground_height < minimum_height) {
        minimum_height = ground_height;
      }
    }
    direction = (direction + 1) & kFlightDirectionMask;
  }
  return minimum_height;
}

static int SignedNavigationGroundHeight(int height) {
  unsigned int value = (unsigned int)height & kNavigationHeightMask;
  return (value & kNavigationHeightSign) != 0
             ? (int)value - kNavigationHeightModulus
             : (int)value;
}

// decomp: BATTLE.PRG func_800E6A6C (src/BATTLE/BATTLE.PRG/7E05C.s:187).
int Battle_Ai_MinimumAiGroundHeightAlongSegment(int start_x, int start_z,
                                                int end_x, int end_z) {
  int step_x = (end_x - start_x) >> kSegmentSampleShift;
  int step_z = (end_z - start_z) >> kSegmentSampleShift;
  int minimum_height = 0;
  int sample_index;

  for (sample_index = 0; sample_index < kSegmentSampleCount; ++sample_index) {
    start_x += step_x;
    start_z += step_z;
    int ground_height = SignedNavigationGroundHeight(
        Field_GroundHeightFlagsAtWorldPosition(start_x, start_z));
    if (ground_height < minimum_height) {
      minimum_height = ground_height;
    }
  }
  return minimum_height;
}

// decomp: BATTLE.PRG func_800E75EC (src/BATTLE/BATTLE.PRG/7E05C.s:1041).
int Battle_Ai_RoomNavigationCeilingHeight(void) {
  return Battle_Ai_roomAiState == NULL ? 0
                                       : Battle_Ai_roomAiState->flightCeiling;
}
