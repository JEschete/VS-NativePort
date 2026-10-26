// Hand-maintained (tools/port/owned.csv).
// C counterpart: rood-boi pc/src/battle_movement.c:685/1238,
// revision 3be5ba38b008cad919a2b1ca196432c8d4817335 (CC0-1.0).
#include "Battle/Ai/NavigationPrimitives.h"

#include <array>
#include <bit>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <memory>

#include "Core/Diagnostics.h"
#include "Legacy/Psx.h"

extern "C" {
#include "Actors/ActorRecord.h"
#include "Actors/ActorState.h"
#include "Battle/Ai/ActorCleanup.h"
#include "Battle/Ai/ActorCommands.h"
#include "Battle/Ai/ActorState.h"
#include "Battle/Ai/Engagement.h"
#include "Battle/Ai/FrameUpdate.h"
#include "Battle/Ai/GroundNavigation.h"
#include "Battle/Ai/Movement.h"
#include "Battle/Ai/Navigation.h"
#include "Battle/Ai/Perception.h"
#include "Battle/Damage.h"
#include "Core/Math/ScalarMath.h"
#include "Core/Memory/Heap.h"
#include "Core/Timing.h"
#include "Field/Gimmicks.h"
#include "Field/Platforms.h"
#include "Field/Room.h"
#include "Field/TerrainHeight.h"
}

