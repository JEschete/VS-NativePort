#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <type_traits>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Legacy/Psx.h"

extern "C" {
#include "Battle/Ai/ActorCleanup.h"
#include "Battle/Ai/ActorState.h"
#include "Battle/Ai/AiState.h"
#include "Battle/Ai/Engagement.h"
#include "Battle/Ai/FrameUpdate.h"
#include "Battle/Ai/Movement.h"
#include "Battle/Ai/Navigation.h"
#include "Battle/Ai/OrbitingBoss.h"
#include "Battle/Damage.h"
#include "Battle/Feedback.h"
#include "Core/Memory.h"
#include "Core/Memory/Heap.h"
}

namespace {
struct CleanupFixture {
  Core_HeapBlockHeader saved_heap = Core_legacyHeapList;
  Core_HeapBlockHeader saved_secondary = Core_legacySecondaryHeapList;
  decltype(std::to_array(Battle_actors)) saved_actors =
      std::to_array(Battle_actors);
  decltype(std::to_array(Battle_Ai_actorAiStates)) saved_states =
      std::to_array(Battle_Ai_actorAiStates);
  decltype(std::to_array(Battle_actorMarkers)) saved_markers =
      std::to_array(Battle_actorMarkers);
  Battle_Ai_RoomAiState* saved_room = Battle_Ai_roomAiState;
  decltype(Battle_Ai_tileFlagMap) saved_flags = Battle_Ai_tileFlagMap;
  Battle_Ai_ArenaOrbitState* saved_orbit = Battle_Ai_arenaOrbit;
  int saved_attribute = Battle_Ai_attributeActorMask;
  int saved_busy = Battle_Ai_candidateEngagedThisUpdate;
  std::array<Core_HeapBlockHeader, 2048> heap{};
  std::array<Battle_Actor, 3> actors{};
  Battle_Ai_RoomAiState room{};
  u_short flags[32][32]{};
  unsigned free_units = 0;

  CleanupFixture() {
    Core_Memory_InitializeHeap(heap.data(), sizeof(heap));
    free_units = Core_legacyHeapList.next->blockSize;
    Battle_Ai_roomAiState = &room;
    Battle_Ai_tileFlagMap = flags;
    Battle_Ai_arenaOrbit = nullptr;
    Battle_Ai_attributeActorMask = 0;
    Battle_Ai_candidateEngagedThisUpdate = 0;
    std::fill_n(Battle_actors, saved_actors.size(), nullptr);
    std::fill_n(Battle_Ai_actorAiStates, saved_states.size(), nullptr);
    const std::array<int, 3> ids{0, 2, 5};
    for (std::size_t index = 0; index < actors.size(); ++index) {
      actors[index].id = ids[index];
      actors[index].kind = 4;
      actors[index].commandFlags = 0xFF;
      actors[index].next =
          index + 1 < actors.size() ? &actors[index + 1] : nullptr;
      Battle_actors[ids[index]] = &actors[index];
    }
    room.maxTileX = room.minTileX = 31;
    room.maxTileZ = room.minTileZ = 0;
    room.engagementLimit = 2;
  }

  ~CleanupFixture() {
    Core_legacyHeapList = saved_heap;
    Core_legacySecondaryHeapList = saved_secondary;
    std::copy(saved_actors.begin(), saved_actors.end(), Battle_actors);
    std::copy(saved_states.begin(), saved_states.end(),
              Battle_Ai_actorAiStates);
    std::copy(saved_markers.begin(), saved_markers.end(), Battle_actorMarkers);
    Battle_Ai_roomAiState = saved_room;
    Battle_Ai_tileFlagMap = saved_flags;
    Battle_Ai_arenaOrbit = saved_orbit;
    Battle_Ai_attributeActorMask = saved_attribute;
    Battle_Ai_candidateEngagedThisUpdate = saved_busy;
  }

  Battle_Ai_ActorAiState* AllocateState(std::size_t index) {
    auto* state = static_cast<Battle_Ai_ActorAiState*>(
        Core_Memory_AllocateFromLowEnd(sizeof(Battle_Ai_ActorAiState)));
    REQUIRE(state != nullptr);
    std::construct_at(state);
    state->actorId = static_cast<u_char>(actors[index].id);
    state->actor = reinterpret_cast<Battle_Ai_EngagementData*>(&actors[index]);
    state->nextTile.word = 0x00070004;
    Battle_Ai_actorAiStates[state->actorId] = state;
    Battle_Ai_attributeActorMask |= 1 << state->actorId;
    return state;
  }
};
}  // namespace

