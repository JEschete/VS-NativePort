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

#include "Actors/ActorRecord.hpp"
#include "Actors/Movement.hpp"
#include "Battle/Actor.h"
#include "Battle/Ai/ActorCleanupLive.hpp"
#include "Battle/Ai/ActorCommandsLive.hpp"
#include "Battle/Ai/ActorState.hpp"
#include "Battle/Ai/AiViews.hpp"
#include "Battle/Ai/AreaRouteLive.hpp"
#include "Battle/Ai/BlockedMovementLive.hpp"
#include "Battle/Ai/CurrentTerrain.hpp"
#include "Battle/Ai/EngagementLive.hpp"
#include "Battle/Ai/FrameUpdateLive.hpp"
#include "Battle/Ai/GroundNavigationLive.hpp"
#include "Battle/Ai/LiveRequire.hpp"
#include "Battle/Ai/Movement.hpp"
#include "Battle/Ai/Navigation.hpp"
#include "Battle/Ai/Perception.hpp"
#include "Battle/Ai/SpecialTerrainLive.hpp"
#include "Battle/Damage.hpp"
#include "Core/Diagnostics.hpp"
#include "Core/FrameTiming.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Field/Gimmicks.hpp"
#include "Field/PlatformPlacementLive.hpp"
#include "Field/Room.hpp"
#include "Legacy/Psx.h"

namespace VS::Battle::Ai {
namespace {
constexpr std::size_t kRouteEndOffset = 0x454;
constexpr std::size_t kRouteAuxiliaryBytes = 0x498;
constexpr unsigned kSuspendedActorCommands = 0xC0;
constexpr unsigned kTileMask = 0xFF;
constexpr int kTileZShift = 16;

static_assert(std::size(VS::Battle::Ai::actorAiStates) ==
              kNavigationActorSlots);
static_assert(kFrameActorSlots == kNavigationActorSlots);
static_assert(std::size(FrameContext{}.directions) == kNavigationDirections);
// The route table is 256 words followed, at 0x454, by eight link end pairs.
static_assert(sizeof(VS::Battle::Ai::RouteGateEndRecord) == 2);
static_assert(kRouteWordCount * sizeof(u_int) <= kRouteEndOffset);
static_assert(kRouteEndOffset + (kRouteGateEndCount *
                                 sizeof(VS::Battle::Ai::RouteGateEndRecord)) <=
              kRouteAuxiliaryBytes);
static_assert(sizeof(VS::Battle::Ai::ActorAiState{}.step_height_bytes) ==
              sizeof(std::uint16_t));

class LiveNavigationGrid final : public NavigationGrid {
 public:
  [[nodiscard]] bool HasTileFlags() const override {
    return VS::Battle::Ai::tileFlagMap != nullptr;
  }
  [[nodiscard]] std::uint16_t TileFlags(int x, int z) const override {
    return VS::Battle::Ai::tileFlagMap[z][x];
  }
  void SetTileFlags(int x, int z, std::uint16_t flags) override {
    VS::Battle::Ai::tileFlagMap[z][x] = flags;
  }
  [[nodiscard]] bool HasOccupancyHeights() const override {
    return VS::Battle::Ai::specialTerrainHeights != nullptr;
  }
  [[nodiscard]] std::uint16_t OccupancyHeight(int x, int z) const override {
    return VS::Battle::Ai::specialTerrainHeights[z][x];
  }
  void SetOccupancyHeight(int x, int z, std::uint16_t value) override {
    VS::Battle::Ai::specialTerrainHeights[z][x] = value;
  }
};

class LiveSpecialTerrainBuilder final : public SpecialTerrainBuilder {
 public:
  [[nodiscard]] bool HasNativeMaps() const override {
    return VS::Battle::Ai::tileFlagMap != nullptr &&
           VS::Battle::Ai::specialTerrainHeights != nullptr;
  }
  void BuildSpecialTerrainHeights() override {
    BuildLiveAiSpecialTerrainHeights();
  }
};

TileOccupantPlacement PlacementOf(const VS::Field::GimmickInstance* gimmick) {
  Require(gimmick != nullptr, "Gimmick occupancy requires an instance");
  return {.x = static_cast<std::uint8_t>(gimmick->tile_x),
          .layer = static_cast<std::uint8_t>(gimmick->tile_layer),
          .z = static_cast<std::uint8_t>(gimmick->tile_z)};
}

std::uint32_t LiveVisiblePlatformMask() {
  constexpr std::size_t kOriginalPlatformCapacity = 13;
  if (VS::Field::roomData.moving_platforms == nullptr) return 0;
  const auto bytes = VS::Field::roomData.header.moving_platforms_length;
  Require(bytes % sizeof(VS::Field::MpdMovingPlatform) == 0 &&
              bytes / sizeof(VS::Field::MpdMovingPlatform) <=
                  kOriginalPlatformCapacity,
          "Route availability: moving-platform count exceeds the original "
          "bounded buffer");
  const auto points = VS::Field::CollectLiveMovingPlatformPositions();
  Require(points.size() <= kOriginalPlatformCapacity,
          "Route availability: moving-platform producer returned an invalid "
          "count");
  std::array<std::int16_t, kOriginalPlatformCapacity> indices{};
  for (std::size_t index = 0; index < points.size(); ++index) {
    indices[index] = points[index].platform_index;
  }
  return VisiblePlatformMask(std::span(indices).first(points.size()));
}

std::array<RouteGateEnd, kRouteGateEndCount> LiveRouteGateEnds() {
  return DecodeRouteGateEnds(
      std::span<const RouteGateEndRecord, kRouteGateEndCount>(
          VS::Battle::Ai::routeGates, kRouteGateEndCount));
}

VS::Battle::Ai::ActorAiState& ActorState(int actor_id) {
  auto* state =
      VS::Battle::Ai::actorAiStates[static_cast<std::size_t>(actor_id)];
  Require(state != nullptr, "AI navigation requires the actor's AI state");
  return *state;
}

VS::Battle::Ai::ActorAiState& OverrideRoute(int actor_id) {
  auto* route = static_cast<VS::Battle::Ai::ActorAiState*>(
      ActorState(actor_id).native_pointer_18c);
  Require(route != nullptr, "AI navigation requires the actor's override");
  return *route;
}

class LiveNavigationOverrideCopy final : public NavigationOverrideCopy {
 public:
  explicit LiveNavigationOverrideCopy(VS::Battle::Ai::ActorAiState* destination)
      : destination_(destination) {}

