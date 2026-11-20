#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <cstring>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Battle/Actor.h"
#include "Battle/Ai/ActorState.hpp"
#include "Battle/Ai/AiViews.hpp"
#include "Battle/Ai/EngagementLive.hpp"
#include "Battle/Ai/Perception.hpp"
#include "Battle/Damage.hpp"
#include "Battle/Feedback.hpp"
#include "Legacy/Psx.h"

namespace {
struct EngagementFixture {
  decltype(VS::Battle::Ai::roomAiState) saved_room =
      VS::Battle::Ai::roomAiState;
  decltype(VS::Battle::actors) saved_actors = VS::Battle::actors;
  decltype(VS::Battle::Ai::actorAiStates) saved_states =
      VS::Battle::Ai::actorAiStates;
  decltype(VS::Battle::actorMarkers) saved_markers = VS::Battle::actorMarkers;
  int saved_busy = VS::Battle::Ai::candidateEngagedThisUpdate;
  VS::Battle::Ai::RoomAiState room{};
  std::array<Battle_Actor, 3> actors{};
  std::array<VS::Battle::Ai::ActorAiState, 3> states{};

  EngagementFixture() {
    VS::Battle::Ai::roomAiState = &room;
    VS::Battle::Ai::candidateEngagedThisUpdate = 0;
    room.engagement_limit = 1;
    VS::Battle::Ai::StoreEngagedMask(room, 0);
    room.engaged_count = 0;
    std::fill_n(VS::Battle::actors.begin(), saved_actors.size(), nullptr);
    std::fill_n(VS::Battle::Ai::actorAiStates.begin(), saved_states.size(),
                nullptr);
    const std::array<int, 3> ids{0, 2, 5};
    for (std::size_t index = 0; index < actors.size(); ++index) {
      actors[index].id = states[index].actor_id = ids[index];
      actors[index].next =
          index + 1 < actors.size() ? &actors[index + 1] : nullptr;
      states[index].actor =
          reinterpret_cast<VS::Battle::Ai::EngagementData*>(&actors[index]);
      states[index].engagement_request = 7;
      VS::Battle::actors[ids[index]] = &actors[index];
      VS::Battle::Ai::actorAiStates[ids[index]] = &states[index];
    }
    std::fill_n(VS::Battle::actorMarkers.begin(), saved_markers.size(),
                std::bit_cast<char>(uint8_t{0x80}));
  }

  ~EngagementFixture() {
    VS::Battle::Ai::roomAiState = saved_room;
    VS::Battle::Ai::candidateEngagedThisUpdate = saved_busy;
    std::copy(saved_actors.begin(), saved_actors.end(),
              VS::Battle::actors.begin());
    std::copy(saved_states.begin(), saved_states.end(),
              VS::Battle::Ai::actorAiStates.begin());
    std::copy(saved_markers.begin(), saved_markers.end(),
              VS::Battle::actorMarkers.begin());
  }

  uint16_t Mask() const {
    uint16_t result = 0;
    std::memcpy(&result, room.engaged_mask_bytes.data(), sizeof(result));
    return result;
  }

  void CheckSingleMember(std::size_t index) const {
    CHECK(room.engaged_count == 1);
    CHECK(Mask() == (1u << states[index].actor_id));
    CHECK(room.engaged_head == &states[index]);
    CHECK(room.engaged_tail == &states[index]);
  }

  void CheckPair() const {
    CHECK(room.engaged_count == 2);
    CHECK(Mask() == 0x24);
    CHECK(room.engaged_head == &states[2]);
    CHECK(states[2].next_engaged == &states[1]);
    CHECK(room.engaged_tail == &states[1]);
  }

  void CheckEmpty() const {
    CHECK(room.engaged_count == 0);
    CHECK(Mask() == 0);
    CHECK(room.engaged_head == nullptr);
    CHECK(room.engaged_tail == nullptr);
  }
};
}  // namespace

TEST_CASE("AI engagement becomes ready at exactly thirty-two buildup frames",
          "[ai][engagement]") {
  const uint16_t timer = GENERATE(0, 1, 31, 32, 33, 65535);
  VS::Battle::Ai::ActorAiState state{};
  state.engagement_frames = timer;
  CHECK(VS::Battle::Ai::IsLiveEngagementReady(&state) == (timer >= 32));
}

