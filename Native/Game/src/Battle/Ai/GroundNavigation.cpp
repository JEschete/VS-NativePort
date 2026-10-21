// Hand-maintained (tools/port/owned.csv).
// C counterparts: rood-boi pc/src/battle_movement.c:702-960,1717-1744,
// revision 3be5ba38b008cad919a2b1ca196432c8d4817335 (CC0-1.0).
#include "Battle/Ai/GroundNavigation.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "Legacy/Psx.h"

extern "C" {
#include "Actors/ActorRecord.h"
#include "Battle/Ai/ActorCommands.h"
#include "Battle/Ai/ActorState.h"
#include "Battle/Ai/AiState.h"
#include "Battle/Ai/DecisionPrimitives.h"
#include "Battle/Ai/FrameUpdate.h"
#include "Battle/Ai/Movement.h"
#include "Battle/Ai/MovementBehavior.h"
#include "Battle/Ai/NavigationPrimitives.h"
#include "Battle/Damage.h"
#include "Core/Math/TileDistance.h"
#include "Field/TerrainHeight.h"
}

namespace {
constexpr unsigned kByteMask = 0xFF;
constexpr unsigned kTileMask = 0xFF00FF;
constexpr unsigned kLowTileMask = 0xFFFFFF;
constexpr int kTileShift = 7;
constexpr int kTileCenter = 64;
constexpr unsigned kTileZShift = 16;
constexpr unsigned kByteShift = 8;
constexpr unsigned kStatusShift = 24;
constexpr unsigned kTerrainLayer = 0x1C00;
constexpr unsigned kGapFlag = 0x20;
constexpr unsigned kCellBlocked = 0x80B0;
constexpr unsigned kNoStepFlags = 0x30;
constexpr unsigned kCellWalkBlocked = 0xB0;
constexpr unsigned kCellFloorBlocked = 0x10;
constexpr unsigned kExtendedStep = 0xF8;
constexpr int kExtendedStepHeight = 0x2000;
constexpr int kOccupancyHeight = 128;
constexpr int kApproachMargin = 96;
constexpr int kMaximumGapAcceptance = 3;
constexpr unsigned kAngleMask = 0xFFF;
constexpr int kDirectionMask = 7;
constexpr int kAreaMask = 15;
constexpr int kShiftMask = 31;
constexpr unsigned kPathDirectionBits = 0xE;
constexpr int kHalfTurnDirections = 4;
constexpr int kDirectionalModes = 4;
constexpr std::size_t kHesitationKindCount = 4;
constexpr int kDetourRandomRange = 4;
constexpr int kPathTurnShift = 4;
constexpr int kInitialHesitationChance = 8;
constexpr int kRouteLinks = 8;
constexpr int kRepeatThreshold = 5;
constexpr int kRepeatWait = 120;
constexpr unsigned kDetourSteps = 126;
constexpr int kRandomRange = 128;
constexpr int kPollKind = 3;
constexpr int kLocomotionModes = 3;
constexpr int kTurnDuration = 64;
constexpr std::int16_t kPitchHigh = 0x500;
constexpr std::int16_t kPitchMiddle = 0x320;
constexpr int kSecondCryThreshold = 11;
constexpr int kFirstCryLow = 18;
constexpr int kFirstCryHigh = 53;
constexpr int kHalfwordBits = 16;
constexpr int kTerrainIntent = 11;
constexpr int kAlternateTerrainIntent = 12;
constexpr int kStepIntent = 14;
constexpr int kWalkDirectionIntent = 5;
constexpr int kWalkHeadingIntent = 7;
constexpr int kTimedTurnIntent = 16;
constexpr int kBlockedWait = 254;
constexpr unsigned kPathCellKeepMask = 0xFFC30FFF;
constexpr unsigned kTileTagMask = 0xFF00;
constexpr unsigned kQuadrantMask = 0x3FF;
constexpr std::array<int, 12> kNeighbors{1, 0, 7, 0, 2, -1, 6, 0, 3, 4, 5, 0};

[[noreturn]] void Fail(const char* message) {
  Core_Debugf("ai", "%s", message);
  std::abort();
}

void Require(bool valid, const char* message) {
  if (!valid) Fail(message);
}

std::int16_t Half(std::uint32_t value) {
  return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(value));
}

int Angle(int value) {
  return static_cast<int>(static_cast<unsigned>(value) & kAngleMask);
}

Battle_Ai_ActorAiState& State(void* view) {
  Require(view != nullptr, "AI ground navigation requires state");
  return *static_cast<Battle_Ai_ActorAiState*>(view);
}

struct Tile {
  int x;
  int z;
};

Tile Unpack(unsigned tile) {
  return {static_cast<int>(tile & kByteMask),
          static_cast<int>((tile >> kTileZShift) & kByteMask)};
}

unsigned Pack(Tile tile) {
  return static_cast<unsigned>(tile.x) |
         (static_cast<unsigned>(tile.z) << kTileZShift);
}

bool InGrid(Tile tile) {
  const auto& frame = *Battle_Ai_FrameState();
  const auto width = frame.roomBounds & kByteMask;
  const auto height = (frame.roomBounds >> kByteShift) & kByteMask;
  Require(width > 0 && height > 0 && width <= kAiFrameGridSide &&
              height <= kAiFrameGridSide && frame.tileFlags != nullptr &&
              frame.heightMap != nullptr,
          "AI ground navigation requires a bounded native room grid");
  return tile.x >= 0 && tile.z >= 0 && static_cast<unsigned>(tile.x) < width &&
         static_cast<unsigned>(tile.z) < height;
}

int Component(unsigned value) {
  constexpr unsigned kComponentMask = 3;
  constexpr int kComponentModulus = 4;
  value &= kComponentMask;
  return value > 1 ? static_cast<int>(value) - kComponentModulus
                   : static_cast<int>(value);
}

