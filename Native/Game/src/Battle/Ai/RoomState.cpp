// Hand-maintained (tools/port/owned.csv).
#include "Battle/Ai/RoomState.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <optional>
#include <span>

#include "Actors/ActorAllocation.hpp"
#include "Actors/ActorRecord.h"
#include "Actors/Animation.hpp"
#include "Actors/ModelInstance.h"
#include "Actors/ShpModel.hpp"
#include "Battle/Actor.h"
#include "Battle/Ai/ActionPlanning.hpp"
#include "Battle/Ai/ActorCleanupLive.hpp"
#include "Battle/Ai/ActorState.hpp"
#include "Battle/Ai/AiState.hpp"
#include "Battle/Ai/FrameUpdateLive.hpp"
#include "Battle/Ai/Movement.hpp"
#include "Battle/Ai/Navigation.hpp"
#include "Battle/Ai/NavigationPrimitivesLive.hpp"
#include "Battle/Ai/OrbitingBoss.hpp"
#include "Battle/Ai/Perception.hpp"
#include "Battle/Ai/PlanningPrimitivesLive.hpp"
#include "Battle/Ai/ReactionDelay.hpp"
#include "Battle/Ai/RoomAiMap.hpp"
#include "Battle/Damage.h"
#include "Core/Diagnostics.hpp"
#include "Core/Math/Distance.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Field/CollisionMapLive.hpp"
#include "Field/Gimmicks.h"
#include "Field/InteractionTargetLive.hpp"
#include "Field/Mpd.h"
#include "Field/Room.h"
#include "Field/RoomLoadingLive.hpp"
#include "Field/RoomLoadingLiveState.hpp"
#include "Items/ItemRecords.hpp"
#include "Legacy/Psx.h"
#include "Render/Palette.hpp"

namespace {
using VS::Battle::Ai::RoomAiGrid;
using VS::Battle::Ai::RoomAiSource;
using VS::Core::Math::TileCoordinate;

enum {
  kPatrolRoutesOffset = 0x400,
  kWaypointsOffset = 0x440,
  kRouteGatesOffset = 0x454,
  kPatrolRouteLength = 8
};

// Published room AI storage. The PS1 allocation (0x1038 bytes with tables at
// 0x38/0x838) is provenance only; the native size comes from this type.
struct RoomAiStorage {
  VS::Battle::Ai::RoomAiState state;
  u_short flags[VS::Battle::Ai::kRoomGridSide][VS::Battle::Ai::kRoomGridSide];
  u_short heights[VS::Battle::Ai::kRoomGridSide][VS::Battle::Ai::kRoomGridSide];
};

void ClearRoomAiBindings() {
  memset(&VS::Battle::Ai::LiveFrameContext(), 0,
         sizeof VS::Battle::Ai::LiveFrameContext());
  VS::Battle::Ai::roomAiState = nullptr;
  VS::Battle::Ai::tileFlagMap = nullptr;
  VS::Battle::Ai::specialTerrainHeights = nullptr;
  VS::Battle::Ai::roomHasWaypoints = nullptr;
  VS::Battle::Ai::patrolRoutes = nullptr;
  VS::Battle::Ai::roomWaypoints = nullptr;
  VS::Battle::Ai::routeGates = nullptr;
}

std::optional<std::uint32_t> CollisionCell(TileCoordinate tile) {
  const Field_MpdTileCell* cell = VS::Field::LiveCollisionCellAt(
      static_cast<u_int>(tile.x), static_cast<u_int>(tile.z), 0);
  if (cell == nullptr) return std::nullopt;
  std::uint32_t bits = 0;
  memcpy(&bits, cell, sizeof bits);
  return bits;
}

std::optional<VS::Battle::Ai::RoomFloorTrigger> FloorTrigger(
    TileCoordinate tile) {
  const auto target = VS::Field::FindLiveRoomTriggerAtTile({tile.x, tile.z, 0});
  const auto* trigger =
      target ? VS::Field::ResolveLiveTrapTarget(*target) : nullptr;
  if (trigger == nullptr) return std::nullopt;
  return VS::Battle::Ai::RoomFloorTrigger{
      static_cast<std::uint16_t>(trigger->kind),
      static_cast<std::uint16_t>(trigger->lockState.switchValue)};
}

// The battle runtime: Field_roomData is the source and the AI frame bindings
// publish the map to the generated AI code that still reads them.
class BattleRuntime final : public VS::Battle::Ai::RoomAiRuntime {
 public:
  [[nodiscard]] RoomAiSource CurrentRoom() const override {
    RoomAiSource source;
    if (const Field_MpdCollisionHeader* extent = Field_roomData.collisionMap) {
      source.collision_extent =
          VS::Battle::Ai::RoomCollisionExtent{extent->width, extent->height};
    }
    source.collision_cell = CollisionCell;
    source.floor_height = [](int world_x, int world_z) {
      return static_cast<int>(VS::Field::LiveFloorHeightAt(world_x, world_z));
    };
    source.floor_trigger = FloorTrigger;
    if (Field_roomData.environment != nullptr) {
      source.environment_parameter = VS::Field::LiveRoomEnvironmentParameter();
    }
    if (Field_roomData.section6 != nullptr) {
      source.auxiliary = std::span<const std::byte>(
          static_cast<const std::byte*>(Field_roomData.section6),
          Field_roomData.header.section6Length);
    }
    // std::function keeps its small target inline; NRVO or the move rebinds it.
    // NOLINTNEXTLINE(clang-analyzer-core.StackAddressEscape)
    return source;
  }

