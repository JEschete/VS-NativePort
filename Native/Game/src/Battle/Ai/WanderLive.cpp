// Hand-maintained (tools/port/owned.csv).
// Live binding for the wander (Wander.cpp).
#include "Battle/Ai/WanderLive.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <optional>

#include "Battle/Ai/ActorState.hpp"
#include "Battle/Ai/AiViews.hpp"
#include "Battle/Ai/DecisionPrimitivesLive.hpp"
#include "Battle/Ai/FlightMovementLive.hpp"
#include "Battle/Ai/FrameUpdateLive.hpp"
#include "Battle/Ai/GroundNavigationLive.hpp"
#include "Battle/Ai/LiveRequire.hpp"
#include "Battle/Ai/Movement.hpp"
#include "Battle/Ai/Navigation.hpp"
#include "Battle/Ai/NavigationPrimitivesLive.hpp"
#include "Battle/Ai/RadialMovementLive.hpp"
#include "Core/Diagnostics.hpp"
#include "Legacy/Psx.hpp"

namespace VS::Battle::Ai {
namespace {
using View = VS::Battle::Ai::WanderView;
using AiState = VS::Battle::Ai::ActorAiState;

// The model record's flight flag (its word at +8).
constexpr std::uint32_t kModelFlies = 0x200000;

// The wander view aliases the actor AI state it is handed.
static_assert(offsetof(View, is_stuck) == offsetof(AiState, is_stuck));
static_assert(offsetof(View, opaque3) == offsetof(AiState, limits_climbs));
static_assert(offsetof(View, stayed_on_tile) == offsetof(AiState, opaque8));
static_assert(offsetof(View, tile) == offsetof(AiState, tile));
static_assert(offsetof(View, saved_destination) ==
              offsetof(AiState, saved_destination));
static_assert(offsetof(View, x) == offsetof(AiState, position));
static_assert(offsetof(View, model) == offsetof(AiState, model));
static_assert(offsetof(View, move_distance) ==
              offsetof(AiState, move_distance));
static_assert(offsetof(View, wander_phase) == offsetof(AiState, wander_phase));
static_assert(offsetof(View, wander_timer) == offsetof(AiState, wander_timer));
static_assert(offsetof(View, wander_range_squared) ==
              offsetof(AiState, wander_range_squared));
static_assert(offsetof(View, wander_heading) ==
              offsetof(AiState, wander_heading));
static_assert(offsetof(View, wander_cooldown) ==
              offsetof(AiState, wander_cooldown));
static_assert(offsetof(View, rest_period) == offsetof(AiState, rest_period));
static_assert(offsetof(View, move_period) == offsetof(AiState, move_period));
static_assert(offsetof(View, wander_rate) == offsetof(AiState, wander_rate));
static_assert(offsetof(View, goal) == offsetof(AiState, goal));
static_assert(offsetof(View, behavior_mode) ==
              offsetof(AiState, behavior_mode));
static_assert(offsetof(View, count) == offsetof(AiState, opaque_15d));
static_assert(offsetof(View, movement_flags) ==
              offsetof(AiState, movement_flags));
static_assert(offsetof(View, destination) == offsetof(AiState, destination));

FrameContext& Frame() { return LiveFrameContext(); }

WanderActor Load(const View& view) {
  return {
      .is_stuck = view.is_stuck,
      .height_filter = view.opaque3,
      .stayed_on_tile = view.stayed_on_tile,
      .tile = view.tile.word,
      .saved_destination = view.saved_destination.word,
      .x = view.x,
      .z = view.z,
      .flies = view.model != nullptr && (view.model->flags & kModelFlies) != 0,
      .move_distance = view.move_distance,
      .phase = view.wander_phase,
      .timer = view.wander_timer,
      .range_squared = view.wander_range_squared,
      .heading = view.wander_heading,
      .cooldown = view.wander_cooldown,
      .rest_period = view.rest_period,
      .move_period = view.move_period,
      .rate = view.wander_rate,
      .goal = view.goal.word,
      .behavior_mode = view.behavior_mode,
      .count = view.count,
      .movement_flags = view.movement_flags,
      .destination = view.destination.word};
}

void Store(const WanderActor& actor, View& view) {
  view.is_stuck = actor.is_stuck;
  view.opaque3 = actor.height_filter;
  view.stayed_on_tile = actor.stayed_on_tile;
  view.tile.word = actor.tile;
  view.saved_destination.word = actor.saved_destination;
  view.x = actor.x;
  view.z = actor.z;
  view.move_distance = actor.move_distance;
  view.wander_phase = actor.phase;
  view.wander_timer = actor.timer;
  view.wander_range_squared = actor.range_squared;
  view.wander_heading = actor.heading;
  view.wander_cooldown = actor.cooldown;
  view.rest_period = actor.rest_period;
  view.move_period = actor.move_period;
  view.wander_rate = actor.rate;
  view.goal.word = actor.goal;
  view.behavior_mode = actor.behavior_mode;
  view.count = actor.count;
  view.movement_flags = actor.movement_flags;
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
      VS::Battle::Ai::TilePosition position{};
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
  VS::Battle::Ai::MoveTargetView* MoveTarget() {
    return reinterpret_cast<VS::Battle::Ai::MoveTargetView*>(&view_);
  }
  AiState* State() { return reinterpret_cast<AiState*>(&view_); }

  View& view_;
};
}  // namespace

int ChooseNextLiveWanderTile(VS::Battle::Ai::WanderView* view,
                             unsigned int tile, unsigned int range,
                             int /*unused*/) {
  Require(view != nullptr, "AI wander requires an AI state");
  LiveWanderWorld world(*view);
  auto actor = Load(*view);
  const auto choice = ChooseNextWanderTile(world, actor, tile, range);
  Store(actor, *view);
  return static_cast<int>(
      Required(choice, "AI wander tile choice refused its room"));
}

void UpdateLiveWanderMovement(VS::Battle::Ai::WanderView* view,
                              unsigned int tile, unsigned int range) {
  Require(view != nullptr && view->model != nullptr,
          "AI wander requires an AI state and a model");
  LiveWanderWorld world(*view);
  auto actor = Load(*view);
  const bool updated = UpdateWanderMovement(world, actor, tile, range);
  Store(actor, *view);
  Require(updated, "AI wander refused its room");
}
}  // namespace VS::Battle::Ai