Tile Delta(int direction) {
  constexpr unsigned kDirectionZShift = 4;
  if (direction < 0 || direction >= kAiDirectionBytes) {
    Fail("AI ground direction is out of bounds");
  }
  const auto packed =
      Battle_Ai_FrameState()->directions[static_cast<unsigned>(direction)];
  const Tile delta{Component(packed), Component(packed >> kDirectionZShift)};
  Require((delta.x != 0 || delta.z != 0) && delta.x >= -1 && delta.z >= -1,
          "AI ground direction has an invalid delta");
  return delta;
}

unsigned Cell(Tile tile) {
  return Battle_Ai_FrameState()->tileFlags[tile.z][tile.x];
}

bool RegionBlocked(const Battle_Ai_ActorAiState& state, int direction) {
  auto& frame = *Battle_Ai_FrameState();
  if (direction < 0) return true;
  if (frame.roomData == nullptr) {
    return Battle_Ai_IsAdjacentTileStepBlocked(state.tile.word, direction) != 0;
  }
  Require(direction < kAiDirectionBytes && frame.terrainRegions != nullptr,
          "AI region query requires native direction and region data");
  const auto tile = Unpack(state.tile.word);
  const auto width = frame.roomBounds & kByteMask;
  const auto index =
      static_cast<std::size_t>(tile.z) * width + static_cast<unsigned>(tile.x);
  Require(index < frame.terrain_region_count,
          "AI region query exceeds the validated room bitmap");
  return ((frame.terrainRegions[index] >> direction) & 1) != 0;
}

struct Bounds {
  int id;
  int x;
  int y;
  int z;
  int radius;
  int reach;
  int height;
};

Bounds ActorBounds(const Battle_Actor& actor) {
  Require(actor.id >= 0 && actor.id < kAiFrameActorSlots &&
              Actors_actorRecords[actor.id] != nullptr,
          "AI collision actor has no native model");
  const auto& model = *Actors_actorRecords[actor.id];
  const auto* ai = Battle_Ai_FrameState()->actors[actor.id];
  const bool planned =
      ai != nullptr && actor.id != 0 &&
      (model.instance.movementMode != 0 || model.instance.locomotionState != 0);
  const auto& position = planned ? ai->nextPosition : model.instance.position;
  return {actor.id,
          static_cast<std::uint16_t>(position.vx),
          position.vy,
          static_cast<std::uint16_t>(position.vz),
          ai != nullptr ? ai->collision_radius : model.occupancyRadius,
          ai != nullptr ? ai->maxRange : 0,
          ai != nullptr ? static_cast<std::uint16_t>(ai->bodyHeight)
                        : model.occupancyHeight};
}

template <typename Test>
int FirstBlockingActor(const Battle_Ai_ActorAiState& state, Test test) {
  unsigned visited = 0;
  for (const auto* actor = Battle_actors[0]; actor != nullptr;
       actor = actor->next) {
    if (actor->id < 0 || actor->id >= kAiFrameActorSlots) {
      Fail("AI collision actor ID is out of bounds");
    }
    const unsigned bit = 1u << static_cast<unsigned>(actor->id);
    Require((visited & bit) == 0, "AI collision actor list contains a cycle");
    visited |= bit;
    if (actor->id != state.actorId && test(ActorBounds(*actor))) {
      return actor->id + 1;
    }
  }
  return 0;
}

int Blocker(Battle_Ai_ActorAiState& state, int direction) {
  const auto delta = Delta(direction);
  auto& frame = *Battle_Ai_FrameState();
  const auto bit = static_cast<u_char>(1u << direction);
  if ((frame.blocker_valid & bit) != 0) {
    return frame.blocker_actor[direction];
  }
  const int x =
      static_cast<std::uint16_t>(state.position.vx) + state.maxRange * delta.x;
  const int z =
      static_cast<std::uint16_t>(state.position.vz) + state.maxRange * delta.z;
  const Tile point{x >> kTileShift, z >> kTileShift};
  int result = 0;
  if (InGrid(point)) {
    const auto current = Unpack(state.tile.word);
    const int index = (point.z - current.z + 1) * 4 + point.x - current.x + 1;
    const int lookup =
        index >= 0 && static_cast<std::size_t>(index) < kNeighbors.size()
            ? kNeighbors[index]
            : -1;
    const int height = RegionBlocked(state, lookup)
                           ? state.position.vy
                           : Field_GroundHeightAtWorldPosition(x, z);
    result = FirstBlockingActor(state, [&](const Bounds& other) {
      const int dx = other.x - x, dz = other.z - z;
      if ((delta.x > 0 && dx < 0) || (delta.x < 0 && dx > 0) ||
          (delta.z > 0 && dz < 0) || (delta.z < 0 && dz > 0)) {
        return false;
      }
      const int reach = other.radius + other.reach + state.collision_radius;
      return std::abs(dx) <= reach && std::abs(dz) <= reach &&
             std::abs(other.y - height) <=
                 (other.height + static_cast<std::uint16_t>(state.bodyHeight)) /
                     2;
    });
  }
  frame.blocker_actor[direction] = static_cast<u_char>(result);
  frame.blocker_valid |= bit;
  return result;
}

