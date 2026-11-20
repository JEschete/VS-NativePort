// Hand-maintained (tools/port/owned.csv).
// Live binding for the typed frame update (FrameUpdate.cpp).
#include "Battle/Ai/FrameUpdateLive.hpp"

#include <array>
#include <cstdlib>
#include <cstring>
#include <optional>

#include "Actors/ActorAllocation.hpp"
#include "Actors/ActorRecord.hpp"
#include "Actors/Animation.hpp"
#include "Actors/ModelInstance.hpp"
#include "Actors/ShpModel.hpp"
#include "Actors/TextureAnimationRecords.hpp"
#include "Battle/Action.hpp"
#include "Battle/ActionTable.hpp"
#include "Battle/Actor.hpp"
#include "Battle/Ai/ActionExecutionLive.hpp"
#include "Battle/Ai/ActionPlanning.hpp"
#include "Battle/Ai/ActorPerceptionLive.hpp"
#include "Battle/Ai/ActorState.hpp"
#include "Battle/Ai/AiViews.hpp"
#include "Battle/Ai/BlockedMovementLive.hpp"
#include "Battle/Ai/EngagementLive.hpp"
#include "Battle/Ai/Movement.hpp"
#include "Battle/Ai/Navigation.hpp"
#include "Battle/Ai/OrbitingBoss.hpp"
#include "Battle/Ai/Perception.hpp"
#include "Battle/Ai/RoomState.hpp"
#include "Battle/Damage.hpp"
#include "Core/Diagnostics.hpp"
#include "Field/Gimmicks.hpp"
#include "Field/Room.hpp"
#include "Items/ItemRecords.hpp"
#include "Legacy/Psx.hpp"
#include "Render/Palette.hpp"

namespace {
VS::Battle::Ai::FrameContext frameContext;
}  // namespace

VS::Battle::Ai::FrameContext& VS::Battle::Ai::LiveFrameContext() {
  return frameContext;
}

std::uint8_t* VS::Battle::Ai::FindTerrainRegionCell(int tile_x, int tile_z) {
  const unsigned width = frameContext.roomBounds & 0xFFu;
  const unsigned height = (frameContext.roomBounds >> 8) & 0xFFu;
  if (frameContext.terrainRegions == nullptr || width == 0 ||
      width > kNavigationGridSide || height == 0 ||
      height > kNavigationGridSide || static_cast<unsigned>(tile_x) >= width ||
      static_cast<unsigned>(tile_z) >= height) {
    VS::Core::Errorf("ai", "Invalid AI direction cell: tile=%d,%d grid=%u,%u",
                     tile_x, tile_z, width, height);
    return nullptr;
  }
  const size_t index =
      static_cast<size_t>(tile_z) * width + static_cast<unsigned>(tile_x);
  if (index >= frameContext.terrain_region_count) {
    VS::Core::Errorf("ai",
                     "AI direction cell exceeds source payload: tile=%d,%d "
                     "index=%zu bytes=%zu",
                     tile_x, tile_z, index, frameContext.terrain_region_count);
    return nullptr;
  }
  return frameContext.terrainRegions + index;
}

