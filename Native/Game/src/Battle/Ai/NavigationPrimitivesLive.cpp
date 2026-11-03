// Hand-maintained (tools/port/owned.csv).
// Live binding for the navigation primitives (NavigationPrimitives.cpp).
#include "Battle/Ai/NavigationPrimitivesLive.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <memory>
#include <optional>
#include <span>

#include "Battle/Ai/ActorCleanupLive.hpp"
#include "Battle/Ai/ActorCommandsLive.hpp"
#include "Battle/Ai/CurrentTerrain.hpp"
#include "Battle/Ai/EngagementLive.hpp"
#include "Battle/Ai/FrameUpdateLive.hpp"
#include "Battle/Ai/GroundNavigationLive.hpp"
#include "Core/FrameTiming.hpp"
#include "Legacy/Psx.h"
extern "C" {
#include "Actors/ActorRecord.h"
#include "Actors/ActorState.h"
#include "Battle/Ai/ActorState.h"
#include "Battle/Ai/FrameUpdate.h"
#include "Battle/Ai/Movement.h"
#include "Battle/Ai/Navigation.h"
#include "Battle/Ai/Perception.h"
#include "Battle/Damage.h"
#include "Core/Diagnostics.h"
#include "Core/Memory/Heap.h"
#include "Field/Gimmicks.h"
#include "Field/Platforms.h"
#include "Field/Room.h"
}

namespace VS::Battle::Ai {
namespace {
constexpr std::size_t kRouteEndOffset = 0x454;
constexpr std::size_t kRouteAuxiliaryBytes = 0x498;
constexpr unsigned kSuspendedActorCommands = 0xC0;
constexpr unsigned kTileMask = 0xFF;
constexpr int kTileZShift = 16;

static_assert(std::size(Battle_Ai_actorAiStates) == kNavigationActorSlots);
static_assert(kAiFrameActorSlots == kNavigationActorSlots);
static_assert(kAiFrameGridSide == kNavigationGridSide);
static_assert(kAiDirectionBytes == kNavigationDirections);
static_assert(std::size(Battle_Ai_FrameContext{}.directions) ==
              kNavigationDirections);
// The route table is 256 words followed, at 0x454, by eight link end pairs.
static_assert(sizeof(Battle_Ai_RouteGateEnd) == 2);
static_assert(kRouteWordCount * sizeof(u_int) <= kRouteEndOffset);
static_assert(kRouteEndOffset +
                  (kRouteGateEndCount * sizeof(Battle_Ai_RouteGateEnd)) <=
              kRouteAuxiliaryBytes);
static_assert(sizeof(Battle_Ai_ActorAiState{}.step_height_bytes) ==
              sizeof(std::uint16_t));

void Require(bool valid, const char* message) {
  if (!valid) {
    Core_Errorf("ai", "%s", message);
    std::abort();
  }
}

class LiveNavigationGrid final : public NavigationGrid {
 public:
  [[nodiscard]] bool HasTileFlags() const override {
    return Battle_Ai_tileFlagMap != nullptr;
  }
  [[nodiscard]] std::uint16_t TileFlags(int x, int z) const override {
    return Battle_Ai_tileFlagMap[z][x];
  }
  void SetTileFlags(int x, int z, std::uint16_t flags) override {
    Battle_Ai_tileFlagMap[z][x] = flags;
  }
  [[nodiscard]] bool HasOccupancyHeights() const override {
    return Battle_Ai_specialTerrainHeights != nullptr;
  }
  [[nodiscard]] std::uint16_t OccupancyHeight(int x, int z) const override {
    return Battle_Ai_specialTerrainHeights[z][x];
  }
  void SetOccupancyHeight(int x, int z, std::uint16_t value) override {
    Battle_Ai_specialTerrainHeights[z][x] = value;
  }
};

class LiveSpecialTerrainBuilder final : public SpecialTerrainBuilder {
 public:
  [[nodiscard]] bool HasNativeMaps() const override {
    return Battle_Ai_tileFlagMap != nullptr &&
           Battle_Ai_specialTerrainHeights != nullptr;
  }
  void BuildSpecialTerrainHeights() override {
    Battle_Ai_BuildAiSpecialTerrainHeights();
  }
};

TileOccupantPlacement PlacementOf(const Field_GimmickInstance* gimmick) {
  Require(gimmick != nullptr, "Gimmick occupancy requires an instance");
  return {.x = static_cast<std::uint8_t>(gimmick->tileX),
          .layer = static_cast<std::uint8_t>(gimmick->tileLayer),
          .z = static_cast<std::uint8_t>(gimmick->tileZ)};
}

std::uint32_t LiveVisiblePlatformMask() {
  constexpr std::size_t kOriginalPlatformCapacity = 13;
  if (Field_roomData.movingPlatforms == nullptr) return 0;
  const auto bytes = Field_roomData.header.movingPlatformsLength;
  Require(
      bytes % sizeof(Field_MpdMovingPlatform) == 0 &&
          bytes / sizeof(Field_MpdMovingPlatform) <= kOriginalPlatformCapacity,
      "Route availability: moving-platform count exceeds the original "
      "bounded buffer");
  std::array<Field_PlatformContactPoint, kOriginalPlatformCapacity> points{};
  const int count = Field_CollectMovingPlatformPositions(points.data());
  Require(count >= 0 && static_cast<std::size_t>(count) <= points.size(),
          "Route availability: moving-platform producer returned an invalid "
          "count");
  std::array<std::int16_t, kOriginalPlatformCapacity> indices{};
  for (std::size_t index = 0; index < static_cast<std::size_t>(count);
       ++index) {
    indices[index] = points[index].platformIndex;
  }
  return VisiblePlatformMask(
      std::span(indices).first(static_cast<std::size_t>(count)));
}

std::array<RouteGateEnd, kRouteGateEndCount> LiveRouteGateEnds() {
  std::array<RouteGateEnd, kRouteGateEndCount> ends{};
  for (std::size_t index = 0; index < ends.size(); ++index) {
    ends[index] = {
        .x = static_cast<std::uint8_t>(Battle_Ai_routeGates[index].tileX),
        .z = static_cast<std::uint8_t>(Battle_Ai_routeGates[index].tileZ)};
  }
  return ends;
}

Battle_Ai_ActorAiState& ActorState(int actor_id) {
  auto* state = Battle_Ai_actorAiStates[static_cast<std::size_t>(actor_id)];
  Require(state != nullptr, "AI navigation requires the actor's AI state");
  return *state;
}

Battle_Ai_ActorAiState& OverrideRoute(int actor_id) {
  auto* route = static_cast<Battle_Ai_ActorAiState*>(
      ActorState(actor_id).nativePointer18C);
  Require(route != nullptr, "AI navigation requires the actor's override");
  return *route;
}

class LiveNavigationOverrideCopy final : public NavigationOverrideCopy {
 public:
  explicit LiveNavigationOverrideCopy(Battle_Ai_ActorAiState* destination)
      : destination_(destination) {}