int Occupancy(Battle_Ai_ActorAiState& state, unsigned packed) {
  if ((static_cast<unsigned>(state.previousTile) & kLowTileMask) ==
      (packed & kTileMask)) {
    return 0;
  }
  const auto tile = Unpack(packed);
  if (!InGrid(tile)) return 0;
  const unsigned blocked =
      kCellBlocked | (static_cast<unsigned>(state.terrain_reject_mask) << 8);
  if ((Cell(tile) & blocked) != 0) return 0;
  const int x = tile.x * (1 << kTileShift) + kTileCenter;
  const int z = tile.z * (1 << kTileShift) + kTileCenter;
  const int height = Field_GroundHeightAtTile(tile.x, tile.z);
  return FirstBlockingActor(
             state,
             [&](const Bounds& other) {
               const int reach = other.radius + state.collision_radius;
               return std::abs(other.x - x) <= reach &&
                      std::abs(other.z - z) <= reach &&
                      std::abs(other.y - height) <= kOccupancyHeight;
             }) != 0
             ? -1
             : 1;
}

int Step(Battle_Ai_ActorAiState& state, unsigned from, unsigned to,
         int direction) {
  Delta(direction);
  const auto source = Unpack(from), target = Unpack(to);
  if (state.movementDisabled || !InGrid(source) || !InGrid(target)) return 0;
  const auto cell = Cell(target);
  if ((cell & kNoStepFlags) != 0 ||
      (state.isStuck == 0 && (cell & kAreaMask) != state.ground_area)) {
    return 0;
  }
  const int dx = target.x - source.x;
  int dz = target.z - source.z;
  if (dx < 0) dz = -dz;
  if (dz < 0) dz = -dz;
  const int rise = Field_GroundHeightAtTile(target.x, target.z) -
                   Field_GroundHeightAtTile(source.x, source.z);
  bool checked = false;
  if (dx < 2 && dz < 2) {
    if (Blocker(state, direction) != 0) return 0;
    checked = !(state.opaque3 != 0 || rise < 1);
  }
  if (!checked) {
    if (state.reservedB2 < dx + dz) return 0;
    std::uint16_t limit;
    std::memcpy(&limit, state.step_height_bytes, sizeof(limit));
    if ((state.movementFlags & kExtendedStep) != 0) {
      limit = kExtendedStepHeight;
    }
    if ((state.opaque3 != 0 && rise > limit) || rise < -limit) return 0;
  }
  if (state.opaque5 == 0) return 1;
  if (state.requestedMovementMode == 1) {
    const auto& approach =
        *reinterpret_cast<const Battle_Ai_TargetApproachView*>(&state);
    Require(approach.targetId < kAiFrameActorSlots &&
                Actors_actorRecords[approach.targetId] != nullptr,
            "AI approach edge query requires a target model");
    const auto& position =
        Actors_actorRecords[approach.targetId]->instance.position;
    const auto dx_world = static_cast<std::int64_t>(
        target.x * (1 << kTileShift) + kTileCenter - position.vx);
    const auto dz_world = static_cast<std::int64_t>(
        target.z * (1 << kTileShift) + kTileCenter - position.vz);
    const auto radius =
        static_cast<std::int64_t>(approach.range) - kApproachMargin;
    if (dx_world * dx_world + dz_world * dz_world < radius * radius) return -1;
  }
  return Core_Math_SquaredTileDistance(Pack(target), state.destination.word) <
         Core_Math_SquaredTileDistance(from, state.destination.word);
}
}  // namespace

// decomp: func_800D954C (src/BATTLE/BATTLE.PRG/70D4C.s:12).
int Battle_Ai_DestinationOccupancy(Battle_Ai_MoveTargetView* view,
                                   unsigned int tile) {
  return Occupancy(State(view), tile);
}

// decomp: func_800D98E8 (src/BATTLE/BATTLE.PRG/70D4C.s:279).
int Battle_Ai_DirectionalBlockingActor(Battle_Ai_MovementPathView* view,
                                       int direction) {
  return Blocker(State(view), direction);
}

// decomp: func_800D96C8 (src/BATTLE/BATTLE.PRG/70D4C.s:123).
int Battle_Ai_CanTraverseTileEdge(Battle_Ai_MoveTargetView* view,
                                  unsigned int from, unsigned int to,
                                  int direction) {
  return Step(State(view), from, to, direction);
}

// decomp: D_800D9B6C (src/BATTLE/BATTLE.PRG/70D4C.s:458).
int Battle_Ai_CanWalkInDirection(Battle_Ai_WanderView* view, int direction) {
  auto& state = State(view);
  const auto delta = Delta(direction);
  auto& frame = *Battle_Ai_FrameState();
  const auto bit = static_cast<u_char>(1u << direction);
  if ((frame.walk_valid & bit) != 0) {
    return (frame.walk_passable & bit) != 0;
  }
  bool blocked = Blocker(state, direction) != 0;
  const Tile current = Unpack(state.tile.word);
  const Tile next{(state.fine_x + delta.x) >> 1, (state.fine_z + delta.z) >> 1};
  if (!blocked && (current.x != next.x || current.z != next.z)) {
    blocked = !InGrid(next);
    if (!blocked) {
      int probe = direction;
      if ((direction & 1) != 0) {
        const int index = next.x - current.x + 1 + (next.z - current.z + 1) * 4;
        Require(
            index >= 0 && static_cast<std::size_t>(index) < kNeighbors.size(),
            "AI fine-tile direction exceeds its neighborhood");
        probe = kNeighbors[index];
      }
      blocked = RegionBlocked(state, probe);
      if (frame.roomData != nullptr &&
          (static_cast<unsigned>(state.previousTile) & kTileMask) ==
              Pack(next)) {
        blocked = true;
      }
      const auto flags = Cell(next);
      blocked |=
          (state.ignore_floor_mask == 0 && (flags & kCellFloorBlocked) != 0) ||
          (flags & (kCellWalkBlocked & ~kCellFloorBlocked)) != 0 ||
          (flags & (static_cast<unsigned>(state.terrain_reject_mask)
                    << kByteShift)) != 0;
      if (state.settingsBit14 != 0 && (state.isStuck & 1) == 0 &&
          (Cell(current) & kAreaMask) != (flags & kAreaMask) &&
          (state.reserved154Area & kAreaMask) != (flags & kAreaMask)) {
        blocked = true;
      }
      blocked |= (frame.heightMap[current.z][current.x] & kTerrainLayer) <
                 (frame.heightMap[next.z][next.x] & kTerrainLayer);
    }
  }
  frame.walk_valid |= bit;
  if (blocked) {
    frame.walk_passable &= static_cast<u_char>(~bit);
  } else {
    frame.walk_passable |= bit;
  }
  return !blocked;
}

