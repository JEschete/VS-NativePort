#include <algorithm>
#include <array>
#include <cstdint>
#include <type_traits>

#include <catch2/catch_test_macros.hpp>

#include "Legacy/Psx.h"

extern "C" {
#include "Battle/Ai/ActionPlanning.h"
#include "Battle/Ai/ActorState.h"
#include "Battle/Ai/FrameUpdate.h"
#include "Battle/Ai/Navigation.h"
#include "Battle/Ai/OrbitingBoss.h"
#include "Battle/Ai/Perception.h"
#include "Battle/Ai/RoomState.h"
}

namespace {
template <typename Pointer>
Pointer InvalidatedBinding() {
  static_assert(std::is_pointer_v<Pointer>);
  return reinterpret_cast<Pointer>(std::uintptr_t{1});
}

struct SessionBindingSnapshot {
  decltype(std::to_array(Battle_Ai_actorAiStates)) actors =
      std::to_array(Battle_Ai_actorAiStates);
  decltype(std::to_array(Battle_Ai_decisionCallbacks)) callbacks =
      std::to_array(Battle_Ai_decisionCallbacks);
  Battle_Ai_FrameContext frame = *Battle_Ai_FrameState();
  decltype(Battle_Ai_roomAiState) room = Battle_Ai_roomAiState;
  decltype(Battle_Ai_tileFlagMap) flags = Battle_Ai_tileFlagMap;
  decltype(Battle_Ai_specialTerrainHeights) heights =
      Battle_Ai_specialTerrainHeights;
  decltype(Battle_Ai_roomHasWaypoints) auxiliary = Battle_Ai_roomHasWaypoints;
  decltype(Battle_Ai_patrolRoutes) patrol = Battle_Ai_patrolRoutes;
  decltype(Battle_Ai_roomWaypoints) waypoints = Battle_Ai_roomWaypoints;
  decltype(Battle_Ai_routeGates) gates = Battle_Ai_routeGates;
  decltype(Battle_Ai_arenaOrbit) orbit = Battle_Ai_arenaOrbit;
  decltype(Battle_Ai_planningStatsSnapshot) stats =
      Battle_Ai_planningStatsSnapshot;
  decltype(Battle_Ai_planningWorkspace) workspace = Battle_Ai_planningWorkspace;
  decltype(Battle_Ai_planningPassCandidates) pass =
      Battle_Ai_planningPassCandidates;
  decltype(Battle_Ai_candidateExclusions) exclusions =
      Battle_Ai_candidateExclusions;
  decltype(Battle_Ai_fallbackCandidates) fallback =
      Battle_Ai_fallbackCandidates;
  int mask = Battle_Ai_attributeActorMask;
  int target = Battle_Ai_secondPerceptionTarget;
  int count = Battle_Ai_fallbackCandidateCount;
  int plan_counter = Battle_Ai_unusedPlanCounter;
  int engaged = Battle_Ai_candidateEngagedThisUpdate;

