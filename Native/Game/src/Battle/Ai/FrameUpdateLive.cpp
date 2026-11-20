// Hand-maintained (tools/port/owned.csv).
// Live binding for the typed frame update (FrameUpdate.cpp).
#include "Battle/Ai/FrameUpdateLive.hpp"

#include <array>
#include <cstdlib>
#include <cstring>
#include <optional>

#include "Actors/ActorAllocation.hpp"
#include "Actors/ActorRecord.h"
#include "Actors/Animation.h"
#include "Actors/ModelInstance.h"
#include "Actors/Movement.h"
#include "Actors/Skeleton.h"
#include "Actors/TextureAnimation.h"
#include "Battle/Action.h"
#include "Battle/ActionTable.h"
#include "Battle/Actor.h"
#include "Battle/Ai/ActionExecutionLive.hpp"
#include "Battle/Ai/ActionPlanning.hpp"
#include "Battle/Ai/ActorPerceptionLive.hpp"
#include "Battle/Ai/ActorState.hpp"
#include "Battle/Ai/AiState.h"
#include "Battle/Ai/AiViews.h"
#include "Battle/Ai/BlockedMovementLive.hpp"
#include "Battle/Ai/EngagementLive.hpp"
#include "Battle/Ai/Movement.hpp"
#include "Battle/Ai/Navigation.hpp"
#include "Battle/Ai/OrbitingBoss.hpp"
#include "Battle/Ai/Perception.hpp"
#include "Battle/Ai/RoomState.hpp"
#include "Battle/Damage.h"
#include "Core/Diagnostics.hpp"
#include "Field/Gimmicks.h"
#include "Field/Room.h"
#include "Items/ItemRecords.hpp"
#include "Legacy/Psx.h"
#include "Render/Palette.hpp"

namespace {
VS::Battle::Ai::FrameContext frame_context;
}  // namespace

VS::Battle::Ai::FrameContext& VS::Battle::Ai::LiveFrameContext() {
  return frame_context;
}

std::uint8_t* VS::Battle::Ai::FindTerrainRegionCell(int tile_x, int tile_z) {
  const unsigned width = frame_context.roomBounds & 0xFFu;
  const unsigned height = (frame_context.roomBounds >> 8) & 0xFFu;
  if (frame_context.terrainRegions == nullptr || width == 0 ||
      width > kNavigationGridSide || height == 0 ||
      height > kNavigationGridSide || static_cast<unsigned>(tile_x) >= width ||
      static_cast<unsigned>(tile_z) >= height) {
    VS::Core::Errorf("ai", "Invalid AI direction cell: tile=%d,%d grid=%u,%u",
                     tile_x, tile_z, width, height);
    return nullptr;
  }
  const size_t index =
      static_cast<size_t>(tile_z) * width + static_cast<unsigned>(tile_x);
  if (index >= frame_context.terrain_region_count) {
    VS::Core::Errorf("ai",
                     "AI direction cell exceeds source payload: tile=%d,%d "
                     "index=%zu bytes=%zu",
                     tile_x, tile_z, index, frame_context.terrain_region_count);
    return nullptr;
  }
  return frame_context.terrainRegions + index;
}

namespace {
Battle_Ai_ActorAiState* AsActorAiState(void* view) {
  return static_cast<Battle_Ai_ActorAiState*>(view);
}

class LiveActorAiFrame final : public VS::Battle::Ai::ActorAiFrame {
 public:
  [[nodiscard]] bool HasTileFlags() const override {
    return VS::Battle::Ai::tileFlagMap != nullptr;
  }
  [[nodiscard]] bool HasRoomAndActors() const override {
    return VS::Battle::Ai::roomAiState != nullptr &&
           Battle_actors[0] != nullptr;
  }
  void CacheWorkingState() override {
    VS::Battle::Ai::CacheLiveActorAiWorkingState();
  }
  [[nodiscard]] std::uint32_t AttributeActorMask() const override {
    return static_cast<std::uint32_t>(VS::Battle::Ai::attributeActorMask);
  }
  [[nodiscard]] std::uint32_t PerceptionTarget() const override {
    return static_cast<std::uint32_t>(VS::Battle::Ai::secondPerceptionTarget);
  }
  void SetPerceptionTarget(std::uint32_t target) override {
    VS::Battle::Ai::secondPerceptionTarget = static_cast<int>(target);
  }
  void UpdatePerception() override {
    VS::Battle::Ai::UpdateLiveActorPerception();
  }
  void UpdateDecisions() override {
    VS::Battle::Ai::UpdateLiveActorDecisions();
  }
  void AnimateEngagement() override {
    VS::Battle::Ai::AnimateLiveEngagementPhase();
  }
  void SaveRandomState() override {
    VS::Battle::Ai::SaveLiveMovementRandomState();
  }
};
}  // namespace

