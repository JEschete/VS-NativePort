// Hand-maintained (tools/port/owned.csv).
// Live binding for the blocked movement (BlockedMovement.cpp).
#include "Battle/Ai/BlockedMovementLive.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iterator>
#include <optional>

#include "Battle/Ai/AiViews.hpp"
#include "Battle/Ai/BlockedMovement.hpp"
#include "Battle/Ai/DecisionPrimitivesLive.hpp"
#include "Battle/Ai/FrameUpdateLive.hpp"
#include "Battle/Ai/GroundNavigationLive.hpp"
#include "Battle/Ai/LiveRequire.hpp"
#include "Battle/Ai/Movement.hpp"
#include "Battle/Ai/OrbitingBoss.hpp"
#include "Battle/Ai/PlanningPrimitivesLive.hpp"
#include "Core/Diagnostics.hpp"
#include "Legacy/Psx.hpp"

using VS::Core::Gte::SVECTOR;

namespace VS::Battle::Ai {
namespace {
using AiState = VS::Battle::Ai::ActorAiState;
using RecoveryView = VS::Battle::Ai::BlockedRecoveryView;

// The recovery view aliases the actor AI state it is handed.
static_assert(offsetof(RecoveryView, is_stuck) == offsetof(AiState, is_stuck));
static_assert(offsetof(RecoveryView, opaque_1c) ==
              offsetof(AiState, opaque_1c));
static_assert(offsetof(RecoveryView, tile) == offsetof(AiState, tile));
static_assert(offsetof(RecoveryView, previous_tile) ==
              offsetof(AiState, previous_tile));
static_assert(offsetof(RecoveryView, x) == offsetof(AiState, position));
static_assert(offsetof(RecoveryView, z) ==
              offsetof(AiState, position) + offsetof(SVECTOR, vz));
static_assert(offsetof(RecoveryView, detour_step) ==
              offsetof(AiState, detour_step));
static_assert(offsetof(RecoveryView, opaque_fb) ==
              offsetof(AiState, opaque_fb));
static_assert(offsetof(RecoveryView, detour_directions) ==
              offsetof(AiState, detour_directions));
static_assert(offsetof(RecoveryView, movement_target) ==
              offsetof(AiState, movement_target));
static_assert(offsetof(RecoveryView, destination) ==
              offsetof(AiState, destination));
static_assert(sizeof(RecoveryView) <= sizeof(AiState));
static_assert(std::size(AiState{}.detour_directions) ==
              std::tuple_size_v<decltype(BlockedMover::detour_directions)>);

VS::Battle::Ai::MoveTargetView* MoveTarget(AiState& state) {
  return reinterpret_cast<VS::Battle::Ai::MoveTargetView*>(&state);
}

BlockedMover Load(const AiState& state) {
  BlockedMover mover{
      .is_stuck = state.is_stuck,
      .holds_stall_tile = state.opaque_1c,
      .tile = state.tile.word,
      .previous_tile = static_cast<std::uint32_t>(state.previous_tile),
      .x = state.position.vx,
      .z = state.position.vz,
      .detour_step = state.detour_step,
      .detour_target = state.opaque_fb,
      .detour_directions = {},
      .movement_target = state.movement_target,
      .destination = state.destination.word};
  std::ranges::copy(state.detour_directions, mover.detour_directions.begin());
  return mover;
}

void Store(const BlockedMover& mover, AiState& state) {
  state.is_stuck = mover.is_stuck;
  state.opaque_1c = mover.holds_stall_tile;
  state.tile.word = mover.tile;
  state.previous_tile = static_cast<int>(mover.previous_tile);
  state.position.vx = mover.x;
  state.position.vz = mover.z;
  state.detour_step = mover.detour_step;
  state.opaque_fb = mover.detour_target;
  std::ranges::copy(mover.detour_directions,
                    std::begin(state.detour_directions));
  state.movement_target = mover.movement_target;
  state.destination.word = mover.destination;
}

// The actor's movement primitives; the state they see is the one the
// routine reads, which they do not change.
class LiveAdjacentDestinationWorld final : public AdjacentDestinationWorld {
 public:
  explicit LiveAdjacentDestinationWorld(VS::Battle::Ai::MoveTargetView& state)
      : state_(state) {}