namespace {
constexpr unsigned kTileMask = 0xFF;
constexpr int kTileZShift = 16;
constexpr unsigned kReservedTileBit = 0x80;
constexpr u_short kDynamicTileBit = 0x400;
constexpr int kDirectionComponentMask = 3;
constexpr int kDirectionZShift = 4;
constexpr int kDirectionComponentModulus = 4;
constexpr int kMaximumGroundStep = 64;
constexpr unsigned kOccupancyCellBytes = sizeof(u_short);
constexpr unsigned kOccupancyRowBytes = kOccupancyCellBytes * kAiFrameGridSide;
constexpr unsigned kOccupancyMapBytes = kOccupancyRowBytes * kAiFrameGridSide;
constexpr std::size_t kRouteWords = 256;
constexpr std::size_t kRouteEndPairs = 8;
constexpr std::size_t kRouteEnds = kRouteEndPairs * 2;
constexpr unsigned kRouteEndOffset = 0x454;
constexpr unsigned kRouteAuxiliaryBytes = 0x498;
constexpr unsigned kFirstPlatformGate = 0x400;
constexpr unsigned kSecondPlatformGate = 0x04000000;
constexpr unsigned kFirstClosedGate = 0x8000;
constexpr unsigned kSecondClosedGate = 0x80000000;
constexpr unsigned kGateSelectorMask = 3;
constexpr unsigned kGateCoordinateMask = 31;
constexpr unsigned kGateCoordinateBits = 5;
constexpr unsigned kSecondGateShift = 16;
constexpr unsigned kGateSelectorShift = 13;
constexpr unsigned kVariableShiftMask = 31;
constexpr unsigned kCoordinateHalfwordBits = 16;
constexpr unsigned kCoordinateByteBits = 8;
constexpr unsigned kCoordinateHalfwordMask = 0xFFFF;
constexpr unsigned kHorizontalTileMask = 0xFF00FF;
constexpr unsigned kScriptedWaitBits = 0xFE0;
constexpr unsigned kScriptedWaitShift = 5;
constexpr unsigned kScriptedWaitMask = 127;
constexpr unsigned kSuspendedActorCommands = 0xC0;
constexpr int kScriptedArrivalSquared = 1024;
constexpr unsigned kFineCoordinateShift = 6;
constexpr unsigned kFineCoordinateHighShift = 14;
constexpr unsigned kTileCoordinateShift = 7;
constexpr unsigned kGroundAreaMask = 15;
constexpr u_char kNoMovementTarget = 16;

void RequireScriptedNavigation(bool valid, const char* message) {
  if (!valid) {
    Core_Errorf("ai", "Scripted navigation: %s", message);
    std::abort();
  }
}

void PrepareScriptedRoute(Battle_Ai_ActorAiState& route,
                          const Actors_ModelInstance& model) {
  route.opaque19A = 0;
  if (route.movement_wait != 0) --route.movement_wait;
  unsigned wait =
      (route.navigationFlags >> kScriptedWaitShift) & kScriptedWaitMask;
  if (wait != 0) --wait;
  route.navigationFlags = (route.navigationFlags & ~kScriptedWaitBits) |
                          (wait << kScriptedWaitShift);
  route.ignore_floor_mask = 1;
  route.opaque1 = 0;
  route.movementTarget = kNoMovementTarget;
  route.isStuck = 0;
  route.opaque3 = 0;
  route.tile.word =
      model.currentTileX |
      (static_cast<unsigned>(model.currentTileLayer) << kCoordinateByteBits) |
      (static_cast<unsigned>(model.currentTileZ) << kTileZShift) |
      (static_cast<unsigned>(model.reserved5F)
       << (kCoordinateHalfwordBits + kCoordinateByteBits));
  route.position = model.position;
  const unsigned x = static_cast<u_short>(model.position.vx);
  const unsigned z = static_cast<u_short>(model.position.vz);
  route.fine_x = static_cast<u_char>(x >> kFineCoordinateShift);
  route.fine_layer = static_cast<u_char>(x >> kFineCoordinateHighShift);
  route.fine_z = static_cast<u_char>(z >> kFineCoordinateShift);
  route.fine_status = static_cast<u_char>(z >> kFineCoordinateHighShift);
}

void ContinueScriptedRoute(Battle_Ai_ActorAiState& route) {
  if (route.route_gate != 0) {
    Battle_Ai_MoveActorTowardDestinationTile(&route, 0);
    return;
  }
  const unsigned x = route.scriptedTargetXZ & kCoordinateHalfwordMask;
  const unsigned z = route.scriptedTargetXZ >> kCoordinateHalfwordBits;
  route.moveX = static_cast<int>(x);
  route.moveZ = static_cast<int>(z);
  route.destination.word =
      (x >> kTileCoordinateShift) |
      ((z >> kTileCoordinateShift) << kCoordinateHalfwordBits);
  Battle_Ai_UpdateNavigationAreaMovement(
      reinterpret_cast<Battle_Ai_TerrainMoveView*>(&route));
}

bool ScriptedRouteReached(Battle_Ai_ActorAiState& route) {
  const unsigned target = route.scriptedTargetXZ;
  if (route.locomotionMode == 0) {
    const int x = static_cast<int>(target & kCoordinateHalfwordMask);
    const int z = static_cast<int>(target >> kCoordinateHalfwordBits);
    const unsigned squared = Core_Math_SquaredVectorLength(
        x - static_cast<u_short>(route.position.vx), 0,
        z - static_cast<u_short>(route.position.vz));
    if (std::bit_cast<std::int32_t>(squared) < kScriptedArrivalSquared) {
      return true;
    }
    ContinueScriptedRoute(route);
    return false;
  }
  if ((route.destination.word & kHorizontalTileMask) ==
      (route.tile.word & kHorizontalTileMask)) {
    return true;
  }
  route.destination.word =
      ((target & kCoordinateHalfwordMask) >> kTileCoordinateShift) |
      ((target >> (kCoordinateHalfwordBits + kTileCoordinateShift))
       << kCoordinateHalfwordBits);
  Battle_Ai_DispatchAiLocomotionMode(&route, 0, 0);
  return false;
}

void RequireRouteAvailability(bool valid, const char* message) {
  if (!valid) {
    Core_Errorf("ai", "Route availability: %s", message);
    std::abort();
  }
}

unsigned VisiblePlatformMask() {
  constexpr std::size_t kOriginalPlatformCapacity = 13;
  if (Field_roomData.movingPlatforms == nullptr) return 0;
  const auto bytes = Field_roomData.header.movingPlatformsLength;
  RequireRouteAvailability(
      bytes % sizeof(Field_MpdMovingPlatform) == 0 &&
          bytes / sizeof(Field_MpdMovingPlatform) <= kOriginalPlatformCapacity,
      "moving-platform count exceeds the original bounded buffer");
  std::array<Field_PlatformContactPoint, kOriginalPlatformCapacity> points{};
  const int count = Field_CollectMovingPlatformPositions(points.data());
  RequireRouteAvailability(
      count >= 0 && static_cast<std::size_t>(count) <= points.size(),
      "moving-platform producer returned an invalid count");
  unsigned mask = 0;
  for (int index = count; index-- > 0;) {
    mask |= 1u << (static_cast<unsigned>(points[index].platformIndex) &
                   kVariableShiftMask);
  }
  return mask;
}

u_int AvailableRouteWord(
    u_int word, const std::array<Battle_Ai_RouteGateEnd, kRouteEnds>& ends,
    unsigned active) {
  if ((word & (kFirstPlatformGate | kSecondPlatformGate)) == 0) return word;
  word |= kFirstClosedGate | kSecondClosedGate;
  const auto first_x = (word >> kGateCoordinateBits) & kGateCoordinateMask;
  const auto first_z = word & kGateCoordinateMask;
  const auto second_x =
      (word >> (kSecondGateShift + kGateCoordinateBits)) & kGateCoordinateMask;
  const auto second_z = (word >> kSecondGateShift) & kGateCoordinateMask;
  auto first_bit = (word >> kGateSelectorShift) & kGateSelectorMask;
  auto second_bit =
      (word >> (kSecondGateShift + kGateSelectorShift)) & kGateSelectorMask;
  for (std::size_t link_index = 0; link_index < kRouteEndPairs; ++link_index) {
    const auto& end = ends[link_index * 2];
    if ((word & kFirstPlatformGate) != 0 && first_x == end.tileX) {
      first_bit = 1u << (first_bit & kVariableShiftMask);
      if (first_z == end.tileZ) {
        first_bit &= active;
        if (first_bit != 0) word &= ~kFirstClosedGate;
      }
    }
    // 73C84.s mutates both shift operands across pairs, not just their masks.
    second_bit = 1u << (second_bit & kVariableShiftMask);
    if ((word & kSecondPlatformGate) != 0) {
      second_bit &= active;
      if (second_x == end.tileX && second_z == end.tileZ && second_bit != 0) {
        word &= ~kSecondClosedGate;
      }
    }
  }
  return word;
}

void RequireTile(int x, int z) {
  if (x < 0 || z < 0 || x >= kAiFrameGridSide || z >= kAiFrameGridSide) {
    Core_Debugf("ai", "AI navigation tile (%d,%d) is out of bounds", x, z);
    std::abort();
  }
}

int SignedDirection(int value) {
  return value > 1 ? value - kDirectionComponentModulus : value;
}

void RequireActorId(int actor_id) {
  if (actor_id < 0 ||
      static_cast<unsigned>(actor_id) >= std::size(Battle_Ai_actorAiStates)) {
    Core_Debugf("ai", "AI navigation actor ID %d is out of bounds", actor_id);
    std::abort();
  }
}

Battle_Ai_ActorAiState* ActorNavigationState(int actor_id) {
  RequireActorId(actor_id);
  return Battle_Ai_tileFlagMap == nullptr ? nullptr
                                          : Battle_Ai_actorAiStates[actor_id];
}

void ChangeGimmickOccupancy(const Field_GimmickInstance* gimmick, int delta) {
  if (gimmick == nullptr) {
    Core_Errorf("ai", "Gimmick occupancy requires an instance");
    std::abort();
  }
  if (Battle_Ai_tileFlagMap == nullptr) return;
  const auto x = static_cast<u_char>(gimmick->tileX);
  const auto layer = static_cast<u_char>(gimmick->tileLayer);
  const auto z = static_cast<u_char>(gimmick->tileZ);
  // 7E05C.s:57-63 forms a byte offset, including the upper layer bits.
  const unsigned offset =
      x * kOccupancyCellBytes + (layer >> 2) + z * kOccupancyRowBytes;
  if (Battle_Ai_specialTerrainHeights == nullptr ||
      offset > kOccupancyMapBytes - kOccupancyCellBytes ||
      offset % kOccupancyCellBytes != 0) {
    Core_Errorf("ai", "Gimmick occupancy placement (%u,%u,%u) is invalid",
                static_cast<unsigned>(x), static_cast<unsigned>(layer),
                static_cast<unsigned>(z));
    std::abort();
  }
  const auto cell_index = offset / kOccupancyCellBytes;
  auto& cell = Battle_Ai_specialTerrainHeights[cell_index / kAiFrameGridSide]
                                              [cell_index % kAiFrameGridSide];
  cell = static_cast<u_short>(static_cast<int>(cell) + delta);
}
}  // namespace