  [[nodiscard]] bool HasSource(int actor_id) const override {
    return VS::Battle::Ai::actorAiStates[static_cast<std::size_t>(actor_id)] !=
           nullptr;
  }
  [[nodiscard]] bool HasDestination() const override {
    return destination_ != nullptr;
  }
  void CopySource(int actor_id) override {
    *destination_ = ActorState(actor_id);
  }
  void ApplySettings(const NavigationOverrideSettings& settings) override {
    destination_->actor_id = settings.actor_id;
    std::memcpy(destination_->step_height_bytes.data(), &settings.step_height,
                sizeof(settings.step_height));
    destination_->reserved_b2 = settings.tile_step_limit;
    destination_->navigation_flags = settings.navigation_flags;
    destination_->opaque_19a = settings.repeat_wait;
    destination_->settings_bit14 = settings.area_settings;
    destination_->movement_enabled = settings.movement_enabled;
  }

 private:
  VS::Battle::Ai::ActorAiState* destination_;
};

class LiveNavigationOverrideHost final : public NavigationOverrideHost {
 public:
  [[nodiscard]] bool HasTileFlags() const override {
    return VS::Battle::Ai::tileFlagMap != nullptr;
  }
  [[nodiscard]] bool HasActorState(int actor_id) const override {
    return VS::Battle::Ai::actorAiStates[static_cast<std::size_t>(actor_id)] !=
           nullptr;
  }
  [[nodiscard]] bool HasOverride(int actor_id) const override {
    return ActorState(actor_id).native_pointer_18c != nullptr;
  }
  [[nodiscard]] std::optional<std::uint32_t> OverrideTrackedTile(
      int actor_id) const override {
    const auto& route = OverrideRoute(actor_id);
    if (route.native_pointer64 == nullptr) return std::nullopt;
    u_int tile;
    std::memcpy(&tile, route.native_pointer64, sizeof(tile));
    return tile;
  }
  [[nodiscard]] std::uint32_t OverrideDestination(int actor_id) const override {
    return OverrideRoute(actor_id).destination.word;
  }
  [[nodiscard]] bool MatchesActorAndRoom(int actor_id) const override {
    const auto& state = ActorState(actor_id);
    return state.actor_id == actor_id && state.actor != nullptr &&
           static_cast<void*>(state.actor) ==
               static_cast<void*>(
                   VS::Battle::actors[static_cast<std::size_t>(actor_id)]) &&
           VS::Battle::Ai::roomAiState != nullptr;
  }
  void ReleaseEngagement(int actor_id) override {
    auto* state = &ActorState(actor_id);
    DisengageLiveActor(state);
    ResetLiveActorAwarenessIndicator(
        reinterpret_cast<VS::Battle::Ai::PlanEvaluationView*>(state));
    UnlinkLiveEngagedActor(state);
  }
  bool AllocateOverride(int actor_id) override {
    auto* storage = static_cast<VS::Battle::Ai::ActorAiState*>(
        VS::Core::Memory::AllocateLiveFromHighEnd(
            sizeof(VS::Battle::Ai::ActorAiState)));
    if (storage == nullptr) return false;
    ActorState(actor_id).native_pointer_18c = std::construct_at(storage);
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
    destination->scripted_target_xz = target;
  }