// decomp: func_800DB93C (src/BATTLE/BATTLE.PRG/72B70.s:483).
unsigned int Battle_Ai_FindGapLandingTile(Battle_Ai_MoveTargetView* view,
                                          unsigned int packed, int direction,
                                          unsigned int from) {
  auto& state = State(view);
  const auto delta = Delta(direction);
  auto tile = Unpack(packed);
  if (!InGrid(tile)) return 0;
  packed &= kLowTileMask;
  if ((Cell(tile) & kGapFlag) == 0) return packed;
  const auto limit = std::min<int>(state.reservedB2, kMaximumGapAcceptance);
  for (int steps = 1; steps <= kAiFrameGridSide; ++steps) {
    tile.x += delta.x;
    tile.z += delta.z;
    if (!InGrid(tile)) return 0;
    packed = Pack(tile);
    if ((Cell(tile) & kGapFlag) != 0) {
      if (state.reservedB2 != 0) continue;
      return packed;
    }
    int result = 0;
    if (steps < limit) {
      result = Step(state, from, packed, direction);
      if (result == 1) result = Occupancy(state, packed);
    }
    return packed | (static_cast<unsigned>(result) << kStatusShift);
  }
  Core_Debugf("ai", "AI gap scan exceeded the bounded room grid");
  std::abort();
}

namespace {
constexpr unsigned kPathHeadingShift = 1;
constexpr unsigned kPathStepMask = 0xFE0;
constexpr unsigned kPathStepShift = 5;
constexpr unsigned kPathLowMask = 0x3FFFFF;
constexpr unsigned kPathXShift = 22;
constexpr unsigned kPathZShift = 27;
constexpr unsigned kPathTurn = 0x10;
constexpr unsigned kPathStop = 0x10000;
constexpr unsigned kPathChanged = 0x20000;
constexpr unsigned kPathCellShift = 18;
constexpr unsigned kNearTargetSquared = 0x31000;
constexpr int kModeSlots = 7;
constexpr int kModeStride = 2;

Battle_Ai_MovementPathView* PathView(Battle_Ai_ActorAiState& state) {
  return reinterpret_cast<Battle_Ai_MovementPathView*>(&state);
}

void Recover(Battle_Ai_ActorAiState& state) {
  Battle_Ai_UpdateMovementRecovery(
      reinterpret_cast<Battle_Ai_MovementRecoveryView*>(&state));
}

void Command(Battle_Ai_ActorAiState& state, int action) {
  Battle_Ai_IssueActorCommand(&state, action);
}

unsigned ReadTile(const u_char* bytes) {
  unsigned result;
  std::memcpy(&result, bytes, sizeof(result));
  return result;
}

void StoreTile(u_char* bytes, unsigned value) {
  std::memcpy(bytes, &value, sizeof(value));
}

void RoomFlag(int bit, bool clear) {
  if (Battle_Ai_roomAiState == nullptr) return;
  unsigned mask = 1u << (static_cast<unsigned>(bit) & kShiftMask);
  if (clear) mask = ~mask;
  auto& value = Battle_Ai_roomAiState->reservedEPrefix[0];
  const unsigned updated =
      (mask & 0x80000000u) != 0 ? value & mask : value | mask;
  value = static_cast<u_char>(updated);
}

int GroundTest(Battle_Ai_MovementPathView* view, int direction) {
  return Battle_Ai_CanWalkInDirection(
      reinterpret_cast<Battle_Ai_WanderView*>(view), direction);
}

int FlightTest(Battle_Ai_MovementPathView* view, int direction) {
  return Battle_Ai_CanFlyInDirection(
      reinterpret_cast<Battle_Ai_DirectionCacheView*>(view), direction);
}

bool NearTarget(const Battle_Ai_ActorAiState& state) {
  const auto from = Unpack(state.tile.word);
  const auto to = Unpack(state.destination.word);
  const int dx = (to.x - from.x) * (1 << kTileShift);
  const int dz = (to.z - from.z) * (1 << kTileShift);
  return static_cast<unsigned>(dx * dx + dz * dz) < kNearTargetSquared &&
         (state.opaque4 & 1) != 0;
}

int FinishStep(Battle_Ai_ActorAiState& state, unsigned flags, int direction,
               bool axis_only) {
  auto plan = Battle_Ai_RefreshDetourProgress(
      &state, (flags & ~kPathDirectionBits) |
                  (static_cast<unsigned>(direction) << kPathHeadingShift));
  const auto tile = Unpack(state.tile.word);
  const auto target = Unpack(state.destination.word);
  unsigned x = 0, z = 0;
  if (!axis_only) {
    x = tile.x == target.x ? 0 : tile.x + (tile.x < target.x);
    z = tile.z == target.z ? 0 : tile.z + (tile.z < target.z);
  } else if ((plan & kHalfTurnDirections) != 0) {
    z = tile.z + (tile.z < target.z);
  } else {
    x = tile.x + (tile.x < target.x);
  }
  state.navigationFlags =
      (plan & kPathLowMask) | (x << kPathXShift) | (z << kPathZShift) | 1;
  return 1;
}

// decomp: 7B5DC.s func_800E3EAC / func_800E3FF4 / func_800E40F0.
int PlanStep(Battle_Ai_ActorAiState& state, int row, int first, int second,
             Battle_Ai_PathTest test) {
  const auto flags =
      Battle_Ai_InitializeObstacleDetour(&state, state.navigationFlags, test);
  const int turn = (flags & kPathTurn) != 0 ? 1 : -1;
  int direction;
  if (row == 0) {
    const int offset = ((first + turn * 2) & 7) == second ? turn * 2 : 4;
    direction = (second + offset) & kDirectionMask;
    if (test(PathView(state), direction)) {
      return FinishStep(state, flags, direction, false);
    }
    direction = (static_cast<int>((flags >> 1) & kDirectionMask) + turn * 2) &
                kDirectionMask;
  } else if (row == 1) {
    if (turn == second) return FinishStep(state, flags, first, true);
    direction = (first + kHalfTurnDirections) & kDirectionMask;
    if (test(PathView(state), direction)) {
      return FinishStep(state, flags, direction, true);
    }
    direction = (direction - turn * 2) & kDirectionMask;
  } else {
    direction = (first - turn * 2) & kDirectionMask;
    if (test(PathView(state), direction)) {
      return FinishStep(state, flags, direction, true);
    }
    direction = (direction + kHalfTurnDirections) & kDirectionMask;
    if (test(PathView(state), direction)) {
      return FinishStep(state, flags, direction, true);
    }
    direction = (first + kHalfTurnDirections) & kDirectionMask;
  }
  if (!test(PathView(state), direction)) {
    state.navigationFlags = flags;
    return 0;
  }
  return FinishStep(state, flags, direction, row != 0);
}

int Detour(Battle_Ai_ActorAiState& state, int row, int mode, int first,
           int second, Battle_Ai_PathTest test) {
  const bool limited = (mode & 1) != 0;
  if (limited && !NearTarget(state)) return 0;
  const int result = PlanStep(state, row, first, second, test);
  if (limited) Battle_Ai_SetDetourBoundary(&state);
  return result;
}
}  // namespace