  [[nodiscard]] bool HasSource(int actor_id) const override {
    return Battle_Ai_actorAiStates[static_cast<std::size_t>(actor_id)] !=
           nullptr;
  }
  [[nodiscard]] bool HasDestination() const override {
    return destination_ != nullptr;
  }
  void CopySource(int actor_id) override {
    *destination_ = ActorState(actor_id);
  }
  void ApplySettings(const NavigationOverrideSettings& settings) override {
    destination_->actorId = settings.actor_id;
    std::memcpy(destination_->step_height_bytes, &settings.step_height,
                sizeof(settings.step_height));
    destination_->reservedB2 = settings.tile_step_limit;
    destination_->navigationFlags = settings.navigation_flags;
    destination_->opaque19A = settings.repeat_wait;
    destination_->settingsBit14 = settings.area_settings;
    destination_->movementEnabled = settings.movement_enabled;
  }

 private:
  Battle_Ai_ActorAiState* destination_;
};

class LiveNavigationOverrideHost final : public NavigationOverrideHost {
 public:
  [[nodiscard]] bool HasTileFlags() const override {
    return Battle_Ai_tileFlagMap != nullptr;
  }
  [[nodiscard]] bool HasActorState(int actor_id) const override {
    return Battle_Ai_actorAiStates[static_cast<std::size_t>(actor_id)] !=
           nullptr;
  }
  [[nodiscard]] bool HasOverride(int actor_id) const override {
    return ActorState(actor_id).nativePointer18C != nullptr;
  }
  [[nodiscard]] std::optional<std::uint32_t> OverrideTrackedTile(
      int actor_id) const override {
    const auto& route = OverrideRoute(actor_id);
    if (route.nativePointer64 == nullptr) return std::nullopt;
    u_int tile;
    std::memcpy(&tile, route.nativePointer64, sizeof(tile));
    return tile;
  }
  [[nodiscard]] std::uint32_t OverrideDestination(int actor_id) const override {
    return OverrideRoute(actor_id).destination.word;
  }
  [[nodiscard]] bool MatchesActorAndRoom(int actor_id) const override {
    const auto& state = ActorState(actor_id);
    return state.actorId == actor_id && state.actor != nullptr &&
           static_cast<void*>(state.actor) ==
               static_cast<void*>(
                   Battle_actors[static_cast<std::size_t>(actor_id)]) &&
           Battle_Ai_roomAiState != nullptr;
  }
  void ReleaseEngagement(int actor_id) override {
    auto* state = &ActorState(actor_id);
    DisengageLiveActor(state);
    ResetLiveActorAwarenessIndicator(
        reinterpret_cast<Battle_Ai_PlanEvaluationView*>(state));
    Battle_Ai_UnlinkEngagedActor(state);
  }
  bool AllocateOverride(int actor_id) override {
    auto* storage = static_cast<Battle_Ai_ActorAiState*>(
        Core_Memory_AllocateFromHighEnd(sizeof(Battle_Ai_ActorAiState)));
    if (storage == nullptr) return false;
    ActorState(actor_id).nativePointer18C = std::construct_at(storage);
    return true;
  }
  bool CopyOverrideState(int actor_id,
                         NavigationOverrideTarget where) override {
    LiveNavigationOverrideCopy copy(Destination(actor_id, where));
    return CopyActorNavigationOverrideState(copy, actor_id);
  }
  void SetScriptedTarget(int actor_id, NavigationOverrideTarget where,
                         std::uint32_t target) override {
    auto* destination = Destination(actor_id, where);
    destination->scriptedTargetXZ = target;
  }