 private:
  static VS::Battle::Ai::ActorAiState* Destination(
      int actor_id, NavigationOverrideTarget where) {
    auto* state = &ActorState(actor_id);
    auto* destination = state;
    if (where == NavigationOverrideTarget::kNewRoute) {
      destination =
          static_cast<VS::Battle::Ai::ActorAiState*>(state->native_pointer_18c);
    }
    return destination;
  }
};

ScriptedRoute LoadRoute(const VS::Battle::Ai::ActorAiState& state) {
  return {.actor_id = state.actor_id,
          .locomotion_mode = state.locomotion_mode,
          .route_gate = state.route_gate,
          .movement_wait = state.movement_wait,
          .navigation_flags = state.navigation_flags,
          .ignore_floor = state.ignore_floor_mask,
          .gait = state.opaque1,
          .movement_target = state.movement_target,
          .is_stuck = state.is_stuck,
          .height_filter = state.opaque3,
          .repeat_wait = state.opaque_19a,
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
          .area_exception = state.reserved154_area,
          .scripted_target = state.scripted_target_xz,
          .move_x = state.move_x,
          .move_z = state.move_z,
          .destination = state.destination.word};
}

void StoreRouteRecord(const ScriptedRoute& route,
                      VS::Battle::Ai::ActorAiState& state) {
  state.actor_id = route.actor_id;
  state.locomotion_mode = route.locomotion_mode;
  state.route_gate = route.route_gate;
  state.movement_wait = route.movement_wait;
  state.navigation_flags = route.navigation_flags;
  state.ignore_floor_mask = route.ignore_floor;
  state.opaque1 = route.gait;
  state.movement_target = route.movement_target;
  state.is_stuck = route.is_stuck;
  state.opaque3 = route.height_filter;
  state.opaque_19a = route.repeat_wait;
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
  state.reserved154_area = route.area_exception;
  state.scripted_target_xz = route.scripted_target;
  state.move_x = route.move_x;
  state.move_z = route.move_z;
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
    actor_ = started_ ? actor_->next : VS::Battle::actors[0];
    started_ = true;
    if (actor_ == nullptr) return std::nullopt;
    return ScriptedRouteActor{
        .id = actor_->id,
        .suspended = (actor_->commandState & kSuspendedActorCommands) != 0};
  }
  void StampBudgetTick() override {
    LiveFrameContext().lastBudgetTick = VS::Core::ElapsedFrameCounterTicks();
  }
  [[nodiscard]] std::optional<ScriptedRoute> Route(int actor_id) override {
    const auto* base =
        LiveFrameContext().actors[static_cast<std::size_t>(actor_id)];
    route_ = base != nullptr ? static_cast<VS::Battle::Ai::ActorAiState*>(
                                   base->native_pointer_18c)
                             : nullptr;
    if (route_ == nullptr) return std::nullopt;
    return LoadRoute(*route_);
  }
  [[nodiscard]] std::optional<RouteModelPlacement> Placement(
      int actor_id) const override {
    const auto* model =
        VS::Actors::actorRecords[static_cast<std::size_t>(actor_id)];
    if (model == nullptr || LiveFrameContext().tileFlags == nullptr) {
      return std::nullopt;
    }
    const auto& instance = model->instance;
    return RouteModelPlacement{.tile_x = instance.current_tile_x,
                               .tile_layer = instance.current_tile_layer,
                               .tile_z = instance.current_tile_z,
                               .tile_status = instance.reserved5f,
                               .x = instance.position.vx,
                               .y = instance.position.vy,
                               .z = instance.position.vz,
                               .position_pad = instance.position.pad};
  }
  [[nodiscard]] std::uint16_t TileFlags(int x, int z) const override {
    return LiveFrameContext().tileFlags[z][x];
  }
  void StoreRoute(const ScriptedRoute& route) override {
    StoreRouteRecord(route, *route_);
  }
  void ClearDirectionCache() override {
    LiveFrameContext().ClearDirectionTests();
  }
  void MoveTowardDestinationTile(ScriptedRoute& route) override {
    StoreRouteRecord(route, *route_);
    MoveLiveActorTowardDestinationTile(route_, 0);
    route = LoadRoute(*route_);
  }
  void UpdateNavigationAreaMovement(ScriptedRoute& route) override {
    StoreRouteRecord(route, *route_);
    UpdateLiveNavigationAreaMovement(
        reinterpret_cast<VS::Battle::Ai::TerrainMoveView*>(route_));
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
    VS::Battle::Ai::FreeLiveActorNavigationOverride(route_->actor_id);
    route_ = nullptr;
  }
  void SaveMovementRandomState() override { SaveLiveMovementRandomState(); }