// decomp: func_800E3BC8 (src/BATTLE/BATTLE.PRG/7B3C8.s:11).
void Battle_Ai_SetDetourBoundary(Battle_Ai_ActorAiState* view) {
  auto& state = State(view);
  const auto target = Unpack(state.destination.word);
  const auto tile = Unpack(state.tile.word);
  const unsigned x = target.x == tile.x ? 0 : target.x + (tile.x < target.x);
  const unsigned z = target.z == tile.z ? 0 : target.z + (tile.z < target.z);
  state.navigationFlags = (state.navigationFlags & kPathLowMask) |
                          (x << kPathXShift) | (z << kPathZShift);
}

// decomp: func_800E3CDC (src/BATTLE/BATTLE.PRG/7B3C8.s:90).
unsigned int Battle_Ai_RefreshDetourProgress(Battle_Ai_ActorAiState* view,
                                             unsigned int flags) {
  auto& state = State(view);
  unsigned repeats = (ReadTile(state.detour_tile_bytes) & kTileMask) ==
                             (state.tile.word & kTileMask)
                         ? state.detour_repeats + 1
                         : 0;
  if (repeats >= kRepeatThreshold) {
    state.opaque19A = kRepeatWait;
    repeats = 0;
  }
  StoreTile(state.detour_tile_bytes, state.tile.word);
  state.detour_repeats = static_cast<u_char>(repeats);
  state.movement_wait = 0;
  unsigned steps = kDetourSteps;
  if (state.movementEnabled == 0 &&
      Battle_Ai_NextActorRandomMasked(kDetourRandomRange) == 0) {
    steps >>= 2;
  }
  return (flags & ~kPathStepMask) | (steps << kPathStepShift);
}

// decomp: func_800E3D5C (src/BATTLE/BATTLE.PRG/7B55C.s:9).
int Battle_Ai_CardinalPassabilityMask(Battle_Ai_MovementPathView* state,
                                      Battle_Ai_PathTest test) {
  Require(state != nullptr && test != nullptr,
          "AI cardinal passability requires state and a native callback");
  int result = 0;
  for (int direction = 0; direction < kAiDirectionBytes; direction += 2) {
    result = (result << 1) | test(state, direction);
  }
  return result;
}

// decomp: func_800E3DDC (src/BATTLE/BATTLE.PRG/7B5DC.s:10).
unsigned int Battle_Ai_InitializeObstacleDetour(Battle_Ai_ActorAiState* view,
                                                unsigned int flags,
                                                Battle_Ai_PathTest test) {
  auto& state = State(view);
  Require(test != nullptr, "AI detour requires a native path callback");
  state.detour_run = state.opaque1;
  if ((flags & kPathStepMask) == 0 || (flags & kPathStop) != 0 ||
      state.opaque4 != 0) {
    flags &= ~kPathStop;
    flags = (flags & ~kPathTurn) |
            (static_cast<unsigned>(Battle_Ai_NextActorRandomMasked(2))
             << kPathTurnShift);
    Battle_Ai_UpdateHesitationTurn(&state, kInitialHesitationChance, 1,
                                   state.requestedMovementMode);
  }
  if ((flags & ~kPathStepMask) == 0) flags &= ~kPathChanged;
  return (flags & kPathCellKeepMask) |
         (static_cast<unsigned>(
              Battle_Ai_CardinalPassabilityMask(PathView(state), test))
          << kPathCellShift);
}

