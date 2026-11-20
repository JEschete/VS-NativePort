#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>

#include <catch2/catch_test_macros.hpp>

#include "Battle/Actor.hpp"
#include "Battle/Ai/ActionPlanning.hpp"
#include "Battle/Ai/ActorState.hpp"
#include "Battle/Ai/FrameUpdateLive.hpp"
#include "Battle/Ai/Navigation.hpp"
#include "Battle/Ai/OrbitingBoss.hpp"
#include "Battle/Ai/Perception.hpp"
#include "Battle/Ai/PlanningPrimitivesLive.hpp"
#include "Battle/Ai/RoomState.hpp"
#include "Battle/Damage.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Legacy/Psx.hpp"

namespace {
template <typename Pointer>
Pointer InvalidatedBinding() {
  static_assert(std::is_pointer_v<Pointer>);
  return reinterpret_cast<Pointer>(std::uintptr_t{1});
}

struct SessionBindingSnapshot {
  decltype(VS::Battle::actors) battle_actors = VS::Battle::actors;
  decltype(VS::Battle::Ai::actorAiStates) actors =
      VS::Battle::Ai::actorAiStates;
  decltype(VS::Battle::Ai::decisionCallbacks) callbacks =
      VS::Battle::Ai::decisionCallbacks;
  VS::Battle::Ai::FrameContext frame = VS::Battle::Ai::LiveFrameContext();
  decltype(VS::Battle::Ai::roomAiState) room = VS::Battle::Ai::roomAiState;
  decltype(VS::Battle::Ai::tileFlagMap) flags = VS::Battle::Ai::tileFlagMap;
  decltype(VS::Battle::Ai::specialTerrainHeights) heights =
      VS::Battle::Ai::specialTerrainHeights;
  decltype(VS::Battle::Ai::roomAuxiliaryData) auxiliary =
      VS::Battle::Ai::roomAuxiliaryData;
  decltype(VS::Battle::Ai::patrolRoutes) patrol = VS::Battle::Ai::patrolRoutes;
  decltype(VS::Battle::Ai::roomWaypoints) waypoints =
      VS::Battle::Ai::roomWaypoints;
  decltype(VS::Battle::Ai::routeGates) gates = VS::Battle::Ai::routeGates;
  decltype(VS::Battle::Ai::arenaOrbit) orbit = VS::Battle::Ai::arenaOrbit;
  decltype(VS::Battle::Ai::planningStatsSnapshot) stats =
      VS::Battle::Ai::planningStatsSnapshot;
  decltype(VS::Battle::Ai::planningWorkspace) workspace =
      VS::Battle::Ai::planningWorkspace;
  decltype(VS::Battle::Ai::planningPassCandidates) pass =
      VS::Battle::Ai::planningPassCandidates;
  decltype(VS::Battle::Ai::candidateExclusions) exclusions =
      VS::Battle::Ai::candidateExclusions;
  decltype(VS::Battle::Ai::fallbackCandidates) fallback =
      VS::Battle::Ai::fallbackCandidates;
  int mask = VS::Battle::Ai::attributeActorMask;
  int target = VS::Battle::Ai::secondPerceptionTarget;
  int count = VS::Battle::Ai::fallbackCandidateCount;
  int plan_counter = VS::Battle::Ai::fallbackSlotMask;
  int engaged = VS::Battle::Ai::candidateEngagedThisUpdate;

