// Hand-maintained (tools/port/owned.csv).
extern "C" {
#include "Battle/Ai/RoomState.h"
}

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <optional>
#include <span>

#include "Battle/Ai/ActorCleanupLive.hpp"
#include "Battle/Ai/FrameUpdateLive.hpp"
#include "Battle/Ai/NavigationPrimitivesLive.hpp"
#include "Battle/Ai/ReactionDelay.hpp"
#include "Battle/Ai/RoomAiMap.hpp"
#include "Core/Math/Distance.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Field/CollisionMapLive.hpp"
#include "Field/RoomLoadingLive.hpp"

extern "C" {
#include "Actors/ActorAllocation.h"
#include "Actors/ActorRecord.h"
#include "Actors/Animation.h"
#include "Actors/ModelInstance.h"
#include "Actors/Shadow.h"
#include "Actors/Skeleton.h"
#include "Battle/Actor.h"
#include "Battle/Ai/ActionPlanning.h"
#include "Battle/Ai/ActionSelection.h"
#include "Battle/Ai/ActorState.h"
#include "Battle/Ai/AiState.h"
#include "Battle/Ai/AiViews.h"
#include "Battle/Ai/Behavior.h"
#include "Battle/Ai/FrameUpdate.h"
#include "Battle/Ai/Movement.h"
#include "Battle/Ai/Navigation.h"
#include "Battle/Ai/OrbitingBoss.h"
#include "Battle/Ai/Perception.h"
#include "Battle/Damage.h"
#include "Core/Diagnostics.h"
#include "Field/Collision.h"
#include "Field/Gimmicks.h"
#include "Field/Mpd.h"
#include "Field/Platforms.h"
#include "Field/Room.h"
#include "Field/RoomLoadingLiveState.h"
#include "Items/Inventory.h"
#include "Items/Loadout.h"
#include "Legacy/Psx.h"
#include "Render/Palette.h"
}  // extern "C"

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
  Battle_Ai_RoomAiState state;
  u_short flags[VS::Battle::Ai::kRoomGridSide][VS::Battle::Ai::kRoomGridSide];
  u_short heights[VS::Battle::Ai::kRoomGridSide][VS::Battle::Ai::kRoomGridSide];
};

void ClearRoomAiBindings() {
  memset(Battle_Ai_FrameState(), 0, sizeof *Battle_Ai_FrameState());
  Battle_Ai_roomAiState = nullptr;
  Battle_Ai_tileFlagMap = nullptr;
  Battle_Ai_specialTerrainHeights = nullptr;
  Battle_Ai_roomHasWaypoints = nullptr;
  Battle_Ai_patrolRoutes = nullptr;
  Battle_Ai_roomWaypoints = nullptr;
  Battle_Ai_routeGates = nullptr;
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
  const Field_MpdFloorTrigger* trigger =
      Field_RoomTriggerAtTile(tile.x, tile.z, 0);
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
      Core_Errorf("ai", "Room AI allocation failed: bytes=%zu",
                  sizeof(RoomAiStorage));
      abort();
    }
    memset(storage, 0, sizeof *storage);
    const VS::Battle::Ai::RoomAiSettings& settings = grid.settings;
    Battle_Ai_RoomAiState& state = storage->state;
    state.maxTileX = settings.max_tile_x;
    state.maxTileZ = settings.max_tile_z;
    state.minTileX = settings.min_tile_x;
    state.minTileZ = settings.min_tile_z;
    state.reserved4[0] = static_cast<char>(settings.collision_extent[0]);
    state.reserved4[1] = static_cast<char>(settings.collision_extent[1]);
    state.defaultActionDelay = settings.default_action_delay;
    state.allowsNonSpellActions = settings.allows_non_spell_actions ? 1 : 0;
    state.engagementLimit = settings.engagement_limit;
    state.flightCeiling = settings.flight_ceiling;
    for (std::size_t z = 0; z < grid.flags.size(); ++z) {
      for (std::size_t x = 0; x < grid.flags[z].size(); ++x) {
        storage->flags[z][x] = grid.flags[z][x];
        storage->heights[z][x] = grid.heights[z][x];
      }
    }
    Battle_Ai_roomAiState = &storage->state;
    Battle_Ai_tileFlagMap = storage->flags;
    Battle_Ai_specialTerrainHeights = storage->heights;
    auto* auxiliary = static_cast<unsigned char*>(Field_roomData.section6);
    Battle_Ai_roomHasWaypoints = auxiliary;
    Battle_Ai_patrolRoutes =
        auxiliary != nullptr
            ? reinterpret_cast<uint16_t (*)[kPatrolRouteLength]>(
                  auxiliary + kPatrolRoutesOffset)
            : nullptr;
    Battle_Ai_roomWaypoints =
        auxiliary != nullptr
            ? reinterpret_cast<uint16_t*>(auxiliary + kWaypointsOffset)
            : nullptr;
    Battle_Ai_routeGates = auxiliary != nullptr
                               ? reinterpret_cast<Battle_Ai_RouteGateEnd*>(
                                     auxiliary + kRouteGatesOffset)
                               : nullptr;
  }

  [[nodiscard]] std::optional<VS::Battle::Ai::RoomAiSettings>
  PublishedSettings() const override {
    const Battle_Ai_RoomAiState* state = Battle_Ai_roomAiState;
    if (state == nullptr) return std::nullopt;
    VS::Battle::Ai::RoomAiSettings settings;
    settings.max_tile_x = state->maxTileX;
    settings.max_tile_z = state->maxTileZ;
    settings.min_tile_x = state->minTileX;
    settings.min_tile_z = state->minTileZ;
    settings.collision_extent = {
        static_cast<std::uint8_t>(state->reserved4[0]),
        static_cast<std::uint8_t>(state->reserved4[1])};
    settings.default_action_delay = state->defaultActionDelay;
    settings.allows_non_spell_actions = state->allowsNonSpellActions != 0;
    settings.engagement_limit = state->engagementLimit;
    settings.flight_ceiling = state->flightCeiling;
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
    if (Battle_Ai_roomAiState != nullptr) {
      VS::Core::Memory::FreeLiveFromLowEnd(Battle_Ai_roomAiState);
    }
    ClearRoomAiBindings();
  }
};

class BattleReactionDelays final : public VS::Battle::Ai::ActorReactionDelays {
 public:
  bool SetMinimumActionDelay(int actor_id, std::uint8_t frames) override {
    static_assert(std::size(Battle_Ai_actorAiStates) ==
                  VS::Battle::Ai::kAiActorSlotCount);
    if (actor_id < 0 || actor_id >= VS::Battle::Ai::kAiActorSlotCount) {
      return false;
    }
    Battle_Ai_ActorAiState* state = Battle_Ai_actorAiStates[actor_id];
    if (state == nullptr) return false;
    state->minActionDelay = frames;
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

extern "C" {
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
}  // extern "C"