// decomp: func_800E4180 (src/BATTLE/BATTLE.PRG/7B5DC.s).
int Battle_Ai_UpdateHesitationTurn(void* view, int chance, int kind, int slot) {
  auto& state = State(view);
  if (slot < 0 || slot >= kModeSlots || kind < 1 || kind > kPollKind) {
    Fail("AI hesitation request has an invalid slot or kind");
  }
  const auto offset = static_cast<std::size_t>(slot) * kModeStride;
  if (offset >= std::size(state.movementModeCounters)) {
    Fail("AI hesitation kind offset is out of bounds");
  }
  auto& stored_kind = state.movementModeCounters[offset];
  u_char* count_byte = &state.reactionPending;
  if (slot != kModeSlots - 1) {
    if (offset + 1 >= std::size(state.movementModeCounters)) {
      Fail("AI hesitation counter offset is out of bounds");
    }
    count_byte = &state.movementModeCounters[offset + 1];
  }
  auto& remaining = *count_byte;
  int count = remaining;
  if (count == 0) {
    if (kind == kPollKind ||
        chance < Battle_Ai_NextActorRandomMasked(kRandomRange)) {
      return 0;
    }
    count = std::bit_cast<std::array<std::uint8_t, kHesitationKindCount>>(
        Content_battleAiHesitationLengths[0])[kind];
    Require(count > 0, "AI hesitation lengths were not loaded");
    stored_kind = static_cast<u_char>(kind);
  }
  remaining = static_cast<u_char>(count - 1);
  state.opaque13A = kTurnDuration;
  if (kind == 2) {
    state.heading = Half(count < kSecondCryThreshold ? 0 : kPitchHigh);
  } else if (kind == 1) {
    state.heading = Half(count >= kFirstCryHigh  ? kPitchMiddle
                         : count >= kFirstCryLow ? -kPitchMiddle
                                                 : 0);
  } else {
    return 0;
  }
  Command(state, kTimedTurnIntent);
  return count >= 2;
}

// decomp: func_800E4764 (src/BATTLE/BATTLE.PRG/7BDD4.s:145).
int Battle_Ai_SelectActorVerticalMovementStep(Battle_Ai_ActorAiState* view,
                                              int* value, int* code) {
  auto& state = State(view);
  Require(value != nullptr && code != nullptr,
          "AI vertical movement requires output storage");
  constexpr std::array<unsigned, 2> kBits{0x20, 0x10};
  constexpr std::array<int, 2> kCodes{10, 9};
  int index = state.opaque1 != 0 ? 0 : 1;
  for (int pass = 0; pass < 2; ++pass, index ^= 1) {
    const auto bit = kBits[index];
    if ((state.locomotionFlags & (bit | (bit << kByteShift))) == 0) continue;
    *value =
        (state.locomotionFlags & bit) != 0 ? state.walkStep : state.runStep;
    *code = kCodes[index];
    return 1;
  }
  Require(state.opaque1 == 0,
          "AI running vertical movement has no configured gait");
  return 0;
}

// decomp: func_800E47E4 (src/BATTLE/BATTLE.PRG/7BDD4.s:189).
int Battle_Ai_IsTileBehindActorTarget(Battle_Ai_ActorAiState* view,
                                      unsigned int tile) {
  auto& state = State(view);
  if (state.reservedA[2] != 0) return 0;
  Require(state.actorId < kAiFrameActorSlots &&
              Actors_actorRecords[state.actorId] != nullptr,
          "AI facing query requires an actor model");
  const auto current = Unpack(state.tile.word), target = Unpack(tile);
  int facing;
  if (state.movementTarget >= kAiFrameActorSlots) {
    facing = Actors_actorRecords[state.actorId]->instance.facing;
  } else {
    Require(Actors_actorRecords[state.movementTarget] != nullptr,
            "AI facing query requires a target model");
    const auto& position =
        Actors_actorRecords[state.movementTarget]->instance.position;
    facing = Core_Gte_Atan2(static_cast<std::uint16_t>(state.position.vx) -
                                static_cast<std::uint16_t>(position.vx),
                            static_cast<std::uint16_t>(state.position.vz) -
                                static_cast<std::uint16_t>(position.vz));
  }
  const int angle = 0xC00 - facing;
  return Core_Gte_Cos(angle) * (target.x - current.x) +
             Core_Gte_Sin(angle) * (target.z - current.z) <
         0;
}

// decomp: func_800E49D8 (src/BATTLE/BATTLE.PRG/7BDD4.s:342).
unsigned int Battle_Ai_FindAiDescentTile(Battle_Ai_ActorAiState* view,
                                         unsigned int packed, int direction,
                                         int steps) {
  auto& state = State(view);
  if (Battle_Ai_FrameState()->roomData == nullptr || steps == 0) return 0;
  const auto delta = Delta(direction);
  auto tile = Unpack(packed);
  if (!InGrid(tile)) return 0;
  const int raw = Field_GroundHeightFlagsAtPackedTile(packed);
  if (raw < 0) return 0;
  const int floor = Field_GroundHeightAtPackedTile(packed);
  if (Field_GroundHeightAtPackedTile(state.tile.word) >= floor) return 0;
  for (int taken = 0; taken < kAiFrameGridSide; ++taken) {
    tile.x += delta.x;
    tile.z += delta.z;
    if (!InGrid(tile)) return 0;
    if (Field_GroundHeightAtTile(tile.x, tile.z) < floor) {
      const unsigned target = (packed & ~kTileMask) | Pack(tile);
      return Step(state, state.tile.word, target, direction) > 0 &&
                     Occupancy(state, target) > 0
                 ? target
                 : 0;
    }
    if (taken + 1 >= steps - 1) return 0;
  }
  Core_Debugf("ai", "AI descent scan exceeded the bounded room grid");
  std::abort();
}

// decomp: func_800E4BD8 / func_800E4BE0 (7BDD4.s:527/533).
void Battle_Ai_ClearAiContactOccupancy(int bit) { RoomFlag(bit, true); }
void Battle_Ai_SetAiContactOccupancy(int bit) { RoomFlag(bit, false); }