TEST_CASE("Reaction requests draw before checking full-width route ownership",
          "[ai][cleanup]") {
  CleanupFixture fixture;
  auto* state = fixture.AllocateState(1);
  const auto saved_frame = *Battle_Ai_FrameState();
  Battle_Ai_FrameState()->randomState = 40;
  Battle_Ai_FrameState()->lastBudgetTick = 1;
  state->nativePointer18C =
      reinterpret_cast<void*>(std::uintptr_t{0x100000000});
  Battle_Ai_SetActorAiReactionWithDelay(
      reinterpret_cast<Battle_Ai_PerceptionUpdateView*>(state), 1, 64);
  CHECK(Battle_Ai_FrameState()->randomState == 41);
  CHECK(state->decisionDelay == 0);
  CHECK(fixture.actors[1].commandFlags == 0xFF);
  state->nativePointer18C = nullptr;
  Battle_Ai_SetActorAiReactionWithDelay(
      reinterpret_cast<Battle_Ai_PerceptionUpdateView*>(state), 0x123, 64);
  CHECK(state->decisionDelay == 30);
  CHECK(fixture.actors[1].commandFlags == 0x23);
  *Battle_Ai_FrameState() = saved_frame;
  Battle_Ai_FreeActorAiState(&fixture.actors[1]);
}

TEST_CASE("Defeat detaches every native peer reference before repeat cleanup",
          "[ai][cleanup]") {
  CleanupFixture fixture;
  auto* first = fixture.AllocateState(1);
  auto* second = fixture.AllocateState(2);
  first->linkedActor = 5;
  second->linkedActor = 5;
  Battle_Ai_ClearActorAiLinksAndPlan(5);
  CHECK(first->linkedActor == 16);
  CHECK(second->linkedActor == 16);
  Battle_Ai_ClearActorAiLinksAndPlan(5);
  Battle_Ai_FreeActorAiState(&fixture.actors[1]);
  Battle_Ai_FreeActorAiState(&fixture.actors[2]);
  CHECK(Core_legacyHeapList.next->blockSize == fixture.free_units);
}

TEST_CASE("AI navigation override views retain a full native pointer",
          "[ai][cleanup][layout]") {
  REQUIRE(sizeof(decltype(Battle_Ai_PerceptionUpdateView::opaque18C)) ==
          sizeof(void*));
  CHECK(std::is_pointer_v<decltype(Battle_Ai_PerceptionUpdateView::opaque18C)>);
  CHECK(offsetof(Battle_Ai_PerceptionUpdateView, opaque18C) ==
        offsetof(Battle_Ai_ActorAiState, nativePointer18C));
}

TEST_CASE(
    "AI navigation override release frees the actual full-width heap block",
    "[ai][cleanup]") {
  CleanupFixture fixture;
  auto* state = fixture.AllocateState(1);
  void* route = Core_Memory_AllocateFromHighEnd(512);
  REQUIRE(route != nullptr);
  state->nativePointer18C = route;
  const auto allocated = Core_legacyHeapList.next->blockSize;
  CHECK(reinterpret_cast<uintptr_t>(route) > 0xFFFFFFFFu);
  Battle_Ai_FreeActorNavigationOverride(2);
  CHECK(state->nativePointer18C == nullptr);
  CHECK(Core_legacyHeapList.next->blockSize > allocated);
  Battle_Ai_FreeActorNavigationOverride(2);
  Battle_Ai_FreeActorAiState(&fixture.actors[1]);
  CHECK(Core_legacyHeapList.next->blockSize == fixture.free_units);
}