  ~SessionBindingSnapshot() {
    std::copy(battle_actors.begin(), battle_actors.end(),
              VS::Battle::actors.begin());
    std::copy(actors.begin(), actors.end(),
              VS::Battle::Ai::actorAiStates.begin());
    std::copy(callbacks.begin(), callbacks.end(),
              VS::Battle::Ai::decisionCallbacks.begin());
    VS::Battle::Ai::LiveFrameContext() = frame;
    VS::Battle::Ai::roomAiState = room;
    VS::Battle::Ai::tileFlagMap = flags;
    VS::Battle::Ai::specialTerrainHeights = heights;
    VS::Battle::Ai::roomAuxiliaryData = auxiliary;
    VS::Battle::Ai::patrolRoutes = patrol;
    VS::Battle::Ai::roomWaypoints = waypoints;
    VS::Battle::Ai::routeGates = gates;
    VS::Battle::Ai::arenaOrbit = orbit;
    VS::Battle::Ai::planningStatsSnapshot = stats;
    VS::Battle::Ai::planningWorkspace = workspace;
    VS::Battle::Ai::planningPassCandidates = pass;
    VS::Battle::Ai::candidateExclusions = exclusions;
    VS::Battle::Ai::fallbackCandidates = fallback;
    VS::Battle::Ai::attributeActorMask = mask;
    VS::Battle::Ai::secondPerceptionTarget = target;
    VS::Battle::Ai::fallbackCandidateCount = count;
    VS::Battle::Ai::fallbackSlotMask = plan_counter;
    VS::Battle::Ai::candidateEngagedThisUpdate = engaged;
  }
};

void SeedInvalidatedRoomBindings() {
  VS::Battle::Ai::roomAiState =
      InvalidatedBinding<decltype(VS::Battle::Ai::roomAiState)>();
  VS::Battle::Ai::tileFlagMap =
      InvalidatedBinding<decltype(VS::Battle::Ai::tileFlagMap)>();
  VS::Battle::Ai::specialTerrainHeights =
      InvalidatedBinding<decltype(VS::Battle::Ai::specialTerrainHeights)>();
  VS::Battle::Ai::roomAuxiliaryData =
      InvalidatedBinding<decltype(VS::Battle::Ai::roomAuxiliaryData)>();
  VS::Battle::Ai::patrolRoutes =
      InvalidatedBinding<decltype(VS::Battle::Ai::patrolRoutes)>();
  VS::Battle::Ai::roomWaypoints =
      InvalidatedBinding<decltype(VS::Battle::Ai::roomWaypoints)>();
  VS::Battle::Ai::routeGates =
      InvalidatedBinding<decltype(VS::Battle::Ai::routeGates)>();
}

void SeedInvalidatedPlanningBindings() {
  VS::Battle::Ai::arenaOrbit =
      InvalidatedBinding<decltype(VS::Battle::Ai::arenaOrbit)>();
  VS::Battle::Ai::planningStatsSnapshot =
      InvalidatedBinding<decltype(VS::Battle::Ai::planningStatsSnapshot)>();
  VS::Battle::Ai::planningWorkspace =
      InvalidatedBinding<decltype(VS::Battle::Ai::planningWorkspace)>();
  VS::Battle::Ai::planningPassCandidates =
      InvalidatedBinding<decltype(VS::Battle::Ai::planningPassCandidates)>();
  VS::Battle::Ai::candidateExclusions =
      InvalidatedBinding<decltype(VS::Battle::Ai::candidateExclusions)>();
  VS::Battle::Ai::fallbackCandidates =
      InvalidatedBinding<decltype(VS::Battle::Ai::fallbackCandidates)>();
}

void SeedInvalidatedFrameBindings() {
  auto& frame = VS::Battle::Ai::LiveFrameContext();
  std::fill_n(frame.actors, std::size(frame.actors),
              InvalidatedBinding<VS::Battle::Ai::ActorAiState*>());
  frame.room = InvalidatedBinding<decltype(frame.room)>();
  frame.heightMap = InvalidatedBinding<decltype(frame.heightMap)>();
  frame.tileFlags = InvalidatedBinding<decltype(frame.tileFlags)>();
  frame.roomData = InvalidatedBinding<decltype(frame.roomData)>();
  frame.terrainRegions = InvalidatedBinding<decltype(frame.terrainRegions)>();
}

void CheckInvalidatedRoomBindings() {
  CHECK(VS::Battle::Ai::roomAiState == nullptr);
  CHECK(VS::Battle::Ai::tileFlagMap == nullptr);
  CHECK(VS::Battle::Ai::specialTerrainHeights == nullptr);
  CHECK(VS::Battle::Ai::roomAuxiliaryData == nullptr);
  CHECK(VS::Battle::Ai::patrolRoutes == nullptr);
  CHECK(VS::Battle::Ai::roomWaypoints == nullptr);
  CHECK(VS::Battle::Ai::routeGates == nullptr);
}

void CheckInvalidatedPlanningBindings() {
  CHECK(VS::Battle::Ai::arenaOrbit == nullptr);
  CHECK(VS::Battle::Ai::planningStatsSnapshot == nullptr);
  CHECK(VS::Battle::Ai::planningWorkspace == nullptr);
  CHECK(VS::Battle::Ai::planningPassCandidates == nullptr);
  CHECK(VS::Battle::Ai::candidateExclusions == nullptr);
  CHECK(VS::Battle::Ai::fallbackCandidates == nullptr);
}

void CheckInvalidatedMembership() {
  CHECK(
      std::ranges::all_of(VS::Battle::Ai::actorAiStates,
                          [](const auto* state) { return state == nullptr; }));
  CHECK(
      std::ranges::all_of(VS::Battle::Ai::LiveFrameContext().actors,
                          [](const auto* state) { return state == nullptr; }));
  CHECK(VS::Battle::Ai::attributeActorMask == 0);
  CHECK(VS::Battle::Ai::secondPerceptionTarget == 0);
  CHECK(VS::Battle::Ai::fallbackCandidateCount == 0);
  CHECK(VS::Battle::Ai::fallbackSlotMask == 0);
  CHECK(VS::Battle::Ai::candidateEngagedThisUpdate == 0);
}

void CheckInvalidatedFrameBindings() {
  const auto& frame = VS::Battle::Ai::LiveFrameContext();
  CHECK(frame.room == nullptr);
  CHECK(frame.heightMap == nullptr);
  CHECK(frame.tileFlags == nullptr);
  CHECK(frame.roomData == nullptr);
  CHECK(frame.terrainRegions == nullptr);
}
}  // namespace