// decomp: func_800E4B10 (src/BATTLE/BATTLE.PRG/7BDD4.s:431).
void Battle_Ai_BeginAiTerrainMovement(void* view) {
  auto& state = State(view);
  state.stallCounter = 0;
  state.nextTile = state.tile;
  Command(state, kTerrainIntent);
}

// decomp: func_800E4B18 (src/BATTLE/BATTLE.PRG/7BDD4.s:438).
void Battle_Ai_BeginAiAlternateTerrainMovement(Battle_Ai_ActorAiState* view) {
  auto& state = State(view);
  state.stallCounter = 0;
  state.nextTile = state.tile;
  Command(state, kAlternateTerrainIntent);
}

namespace {
// decomp: L800DF8A4 (src/BATTLE/BATTLE.PRG/766FC.s).
void ContinueGate(Battle_Ai_ActorAiState& state) {
  if (state.tile.tile.tag < 2) {
    StoreTile(state.route_exit_bytes, ~0u);
    Battle_Ai_ClearAiContactOccupancy(state.route_gate);
    state.route_gate = 0;
    return;
  }
  Require(Battle_Ai_routeGates != nullptr,
          "AI route continuation requires native gates");
  const int index = Battle_Ai_FindRouteGateAtTile(
      reinterpret_cast<intptr_t>(&state), ReadTile(state.route_exit_bytes), 0);
  if (index >= 0) {
    Require(index < kRouteLinks, "AI route gate index is out of bounds");
    const auto& entry =
        Battle_Ai_routeGates[static_cast<std::size_t>(index) * 2];
    const unsigned tile = Pack({entry.tileX, entry.tileZ});
    if (Occupancy(state, tile) > 0) {
      StoreTile(state.route_exit_bytes, ~0u);
      Battle_Ai_ClearAiContactOccupancy((index >> 1) + 2);
      state.route_gate = 0;
      state.nextTile.word = tile;
      Command(state, kStepIntent);
      return;
    }
  }
  Recover(state);
}
}  // namespace

// decomp: func_800E4B2C (src/BATTLE/BATTLE.PRG/7BDD4.s:449).
void Battle_Ai_MoveActorTowardDestinationTile(void* view, int /*angle*/) {
  auto& state = State(view);
  if (state.route_gate != 0) {
    ContinueGate(state);
  } else if ((state.destination.word & kTileMask) ==
             (state.tile.word & kTileMask)) {
    Recover(state);
  } else {
    const auto target = Unpack(state.destination.word);
    state.moveX = target.x * (1 << kTileShift) + kTileCenter;
    state.moveZ = target.z * (1 << kTileShift) + kTileCenter;
    Battle_Ai_UpdateNavigationAreaMovement(
        reinterpret_cast<Battle_Ai_TerrainMoveView*>(&state));
  }
}

// decomp: func_800E4B70 (src/BATTLE/BATTLE.PRG/7BDD4.s:473).
void Battle_Ai_BeginAiGroundMovement(void* view, int angle) {
  auto& state = State(view);
  if (state.opaque1A != 0) {
    Battle_Ai_MoveWithHeightFilteredDirections(
        reinterpret_cast<Battle_Ai_TerrainMoveView*>(&state));
  } else if (state.route_gate != 0) {
    ContinueGate(state);
  } else {
    state.isStuck = 1;
    Battle_Ai_FrameState()->walk_valid = 0;
    Battle_Ai_ResolveDirectionalMovement(&state, Angle(angle), 1);
  }
}

namespace {
enum class LocomotionRoute {
  kGroundTile,
  kGroundHeading,
  kFlightTile,
  kFlightHeading,
  kRadialTile,
  kRadialHeading
};
}  // namespace

// decomp: func_800E50A0 (src/BATTLE/BATTLE.PRG/7C7E0.s:66).
void Battle_Ai_DispatchAiLocomotionMode(void* view, int kind, int angle) {
  auto& state = State(view);
  Require(state.locomotionMode < kLocomotionModes && kind >= 0 && kind < 2,
          "AI locomotion dispatch has an invalid mode or request");
  switch (static_cast<LocomotionRoute>(state.locomotionMode * 2 + kind)) {
    case LocomotionRoute::kGroundTile:
      Battle_Ai_MoveActorTowardDestinationTile(&state, angle);
      break;
    case LocomotionRoute::kGroundHeading:
      Battle_Ai_BeginAiGroundMovement(&state, angle);
      break;
    case LocomotionRoute::kFlightTile:
      Battle_Ai_UpdateFlightMovement(
          reinterpret_cast<Battle_Ai_MovementStepView*>(&state), angle);
      break;
    case LocomotionRoute::kFlightHeading:
      Battle_Ai_UpdateFlightDirectionalMovement(&state, angle);
      break;
    case LocomotionRoute::kRadialTile:
      Battle_Ai_MoveToRadialDestination(
          reinterpret_cast<Battle_Ai_MovementDestinationView*>(&state), angle);
      break;
    case LocomotionRoute::kRadialHeading:
      Battle_Ai_MoveAwayFromReference(
          reinterpret_cast<Battle_Ai_MovementDestinationView*>(&state), angle);
      break;
  }
}

// decomp: func_800E4FE0 (src/BATTLE/BATTLE.PRG/7C7E0.s:10).
void Battle_Ai_BeginAiMovementTowardPoint(Battle_Ai_ActorAiState* view,
                                          int kind) {
  auto& state = State(view);
  state.opaque1 = static_cast<u_char>(kind);
  state.opaque9 = 1;
  const unsigned packed = static_cast<unsigned>(state.moveX) |
                          (static_cast<unsigned>(state.moveZ) << 16);
  state.targetX =
      std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(packed));
  state.targetZ = std::bit_cast<std::int16_t>(
      static_cast<std::uint16_t>(packed >> kHalfwordBits));
  Battle_Ai_DispatchAiLocomotionMode(
      &state, 1,
      Core_Gte_Atan2(state.moveX - state.position.vx,
                     state.moveZ - state.position.vz));
}

