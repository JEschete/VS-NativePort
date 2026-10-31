// Hand-maintained (tools/port/owned.csv).
extern "C" {
#include "Battle/Ai/RoomState.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "Actors/ActorAllocation.h"
#include "Actors/ActorRecord.h"
#include "Actors/Animation.h"
#include "Actors/ModelInstance.h"
#include "Actors/Shadow.h"
#include "Actors/Skeleton.h"
#include "Battle/Actor.h"
#include "Battle/Ai/ActionPlanning.h"
#include "Battle/Ai/ActionSelection.h"
#include "Battle/Ai/ActorCleanup.h"
#include "Battle/Ai/ActorState.h"
#include "Battle/Ai/AiState.h"
#include "Battle/Ai/AiViews.h"
#include "Battle/Ai/Behavior.h"
#include "Battle/Ai/FrameUpdate.h"
#include "Battle/Ai/Movement.h"
#include "Battle/Ai/Navigation.h"
#include "Battle/Ai/NavigationPrimitives.h"
#include "Battle/Ai/OrbitingBoss.h"
#include "Battle/Ai/Perception.h"
#include "Battle/Damage.h"
#include "Core/Diagnostics.h"
#include "Core/Math/ScalarMath.h"
#include "Core/Math/Vector.h"
#include "Core/Memory/Heap.h"
#include "Core/Memory/WordCopy.h"
#include "Field/Collision.h"
#include "Field/Gimmicks.h"
#include "Field/Mpd.h"
#include "Field/Platforms.h"
#include "Field/Room.h"
#include "Field/RoomLoading.h"
#include "Items/Inventory.h"
#include "Items/Loadout.h"
#include "Legacy/Psx.h"
#include "Render/Palette.h"

static void ClearRoomAiBindings(void) {
  memset(Battle_Ai_FrameState(), 0, sizeof *Battle_Ai_FrameState());
  Battle_Ai_roomAiState = nullptr;
  Battle_Ai_tileFlagMap = nullptr;
  Battle_Ai_specialTerrainHeights = nullptr;
  Battle_Ai_roomHasWaypoints = nullptr;
  Battle_Ai_patrolRoutes = nullptr;
  Battle_Ai_roomWaypoints = nullptr;
  Battle_Ai_routeGates = nullptr;
}

// decomp: BATTLE.PRG func_800E6F1C (src/BATTLE/BATTLE.PRG/7E05C.s:541).
void Battle_Ai_FreeRoomAiState(void) {
  for (size_t index = 0; index < sizeof Battle_actors / sizeof Battle_actors[0];
       ++index) {
    if (Battle_actors[index] != nullptr) {
      Battle_Ai_FreeActorAiState(Battle_actors[index]);
    }
  }
  if (Battle_Ai_roomAiState != nullptr) {
    Core_Memory_FreeFromLowEnd(Battle_Ai_roomAiState);
  }
  ClearRoomAiBindings();
}

void Battle_Ai_InvalidateSessionBindings(void) {
  // The arena was already reset: never dereference or free its former bindings.
  ClearRoomAiBindings();
  for (size_t slot = 0; slot < sizeof Battle_Ai_actorAiStates /
                                   sizeof Battle_Ai_actorAiStates[0];
       ++slot) {
    Battle_Ai_actorAiStates[slot] = nullptr;
  }
  Battle_Ai_attributeActorMask = 0;
  Battle_Ai_secondPerceptionTarget = 0;
  Battle_Ai_arenaOrbit = nullptr;
  Battle_Ai_planningStatsSnapshot = nullptr;
  Battle_Ai_planningWorkspace = nullptr;
  Battle_Ai_planningPassCandidates = nullptr;
  Battle_Ai_candidateExclusions = nullptr;
  Battle_Ai_fallbackCandidates = nullptr;
  Battle_Ai_fallbackCandidateCount = 0;
  Battle_Ai_unusedPlanCounter = 0;
  Battle_Ai_candidateEngagedThisUpdate = 0;
  Core_Debugf("ai.lifecycle", "invalidated AI bindings after game-arena reset");
}

enum {
  kRoomAuxiliaryHeaderBytes = 0x498,
  kRoomSettingsOffset = 0x450,
  kRoomBoundsOffset = 0x494,
  kPatrolRoutesOffset = 0x400,
  kWaypointsOffset = 0x440,
  kRouteGatesOffset = 0x454,
  kPatrolRouteLength = 8,
  kDefaultActionDelay = 150,
  kBlockedTile = 0x8000,
  kHeightFractionBits = 6,
  kStoredHeightMask = 0x3FF,
  kTriggerKindCount = 16,
  kTriggerFlag = 0x100,
  kActionDelayShift = 21,
  kNonSpellActionsShift = 20,
  kRoomOptionsShift = 29
};

u_short Battle_Ai_DecodeCollisionFlags(unsigned bits) {
  static const struct {
    unsigned shift;
    unsigned output_mask;
  } fields[] = {{23, 0x1F},  {12, 0x20},   {22, 0x40},
                {20, 0x800}, {18, 0x1000}, {16, 0x2000}};
  unsigned flags = 0;
  for (size_t index = 0; index < sizeof fields / sizeof fields[0]; ++index) {
    flags |= (bits >> fields[index].shift) & fields[index].output_mask;
  }
  return (u_short)flags;
}

int Battle_Ai_BuildRoomAiMap(Battle_Ai_RoomAiMap* map) {
  enum { kMapSide = sizeof map->flags / sizeof map->flags[0] };
  const unsigned char* auxiliary =
      (const unsigned char*)(Field_roomData.section6);
  const Field_MpdCollisionHeader* dimensions = Field_roomData.collisionMap;
  if (map == nullptr ||
      (auxiliary != nullptr &&
       Field_roomData.header.section6Length < kRoomAuxiliaryHeaderBytes) ||
      (dimensions != nullptr &&
       (dimensions->width < 0 || dimensions->width > kMapSide ||
        dimensions->height < 0 || dimensions->height > kMapSide))) {
    Core_Errorf("ai", "Invalid room AI header: auxiliary_bytes=%u",
                Field_roomData.header.section6Length);
    return 0;
  }
  memset(map, 0, sizeof *map);
  const uint16_t bounds =
      static_cast<uint16_t>((Field_CollisionMapDimensions(0) & 0xFFFF) >> 4 |
                            (unsigned)Field_CollisionMapDimensions(0) >> 12);
  // Original MAP212/MAP214 have shorter direction tails; bound actual accesses.
  memcpy(&map->state.maxTileX, &bounds, sizeof bounds);
  memcpy(map->state.reserved4, &bounds, sizeof bounds);
  map->state.flightCeiling =
      Field_roomData.environment != nullptr
          ? -Field_RoomEnvironmentParameter() * (1 << kHeightFractionBits)
          : 0;
  map->state.defaultActionDelay = kDefaultActionDelay;
  for (int tile = 0; tile < kMapSide * kMapSide; ++tile) {
    const int tile_x = tile % kMapSide;
    const int tile_z = tile / kMapSide;
    const Field_MpdTileCell* cell = Field_CollisionCellAt(tile_x, tile_z, 0);
    if (cell == nullptr) {
      map->flags[tile_z][tile_x] = kBlockedTile;
      continue;
    }
    unsigned bits;
    memcpy(&bits, cell, sizeof bits);
    unsigned flags = Battle_Ai_DecodeCollisionFlags(bits);
    map->heights[tile_z][tile_x] =
        (u_short)((Battle_Ai_SampleAiTileGroundHeight(tile_x, tile_z) >>
                   kHeightFractionBits) &
                  kStoredHeightMask);
    const Field_MpdFloorTrigger* trigger =
        Field_RoomTriggerAtTile(tile_x, tile_z, 0);
    if (trigger != nullptr &&
        static_cast<uint16_t>(trigger->kind) < kTriggerKindCount &&
        static_cast<uint16_t>(trigger->lockState.switchValue) < 2) {
      flags |= (unsigned)kTriggerFlag << trigger->lockState.switchValue;
    }
    map->flags[tile_z][tile_x] = (u_short)flags;
  }
  if (auxiliary != nullptr) {
    unsigned settings;
    memcpy(&settings, auxiliary + kRoomSettingsOffset, sizeof settings);
    memcpy(&map->state.maxTileX, auxiliary + kRoomBoundsOffset,
           sizeof(uint32_t));
    map->state.defaultActionDelay =
        (unsigned char)(settings >> kActionDelayShift);
    map->state.allowsNonSpellActions =
        (unsigned char)((settings >> kNonSpellActionsShift) & 1);
    map->state.reservedB[2] = static_cast<char>(settings >> kRoomOptionsShift);
  }
  return 1;
}

// decomp: BATTLE.PRG func_800E6F9C (src/BATTLE/BATTLE.PRG/7E05C.s:582).
void Battle_Ai_InitializeRoomAiMap(void) {
  Battle_Ai_RoomAiMap* map = reinterpret_cast<Battle_Ai_RoomAiMap*>(
      Core_Memory_AllocateFromLowEnd(sizeof *map));
  if (map == nullptr) {
    Core_Errorf("ai", "Room AI allocation failed: bytes=%zu", sizeof *map);
    abort();
  }
  if (!Battle_Ai_BuildRoomAiMap(map)) {
    Core_Memory_FreeFromLowEnd(map);
    Core_Errorf("ai", "Room AI initialization rejected the room header");
    abort();
  }
  Battle_Ai_roomAiState = &map->state;
  Battle_Ai_tileFlagMap = map->flags;
  Battle_Ai_specialTerrainHeights = map->heights;
  unsigned char* auxiliary = (unsigned char*)(Field_roomData.section6);
  Battle_Ai_roomHasWaypoints = auxiliary;
  Battle_Ai_patrolRoutes =
      auxiliary != nullptr
          ? (uint16_t (*)[kPatrolRouteLength])(auxiliary + kPatrolRoutesOffset)
          : nullptr;
  Battle_Ai_roomWaypoints =
      auxiliary != nullptr
          ? reinterpret_cast<uint16_t*>(auxiliary + kWaypointsOffset)
          : nullptr;
  Battle_Ai_routeGates = auxiliary != nullptr
                             ? reinterpret_cast<Battle_Ai_RouteGateEnd*>(
                                   auxiliary + kRouteGatesOffset)
                             : nullptr;
  // Actor creation can query terrain before the first AI update.
  Battle_Ai_CacheActorAiWorkingState();
  Battle_Ai_BuildAiTerrainHeightsWithWorkspace();
  Battle_Ai_UpdateRouteGateAvailability();
}

// decomp: BATTLE.PRG func_800E71DC (src/BATTLE/BATTLE.PRG/7E05C.s:0) [asm-file:
// not yet ported]
int Battle_Ai_SelectTileHeight(const int samples[kAiTileSampleCount],
                               int center) {
  enum {
    kSampleRowWidth = 4,
    kFallbackSample = 10,
    kHeightSignBit = 0x4000,
    kHeightMagnitudeMask = kHeightSignBit - 1,
    kSolidPatchMask = 0x33
  };
  int highest = center;
  unsigned invalid = 0;
  for (unsigned index = 0; index < kAiTileSampleCount; ++index) {
    const int sample = samples[index];
    if (sample < 0) invalid |= 1u << index;
    const int height =
        (sample & kHeightMagnitudeMask) - (sample & kHeightSignBit);
    const int previous =
        (highest & kHeightMagnitudeMask) - (highest & kHeightSignBit);
    if (height > previous) highest = sample;
  }
  if (invalid == 0) return highest;
  for (unsigned index = 0; index < kFallbackSample; ++index) {
    if (index % kSampleRowWidth == kSampleRowWidth - 1) continue;
    const unsigned patch = (unsigned)kSolidPatchMask << index;
    if ((~invalid & patch) == patch) return highest;
  }
  return samples[kFallbackSample];
}

// decomp: BATTLE.PRG func_800E71DC (src/BATTLE/BATTLE.PRG/7E05C.s:738).
int Battle_Ai_SampleAiTileGroundHeight(int tile_x, int tile_z) {
  enum {
    kTileUnits = 128,
    kSamplesPerAxis = 4,
    kSampleStep = kTileUnits / kSamplesPerAxis,
    kSampleCount = kSamplesPerAxis * kSamplesPerAxis
  };
  const int origin_x = tile_x * kTileUnits;
  const int origin_z = tile_z * kTileUnits;
  const int center =
      Field_FloorHeightAt(origin_x + kTileUnits / 2, origin_z + kTileUnits / 2);
  int samples[kSampleCount];
  for (int index = 0; index < kSampleCount; ++index) {
    samples[index] =
        Field_FloorHeightAt(origin_x + (index / kSamplesPerAxis) * kSampleStep,
                            origin_z + (index % kSamplesPerAxis) * kSampleStep);
  }
  return Battle_Ai_SelectTileHeight(samples, center);
}
}  // extern "C"