TEST_CASE(
    "AI session invalidation forgets captured stats without freeing reset "
    "storage",
    "[ai][planning][session-reset][stats-lifecycle-live]") {
  SessionBindingSnapshot saved;
  std::array<std::byte, 65536> arena{};
  const VS::Core::Memory::ScopedLiveGameHeap heap(arena);
  VS::Battle::Actor actor{};
  VS::Battle::ActorStats stats{};
  actor.id = 1;
  actor.stats = &stats;
  std::fill_n(VS::Battle::actors.begin(), saved.battle_actors.size(), nullptr);
  VS::Battle::actors[0] = &actor;
  VS::Battle::Ai::InvalidateLivePlanningStatsSnapshot();
  REQUIRE(VS::Battle::Ai::SnapshotLivePlanningActorStats());
  VS::Core::Memory::InitializeLiveHeapAt(
      arena.data(), static_cast<std::uint32_t>(arena.size()));
  const auto refused = VS::Core::Memory::RefusedLiveHeapRequestCount();
  VS::Battle::Ai::InvalidateSessionBindings();
  CHECK(VS::Battle::Ai::planningStatsSnapshot == nullptr);
  REQUIRE(VS::Battle::Ai::SnapshotLivePlanningActorStats());
  VS::Battle::Ai::RestoreLivePlanningActorStats();
  CHECK(VS::Battle::Ai::planningStatsSnapshot == nullptr);
  CHECK(VS::Core::Memory::RefusedLiveHeapRequestCount() == refused);
}

TEST_CASE("AI session invalidation never touches reclaimed allocation storage",
          "[ai][session-reset]") {
  SessionBindingSnapshot saved;
  SeedInvalidatedRoomBindings();
  SeedInvalidatedPlanningBindings();
  SeedInvalidatedFrameBindings();
  auto* dead = InvalidatedBinding<VS::Battle::Ai::ActorAiState*>();
  std::fill_n(VS::Battle::Ai::actorAiStates.begin(), saved.actors.size(), dead);
  VS::Battle::Ai::attributeActorMask = 0x1D;
  VS::Battle::Ai::secondPerceptionTarget = 4;
  VS::Battle::Ai::fallbackCandidateCount = 3;
  VS::Battle::Ai::fallbackSlotMask = 7;
  VS::Battle::Ai::candidateEngagedThisUpdate = 1;
  VS::Battle::Ai::InvalidateSessionBindings();
  CheckInvalidatedRoomBindings();
  CheckInvalidatedPlanningBindings();
  CheckInvalidatedFrameBindings();
  CheckInvalidatedMembership();
  CHECK(VS::Battle::Ai::decisionCallbacks == saved.callbacks);
  VS::Battle::Ai::InvalidateSessionBindings();
  CheckInvalidatedMembership();
}
