#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <cstring>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Legacy/Psx.h"

extern "C" {
#include "Battle/Ai/ActorState.h"
#include "Battle/Ai/Engagement.h"
#include "Battle/Ai/Perception.h"
#include "Battle/Damage.h"
#include "Battle/Feedback.h"
}

namespace {
struct EngagementFixture {
  decltype(Battle_Ai_roomAiState) saved_room = Battle_Ai_roomAiState;
  decltype(std::to_array(Battle_actors)) saved_actors =
      std::to_array(Battle_actors);
  decltype(std::to_array(Battle_Ai_actorAiStates)) saved_states =
      std::to_array(Battle_Ai_actorAiStates);
  decltype(std::to_array(Battle_actorMarkers)) saved_markers =
      std::to_array(Battle_actorMarkers);
  int saved_busy = Battle_Ai_candidateEngagedThisUpdate;
  Battle_Ai_RoomAiState room{};
  std::array<Battle_Actor, 3> actors{};
  std::array<Battle_Ai_ActorAiState, 3> states{};

  EngagementFixture() {
    Battle_Ai_roomAiState = &room;
    Battle_Ai_candidateEngagedThisUpdate = 0;
    room.engagementLimit = 1;
    std::fill_n(Battle_actors, saved_actors.size(), nullptr);
    std::fill_n(Battle_Ai_actorAiStates, saved_states.size(), nullptr);
    const std::array<int, 3> ids{0, 2, 5};
    for (std::size_t index = 0; index < actors.size(); ++index) {
      actors[index].id = states[index].actorId = ids[index];
      actors[index].next =
          index + 1 < actors.size() ? &actors[index + 1] : nullptr;
      states[index].actor =
          reinterpret_cast<Battle_Ai_EngagementData*>(&actors[index]);
      states[index].engagementRequest = 7;
      Battle_actors[ids[index]] = &actors[index];
      Battle_Ai_actorAiStates[ids[index]] = &states[index];
    }
    std::fill_n(Battle_actorMarkers, saved_markers.size(),
                std::bit_cast<char>(uint8_t{0x80}));
  }

  ~EngagementFixture() {
    Battle_Ai_roomAiState = saved_room;
    Battle_Ai_candidateEngagedThisUpdate = saved_busy;
    std::copy(saved_actors.begin(), saved_actors.end(), Battle_actors);
    std::copy(saved_states.begin(), saved_states.end(),
              Battle_Ai_actorAiStates);
    std::copy(saved_markers.begin(), saved_markers.end(), Battle_actorMarkers);
  }

  uint16_t Mask() const {
    uint16_t result = 0;
    std::memcpy(&result, room.engaged_mask_bytes, sizeof(result));
    return result;
  }

  void CheckSingleMember(std::size_t index) const {
    CHECK(room.engagedCount == 1);
    CHECK(Mask() == (1u << states[index].actorId));
    CHECK(room.engagedHead == &states[index]);
    CHECK(room.engagedTail == &states[index]);
  }

  void CheckPair() const {
    CHECK(room.engagedCount == 2);
    CHECK(Mask() == 0x24);
    CHECK(room.engagedHead == &states[2]);
    CHECK(states[2].nextEngaged == &states[1]);
    CHECK(room.engagedTail == &states[1]);
  }

  void CheckEmpty() const {
    CHECK(room.engagedCount == 0);
    CHECK(Mask() == 0);
    CHECK(room.engagedHead == nullptr);
    CHECK(room.engagedTail == nullptr);
  }
};
}  // namespace

TEST_CASE("AI engagement becomes ready at exactly thirty-two buildup frames",
          "[ai][engagement]") {
  const uint16_t timer = GENERATE(0, 1, 31, 32, 33, 65535);
  Battle_Ai_ActorAiState state{};
  state.engagementFrames = timer;
  CHECK(Battle_Ai_IsEngagementReady(&state) == (timer >= 32));
}

TEST_CASE(
    "AI joining owns membership, list links and repeat-request bookkeeping",
    "[ai][engagement]") {
  EngagementFixture fixture;
  CHECK(Battle_Ai_TryEngageActor(&fixture.states[1]) == 1);
  CHECK(fixture.states[1].engagementFrames == 1);
  CHECK(fixture.states[1].engagementRequest == 0);
  fixture.CheckSingleMember(1);
  CHECK(Battle_Ai_TryEngageActor(&fixture.states[2]) == 1);
  fixture.CheckPair();
  CHECK(Battle_Ai_TryEngageActor(&fixture.states[2]) == 1);
  fixture.CheckPair();
  CHECK(Battle_Ai_TryEngageActor(&fixture.states[0]) == 0);
  fixture.CheckPair();
}

TEST_CASE(
    "AI engagement rejects scheduling and cooldown blocks without mutation",
    "[ai][engagement]") {
  const auto blocked = GENERATE(0, 1, 2);
  EngagementFixture fixture;
  if (blocked == 0) fixture.states[1].engagementCooldownFrames = 1;
  if (blocked == 1) Battle_Ai_candidateEngagedThisUpdate = 1;
  if (blocked == 2) {
    fixture.room.engagementLimit = 0;
    REQUIRE(Battle_Ai_TryEngageActor(&fixture.states[2]) == 1);
  }
  const auto count = fixture.room.engagedCount;
  const auto mask = fixture.Mask();
  CHECK(Battle_Ai_TryEngageActor(&fixture.states[1]) == 0);
  CHECK(fixture.states[1].engagementFrames == 0);
  CHECK(fixture.states[1].engagementRequest == 7);
  CHECK(fixture.room.engagedCount == count);
  CHECK(fixture.Mask() == mask);
}