// decomp: func_800E6B4C (src/BATTLE/BATTLE.PRG/7E05C.s:257).
// Its allocation held a temporary PS1 stack, not terrain data. The bounded
// 32x32 builder uses the native game fiber's existing 16 MiB stack.
void Battle_Ai_BuildAiTerrainHeightsWithWorkspace(void) {
  if (Battle_Ai_tileFlagMap == nullptr ||
      Battle_Ai_specialTerrainHeights == nullptr) {
    Core_Debugf("ai", "Special terrain build requires active native maps");
    std::abort();
  }
  Battle_Ai_BuildAiSpecialTerrainHeights();
}

// decomp: func_800DC638 (src/BATTLE/BATTLE.PRG/73C84.s:149-243).
void Battle_Ai_UpdateRouteGateAvailability(void) {
  const unsigned active = VisiblePlatformMask();
  if (Battle_Ai_roomHasWaypoints == nullptr) return;
  auto* auxiliary = static_cast<u_char*>(Battle_Ai_roomHasWaypoints);
  RequireRouteAvailability(
      auxiliary == Field_roomData.section6 &&
          Field_roomData.header.section6Length >= kRouteAuxiliaryBytes &&
          reinterpret_cast<const u_char*>(Battle_Ai_routeGates) ==
              auxiliary + kRouteEndOffset,
      "route table does not belong to the active room auxiliary section");
  std::array<Battle_Ai_RouteGateEnd, kRouteEnds> ends{};
  std::memcpy(ends.data(), Battle_Ai_routeGates, sizeof ends);
  for (std::size_t index = 0; index < kRouteWords; ++index) {
    auto* location = auxiliary + index * sizeof(u_int);
    u_int word;
    std::memcpy(&word, location, sizeof word);
    // No original valid room has a first-word second-only platform gate.
    RequireRouteAvailability(
        index != 0 || active == 0 ||
            (word & (kFirstPlatformGate | kSecondPlatformGate)) !=
                kSecondPlatformGate,
        "first route word depends on an original stack-register value");
    word = AvailableRouteWord(word, ends, active);
    std::memcpy(location, &word, sizeof word);
  }
}
// decomp: func_800E4C1C (src/BATTLE/BATTLE.PRG/7BDD4.s:542).
void Battle_Ai_ReserveAiDestinationTile(int x, int z) {
  if (Battle_Ai_tileFlagMap == nullptr) return;
  RequireTile(x, z);
  Battle_Ai_tileFlagMap[z][x] |= kReservedTileBit;
}