namespace VS::Battle::Ai {
// decomp: BATTLE.PRG func_800E65DC (src/BATTLE/BATTLE.PRG/7DDDC.s:8).
int CacheLiveActorAiWorkingState() {
  static_assert(std::size(frame_context.actors) == kFrameActorSlots);
  static_assert(std::size(VS::Battle::Ai::actorAiStates) == kFrameActorSlots);
  for (std::size_t slot = 0; slot < kFrameActorSlots; ++slot) {
    frame_context.actors[slot] = VS::Battle::Ai::actorAiStates[slot];
  }
  frame_context.room = VS::Battle::Ai::roomAiState;
  frame_context.heightMap = VS::Battle::Ai::specialTerrainHeights;
  frame_context.tileFlags = VS::Battle::Ai::tileFlagMap;
  frame_context.roomData = VS::Battle::Ai::roomHasWaypoints;
  WorkingStateSource source;
  source.has_room_data = frame_context.roomData != nullptr;
  source.auxiliary_bytes = Field_roomData.header.section6Length;
  if (frame_context.room != nullptr) {
    std::array<std::uint8_t, kRoomExtentBytes> extent{};
    static_assert(sizeof frame_context.room->reserved4 == extent.size());
    memcpy(extent.data(), frame_context.room->reserved4, extent.size());
    source.room_extent = extent;
  }
  const std::optional<WorkingStateWindow> window =
      CacheActorAiWorkingState(source);
  if (!window) {
    VS::Core::Errorf("ai", "AI terrain bitmap header is truncated: bytes=%zu",
                     source.auxiliary_bytes);
    abort();
  }
  frame_context.terrainRegions = nullptr;
  frame_context.terrain_region_count = 0;
  if (window->terrain_regions) {
    frame_context.terrainRegions =
        static_cast<u_char*>(frame_context.roomData) +
        window->terrain_regions->offset;
    frame_context.terrain_region_count = window->terrain_regions->count;
  }
  frame_context.randomState =
      static_cast<u_int>(VS::Battle::Ai::savedMovementRandomState);
  memcpy(frame_context.directions, VS::Battle::Ai::directionDeltas.data(),
         sizeof frame_context.directions);
  frame_context.roomBounds = window->room_bounds;
  return static_cast<int>(frame_context.roomBounds);
}

// decomp: BATTLE.PRG func_800E511C; stages at 0x800F16BC (7C7E0.s:108).
void RunLiveActorAiUpdateStages() {
  LiveActorAiFrame frame;
  if (RunActorAiUpdateStages(frame) ==
      FrameUpdateResult::kMissingRoomOrActors) {
    VS::Core::Errorf("ai",
                     "AI update requires a room and the linked actor list");
    abort();
  }
}

// decomp: BATTLE.PRG func_800E4358 (src/BATTLE/BATTLE.PRG/7B5DC.s:433).
void TickLiveActorDecisionTimers(Battle_Ai_PerceptionUpdateView* view) {
  Battle_Ai_ActorAiState* state = AsActorAiState(view);
  DecisionTimers timers{state->decisionDelay, state->minActionDelay};
  TickActorDecisionTimers(timers);
  state->decisionDelay = timers.decision_delay;
  state->minActionDelay = timers.min_action_delay;
}

// decomp: BATTLE.PRG func_800E6828 (src/BATTLE/BATTLE.PRG/7E028.s:8).
void InitializeLiveActorLocomotionMode(Battle_Ai_AiCreationView* view) {
  Battle_Ai_ActorAiState* state = AsActorAiState(view);
  LocomotionState locomotion{state->locomotionFlags, state->movementFlags,
                             state->locomotionMode};
  InitializeActorLocomotionMode(locomotion);
  state->movementFlags = locomotion.movement_flags;
  state->locomotionMode = locomotion.mode;
}

// decomp: BATTLE.PRG func_800E685C (src/BATTLE/BATTLE.PRG/7E05C.s:16).
void SetLiveActorVerticalMovementSteps(int actor_id, int walk_step,
                                       int run_step) {
  if (static_cast<unsigned>(actor_id) >= kFrameActorSlots) {
    VS::Core::Errorf("ai", "Vertical movement steps for invalid actor %d",
                     actor_id);
    abort();
  }
  Battle_Ai_ActorAiState* state = VS::Battle::Ai::actorAiStates[actor_id];
  if (state == nullptr) return;
  VerticalMovementSteps steps;
  SetActorVerticalMovementSteps(steps, walk_step, run_step);
  state->walkStep = steps.walk_step;
  state->runStep = steps.run_step;
  state->maxRange = steps.max_range;
}

NavigationDirectionCodes LiveDirectionCodes() {
  NavigationDirectionCodes codes{};
  std::memcpy(codes.data(), frame_context.directions, codes.size());
  return codes;
}

bool LiveHasRoomData() { return frame_context.roomData != nullptr; }

std::optional<std::uint8_t> LiveTerrainRegionCell(int x, int z) {
  if (frame_context.terrainRegions == nullptr) return std::nullopt;
  const std::uint8_t* cell = FindTerrainRegionCell(x, z);
  if (cell == nullptr) return std::nullopt;
  return *cell;
}

std::optional<RoomTileBounds> LiveRoomTileBounds() {
  if (VS::Battle::Ai::roomAiState == nullptr) return std::nullopt;
  return RoomTileBounds{.min_x = VS::Battle::Ai::roomAiState->minTileX,
                        .min_z = VS::Battle::Ai::roomAiState->minTileZ,
                        .max_x = VS::Battle::Ai::roomAiState->maxTileX,
                        .max_z = VS::Battle::Ai::roomAiState->maxTileZ};
}
}  // namespace VS::Battle::Ai