 private:
  static Battle_Ai_ActorAiState* Destination(int actor_id,
                                             NavigationOverrideTarget where) {
    auto* state = &ActorState(actor_id);
    auto* destination = state;
    if (where == NavigationOverrideTarget::kNewRoute) {
      destination =
          static_cast<Battle_Ai_ActorAiState*>(state->nativePointer18C);
    }
    return destination;
  }
};

ScriptedRoute LoadRoute(const Battle_Ai_ActorAiState& state) {
  return {.actor_id = state.actorId,
          .locomotion_mode = state.locomotionMode,
          .route_gate = state.route_gate,
          .movement_wait = state.movement_wait,
          .navigation_flags = state.navigationFlags,
          .ignore_floor = state.ignore_floor_mask,
          .gait = state.opaque1,
          .movement_target = state.movementTarget,
          .is_stuck = state.isStuck,
          .height_filter = state.opaque3,
          .repeat_wait = state.opaque19A,
          .tile = state.tile.word,
          .x = state.position.vx,
          .y = state.position.vy,
          .z = state.position.vz,
          .position_pad = state.position.pad,
          .fine_x = state.fine_x,
          .fine_layer = state.fine_layer,
          .fine_z = state.fine_z,
          .fine_status = state.fine_status,
          .ground_area = state.ground_area,
          .area_exception = state.reserved154Area,
          .scripted_target = state.scriptedTargetXZ,
          .move_x = state.moveX,
          .move_z = state.moveZ,
          .destination = state.destination.word};
}

void StoreRouteRecord(const ScriptedRoute& route,
                      Battle_Ai_ActorAiState& state) {
  state.actorId = route.actor_id;
  state.locomotionMode = route.locomotion_mode;
  state.route_gate = route.route_gate;
  state.movement_wait = route.movement_wait;
  state.navigationFlags = route.navigation_flags;
  state.ignore_floor_mask = route.ignore_floor;
  state.opaque1 = route.gait;
  state.movementTarget = route.movement_target;
  state.isStuck = route.is_stuck;
  state.opaque3 = route.height_filter;
  state.opaque19A = route.repeat_wait;
  state.tile.word = route.tile;
  state.position.vx = route.x;
  state.position.vy = route.y;
  state.position.vz = route.z;
  state.position.pad = route.position_pad;
  state.fine_x = route.fine_x;
  state.fine_layer = route.fine_layer;
  state.fine_z = route.fine_z;
  state.fine_status = route.fine_status;
  state.ground_area = route.ground_area;
  state.reserved154Area = route.area_exception;
  state.scriptedTargetXZ = route.scripted_target;
  state.moveX = route.move_x;
  state.moveZ = route.move_z;
  state.destination.word = route.destination;
}

class LiveScriptedRouteHost final : public ScriptedRouteHost {
 public:
  void BeginUpdate() override {
    CacheLiveActorAiWorkingState();
    started_ = false;
    actor_ = nullptr;
  }
  [[nodiscard]] std::optional<ScriptedRouteActor> NextActor() override {
    actor_ = started_ ? actor_->next : Battle_actors[0];
    started_ = true;
    if (actor_ == nullptr) return std::nullopt;
    return ScriptedRouteActor{
        .id = actor_->id,
        .suspended = (actor_->commandState & kSuspendedActorCommands) != 0};
  }
  void StampBudgetTick() override {
    Battle_Ai_FrameState()->lastBudgetTick =
        VS::Core::ElapsedFrameCounterTicks();
  }
  [[nodiscard]] std::optional<ScriptedRoute> Route(int actor_id) override {
    const auto* base =
        Battle_Ai_FrameState()->actors[static_cast<std::size_t>(actor_id)];
    route_ = base != nullptr
                 ? static_cast<Battle_Ai_ActorAiState*>(base->nativePointer18C)
                 : nullptr;
    if (route_ == nullptr) return std::nullopt;
    return LoadRoute(*route_);
  }
  [[nodiscard]] std::optional<RouteModelPlacement> Placement(
      int actor_id) const override {
    const auto* model = Actors_actorRecords[static_cast<std::size_t>(actor_id)];
    if (model == nullptr || Battle_Ai_FrameState()->tileFlags == nullptr) {
      return std::nullopt;
    }
    const auto& instance = model->instance;
    return RouteModelPlacement{.tile_x = instance.currentTileX,
                               .tile_layer = instance.currentTileLayer,
                               .tile_z = instance.currentTileZ,
                               .tile_status = instance.reserved5F,
                               .x = instance.position.vx,
                               .y = instance.position.vy,
                               .z = instance.position.vz,
                               .position_pad = instance.position.pad};
  }
  [[nodiscard]] std::uint16_t TileFlags(int x, int z) const override {
    return Battle_Ai_FrameState()->tileFlags[z][x];
  }
  void StoreRoute(const ScriptedRoute& route) override {
    StoreRouteRecord(route, *route_);
  }
  void ClearDirectionCache() override {
    Battle_Ai_FrameState()->directionCache[0] = 0;
  }
  void MoveTowardDestinationTile(ScriptedRoute& route) override {
    StoreRouteRecord(route, *route_);
    MoveLiveActorTowardDestinationTile(route_, 0);
    route = LoadRoute(*route_);
  }
  void UpdateNavigationAreaMovement(ScriptedRoute& route) override {
    StoreRouteRecord(route, *route_);
    Battle_Ai_UpdateNavigationAreaMovement(
        reinterpret_cast<Battle_Ai_TerrainMoveView*>(route_));
    route = LoadRoute(*route_);
  }
  void DispatchTileMovement(ScriptedRoute& route) override {
    StoreRouteRecord(route, *route_);
    DispatchLiveAiLocomotionMode(route_, 0, 0);
    route = LoadRoute(*route_);
  }
  void FinishRoute(ScriptedRoute& route) override {
    StoreRouteRecord(route, *route_);
    IssueLiveActorCommand(route_, 0);
    VS::Battle::Ai::FreeLiveActorNavigationOverride(route_->actorId);
    route_ = nullptr;
  }
  void SaveMovementRandomState() override {
    Battle_Ai_SaveMovementRandomState();
  }

