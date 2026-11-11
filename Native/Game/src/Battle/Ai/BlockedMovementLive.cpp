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

#include "Battle/Ai/BlockedMovement.hpp"
#include "Battle/Ai/DecisionPrimitivesLive.hpp"
#include "Battle/Ai/FrameUpdateLive.hpp"
#include "Battle/Ai/GroundNavigationLive.hpp"
#include "Battle/Ai/PlanningPrimitivesLive.hpp"
#include "Legacy/Psx.h"
extern "C" {
#include "Battle/Ai/AiState.h"
#include "Battle/Ai/FrameUpdate.h"
#include "Battle/Ai/Movement.h"
#include "Battle/Ai/OrbitingBoss.h"
#include "Core/Diagnostics.h"
}

namespace VS::Battle::Ai {
namespace {
using AiState = Battle_Ai_ActorAiState;
using RecoveryView = Battle_Ai_BlockedRecoveryView;

// The recovery view aliases the actor AI state it is handed.
static_assert(offsetof(RecoveryView, isStuck) == offsetof(AiState, isStuck));
static_assert(offsetof(RecoveryView, opaque1C) == offsetof(AiState, opaque1C));
static_assert(offsetof(RecoveryView, tile) == offsetof(AiState, tile));
static_assert(offsetof(RecoveryView, previousTile) ==
              offsetof(AiState, previousTile));
static_assert(offsetof(RecoveryView, x) == offsetof(AiState, position));
static_assert(offsetof(RecoveryView, z) ==
              offsetof(AiState, position) + offsetof(SVECTOR, vz));
static_assert(offsetof(RecoveryView, detourStep) ==
              offsetof(AiState, detourStep));
static_assert(offsetof(RecoveryView, opaqueFB) == offsetof(AiState, opaqueFB));
static_assert(offsetof(RecoveryView, detourDirections) ==
              offsetof(AiState, detourDirections));
static_assert(offsetof(RecoveryView, movementTarget) ==
              offsetof(AiState, movementTarget));
static_assert(offsetof(RecoveryView, destination) ==
              offsetof(AiState, destination));
static_assert(sizeof(RecoveryView) <= sizeof(AiState));
static_assert(std::size(AiState{}.detourDirections) ==
              std::tuple_size_v<decltype(BlockedMover::detour_directions)>);

void Require(bool valid, const char* message) {
  if (!valid) {
    Core_Errorf("ai", "%s", message);
    std::abort();
  }
}

Battle_Ai_MoveTargetView* MoveTarget(AiState& state) {
  return reinterpret_cast<Battle_Ai_MoveTargetView*>(&state);
}

BlockedMover Load(const AiState& state) {
  BlockedMover mover{
      .is_stuck = state.isStuck,
      .holds_stall_tile = state.opaque1C,
      .tile = state.tile.word,
      .previous_tile = static_cast<std::uint32_t>(state.previousTile),
      .x = state.position.vx,
      .z = state.position.vz,
      .detour_step = state.detourStep,
      .detour_target = state.opaqueFB,
      .detour_directions = {},
      .movement_target = state.movementTarget,
      .destination = state.destination.word};
  std::ranges::copy(state.detourDirections, mover.detour_directions.begin());
  return mover;
}

void Store(const BlockedMover& mover, AiState& state) {
  state.isStuck = mover.is_stuck;
  state.opaque1C = mover.holds_stall_tile;
  state.tile.word = mover.tile;
  state.previousTile = static_cast<int>(mover.previous_tile);
  state.position.vx = mover.x;
  state.position.vz = mover.z;
  state.detourStep = mover.detour_step;
  state.opaqueFB = mover.detour_target;
  std::ranges::copy(mover.detour_directions,
                    std::begin(state.detourDirections));
  state.movementTarget = mover.movement_target;
  state.destination.word = mover.destination;
}

// The actor's movement primitives; the state they see is the one the
// routine reads, which they do not change.
class LiveAdjacentDestinationWorld final : public AdjacentDestinationWorld {
 public:
  explicit LiveAdjacentDestinationWorld(Battle_Ai_MoveTargetView& state)
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
  Battle_Ai_MoveTargetView& state_;
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
    return Battle_Ai_arenaOrbit != nullptr;
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
    Battle_Ai_MoveTargetView* state, int first, int second,
    unsigned int direction) {
  Require(state != nullptr, "AI adjacent destination requires an AI state");
  LiveAdjacentDestinationWorld world(*state);
  const auto destination = FindAdjacentMovementDestination(
      world,
      {.movement_disabled = state->movementDisabled != 0,
       .tile = state->tile.word},
      first, second, direction);
  Require(destination.has_value(),
          "AI adjacent destination refused its direction");
  return *destination;
}

int IsLiveMovementHistoryStalled(Battle_Ai_PerceptionUpdateView* state) {
  Require(state != nullptr, "AI movement history requires an AI state");
  const MovementHistory history{
      .x_words = {static_cast<std::uint32_t>(state->historyX.words[0]),
                  static_cast<std::uint32_t>(state->historyX.words[1])},
      .z_words = {static_cast<std::uint32_t>(state->historyZ.words[0]),
                  static_cast<std::uint32_t>(state->historyZ.words[1])},
      .mask = state->historyMask};
  return IsMovementHistoryStalled(history) ? 1 : 0;
}

void SaveLiveMovementRandomState() {
  MovementRandomStates states{.frame = Battle_Ai_FrameState()->randomState,
                              .saved = Battle_Ai_savedMovementRandomState};
  SaveMovementRandomState(states);
  Battle_Ai_savedMovementRandomState = states.saved;
}

int TryRecoverLiveBlockedMovement(Battle_Ai_BlockedRecoveryView* state) {
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
