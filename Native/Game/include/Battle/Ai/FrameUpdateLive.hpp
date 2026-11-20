#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstddef>
#include <cstdint>
#include <optional>

#include "Battle/Ai/FrameUpdate.hpp"
#include "Battle/Ai/NavigationPrimitives.hpp"

namespace VS::Battle::Ai {
struct ActorAiState;
struct RoomAiState;
struct PerceptionUpdateView;
struct AiCreationView;
}  // namespace VS::Battle::Ai

// Binding from the battle AI tables and the frame context to the typed frame
// update. The signatures keep the module-image entries' calling shape, so
// relocated BATTLE.PRG addresses resolve to these.
namespace VS::Battle::Ai {
// The battle AI tables cached for the current frame.
struct FrameContext {
  VS::Battle::Ai::ActorAiState* actors[kFrameActorSlots];
  VS::Battle::Ai::RoomAiState* room;
  std::uint16_t (*heightMap)[kNavigationGridSide];
  std::uint16_t (*tileFlags)[kNavigationGridSide];
  std::uint8_t* terrainRegions;
  std::size_t terrain_region_count;
  int movementResult;
  // One bit per direction: walking probed, walking open and blocker probed.
  std::uint8_t walk_valid;
  std::uint8_t walk_passable;
  std::uint8_t blocker_valid;
  std::uint8_t reserved_cache;
  // The actor blocking each direction whose blocker_valid bit is set.
  std::uint8_t blocker_actor[kNavigationDirections];
  std::uint32_t roomBounds;
  std::uint8_t directions[kNavigationDirections];
  void* roomData;
  std::uint32_t randomState;
  int lastBudgetTick;

  // Forgets the frame's direction probes; the blocking actors are kept.
  void ClearDirectionTests() {
    walk_valid = 0;
    walk_passable = 0;
    blocker_valid = 0;
    reserved_cache = 0;
  }
};

[[nodiscard]] FrameContext& LiveFrameContext();
// The tile's blocked-direction byte in the room's terrain regions; nullptr,
// with a diagnostic, outside the room or its regions.
[[nodiscard]] std::uint8_t* FindTerrainRegionCell(int tile_x, int tile_z);

// Caches the actor and room tables into the frame context; returns the packed
// room bounds. A room whose auxiliary section cannot hold the terrain region
// header aborts with a diagnostic.
int CacheLiveActorAiWorkingState();
// Runs the frame's AI stages; a frame with a room map but no room state or
// linked actor list aborts with a diagnostic.
void RunLiveActorAiUpdateStages();
void TickLiveActorDecisionTimers(VS::Battle::Ai::PerceptionUpdateView* view);
void InitializeLiveActorLocomotionMode(VS::Battle::Ai::AiCreationView* view);
// An actor ID outside the AI table aborts; an empty slot is left alone.
void SetLiveActorVerticalMovementSteps(int actor_id, int walk_step,
                                       int run_step);
// The room's direction codes cached in the frame context.
[[nodiscard]] NavigationDirectionCodes LiveDirectionCodes();
// Whether the room has route data, whose terrain regions hold each tile's
// blocked directions.
[[nodiscard]] bool LiveHasRoomData();
// The tile's blocked-direction bits; nothing without the room's regions or
// outside them.
[[nodiscard]] std::optional<std::uint8_t> LiveTerrainRegionCell(int x, int z);
// The room AI state's walkable tile bounds; nothing without a room AI state.
[[nodiscard]] std::optional<RoomTileBounds> LiveRoomTileBounds();

// The room-region queries a typed world answers from the frame context.
template <typename World>
class LiveRoomRegions : public World {
 public:
  [[nodiscard]] NavigationDirectionCodes DirectionCodes() const override {
    return LiveDirectionCodes();
  }
  [[nodiscard]] bool HasRoomData() const override { return LiveHasRoomData(); }
  [[nodiscard]] std::optional<std::uint8_t> TerrainRegionCell(
      int x, int z) const override {
    return LiveTerrainRegionCell(x, z);
  }
};
}  // namespace VS::Battle::Ai