namespace {
bool AdjacentAction(Battle_Ai_ActorAiState& state, unsigned tile) {
  const auto status = tile >> kStatusShift;
  if (status == 2) {
    if (state.movement_wait == 0) state.movement_wait = kBlockedWait;
    Recover(state);
    return true;
  }
  if (tile == 0) return false;
  if (Battle_Ai_IsTileBehindActorTarget(&state, tile) != 0 &&
      Battle_Ai_UpdateHesitationTurn(&state, kRandomRange, 2,
                                     state.requestedMovementMode) != 0) {
    return true;
  }
  state.nextTile.word = tile;
  Command(state, kStepIntent);
  return true;
}

unsigned Adjacent(Battle_Ai_ActorAiState& state, int first, int second,
                  unsigned angle) {
  return Battle_Ai_FindAdjacentMovementDestination(
      reinterpret_cast<Battle_Ai_MoveTargetView*>(&state), first, second,
      angle);
}

void DetourOrRecover(Battle_Ai_ActorAiState& state, int row, int mode,
                     int first, int second, Battle_Ai_PathTest test) {
  if (Detour(state, row, mode, first, second, test) == 0) {
    Recover(state);
  } else {
    state.heading = Half((state.navigationFlags >> 1) & kDirectionMask);
    Command(state, kWalkDirectionIntent);
  }
}
}  // namespace

// decomp: func_800E3600 (src/BATTLE/BATTLE.PRG/771A8.c:2549;
// nonmatchings/771A8/func_800E3600.s). C counterpart: 771A8.c:2266.
void Battle_Ai_ResolveDirectionalMovement(Battle_Ai_ActorAiState* view,
                                          int angle_arg, int mode) {
  auto& state = State(view);
  Require(mode >= 0 && mode < kDirectionalModes,
          "AI directional movement mode is invalid");
  const unsigned angle = static_cast<unsigned>(angle_arg);
  const auto test = mode >= 2 ? FlightTest : GroundTest;
  if ((state.navigationFlags & 1) != 0) {
    const int action = Battle_Ai_FollowObstacleDetour(PathView(state), test,
                                                      mode >= 2 ? 12 : 11);
    if (action < 0) {
      Battle_Ai_RefreshAiMovementCooldown(
          reinterpret_cast<Battle_Ai_TerrainMoveView*>(&state));
      Recover(state);
    } else {
      state.heading = Half((state.navigationFlags >> 1) & kDirectionMask);
      Command(state, action);
    }
    return;
  }
  const unsigned quadrant = (angle & kAngleMask) >> 10;
  const int nearest = static_cast<int>(
      ((angle & 0x3FF) > 512 ? (quadrant + 1) & 3 : quadrant) * 2);
  const auto delta = Delta(nearest);
  const auto current = Unpack(state.tile.word);
  const auto target = Unpack(state.destination.word);
  const int distance = std::min<int>(
      state.maxRange, std::abs((nearest & 2) != 0 ? target.x - current.x
                                                  : target.z - current.z));
  const Tile point{current.x + delta.x, current.z + delta.z};
  const auto descent =
      Battle_Ai_FindAiDescentTile(&state, Pack(point), nearest, distance);
  if (descent != 0) {
    state.nextTile.word = descent & ~kTileTagMask;
    Command(state, kStepIntent);
    return;
  }
  if ((angle & kQuadrantMask) == 0) {
    const int direction = static_cast<int>((angle >> 9) & 6);
    if (test(PathView(state), direction)) {
      state.heading = Half(angle);
      Command(state, kWalkHeadingIntent);
    } else if ((static_cast<unsigned>(state.movement_wait) - 1 >=
                kDetourSteps) &&
               AdjacentAction(state, Adjacent(state, 0, 0, direction))) {
      return;
    } else if (mode == 1 && state.opaque4 == 0) {
      Recover(state);
    } else {
      DetourOrRecover(state, 2, mode, direction, 0, test);
    }
    return;
  }
  const int left = static_cast<int>(((angle >> 10) + 1) * 2) & 7;
  const int right = static_cast<int>((angle >> 10) * 2) & 7;
  const int diagonal = static_cast<int>((angle >> 10) * 2 + 1) & 7;
  const int left_open = test(PathView(state), left);
  const int right_open = test(PathView(state), right);
  if (!left_open && !right_open) {
    if ((static_cast<unsigned>(state.movement_wait) - 1 >= kDetourSteps) &&
        AdjacentAction(state, Adjacent(state, left, right, angle))) {
      return;
    }
    DetourOrRecover(state, 0, mode, left, right, test);
    return;
  }
  if (left_open && right_open) {
    if (!test(PathView(state), diagonal)) {
      state.heading = Half(nearest == left ? left : right);
      Command(state, kWalkDirectionIntent);
    } else {
      state.heading = Half(angle);
      Command(state, kWalkHeadingIntent);
    }
    return;
  }
  state.heading = Half(!right_open ? left : right);
  if ((static_cast<unsigned>(state.movement_wait) - 1 >= kDetourSteps) &&
      AdjacentAction(state, Adjacent(state, 0, 0, !left_open ? left : right))) {
    return;
  }
  const bool aligned =
      (state.heading & 2) != 0 ? current.x == target.x : current.z == target.z;
  const bool detour =
      (mode & 1) == 0
          ? aligned
          : state.opaque4 != 0 &&
                Core_Gte_Cos(Angle(((left_open ? right : left) << 9) -
                                   angle_arg)) >= 3959;
  if (detour) {
    DetourOrRecover(state, 1, mode, state.heading, left_open ? 1 : -1, test);
  } else {
    Command(state, kWalkDirectionIntent);
  }
}