  [[nodiscard]] NavigationDirectionCodes DirectionCodes() const override {
    return LiveDirectionCodes();
  }
  [[nodiscard]] std::optional<int> CanTraverseTileEdge(std::uint32_t from,
                                                       std::uint32_t to,
                                                       int direction) override {
    return CanTraverseLiveTileEdge(&state_, from, to, direction);
  }
  [[nodiscard]] std::optional<int> DestinationOccupancy(
      std::uint32_t tile) override {
    return LiveDestinationOccupancy(&state_, tile);
  }
  [[nodiscard]] std::optional<std::uint32_t> FindGapLandingTile(
      std::uint32_t tile, int direction, std::uint32_t from) override {
    return FindLiveGapLandingTile(&state_, tile, direction, from);
  }

 private:
  VS::Battle::Ai::MoveTargetView& state_;
};

// The room's AI frame and the actor's movement code. Each call taking the
// mover stores it into the AI state first and reloads it after.
class LiveBlockedMovementWorld final : public BlockedMovementWorld {
 public:
  explicit LiveBlockedMovementWorld(AiState& state) : state_(state) {}

  [[nodiscard]] NavigationDirectionCodes DirectionCodes() const override {
    return LiveDirectionCodes();
  }
  [[nodiscard]] bool HasArenaOrbit() const override {
    return VS::Battle::Ai::arenaOrbit != nullptr;
  }
  [[nodiscard]] int NextRandomMasked(int range) override {
    return NextLiveActorRandomMasked(range);
  }
  [[nodiscard]] std::optional<int> DestinationOccupancy(
      BlockedMover& mover, std::uint32_t tile) override {
    int occupancy = 0;
    Run(mover, [&] {
      occupancy = LiveDestinationOccupancy(MoveTarget(state_), tile);
    });
    return occupancy;
  }
  void ForgetActionTarget(BlockedMover& mover) override {
    Run(mover, [&] { ForgetLiveAiActionTarget(&state_); });
  }
  void DispatchLocomotion(BlockedMover& mover) override {
    Run(mover, [&] { DispatchLiveAiLocomotionMode(&state_, 0, 0); });
  }

 private:
  template <typename Call>
  void Run(BlockedMover& mover, Call&& call) {
    Store(mover, state_);
    call();
    mover = Load(state_);
  }

  AiState& state_;
};
}  // namespace

unsigned int FindLiveAdjacentMovementDestination(
    VS::Battle::Ai::MoveTargetView* state, int first, int second,
    unsigned int direction) {
  Require(state != nullptr, "AI adjacent destination requires an AI state");
  LiveAdjacentDestinationWorld world(*state);
  const auto destination = FindAdjacentMovementDestination(
      world,
      {.movement_disabled = state->movement_disabled != 0,
       .tile = state->tile.word},
      first, second, direction);
  Require(destination.has_value(),
          "AI adjacent destination refused its direction");
  return *destination;
}

int IsLiveMovementHistoryStalled(VS::Battle::Ai::PerceptionUpdateView* state) {
  Require(state != nullptr, "AI movement history requires an AI state");
  const MovementHistory history{
      .x_words = {static_cast<std::uint32_t>(state->history_x.words[0]),
                  static_cast<std::uint32_t>(state->history_x.words[1])},
      .z_words = {static_cast<std::uint32_t>(state->history_z.words[0]),
                  static_cast<std::uint32_t>(state->history_z.words[1])},
      .mask = state->history_mask};
  return IsMovementHistoryStalled(history) ? 1 : 0;
}

void SaveLiveMovementRandomState() {
  MovementRandomStates states{
      .frame = LiveFrameContext().randomState,
      .saved = VS::Battle::Ai::savedMovementRandomState};
  SaveMovementRandomState(states);
  VS::Battle::Ai::savedMovementRandomState = states.saved;
}

int TryRecoverLiveBlockedMovement(VS::Battle::Ai::BlockedRecoveryView* state) {
  Require(state != nullptr, "AI blocked movement requires an AI state");
  auto& ai_state = *reinterpret_cast<AiState*>(state);
  LiveBlockedMovementWorld world(ai_state);
  auto mover = Load(ai_state);
  const auto recovered = TryRecoverBlockedMovement(world, mover);
  Store(mover, ai_state);
  Require(recovered.has_value(),
          "AI blocked movement refused its detour direction");
  return *recovered ? 1 : 0;
}
}  // namespace VS::Battle::Ai