 private:
  bool started_ = false;
  const Battle_Actor* actor_ = nullptr;
  VS::Battle::Ai::ActorAiState* route_ = nullptr;
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

void AddLiveAiTileOccupant(const VS::Field::GimmickInstance* gimmick) {
  LiveNavigationGrid grid;
  Require(AddAiTileOccupant(grid, PlacementOf(gimmick)),
          "Gimmick occupancy placement is invalid");
}

void RemoveLiveAiTileOccupant(const VS::Field::GimmickInstance* gimmick) {
  LiveNavigationGrid grid;
  Require(RemoveAiTileOccupant(grid, PlacementOf(gimmick)),
          "Gimmick occupancy placement is invalid");
}

int IsLiveAdjacentTileStepBlocked(unsigned int tile, int direction) {
  NavigationDirectionCodes directions{};
  const auto& frame = LiveFrameContext();
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
  if (VS::Battle::Ai::roomHasWaypoints == nullptr) return;
  auto* auxiliary = static_cast<u_char*>(VS::Battle::Ai::roomHasWaypoints);
  Require(
      auxiliary == VS::Field::roomData.section6 &&
          VS::Field::roomData.header.section6_length >= kRouteAuxiliaryBytes &&
          reinterpret_cast<const u_char*>(VS::Battle::Ai::routeGates) ==
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

void CopyLiveActorNavigationOverrideState(
    int actor_id, VS::Battle::Ai::ActorAiState* destination) {
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