 private:
  bool started_ = false;
  const Battle_Actor* actor_ = nullptr;
  Battle_Ai_ActorAiState* route_ = nullptr;
};
}  // namespace

void ReserveLiveAiDestinationTile(int x, int z) {
  LiveNavigationGrid grid;
  Require(ReserveAiDestinationTile(grid, x, z),
          "AI tile reservation is outside the grid");
}

void MarkLiveAiTileDynamicObstacle(int x, int z) {
  LiveNavigationGrid grid;
  Require(MarkAiTileDynamicObstacle(grid, x, z),
          "AI dynamic obstacle is outside the grid");
}

void AddLiveAiTileOccupant(const Field_GimmickInstance* gimmick) {
  LiveNavigationGrid grid;
  Require(AddAiTileOccupant(grid, PlacementOf(gimmick)),
          "Gimmick occupancy placement is invalid");
}

void RemoveLiveAiTileOccupant(const Field_GimmickInstance* gimmick) {
  LiveNavigationGrid grid;
  Require(RemoveAiTileOccupant(grid, PlacementOf(gimmick)),
          "Gimmick occupancy placement is invalid");
}

int IsLiveAdjacentTileStepBlocked(unsigned int tile, int direction) {
  NavigationDirectionCodes directions{};
  const auto& frame = *Battle_Ai_FrameState();
  std::copy(std::begin(frame.directions), std::end(frame.directions),
            directions.begin());
  const auto blocked = IsAdjacentTileStepBlocked(
      CurrentTerrain(), directions, static_cast<int>(tile & kTileMask),
      static_cast<int>((tile >> kTileZShift) & kTileMask), direction);
  Require(blocked.has_value(), "AI neighbor step query is invalid");
  return *blocked ? 1 : 0;
}

void BuildLiveAiTerrainHeightsWithWorkspace() {
  LiveSpecialTerrainBuilder builder;
  Require(BuildAiTerrainHeightsWithWorkspace(builder),
          "Special terrain build requires active native maps");
}

void UpdateLiveRouteGateAvailability() {
  const auto active = LiveVisiblePlatformMask();
  if (Battle_Ai_roomHasWaypoints == nullptr) return;
  auto* auxiliary = static_cast<u_char*>(Battle_Ai_roomHasWaypoints);
  Require(auxiliary == Field_roomData.section6 &&
              Field_roomData.header.section6Length >= kRouteAuxiliaryBytes &&
              reinterpret_cast<const u_char*>(Battle_Ai_routeGates) ==
                  auxiliary + kRouteEndOffset,
          "Route availability: route table does not belong to the active "
          "room auxiliary section");
  std::array<std::uint32_t, kRouteWordCount> routes{};
  for (std::size_t index = 0; index < kRouteWordCount; ++index) {
    u_int word;
    std::memcpy(&word, auxiliary + (index * sizeof(u_int)), sizeof word);
    routes[index] = word;
  }
  Require(UpdateRouteGateAvailability(routes, LiveRouteGateEnds(), active),
          "Route availability: first route word depends on an original "
          "stack-register value");
  for (std::size_t index = 0; index < kRouteWordCount; ++index) {
    auto* location = auxiliary + (index * sizeof(u_int));
    const u_int word = routes[index];
    std::memcpy(location, &word, sizeof word);
  }
}

int HasLiveActorNavigationState(int actor_id) {
  const LiveNavigationOverrideHost host;
  const auto present = HasActorNavigationState(host, actor_id);
  Require(present.has_value(), "AI navigation actor ID is out of bounds");
  return *present ? 1 : 0;
}

int LiveActorNavigationOverrideStatus(int actor_id) {
  const LiveNavigationOverrideHost host;
  const auto status = ActorNavigationOverrideStatus(host, actor_id);
  Require(status.has_value(), "AI navigation override status is invalid");
  return *status;
}

void CopyLiveActorNavigationOverrideState(int actor_id,
                                          Battle_Ai_ActorAiState* destination) {
  LiveNavigationOverrideCopy copy(destination);
  Require(CopyActorNavigationOverrideState(copy, actor_id),
          "AI navigation copy requires a valid actor and both states");
}

void SetLiveActorNavigationOverride(int actor_id, int x, int z) {
  LiveNavigationOverrideHost host;
  Require(SetActorNavigationOverride(host, actor_id, x, z),
          "Scripted navigation request is invalid");
}

void UpdateLiveActorNavigationOverrides() {
  LiveScriptedRouteHost host;
  Require(UpdateActorNavigationOverrides(host),
          "Scripted navigation update is invalid");
}
}  // namespace VS::Battle::Ai