TEST_CASE(
    "AI joining owns membership, list links and repeat-request bookkeeping",
    "[ai][engagement]") {
  EngagementFixture fixture;
  CHECK(VS::Battle::Ai::TryEngageLiveActor(&fixture.states[1]));
  CHECK(fixture.states[1].engagement_frames == 1);
  CHECK(fixture.states[1].engagement_request == 0);
  fixture.CheckSingleMember(1);
  CHECK(VS::Battle::Ai::TryEngageLiveActor(&fixture.states[2]));
  fixture.CheckPair();
  CHECK(VS::Battle::Ai::TryEngageLiveActor(&fixture.states[2]));
  fixture.CheckPair();
  CHECK_FALSE(VS::Battle::Ai::TryEngageLiveActor(&fixture.states[0]));
  fixture.CheckPair();
}

TEST_CASE(
    "AI engagement rejects scheduling and cooldown blocks without mutation",
    "[ai][engagement]") {
  const auto blocked = GENERATE(0, 1, 2);
  EngagementFixture fixture;
  if (blocked == 0) fixture.states[1].engagement_cooldown_frames = 1;
  if (blocked == 1) VS::Battle::Ai::candidateEngagedThisUpdate = 1;
  if (blocked == 2) {
    fixture.room.engagement_limit = 0;
    REQUIRE(VS::Battle::Ai::TryEngageLiveActor(&fixture.states[2]));
  }
  const auto count = fixture.room.engaged_count;
  const auto mask = fixture.Mask();
  CHECK_FALSE(VS::Battle::Ai::TryEngageLiveActor(&fixture.states[1]));
  CHECK(fixture.states[1].engagement_frames == 0);
  CHECK(fixture.states[1].engagement_request == 7);
  CHECK(fixture.room.engaged_count == count);
  CHECK(fixture.Mask() == mask);
}

TEST_CASE(
    "AI recent-leave state rejoins at ready threshold without a fresh warmup",
    "[ai][engagement]") {
  EngagementFixture fixture;
  fixture.states[1].reengagement_grace_frames = 1;
  CHECK(VS::Battle::Ai::TryEngageLiveActor(&fixture.states[1]));
  CHECK(fixture.states[1].engagement_frames == 32);
  CHECK(VS::Battle::Ai::IsLiveEngagementReady(&fixture.states[1]));
}

TEST_CASE("AI existing buildup consumer advances a real join to readiness",
          "[ai][engagement]") {
  EngagementFixture fixture;
  REQUIRE(VS::Battle::Ai::TryEngageLiveActor(&fixture.states[1]));
  for (int tick = 0; tick < 30; ++tick) {
    VS::Battle::Ai::UpdateLiveEngagementBuildup(&fixture.states[1]);
  }
  CHECK(fixture.states[1].engagement_frames == 31);
  CHECK_FALSE(VS::Battle::Ai::IsLiveEngagementReady(&fixture.states[1]));
  VS::Battle::Ai::UpdateLiveEngagementBuildup(&fixture.states[1]);
  CHECK(VS::Battle::Ai::IsLiveEngagementReady(&fixture.states[1]));
  CHECK(static_cast<unsigned char>(VS::Battle::actorMarkers[2]) == 0x84);
}

TEST_CASE(
    "AI leave unlinks the actor and preserves the two-actor cooldown exception",
    "[ai][engagement]") {
  const bool two_actors = GENERATE(false, true);
  EngagementFixture fixture;
  if (two_actors) fixture.actors[1].next = nullptr;
  REQUIRE(VS::Battle::Ai::TryEngageLiveActor(&fixture.states[1]));
  VS::Battle::Ai::DisengageLiveActor(&fixture.states[1]);
  CHECK(fixture.states[1].engagement_frames == 0);
  CHECK(fixture.states[1].engagement_cooldown_frames == (two_actors ? 0 : 48));
  CHECK(fixture.states[1].reengagement_grace_frames == 90);
  fixture.CheckEmpty();
  CHECK(static_cast<unsigned char>(VS::Battle::actorMarkers[2]) == 0x80);
  VS::Battle::Ai::DisengageLiveActor(&fixture.states[1]);
  fixture.CheckEmpty();
}

