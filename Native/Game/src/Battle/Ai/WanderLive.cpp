// Hand-maintained (tools/port/owned.csv).
// Live binding for the wander (Wander.cpp).
#include "Battle/Ai/WanderLive.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <optional>

#include "Battle/Ai/DecisionPrimitivesLive.hpp"
#include "Battle/Ai/FlightMovementLive.hpp"
#include "Battle/Ai/FrameUpdateLive.hpp"
#include "Battle/Ai/GroundNavigationLive.hpp"
#include "Battle/Ai/NavigationPrimitivesLive.hpp"
#include "Battle/Ai/RadialMovementLive.hpp"
#include "Legacy/Psx.h"
extern "C" {
#include "Battle/Ai/ActorState.h"
#include "Battle/Ai/AiState.h"
#include "Battle/Ai/AiViews.h"
#include "Battle/Ai/FrameUpdate.h"
#include "Battle/Ai/Movement.h"
#include "Battle/Ai/Navigation.h"
#include "Core/Diagnostics.h"
}

namespace VS::Battle::Ai {
namespace {
using View = Battle_Ai_WanderView;
using AiState = Battle_Ai_ActorAiState;

// The model record's flight flag (its word at +8).
constexpr std::uint32_t kModelFlies = 0x200000;

// The wander view aliases the actor AI state it is handed.
static_assert(offsetof(View, isStuck) == offsetof(AiState, isStuck));
static_assert(offsetof(View, opaque3) == offsetof(AiState, opaque3));
static_assert(offsetof(View, opaque8) == offsetof(AiState, opaque8));
static_assert(offsetof(View, tile) == offsetof(AiState, tile));
static_assert(offsetof(View, savedDestination) ==
              offsetof(AiState, savedDestination));
static_assert(offsetof(View, x) == offsetof(AiState, position));
static_assert(offsetof(View, model) == offsetof(AiState, model));
static_assert(offsetof(View, moveDistance) == offsetof(AiState, moveDistance));
static_assert(offsetof(View, wanderPhase) == offsetof(AiState, wanderPhase));
static_assert(offsetof(View, wanderTimer) == offsetof(AiState, wanderTimer));
static_assert(offsetof(View, wanderRangeSquared) ==
              offsetof(AiState, wanderRangeSquared));
static_assert(offsetof(View, wanderHeading) ==
              offsetof(AiState, wanderHeading));
static_assert(offsetof(View, wanderCooldown) ==
              offsetof(AiState, wanderCooldown));
static_assert(offsetof(View, restPeriod) == offsetof(AiState, restPeriod));
static_assert(offsetof(View, movePeriod) == offsetof(AiState, movePeriod));
static_assert(offsetof(View, wanderRate) == offsetof(AiState, wanderRate));
static_assert(offsetof(View, goal) == offsetof(AiState, goal));
static_assert(offsetof(View, behaviorMode) == offsetof(AiState, behaviorMode));
static_assert(offsetof(View, count) == offsetof(AiState, opaque15D));
static_assert(offsetof(View, movementFlags) ==
              offsetof(AiState, movementFlags));
static_assert(offsetof(View, destination) == offsetof(AiState, destination));
static_assert(kAiDirectionBytes == kNavigationDirections);
static_assert(kAiFrameGridSide == kNavigationGridSide);

void Require(bool valid, const char* message) {
  if (!valid) {
    Core_Errorf("ai", "%s", message);
    std::abort();
  }
}

template <typename T>
T Required(const std::optional<T>& value, const char* message) {
  Require(value.has_value(), message);
  return *value;
}

Battle_Ai_FrameContext& Frame() { return *Battle_Ai_FrameState(); }

WanderActor Load(const View& view) {
  return {.is_stuck = view.isStuck,
          .height_filter = view.opaque3,
          .stayed_on_tile = view.opaque8,
          .tile = view.tile.word,
          .saved_destination = view.savedDestination.word,
          .x = view.x,
          .z = view.z,
          .flies =
              view.model != nullptr && (view.model->opaque8 & kModelFlies) != 0,
          .move_distance = view.moveDistance,
          .phase = view.wanderPhase,
          .timer = view.wanderTimer,
          .range_squared = view.wanderRangeSquared,
          .heading = view.wanderHeading,
          .cooldown = view.wanderCooldown,
          .rest_period = view.restPeriod,
          .move_period = view.movePeriod,
          .rate = view.wanderRate,
          .goal = view.goal.word,
          .behavior_mode = view.behaviorMode,
          .count = view.count,
          .movement_flags = view.movementFlags,
          .destination = view.destination.word};
}

void Store(const WanderActor& actor, View& view) {
  view.isStuck = actor.is_stuck;
  view.opaque3 = actor.height_filter;
  view.opaque8 = actor.stayed_on_tile;
  view.tile.word = actor.tile;
  view.savedDestination.word = actor.saved_destination;
  view.x = actor.x;
  view.z = actor.z;
  view.moveDistance = actor.move_distance;
  view.wanderPhase = actor.phase;
  view.wanderTimer = actor.timer;
  view.wanderRangeSquared = actor.range_squared;
  view.wanderHeading = actor.heading;
  view.wanderCooldown = actor.cooldown;
  view.restPeriod = actor.rest_period;
  view.movePeriod = actor.move_period;
  view.wanderRate = actor.rate;
  view.goal.word = actor.goal;
  view.behaviorMode = actor.behavior_mode;
  view.count = actor.count;
  view.movementFlags = actor.movement_flags;
  view.destination.word = actor.destination;
}

bool InGrid(int x, int z) {
  return x >= 0 && z >= 0 && x < kNavigationGridSide && z < kNavigationGridSide;
}

// The room's AI frame and the actor's movement code. Each call taking the
// actor stores it into the AI state first and reloads it after.
class LiveWanderWorld final : public LiveRoomRegions<WanderWorld> {
 public:
  explicit LiveWanderWorld(View& view) : view_(view) {}