TEST_CASE(
    "AI reservation cleanup respects bounds and preserves other cell flags",
    "[ai][cleanup]") {
  CleanupFixture fixture;
  auto* state = fixture.AllocateState(1);
  fixture.flags[7][4] = 0xFFFF;
  state->opaque1A = 7;
  Battle_Ai_ReleaseActorAiReservations(state);
  CHECK(state->opaque1A == 0);
  CHECK(fixture.flags[7][4] == 0xFF7F);
  Battle_Ai_ForceReleaseActorAiReservations(2);
  CHECK(state->opaque1A == 1);
  fixture.room.maxTileZ = 5;
  fixture.flags[7][4] = 0xFFFF;
  Battle_Ai_ReleaseActorAiReservations(state);
  CHECK(fixture.flags[7][4] == 0xFFFF);
  Battle_Ai_FreeActorAiState(&fixture.actors[1]);
}

TEST_CASE(
    "AI teardown detaches peers and special actors before releasing state",
    "[ai][cleanup]") {
  CleanupFixture fixture;
  auto* removed = fixture.AllocateState(1);
  auto* peer = fixture.AllocateState(2);
  peer->linkedActor = 2;
  fixture.room.specialActors[0] = fixture.room.specialActors[3] =
      &fixture.actors[1];
  fixture.room.specialActors[1] = &fixture.actors[2];
  REQUIRE(Battle_Ai_TryEngageActor(removed) == 1);
  Battle_Ai_FreeActorAiState(&fixture.actors[1]);
  CHECK(Battle_Ai_actorAiStates[2] == nullptr);
  CHECK(Battle_Ai_actorAiStates[5] == peer);
  CHECK(peer->linkedActor == 16);
  CHECK(Battle_Ai_attributeActorMask == (1 << 5));
  CHECK(fixture.room.specialActors[0] == nullptr);
  CHECK(fixture.room.specialActors[3] == nullptr);
  CHECK(fixture.room.specialActors[1] == &fixture.actors[2]);
  Battle_Ai_FreeActorAiState(&fixture.actors[2]);
  CHECK(Core_legacyHeapList.next->blockSize == fixture.free_units);
}

TEST_CASE("AI teardown leaves no engaged-list or marker references",
          "[ai][cleanup]") {
  CleanupFixture fixture;
  auto* state = fixture.AllocateState(1);
  REQUIRE(Battle_Ai_TryEngageActor(state) == 1);
  Battle_actorMarkers[2] = 0x47;
  Battle_Ai_FreeActorAiState(&fixture.actors[1]);
  CHECK(fixture.room.engagedCount == 0);
  CHECK(fixture.room.engagedHead == nullptr);
  CHECK(fixture.room.engagedTail == nullptr);
  CHECK(Battle_actorMarkers[2] == 0);
  CHECK(fixture.actors[1].commandFlags == 0xF8);
  Battle_Ai_FreeActorAiState(&fixture.actors[1]);
  CHECK(Core_legacyHeapList.next->blockSize == fixture.free_units);
}

TEST_CASE("AI teardown preserves the original Ashley state lifetime",
          "[ai][cleanup]") {
  CleanupFixture fixture;
  auto* state = fixture.AllocateState(0);
  state->nativePointer18C = Core_Memory_AllocateFromHighEnd(128);
  REQUIRE(state->nativePointer18C != nullptr);
  Battle_Ai_FreeActorAiState(&fixture.actors[0]);
  CHECK(Battle_Ai_actorAiStates[0] == state);
  CHECK(state->nativePointer18C == nullptr);
  Core_Memory_FreeFromLowEnd(state);
  Battle_Ai_actorAiStates[0] = nullptr;
  CHECK(Core_legacyHeapList.next->blockSize == fixture.free_units);
}

TEST_CASE("AI special orbit teardown releases shared ownership once",
          "[ai][cleanup]") {
  const auto type = GENERATE(0xAC, 0xAD);
  CleanupFixture fixture;
  fixture.AllocateState(1);
  fixture.actors[1].subType = static_cast<u_short>(type);
  Battle_Ai_arenaOrbit = static_cast<Battle_Ai_ArenaOrbitState*>(
      Core_Memory_AllocateFromLowEnd(sizeof(Battle_Ai_ArenaOrbitState)));
  REQUIRE(Battle_Ai_arenaOrbit != nullptr);
  Battle_Ai_FreeActorAiState(&fixture.actors[1]);
  CHECK(Battle_Ai_arenaOrbit == nullptr);
  CHECK(Core_legacyHeapList.next->blockSize == fixture.free_units);
}