TEST_CASE("AI leaving the list head preserves the remaining encounter",
          "[ai][engagement]") {
  EngagementFixture fixture;
  REQUIRE(VS::Battle::Ai::TryEngageLiveActor(&fixture.states[1]));
  REQUIRE(VS::Battle::Ai::TryEngageLiveActor(&fixture.states[2]));
  VS::Battle::Ai::DisengageLiveActorById(5);
  CHECK(fixture.room.engaged_head == &fixture.states[1]);
  CHECK(fixture.room.engaged_tail == &fixture.states[1]);
  CHECK(fixture.room.engaged_count == 1);
  CHECK(fixture.Mask() == 4);
  VS::Battle::Ai::DisengageLiveActorById(0);
  VS::Battle::Ai::DisengageLiveActorById(7);
  CHECK(fixture.room.engaged_count == 1);
}

TEST_CASE("AI disengagement by ID ignores IDs outside the AI state table",
          "[ai][engagement]") {
  const int actor_id = GENERATE(-1, 16, 255);
  EngagementFixture fixture;
  REQUIRE(VS::Battle::Ai::TryEngageLiveActor(&fixture.states[1]));
  VS::Battle::Ai::DisengageLiveActorById(actor_id);
  fixture.CheckSingleMember(1);
  CHECK(fixture.states[1].engagement_frames == 1);
}

TEST_CASE("AI marker wrappers never reset every actor through Ashley's index",
          "[ai][engagement]") {
  EngagementFixture fixture;
  const auto before = VS::Battle::actorMarkers;
  VS::Battle::Ai::SetLiveActorAwarenessIndicator(0, 0);
  CHECK(VS::Battle::actorMarkers == before);
  VS::Battle::Ai::SetLiveActorAwarenessIndicator(2, 5);
  CHECK(static_cast<unsigned char>(VS::Battle::actorMarkers[2]) == 0x85);
  VS::Battle::Ai::ResetLiveActorAwarenessIndicator(
      reinterpret_cast<VS::Battle::Ai::PlanEvaluationView*>(
          &fixture.states[1]));
  CHECK(static_cast<unsigned char>(VS::Battle::actorMarkers[2]) == 0);
}

TEST_CASE(
    "AI temporary awareness consumes the existing indicator producer's timer",
    "[ai][engagement]") {
  EngagementFixture fixture;
  auto& state = fixture.states[1];
  VS::Battle::Ai::SetLiveTemporaryAwarenessIndicator(&state, 7, 2);
  auto* view = reinterpret_cast<VS::Battle::Ai::PerceptionUpdateView*>(&state);
  VS::Battle::Ai::UpdateLiveTemporaryAwarenessIndicator(view);
  CHECK(state.awareness_frames_remaining == 1);
  CHECK(static_cast<unsigned char>(VS::Battle::actorMarkers[2]) == 0x87);
  VS::Battle::Ai::UpdateLiveTemporaryAwarenessIndicator(view);
  CHECK(state.awareness_frames_remaining == 0);
  CHECK(static_cast<unsigned char>(VS::Battle::actorMarkers[2]) == 0x80);
  VS::Battle::Ai::UpdateLiveTemporaryAwarenessIndicator(view);
  CHECK(state.awareness_marker == 7);
}

TEST_CASE("AI temporary awareness keeps the stored widths of marker and frames",
          "[ai][engagement]") {
  VS::Battle::Ai::TemporaryAwareness awareness{.frames_remaining = 9,
                                               .marker = 3};
  VS::Battle::Ai::SetTemporaryAwarenessIndicator(awareness, 0x12345, 0x1FF);
  CHECK(awareness.marker == 0x2345);
  CHECK(awareness.frames_remaining == 0xFF);
  VS::Battle::Ai::SetTemporaryAwarenessIndicator(awareness, -1, -1);
  CHECK(awareness.marker == 0xFFFF);
  CHECK(awareness.frames_remaining == 0xFF);

  EngagementFixture fixture;
  auto& state = fixture.states[1];
  VS::Battle::Ai::SetLiveTemporaryAwarenessIndicator(&state, 0x10007, 0x128);
  CHECK(state.awareness_marker == 7);
  CHECK(state.awareness_frames_remaining == 0x28);
  CHECK(static_cast<unsigned char>(VS::Battle::actorMarkers[2]) == 0x80);
}