  ~SessionBindingSnapshot() {
    std::copy(actors.begin(), actors.end(), Battle_Ai_actorAiStates);
    std::copy(callbacks.begin(), callbacks.end(), Battle_Ai_decisionCallbacks);
    *Battle_Ai_FrameState() = frame;
    Battle_Ai_roomAiState = room;
    Battle_Ai_tileFlagMap = flags;
    Battle_Ai_specialTerrainHeights = heights;
    Battle_Ai_roomHasWaypoints = auxiliary;
    Battle_Ai_patrolRoutes = patrol;
    Battle_Ai_roomWaypoints = waypoints;
    Battle_Ai_routeGates = gates;
    Battle_Ai_arenaOrbit = orbit;
    Battle_Ai_planningStatsSnapshot = stats;
    Battle_Ai_planningWorkspace = workspace;
    Battle_Ai_planningPassCandidates = pass;
    Battle_Ai_candidateExclusions = exclusions;
    Battle_Ai_fallbackCandidates = fallback;
    Battle_Ai_attributeActorMask = mask;
    Battle_Ai_secondPerceptionTarget = target;
    Battle_Ai_fallbackCandidateCount = count;
    Battle_Ai_unusedPlanCounter = plan_counter;
    Battle_Ai_candidateEngagedThisUpdate = engaged;
  }
};

void SeedInvalidatedRoomBindings() {
  Battle_Ai_roomAiState = InvalidatedBinding<decltype(Battle_Ai_roomAiState)>();
  Battle_Ai_tileFlagMap = InvalidatedBinding<decltype(Battle_Ai_tileFlagMap)>();
  Battle_Ai_specialTerrainHeights =
      InvalidatedBinding<decltype(Battle_Ai_specialTerrainHeights)>();
  Battle_Ai_roomHasWaypoints =
      InvalidatedBinding<decltype(Battle_Ai_roomHasWaypoints)>();
  Battle_Ai_patrolRoutes =
      InvalidatedBinding<decltype(Battle_Ai_patrolRoutes)>();
  Battle_Ai_roomWaypoints =
      InvalidatedBinding<decltype(Battle_Ai_roomWaypoints)>();
  Battle_Ai_routeGates = InvalidatedBinding<decltype(Battle_Ai_routeGates)>();
}

void SeedInvalidatedPlanningBindings() {
  Battle_Ai_arenaOrbit = InvalidatedBinding<decltype(Battle_Ai_arenaOrbit)>();
  Battle_Ai_planningStatsSnapshot =
      InvalidatedBinding<decltype(Battle_Ai_planningStatsSnapshot)>();
  Battle_Ai_planningWorkspace =
      InvalidatedBinding<decltype(Battle_Ai_planningWorkspace)>();
  Battle_Ai_planningPassCandidates =
      InvalidatedBinding<decltype(Battle_Ai_planningPassCandidates)>();
  Battle_Ai_candidateExclusions =
      InvalidatedBinding<decltype(Battle_Ai_candidateExclusions)>();
  Battle_Ai_fallbackCandidates =
      InvalidatedBinding<decltype(Battle_Ai_fallbackCandidates)>();
}

void SeedInvalidatedFrameBindings() {
  auto& frame = *Battle_Ai_FrameState();
  std::fill_n(frame.actors, std::size(frame.actors),
              InvalidatedBinding<Battle_Ai_ActorAiState*>());
  frame.room = InvalidatedBinding<decltype(frame.room)>();
  frame.heightMap = InvalidatedBinding<decltype(frame.heightMap)>();
  frame.tileFlags = InvalidatedBinding<decltype(frame.tileFlags)>();
  frame.roomData = InvalidatedBinding<decltype(frame.roomData)>();
  frame.terrainRegions = InvalidatedBinding<decltype(frame.terrainRegions)>();
}

void CheckInvalidatedRoomBindings() {
  CHECK(Battle_Ai_roomAiState == nullptr);
  CHECK(Battle_Ai_tileFlagMap == nullptr);
  CHECK(Battle_Ai_specialTerrainHeights == nullptr);
  CHECK(Battle_Ai_roomHasWaypoints == nullptr);
  CHECK(Battle_Ai_patrolRoutes == nullptr);
  CHECK(Battle_Ai_roomWaypoints == nullptr);
  CHECK(Battle_Ai_routeGates == nullptr);
}

void CheckInvalidatedPlanningBindings() {
  CHECK(Battle_Ai_arenaOrbit == nullptr);
  CHECK(Battle_Ai_planningStatsSnapshot == nullptr);
  CHECK(Battle_Ai_planningWorkspace == nullptr);
  CHECK(Battle_Ai_planningPassCandidates == nullptr);
  CHECK(Battle_Ai_candidateExclusions == nullptr);
  CHECK(Battle_Ai_fallbackCandidates == nullptr);
}

void CheckInvalidatedMembership() {
  CHECK(std::ranges::all_of(Battle_Ai_actorAiStates, [](const auto* state) {
    return state == nullptr;
  }));
  CHECK(
      std::ranges::all_of(Battle_Ai_FrameState()->actors,
                          [](const auto* state) { return state == nullptr; }));
  CHECK(Battle_Ai_attributeActorMask == 0);
  CHECK(Battle_Ai_secondPerceptionTarget == 0);
  CHECK(Battle_Ai_fallbackCandidateCount == 0);
  CHECK(Battle_Ai_unusedPlanCounter == 0);
  CHECK(Battle_Ai_candidateEngagedThisUpdate == 0);
}

void CheckInvalidatedFrameBindings() {
  const auto& frame = *Battle_Ai_FrameState();
  CHECK(frame.room == nullptr);
  CHECK(frame.heightMap == nullptr);
  CHECK(frame.tileFlags == nullptr);
  CHECK(frame.roomData == nullptr);
  CHECK(frame.terrainRegions == nullptr);
}
}  // namespace

TEST_CASE("AI session invalidation never touches reclaimed allocation storage",
          "[ai][session-reset]") {
  SessionBindingSnapshot saved;
  SeedInvalidatedRoomBindings();
  SeedInvalidatedPlanningBindings();
  SeedInvalidatedFrameBindings();
  auto* dead = InvalidatedBinding<Battle_Ai_ActorAiState*>();
  std::fill_n(Battle_Ai_actorAiStates, saved.actors.size(), dead);
  Battle_Ai_attributeActorMask = 0x1D;
  Battle_Ai_secondPerceptionTarget = 4;
  Battle_Ai_fallbackCandidateCount = 3;
  Battle_Ai_unusedPlanCounter = 7;
  Battle_Ai_candidateEngagedThisUpdate = 1;
  Battle_Ai_InvalidateSessionBindings();
  CheckInvalidatedRoomBindings();
  CheckInvalidatedPlanningBindings();
  CheckInvalidatedFrameBindings();
  CheckInvalidatedMembership();
  CHECK(std::to_array(Battle_Ai_decisionCallbacks) == saved.callbacks);
  Battle_Ai_InvalidateSessionBindings();
  CheckInvalidatedMembership();
}