  [[nodiscard]] std::optional<std::uint16_t> TileFlags(int x,
                                                       int z) const override {
    if (Frame().tileFlags == nullptr || !InGrid(x, z)) return std::nullopt;
    return Frame().tileFlags[z][x];
  }
  [[nodiscard]] std::optional<WanderRoomBounds> RoomBounds() const override {
    return LiveRoomTileBounds();
  }
  int NextRandomMasked(int range) override {
    return NextLiveActorRandomMasked(range);
  }
  [[nodiscard]] bool IsAdjacentStepBlocked(std::uint32_t tile,
                                           int direction) override {
    return IsLiveAdjacentTileStepBlocked(tile, direction) != 0;
  }
  [[nodiscard]] bool CanTraverseEdge(WanderActor& actor, std::uint32_t from,
                                     std::uint32_t to, int direction) override {
    bool traversable = false;
    Run(actor, [&] {
      traversable =
          CanTraverseLiveTileEdge(MoveTarget(), from, to, direction) != 0;
    });
    return traversable;
  }
  [[nodiscard]] bool CanFlyInDirection(WanderActor& actor,
                                       int direction) override {
    bool open = false;
    Run(actor,
        [&] { open = CanLiveActorFlyInDirection(State(), direction) != 0; });
    return open;
  }
  [[nodiscard]] bool CanWalkInDirection(WanderActor& actor,
                                        int direction) override {
    bool open = false;
    Run(actor,
        [&] { open = CanLiveActorWalkInDirection(&view_, direction) != 0; });
    return open;
  }
  void ClearDirectionCache() override {
    // The original cleared only the first cache byte, 0x1F8003D0.
    Frame().walk_valid = 0;
  }
  void ProjectClearRadialDestination(WanderActor& actor, std::uint32_t source,
                                     int angle,
                                     std::uint32_t distance) override {
    Run(actor, [&] {
      Battle_Ai_TilePosition position{};
      position.word = source;
      ProjectLiveClearRadialDestination(State(), position, angle, distance);
    });
  }
  void DispatchLocomotion(WanderActor& actor, int kind, int angle) override {
    Run(actor, [&] { DispatchLiveAiLocomotionMode(&view_, kind, angle); });
  }
  void RecoverMovement(WanderActor& actor) override {
    Run(actor, [&] { RecoverLiveMovement(State()); });
  }

 private:
  template <typename Call>
  void Run(WanderActor& actor, Call&& call) {
    Store(actor, view_);
    call();
    actor = Load(view_);
  }
  Battle_Ai_MoveTargetView* MoveTarget() {
    return reinterpret_cast<Battle_Ai_MoveTargetView*>(&view_);
  }
  AiState* State() { return reinterpret_cast<AiState*>(&view_); }

  View& view_;
};
}  // namespace

int ChooseNextLiveWanderTile(Battle_Ai_WanderView* view, unsigned int tile,
                             unsigned int range, int /*unused*/) {
  Require(view != nullptr, "AI wander requires an AI state");
  LiveWanderWorld world(*view);
  auto actor = Load(*view);
  const auto choice = ChooseNextWanderTile(world, actor, tile, range);
  Store(actor, *view);
  return static_cast<int>(
      Required(choice, "AI wander tile choice refused its room"));
}

void UpdateLiveWanderMovement(Battle_Ai_WanderView* view, unsigned int tile,
                              unsigned int range) {
  Require(view != nullptr && view->model != nullptr,
          "AI wander requires an AI state and a model");
  LiveWanderWorld world(*view);
  auto actor = Load(*view);
  const bool updated = UpdateWanderMovement(world, actor, tile, range);
  Store(actor, *view);
  Require(updated, "AI wander refused its room");
}
}  // namespace VS::Battle::Ai