// decomp: func_800E68EC (src/BATTLE/BATTLE.PRG/7E05C.s:72).
void Battle_Ai_MarkAiTileDynamicObstacle(int x, int z) {
  if (Battle_Ai_tileFlagMap == nullptr) return;
  RequireTile(x, z);
  Battle_Ai_tileFlagMap[z][x] |= kDynamicTileBit;
}

// decomp: func_800E6898 (src/BATTLE/BATTLE.PRG/7E05C.s:38-69).
void Battle_Ai_AddAiTileOccupant(const Field_GimmickInstance* gimmick) {
  ChangeGimmickOccupancy(gimmick, kDynamicTileBit);
}

// decomp: func_800E68A0 (src/BATTLE/BATTLE.PRG/7E05C.s:46-69).
void Battle_Ai_RemoveAiTileOccupant(const Field_GimmickInstance* gimmick) {
  ChangeGimmickOccupancy(gimmick, -static_cast<int>(kDynamicTileBit));
}

// decomp: func_800E4690 (src/BATTLE/BATTLE.PRG/7BDD4.s:72).
int Battle_Ai_IsAdjacentTileStepBlocked(unsigned int tile, int direction) {
  const auto* frame = Battle_Ai_FrameState();
  if (direction < 0 ||
      static_cast<unsigned>(direction) >= std::size(frame->directions)) {
    Core_Debugf("ai", "AI neighbor direction %d is out of bounds", direction);
    std::abort();
  }
  const int x = static_cast<int>(tile & kTileMask);
  const int z = static_cast<int>((tile >> kTileZShift) & kTileMask);
  RequireTile(x, z);
  const auto packed = frame->directions[direction];
  const auto dx = SignedDirection(packed & kDirectionComponentMask);
  const auto dz =
      SignedDirection((packed >> kDirectionZShift) & kDirectionComponentMask);
  if ((dx == 0 && dz == 0) || dx < -1 || dz < -1) {
    Core_Debugf("ai", "AI neighbor direction %d has invalid delta (%d,%d)",
                direction, dx, dz);
    std::abort();
  }
  const auto next_x = x + dx;
  const auto next_z = z + dz;
  if (next_x < 0 || next_z < 0 || next_x >= kAiFrameGridSide ||
      next_z >= kAiFrameGridSide) {
    return 1;
  }
  const int raw = Field_GroundHeightFlagsAtTile(next_x, next_z);
  if (raw < 0) return 1;
  return std::abs(Field_GroundHeightAtTile(x, z) -
                  Field_GroundHeightAtTile(next_x, next_z)) >=
         kMaximumGroundStep;
}