  void PublishRoomAiMap(const RoomAiGrid& grid) override {
    auto* storage = static_cast<RoomAiStorage*>(
        VS::Core::Memory::AllocateLiveFromLowEnd(sizeof(RoomAiStorage)));
    if (storage == nullptr) {
      VS::Core::Errorf("ai", "Room AI allocation failed: bytes=%zu",
                       sizeof(RoomAiStorage));
      abort();
    }
    memset(storage, 0, sizeof *storage);
    const VS::Battle::Ai::RoomAiSettings& settings = grid.settings;
    VS::Battle::Ai::RoomAiState& state = storage->state;
    state.max_tile_x = settings.max_tile_x;
    state.max_tile_z = settings.max_tile_z;
    state.min_tile_x = settings.min_tile_x;
    state.min_tile_z = settings.min_tile_z;
    state.reserved4[0] = static_cast<char>(settings.collision_extent[0]);
    state.reserved4[1] = static_cast<char>(settings.collision_extent[1]);
    state.default_action_delay = settings.default_action_delay;
    state.allows_non_spell_actions = settings.allows_non_spell_actions ? 1 : 0;
    state.engagement_limit = settings.engagement_limit;
    state.flight_ceiling = settings.flight_ceiling;
    for (std::size_t z = 0; z < grid.flags.size(); ++z) {
      for (std::size_t x = 0; x < grid.flags[z].size(); ++x) {
        storage->flags[z][x] = grid.flags[z][x];
        storage->heights[z][x] = grid.heights[z][x];
      }
    }
    VS::Battle::Ai::roomAiState = &storage->state;
    VS::Battle::Ai::tileFlagMap = storage->flags;
    VS::Battle::Ai::specialTerrainHeights = storage->heights;
    auto* auxiliary = static_cast<unsigned char*>(Field_roomData.section6);
    VS::Battle::Ai::roomHasWaypoints = auxiliary;
    VS::Battle::Ai::patrolRoutes =
        auxiliary != nullptr
            ? reinterpret_cast<uint16_t (*)[kPatrolRouteLength]>(
                  auxiliary + kPatrolRoutesOffset)
            : nullptr;
    VS::Battle::Ai::roomWaypoints =
        auxiliary != nullptr
            ? reinterpret_cast<uint16_t*>(auxiliary + kWaypointsOffset)
            : nullptr;
    VS::Battle::Ai::routeGates =
        auxiliary != nullptr
            ? reinterpret_cast<VS::Battle::Ai::RouteGateEndRecord*>(
                  auxiliary + kRouteGatesOffset)
            : nullptr;
  }

