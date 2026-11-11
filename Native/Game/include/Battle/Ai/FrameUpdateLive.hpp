#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstdint>
#include <optional>

#include "Battle/Ai/FrameUpdate.hpp"
#include "Battle/Ai/NavigationPrimitives.hpp"

struct Battle_Ai_PerceptionUpdateView;
struct Battle_Ai_AiCreationView;

// Binding from the battle AI tables and the frame context (FrameUpdate.h) to
// the typed frame update. The signatures keep the module-image entries'
// calling shape, so relocated BATTLE.PRG addresses resolve to these.
namespace VS::Battle::Ai {
// Caches the actor and room tables into the frame context; returns the packed
// room bounds. A room whose auxiliary section cannot hold the terrain region
// header aborts with a diagnostic.
int CacheLiveActorAiWorkingState();
// Runs the frame's AI stages; a frame with a room map but no room state or
// linked actor list aborts with a diagnostic.
void RunLiveActorAiUpdateStages();
void TickLiveActorDecisionTimers(Battle_Ai_PerceptionUpdateView* view);
void InitializeLiveActorLocomotionMode(Battle_Ai_AiCreationView* view);
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
