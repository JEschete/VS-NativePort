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
int Battle_Ai_LowestReachableGroundHeight(Battle_Ai_ActorAiState* arg0,
                                          u_int arg1) {
  int min = 0;
  int count = 1;
  int direction;

  if (arg1 & kFlightAngleFractionMask) {
    count = kFlightDiagonalDirectionCount;
  }
  direction =
      (int)((arg1 >> kFlightDirectionShift) & kFlightCardinalDirectionMask);
  while (--count != -1) {
    if (Battle_Ai_CanFlyInDirection((void*)arg0, direction) != 0) {
      Battle_Ai_DirectionDelta* directions =
          (Battle_Ai_DirectionDelta*)Battle_Ai_FrameState()->directions;
      int x = arg0->tile.tile.tileX + directions[direction].dx;
      int z = arg0->tile.tile.tileZ + directions[direction].dz;
      int h = Field_GroundHeightAtTile(x, z);
      if (h < min) {
        min = h;
      }
    }
    direction = (direction + 1) & kFlightDirectionMask;
  }
  return min;
}

static int SignedNavigationGroundHeight(int height) {
  unsigned int value = (unsigned int)height & kNavigationHeightMask;
  return (value & kNavigationHeightSign) != 0
             ? (int)value - kNavigationHeightModulus
             : (int)value;
}

// decomp: BATTLE.PRG func_800E6A6C (src/BATTLE/BATTLE.PRG/7E05C.s:187).
int Battle_Ai_MinimumAiGroundHeightAlongSegment(int x1, int z1, int x2,
                                                int z2) {
  int dx = (x2 - x1) >> kSegmentSampleShift;
  int dz = (z2 - z1) >> kSegmentSampleShift;
  int minimum = 0;
  int index;

  for (index = 0; index < kSegmentSampleCount; ++index) {
    x1 += dx;
    z1 += dz;
    int height = SignedNavigationGroundHeight(
        Field_GroundHeightFlagsAtWorldPosition(x1, z1));
    if (height < minimum) {
      minimum = height;
    }
  }
  return minimum;
}

// decomp: BATTLE.PRG func_800E75EC (src/BATTLE/BATTLE.PRG/7E05C.s:1041).
int Battle_Ai_RoomNavigationCeilingHeight(void) {
  return Battle_Ai_roomAiState == NULL ? 0
                                       : Battle_Ai_roomAiState->flightCeiling;
}