namespace {
VS::Battle::Ai::ActorAiState* AsActorAiState(void* view) {
  return static_cast<VS::Battle::Ai::ActorAiState*>(view);
}

class LiveActorAiFrame final : public VS::Battle::Ai::ActorAiFrame {
 public:
  [[nodiscard]] bool HasTileFlags() const override {
    return VS::Battle::Ai::tileFlagMap != nullptr;
  }
  [[nodiscard]] bool HasRoomAndActors() const override {
    return VS::Battle::Ai::roomAiState != nullptr &&
           VS::Battle::actors[0] != nullptr;
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
  static_assert(std::size(frameContext.actors) == kFrameActorSlots);
  static_assert(std::size(VS::Battle::Ai::actorAiStates) == kFrameActorSlots);
  for (std::size_t slot = 0; slot < kFrameActorSlots; ++slot) {
    frameContext.actors[slot] = VS::Battle::Ai::actorAiStates[slot];
  }
  frameContext.room = VS::Battle::Ai::roomAiState;
  frameContext.heightMap = VS::Battle::Ai::specialTerrainHeights;
  frameContext.tileFlags = VS::Battle::Ai::tileFlagMap;
  frameContext.roomData = VS::Battle::Ai::roomAuxiliaryData;
  WorkingStateSource source;
  source.has_room_data = frameContext.roomData != nullptr;
  source.auxiliary_bytes = VS::Field::roomData.header.section6_length;
  if (frameContext.room != nullptr) {
    std::array<std::uint8_t, kRoomExtentBytes> extent{};
    static_assert(sizeof frameContext.room->collision_extent == extent.size());
    memcpy(extent.data(), frameContext.room->collision_extent.data(), extent.size());
    source.room_extent = extent;
  }
  const std::optional<WorkingStateWindow> window =
      CacheActorAiWorkingState(source);
  if (!window) {
    VS::Core::Errorf("ai", "AI terrain bitmap header is truncated: bytes=%zu",
                     source.auxiliary_bytes);
    abort();
  }
  frameContext.terrainRegions = nullptr;
  frameContext.terrain_region_count = 0;
  if (window->terrain_regions) {
    frameContext.terrainRegions =
        static_cast<std::uint8_t*>(frameContext.roomData) +
        window->terrain_regions->offset;
    frameContext.terrain_region_count = window->terrain_regions->count;
  }
  frameContext.randomState =
      static_cast<std::uint32_t>(VS::Battle::Ai::savedMovementRandomState);
  memcpy(frameContext.directions, VS::Battle::Ai::directionDeltas.data(),
         sizeof frameContext.directions);
  frameContext.roomBounds = window->room_bounds;
  return static_cast<int>(frameContext.roomBounds);
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
void TickLiveActorDecisionTimers(VS::Battle::Ai::PerceptionUpdateView* view) {
  VS::Battle::Ai::ActorAiState* state = AsActorAiState(view);
  DecisionTimers timers{state->decision_delay, state->min_action_delay};
  TickActorDecisionTimers(timers);
  state->decision_delay = timers.decision_delay;
  state->min_action_delay = timers.min_action_delay;
}

// decomp: BATTLE.PRG func_800E6828 (src/BATTLE/BATTLE.PRG/7E028.s:8).
void InitializeLiveActorLocomotionMode(VS::Battle::Ai::AiCreationView* view) {
  VS::Battle::Ai::ActorAiState* state = AsActorAiState(view);
  LocomotionState locomotion{state->locomotion_flags, state->movement_flags,
                             state->locomotion_mode};
  InitializeActorLocomotionMode(locomotion);
  state->movement_flags = locomotion.movement_flags;
  state->locomotion_mode = locomotion.mode;
}

// decomp: BATTLE.PRG func_800E685C (src/BATTLE/BATTLE.PRG/7E05C.s:16).
void SetLiveActorVerticalMovementSteps(int actor_id, int walk_step,
                                       int run_step) {
  if (static_cast<unsigned>(actor_id) >= kFrameActorSlots) {
    VS::Core::Errorf("ai", "Vertical movement steps for invalid actor %d",
                     actor_id);
    abort();
  }
  VS::Battle::Ai::ActorAiState* state = VS::Battle::Ai::actorAiStates[actor_id];
  if (state == nullptr) return;
  VerticalMovementSteps steps;
  SetActorVerticalMovementSteps(steps, walk_step, run_step);
  state->walk_step = steps.walk_step;
  state->run_step = steps.run_step;
  state->max_range = steps.max_range;
}

NavigationDirectionCodes LiveDirectionCodes() {
  NavigationDirectionCodes codes{};
  std::memcpy(codes.data(), frameContext.directions, codes.size());
  return codes;
}

bool LiveHasRoomData() { return frameContext.roomData != nullptr; }

std::optional<std::uint8_t> LiveTerrainRegionCell(int x, int z) {
  if (frameContext.terrainRegions == nullptr) return std::nullopt;
  const std::uint8_t* cell = FindTerrainRegionCell(x, z);
  if (cell == nullptr) return std::nullopt;
  return *cell;
}

std::optional<RoomTileBounds> LiveRoomTileBounds() {
  if (VS::Battle::Ai::roomAiState == nullptr) return std::nullopt;
  return RoomTileBounds{.min_x = VS::Battle::Ai::roomAiState->min_tile_x,
                        .min_z = VS::Battle::Ai::roomAiState->min_tile_z,
                        .max_x = VS::Battle::Ai::roomAiState->max_tile_x,
                        .max_z = VS::Battle::Ai::roomAiState->max_tile_z};
}
}  // namespace VS::Battle::Ai