// decomp: func_800E5158 (src/BATTLE/BATTLE.PRG/7C7E0.s:127), $a3 = 0.
int Battle_Ai_HasActorNavigationState(int actor_id) {
  return ActorNavigationState(actor_id) != nullptr;
}

// decomp: func_800E5154 (src/BATTLE/BATTLE.PRG/7C7E0.s:122), shared $a3 = 1.
int Battle_Ai_ActorNavigationOverrideStatus(int actor_id) {
  const auto* state = ActorNavigationState(actor_id);
  if (state == nullptr || state->nativePointer18C == nullptr) return 2;
  const auto* route =
      static_cast<const Battle_Ai_ActorAiState*>(state->nativePointer18C);
  if (route->nativePointer64 == nullptr) {
    Core_Debugf("ai", "AI navigation override for actor %d requires a tile",
                actor_id);
    std::abort();
  }
  u_int tile;
  std::memcpy(&tile, route->nativePointer64, sizeof(tile));
  constexpr unsigned kHorizontalTileMask = 0xFF00FF;
  return (tile & kHorizontalTileMask) !=
         (route->destination.word & kHorizontalTileMask);
}

// decomp: func_800E51C8 (src/BATTLE/BATTLE.PRG/7C7E0.s:159).
void Battle_Ai_CopyActorNavigationOverrideState(
    int actor_id, Battle_Ai_ActorAiState* destination) {
  RequireActorId(actor_id);
  const auto* source = Battle_Ai_actorAiStates[actor_id];
  if (source == nullptr || destination == nullptr) {
    Core_Debugf("ai", "AI navigation copy for actor %d requires both states",
                actor_id);
    std::abort();
  }
  *destination = *source;
  destination->actorId = static_cast<u_char>(actor_id);
  constexpr u_short kOverrideStepHeight = 0x140;
  constexpr u_char kOverrideTileStepLimit = 3;
  constexpr u_char kOverrideSettings = 3;
  std::memcpy(destination->step_height_bytes, &kOverrideStepHeight,
              sizeof(kOverrideStepHeight));
  destination->reservedB2 = kOverrideTileStepLimit;
  destination->navigationFlags = 0;
  destination->opaque19A = 0;
  destination->settingsBit14 = kOverrideSettings;
  destination->movementEnabled = 1;
}