  [[nodiscard]] std::optional<VS::Battle::Ai::RoomAiSettings>
  PublishedSettings() const override {
    const VS::Battle::Ai::RoomAiState* state = VS::Battle::Ai::roomAiState;
    if (state == nullptr) return std::nullopt;
    VS::Battle::Ai::RoomAiSettings settings;
    settings.max_tile_x = state->max_tile_x;
    settings.max_tile_z = state->max_tile_z;
    settings.min_tile_x = state->min_tile_x;
    settings.min_tile_z = state->min_tile_z;
    settings.collision_extent = {
        static_cast<std::uint8_t>(state->reserved4[0]),
        static_cast<std::uint8_t>(state->reserved4[1])};
    settings.default_action_delay = state->default_action_delay;
    settings.allows_non_spell_actions = state->allows_non_spell_actions != 0;
    settings.engagement_limit = state->engagement_limit;
    settings.flight_ceiling = state->flight_ceiling;
    return settings;
  }

  void RefreshDerivedRoomState() override {
    VS::Battle::Ai::CacheLiveActorAiWorkingState();
    VS::Battle::Ai::BuildLiveAiTerrainHeightsWithWorkspace();
    VS::Battle::Ai::UpdateLiveRouteGateAvailability();
  }

  void FreeActorAiStates() override {
    for (Battle_Actor* actor : Battle_actors) {
      if (actor != nullptr) VS::Battle::Ai::FreeLiveActorAiState(actor);
    }
  }

  void ReleaseRoomAiMap() override {
    if (VS::Battle::Ai::roomAiState != nullptr) {
      VS::Core::Memory::FreeLiveFromLowEnd(VS::Battle::Ai::roomAiState);
    }
    ClearRoomAiBindings();
  }
};

class BattleReactionDelays final : public VS::Battle::Ai::ActorReactionDelays {
 public:
  bool SetMinimumActionDelay(int actor_id, std::uint8_t frames) override {
    static_assert(std::size(VS::Battle::Ai::actorAiStates) ==
                  VS::Battle::Ai::kAiActorSlotCount);
    if (actor_id < 0 || actor_id >= VS::Battle::Ai::kAiActorSlotCount) {
      return false;
    }
    VS::Battle::Ai::ActorAiState* state =
        VS::Battle::Ai::actorAiStates[actor_id];
    if (state == nullptr) return false;
    state->min_action_delay = frames;
    return true;
  }
};
}  // namespace

VS::Battle::Ai::RoomAiRuntime& VS::Battle::Ai::BattleRoomAiRuntime() {
  static BattleRuntime runtime;
  return runtime;
}

VS::Battle::Ai::ActorReactionDelays&
VS::Battle::Ai::BattleActorReactionDelays() {
  static BattleReactionDelays delays;
  return delays;
}

void VS::Battle::Ai::InvalidateSessionBindings(void) {
  // The arena was already reset: never dereference or free its former bindings.
  ClearRoomAiBindings();
  VS::Battle::Ai::actorAiStates.fill(nullptr);
  VS::Battle::Ai::attributeActorMask = 0;
  VS::Battle::Ai::secondPerceptionTarget = 0;
  VS::Battle::Ai::arenaOrbit = nullptr;
  VS::Battle::Ai::InvalidateLivePlanningStatsSnapshot();
  VS::Battle::Ai::planningWorkspace = nullptr;
  VS::Battle::Ai::planningPassCandidates = nullptr;
  VS::Battle::Ai::candidateExclusions = nullptr;
  VS::Battle::Ai::fallbackCandidates = nullptr;
  VS::Battle::Ai::fallbackCandidateCount = 0;
  VS::Battle::Ai::unusedPlanCounter = 0;
  VS::Battle::Ai::candidateEngagedThisUpdate = 0;
  VS::Core::Debugf("ai.lifecycle",
                   "invalidated AI bindings after game-arena reset");
}