TEST_CASE(
    "AI recent-leave state rejoins at ready threshold without a fresh warmup",
    "[ai][engagement]") {
  EngagementFixture fixture;
  fixture.states[1].reengagementGraceFrames = 1;
  CHECK(Battle_Ai_TryEngageActor(&fixture.states[1]) == 1);
  CHECK(fixture.states[1].engagementFrames == 32);
  CHECK(Battle_Ai_IsEngagementReady(&fixture.states[1]) == 1);
}

TEST_CASE("AI existing buildup consumer advances a real join to readiness",
          "[ai][engagement]") {
  EngagementFixture fixture;
  REQUIRE(Battle_Ai_TryEngageActor(&fixture.states[1]) == 1);
  for (int tick = 0; tick < 30; ++tick) {
    Battle_Ai_UpdateEngagementBuildup(&fixture.states[1]);
  }
  CHECK(fixture.states[1].engagementFrames == 31);
  CHECK(Battle_Ai_IsEngagementReady(&fixture.states[1]) == 0);
  Battle_Ai_UpdateEngagementBuildup(&fixture.states[1]);
  CHECK(Battle_Ai_IsEngagementReady(&fixture.states[1]) == 1);
  CHECK(static_cast<unsigned char>(Battle_actorMarkers[2]) == 0x84);
}

TEST_CASE(
    "AI leave unlinks the actor and preserves the two-actor cooldown exception",
    "[ai][engagement]") {
  const bool two_actors = GENERATE(false, true);
  EngagementFixture fixture;
  if (two_actors) fixture.actors[1].next = nullptr;
  REQUIRE(Battle_Ai_TryEngageActor(&fixture.states[1]) == 1);
  Battle_Ai_DisengageActor(&fixture.states[1]);
  CHECK(fixture.states[1].engagementFrames == 0);
  CHECK(fixture.states[1].engagementCooldownFrames == (two_actors ? 0 : 48));
  CHECK(fixture.states[1].reengagementGraceFrames == 90);
  fixture.CheckEmpty();
  CHECK(static_cast<unsigned char>(Battle_actorMarkers[2]) == 0x80);
  Battle_Ai_DisengageActor(&fixture.states[1]);
  fixture.CheckEmpty();
}

TEST_CASE("AI leaving the list head preserves the remaining encounter",
          "[ai][engagement]") {
  EngagementFixture fixture;
  REQUIRE(Battle_Ai_TryEngageActor(&fixture.states[1]) == 1);
  REQUIRE(Battle_Ai_TryEngageActor(&fixture.states[2]) == 1);
  Battle_Ai_DisengageActorById(5);
  CHECK(fixture.room.engagedHead == &fixture.states[1]);
  CHECK(fixture.room.engagedTail == &fixture.states[1]);
  CHECK(fixture.room.engagedCount == 1);
  CHECK(fixture.Mask() == 4);
  Battle_Ai_DisengageActorById(0);
  Battle_Ai_DisengageActorById(7);
  CHECK(fixture.room.engagedCount == 1);
}

TEST_CASE("AI marker wrappers never reset every actor through Ashley's index",
          "[ai][engagement]") {
  EngagementFixture fixture;
  const auto before = std::to_array(Battle_actorMarkers);
  Battle_Ai_SetActorAwarenessIndicator(0, 0);
  CHECK(std::to_array(Battle_actorMarkers) == before);
  Battle_Ai_SetActorAwarenessIndicator(2, 5);
  CHECK(static_cast<unsigned char>(Battle_actorMarkers[2]) == 0x85);
  Battle_Ai_ResetActorAwarenessIndicator(
      reinterpret_cast<Battle_Ai_PlanEvaluationView*>(&fixture.states[1]));
  CHECK(static_cast<unsigned char>(Battle_actorMarkers[2]) == 0);
}

TEST_CASE(
    "AI temporary awareness consumes the existing indicator producer's timer",
    "[ai][engagement]") {
  EngagementFixture fixture;
  auto& state = fixture.states[1];
  Battle_Ai_SetTemporaryAwarenessIndicator(&state, 7, 2);
  auto* view = reinterpret_cast<Battle_Ai_PerceptionUpdateView*>(&state);
  Battle_Ai_UpdateTemporaryAwarenessIndicator(view);
  CHECK(state.awarenessFramesRemaining == 1);
  CHECK(static_cast<unsigned char>(Battle_actorMarkers[2]) == 0x87);
  Battle_Ai_UpdateTemporaryAwarenessIndicator(view);
  CHECK(state.awarenessFramesRemaining == 0);
  CHECK(static_cast<unsigned char>(Battle_actorMarkers[2]) == 0x80);
  Battle_Ai_UpdateTemporaryAwarenessIndicator(view);
  CHECK(state.awarenessMarker == 7);
}