// decomp: func_800E527C (src/BATTLE/BATTLE.PRG/7C7E0.s:216-256).
void Battle_Ai_SetActorNavigationOverride(int actor_id, int x, int z) {
  auto* state = ActorNavigationState(actor_id);
  if (state == nullptr) return;
  RequireScriptedNavigation(
      state->actorId == actor_id && state->actor != nullptr &&
          static_cast<void*>(state->actor) ==
              static_cast<void*>(Battle_actors[actor_id]) &&
          Battle_Ai_roomAiState != nullptr,
      "path request requires matching actor and room state");
  const unsigned target = static_cast<unsigned>(x) |
                          (static_cast<unsigned>(z) << kCoordinateHalfwordBits);
  Battle_Ai_DisengageActor(state);
  Battle_Ai_ResetActorAwarenessIndicator(
      reinterpret_cast<Battle_Ai_PlanEvaluationView*>(state));
  Battle_Ai_UnlinkEngagedActor(state);
  auto* destination = state;
  if (state->nativePointer18C == nullptr) {
    auto* storage = static_cast<Battle_Ai_ActorAiState*>(
        Core_Memory_AllocateFromHighEnd(sizeof(Battle_Ai_ActorAiState)));
    RequireScriptedNavigation(storage != nullptr, "route allocation failed");
    destination = std::construct_at(storage);
    state->nativePointer18C = destination;
  }
  // On repeat entry, the original copies onto the live base, not its old route.
  Battle_Ai_CopyActorNavigationOverrideState(actor_id, destination);
  destination->scriptedTargetXZ = target;
}

// decomp: func_800E5308 (src/BATTLE/BATTLE.PRG/7C7E0.s:260-421).
void Battle_Ai_UpdateActorNavigationOverrides(void) {
  Battle_Ai_CacheActorAiWorkingState();
  auto& frame = *Battle_Ai_FrameState();
  std::size_t visited = 0;
  for (auto* actor = Battle_actors[0]; actor != nullptr; actor = actor->next) {
    RequireScriptedNavigation(++visited <= std::size(Battle_actors),
                              "actor list is cyclic or exceeds native storage");
    if ((actor->commandState & kSuspendedActorCommands) != 0) continue;
    RequireActorId(actor->id);
    const auto* base = frame.actors[actor->id];
    auto* route =
        base != nullptr
            ? static_cast<Battle_Ai_ActorAiState*>(base->nativePointer18C)
            : nullptr;
    frame.lastBudgetTick = Core_ElapsedFrameCounterTicks();
    if (route == nullptr) continue;
    const auto* model = Actors_actorRecords[actor->id];
    RequireScriptedNavigation(
        model != nullptr && frame.tileFlags != nullptr &&
            route->actorId == actor->id,
        "active route requires model, map and actor identity");
    PrepareScriptedRoute(*route, model->instance);
    RequireTile(model->instance.currentTileX, model->instance.currentTileZ);
    const auto area =
        static_cast<u_char>(frame.tileFlags[model->instance.currentTileZ]
                                           [model->instance.currentTileX] &
                            kGroundAreaMask);
    route->ground_area = route->reserved154Area = area;
    frame.directionCache[0] = 0;
    // The source clears guest pointer tag bits at actor byte3, not native
    // links.
    if (ScriptedRouteReached(*route)) {
      Battle_Ai_IssueActorCommand(route, 0);
      Battle_Ai_FreeActorNavigationOverride(route->actorId);
    }
  }
  Battle_Ai_SaveMovementRandomState();
}
