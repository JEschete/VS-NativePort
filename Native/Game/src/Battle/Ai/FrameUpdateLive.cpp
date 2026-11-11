// Hand-maintained (tools/port/owned.csv).
// Live binding for the typed frame update (FrameUpdate.cpp).
#include "Battle/Ai/FrameUpdateLive.hpp"

#include <array>
#include <cstdlib>
#include <cstring>
#include <optional>

extern "C" {

#include "Actors/ActorAllocation.h"
#include "Actors/ActorRecord.h"
#include "Actors/Animation.h"
#include "Actors/ModelInstance.h"
#include "Actors/Movement.h"
#include "Actors/Shadow.h"
#include "Actors/Skeleton.h"
#include "Actors/TextureAnimation.h"
#include "Battle/Action.h"
#include "Battle/ActionTable.h"
#include "Battle/Actor.h"
#include "Battle/Ai/ActionPlanning.h"
#include "Battle/Ai/ActorState.h"
#include "Battle/Ai/AiState.h"
#include "Battle/Ai/AiViews.h"
#include "Battle/Ai/Behavior.h"
#include "Battle/Ai/FrameUpdate.h"
#include "Battle/Ai/Movement.h"
#include "Battle/Ai/Navigation.h"
#include "Battle/Ai/OrbitingBoss.h"
#include "Battle/Ai/Perception.h"
#include "Battle/Ai/RoomState.h"
#include "Battle/Damage.h"
#include "Core/Diagnostics.h"
#include "Field/Collision.h"
#include "Field/Gimmicks.h"
#include "Field/Platforms.h"
#include "Field/Room.h"
#include "Items/Inventory.h"
#include "Items/Loadout.h"
#include "Legacy/Psx.h"
#include "Render/Palette.h"

static Battle_Ai_FrameContext frame_context;

Battle_Ai_FrameContext* Battle_Ai_FrameState(void) { return &frame_context; }

int Battle_Ai_TryTerrainRegionCell(int tile_x, int tile_z, u_char** cell) {
  const unsigned width = frame_context.roomBounds & 0xFFu;
  const unsigned height = (frame_context.roomBounds >> 8) & 0xFFu;
  if (cell == nullptr || frame_context.terrainRegions == nullptr ||
      width == 0 || width > kAiFrameGridSide || height == 0 ||
      height > kAiFrameGridSide || (unsigned)tile_x >= width ||
      (unsigned)tile_z >= height) {
    Core_Errorf("ai", "Invalid AI direction cell: tile=%d,%d grid=%u,%u",
                tile_x, tile_z, width, height);
    return 0;
  }
  const size_t index = static_cast<size_t>(tile_z) * width + (unsigned)tile_x;
  if (index >= frame_context.terrain_region_count) {
    Core_Errorf("ai",
                "AI direction cell exceeds source payload: tile=%d,%d "
                "index=%zu bytes=%zu",
                tile_x, tile_z, index, frame_context.terrain_region_count);
    return 0;
  }
  *cell = frame_context.terrainRegions + index;
  return 1;
}

u_char* Battle_Ai_TerrainRegionCell(int tile_x, int tile_z) {
  u_char* cell = nullptr;
  if (!Battle_Ai_TryTerrainRegionCell(tile_x, tile_z, &cell)) abort();
  return cell;
}
}  // extern "C"

namespace {
Battle_Ai_ActorAiState* AsActorAiState(void* view) {
  return static_cast<Battle_Ai_ActorAiState*>(view);
}

class LiveActorAiFrame final : public VS::Battle::Ai::ActorAiFrame {
 public:
  [[nodiscard]] bool HasTileFlags() const override {
    return Battle_Ai_tileFlagMap != nullptr;
  }
  [[nodiscard]] bool HasRoomAndActors() const override {
    return Battle_Ai_roomAiState != nullptr && Battle_actors[0] != nullptr;
  }
  void CacheWorkingState() override {
    VS::Battle::Ai::CacheLiveActorAiWorkingState();
  }
  [[nodiscard]] std::uint32_t AttributeActorMask() const override {
    return static_cast<std::uint32_t>(Battle_Ai_attributeActorMask);
  }
  [[nodiscard]] std::uint32_t PerceptionTarget() const override {
    return static_cast<std::uint32_t>(Battle_Ai_secondPerceptionTarget);
  }
  void SetPerceptionTarget(std::uint32_t target) override {
    Battle_Ai_secondPerceptionTarget = static_cast<int>(target);
  }
  void UpdatePerception() override { Battle_Ai_UpdateActorPerception(); }
  void UpdateDecisions() override { Battle_Ai_UpdateActorDecisions(); }
  void AnimateEngagement() override { Battle_Ai_AnimateEngagementPhase(); }
  void SaveRandomState() override { Battle_Ai_SaveMovementRandomState(); }
};
}  // namespace

namespace VS::Battle::Ai {
// decomp: BATTLE.PRG func_800E65DC (src/BATTLE/BATTLE.PRG/7DDDC.s:8).
int CacheLiveActorAiWorkingState() {
  static_assert(std::size(frame_context.actors) == kFrameActorSlots);
  static_assert(std::size(Battle_Ai_actorAiStates) == kFrameActorSlots);
  for (std::size_t slot = 0; slot < kFrameActorSlots; ++slot) {
    frame_context.actors[slot] = Battle_Ai_actorAiStates[slot];
  }
  frame_context.room = Battle_Ai_roomAiState;
  frame_context.heightMap = Battle_Ai_specialTerrainHeights;
  frame_context.tileFlags = Battle_Ai_tileFlagMap;
  frame_context.roomData = Battle_Ai_roomHasWaypoints;
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
    Core_Errorf("ai", "AI terrain bitmap header is truncated: bytes=%zu",
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
      static_cast<u_int>(Battle_Ai_savedMovementRandomState);
  memcpy(frame_context.directions, Battle_Ai_directionDeltas,
         sizeof frame_context.directions);
  frame_context.roomBounds = window->room_bounds;
  return static_cast<int>(frame_context.roomBounds);
}

// decomp: BATTLE.PRG func_800E511C; stages at 0x800F16BC (7C7E0.s:108).
void RunLiveActorAiUpdateStages() {
  LiveActorAiFrame frame;
  if (RunActorAiUpdateStages(frame) ==
      FrameUpdateResult::kMissingRoomOrActors) {
    Core_Errorf("ai", "AI update requires a room and the linked actor list");
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
    Core_Errorf("ai", "Vertical movement steps for invalid actor %d", actor_id);
    abort();
  }
  Battle_Ai_ActorAiState* state = Battle_Ai_actorAiStates[actor_id];
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
  u_char* cell = nullptr;
  if (frame_context.terrainRegions == nullptr ||
      Battle_Ai_TryTerrainRegionCell(x, z, &cell) == 0) {
    return std::nullopt;
  }
  return *cell;
}

std::optional<RoomTileBounds> LiveRoomTileBounds() {
  if (Battle_Ai_roomAiState == nullptr) return std::nullopt;
  return RoomTileBounds{.min_x = Battle_Ai_roomAiState->minTileX,
                        .min_z = Battle_Ai_roomAiState->minTileZ,
                        .max_x = Battle_Ai_roomAiState->maxTileX,
                        .max_z = Battle_Ai_roomAiState->maxTileZ};
}
}  // namespace VS::Battle::Ai
