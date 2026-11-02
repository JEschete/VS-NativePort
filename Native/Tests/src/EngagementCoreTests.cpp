#include <array>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Battle/Ai/Engagement.hpp"

namespace {
namespace ai = VS::Battle::Ai;
using MarkerCalls = std::vector<std::pair<int, int>>;

class FakeMarkers final : public ai::AwarenessMarkers {
 public:
  void SetMarker(int actor_id, int marker) override {
    calls.emplace_back(actor_id, marker);
  }

  MarkerCalls calls;
};

class FakeEngagementHost final : public ai::EngagementHost {
 public:
  void SetMarker(int actor_id, int marker) override {
    markers.emplace_back(actor_id, marker);
  }
  [[nodiscard]] bool CandidateEngagedThisUpdate() const override {
    return busy;
  }
  void ClearEngagementRequest() override { ++cleared; }
  void LinkEngagedActor() override { ++linked; }
  void UnlinkEngagedActor() override { ++unlinked; }
  [[nodiscard]] std::optional<bool> HasSingleOpponent() const override {
    return single_opponent;
  }

  bool busy = false;
  std::optional<bool> single_opponent = false;
  int cleared = 0;
  int linked = 0;
  int unlinked = 0;
  MarkerCalls markers;
};

class FakeEngagementTable final : public ai::EngagementTable {
 public:
  [[nodiscard]] bool HasActorState(int actor_id) const override {
    return present.at(static_cast<std::size_t>(actor_id));
  }
  void DisengageActorState(int actor_id) override {
    disengaged.push_back(actor_id);
  }

  std::array<bool, ai::kEngagementActorSlots> present{};
  std::vector<int> disengaged;
};

bool SameTimers(const ai::EngagementTimers& a, const ai::EngagementTimers& b) {
  return a.engaged_frames == b.engaged_frames &&
         a.cooldown_frames == b.cooldown_frames &&
         a.grace_frames == b.grace_frames;
}

bool SameRoster(const ai::EngagementRoster& a, const ai::EngagementRoster& b) {
  return a.engaged_mask == b.engaged_mask &&
         a.engaged_count == b.engaged_count && a.limit == b.limit &&
         a.approach_moves == b.approach_moves;
}
}  // namespace

TEST_CASE(
    "Awareness markers skip Ashley and refuse IDs outside the AI state table",
    "[ai][engagement]") {
  const int actor_id = GENERATE(-1, 0, 1, 15, 16, 255);
  FakeMarkers markers;
  const bool in_table = actor_id >= 0 && actor_id < 16;
  CHECK(ai::SetActorAwarenessIndicator(markers, actor_id, 5) == in_table);
  if (in_table && actor_id != 0) {
    CHECK(markers.calls == MarkerCalls{{actor_id, 5}});
  } else {
    CHECK(markers.calls.empty());
  }
}

TEST_CASE("Awareness reset clears the marker before showing the reset marker",
          "[ai][engagement]") {
  FakeMarkers markers;
  CHECK(ai::ResetActorAwarenessIndicator(markers, 3));
  CHECK(markers.calls == MarkerCalls{{3, 0}, {3, 0x40}});
  markers.calls.clear();
  CHECK(ai::ResetActorAwarenessIndicator(markers, 0));
  CHECK(markers.calls.empty());
  CHECK_FALSE(ai::ResetActorAwarenessIndicator(markers, 16));
  CHECK_FALSE(ai::ResetActorAwarenessIndicator(markers, -1));
  CHECK(markers.calls.empty());
}

TEST_CASE("Engagement readiness starts at exactly thirty-two frames",
          "[ai][engagement]") {
  const std::uint16_t frames = GENERATE(0, 1, 31, 32, 33, 0x7FFF, 0xFFFF);
  CHECK(ai::IsEngagementReady(frames) == (frames >= 32));
}

TEST_CASE(
    "Temporary awareness counts down and clears the marker on its last frame",
    "[ai][engagement]") {
  FakeMarkers markers;
  ai::TemporaryAwareness awareness{.frames_remaining = 2, .marker = 7};
  CHECK(ai::UpdateTemporaryAwarenessIndicator(markers, 4, awareness));
  CHECK(awareness.frames_remaining == 1);
  CHECK(ai::UpdateTemporaryAwarenessIndicator(markers, 4, awareness));
  CHECK(awareness.frames_remaining == 0);
  CHECK(ai::UpdateTemporaryAwarenessIndicator(markers, 4, awareness));
  CHECK(awareness.frames_remaining == 0);
  CHECK(awareness.marker == 7);
  CHECK(markers.calls == MarkerCalls{{4, 7}, {4, 0}});
}

TEST_CASE(
    "Temporary awareness refuses IDs outside the table without counting down",
    "[ai][engagement]") {
  const int actor_id = GENERATE(-1, 16);
  FakeMarkers markers;
  ai::TemporaryAwareness awareness{.frames_remaining = 3, .marker = 7};
  CHECK_FALSE(
      ai::UpdateTemporaryAwarenessIndicator(markers, actor_id, awareness));
  CHECK(awareness.frames_remaining == 3);
  awareness.frames_remaining = 0;
  CHECK(ai::UpdateTemporaryAwarenessIndicator(markers, actor_id, awareness));
  CHECK(markers.calls.empty());
}

TEST_CASE("Temporary awareness counts down Ashley without showing a marker",
          "[ai][engagement]") {
  FakeMarkers markers;
  ai::TemporaryAwareness awareness{.frames_remaining = 2, .marker = 7};
  CHECK(ai::UpdateTemporaryAwarenessIndicator(markers, 0, awareness));
  CHECK(awareness.frames_remaining == 1);
  CHECK(markers.calls.empty());
}

TEST_CASE(
    "Disengagement by ID ignores Ashley and empty slots and refuses IDs "
    "outside the table",
    "[ai][engagement]") {
  FakeEngagementTable table;
  table.present[0] = table.present[3] = table.present[15] = true;
  CHECK(ai::DisengageActorById(table, 3) == std::optional<bool>{true});
  CHECK(ai::DisengageActorById(table, 15) == std::optional<bool>{true});
  CHECK(ai::DisengageActorById(table, 0) == std::optional<bool>{false});
  CHECK(ai::DisengageActorById(table, 5) == std::optional<bool>{false});
  CHECK_FALSE(ai::DisengageActorById(table, -1).has_value());
  CHECK_FALSE(ai::DisengageActorById(table, 16).has_value());
  CHECK(table.disengaged == std::vector<int>{3, 15});
}

TEST_CASE("Disengagement removes an engaged actor and starts its rejoin timers",
          "[ai][engagement]") {
  const bool single_opponent = GENERATE(false, true);
  FakeEngagementHost host;
  host.single_opponent = single_opponent;
  ai::EngagementTimers timers{.engaged_frames = 40, .grace_frames = 3};
  ai::EngagementRoster roster{.engaged_mask = 0x8028, .engaged_count = 3};
  CHECK(ai::DisengageActor(host, 15, timers, roster) ==
        std::optional<bool>{true});
  CHECK(roster.engaged_mask == 0x28);
  CHECK(roster.engaged_count == 2);
  CHECK(timers.engaged_frames == 0);
  CHECK(timers.cooldown_frames == (single_opponent ? 0 : 48));
  CHECK(timers.grace_frames == 90);
  CHECK(host.unlinked == 1);
  CHECK(host.markers == MarkerCalls{{15, 0}});
}

TEST_CASE("Disengagement leaves an actor outside the roster untouched",
          "[ai][engagement]") {
  FakeEngagementHost host;
  ai::EngagementTimers timers{.engaged_frames = 40};
  ai::EngagementRoster roster{.engaged_mask = 0x20, .engaged_count = 1};
  const auto saved_timers = timers;
  const auto saved_roster = roster;
  CHECK(ai::DisengageActor(host, 3, timers, roster) ==
        std::optional<bool>{false});
  CHECK(SameTimers(timers, saved_timers));
  CHECK(SameRoster(roster, saved_roster));
  CHECK(host.unlinked == 0);
  CHECK(host.markers.empty());
}

TEST_CASE(
    "Disengagement refuses bad IDs, an empty count and a missing actor list",
    "[ai][engagement]") {
  const int refusal = GENERATE(0, 1, 2, 3);
  FakeEngagementHost host;
  int actor_id = 3;
  ai::EngagementTimers timers{.engaged_frames = 40};
  ai::EngagementRoster roster{.engaged_mask = 0x8, .engaged_count = 1};
  if (refusal == 0) actor_id = -1;
  if (refusal == 1) actor_id = 16;
  if (refusal == 2) roster.engaged_count = 0;
  if (refusal == 3) host.single_opponent.reset();
  const auto saved_timers = timers;
  const auto saved_roster = roster;
  CHECK_FALSE(ai::DisengageActor(host, actor_id, timers, roster).has_value());
  CHECK(SameTimers(timers, saved_timers));
  CHECK(SameRoster(roster, saved_roster));
  CHECK(host.unlinked == 0);
  CHECK(host.markers.empty());
}

TEST_CASE("Engagement joins the roster with a fresh or grace warmup",
          "[ai][engagement]") {
  const std::uint16_t grace = GENERATE(0, 1, 90);
  FakeEngagementHost host;
  ai::EngagementTimers timers{.grace_frames = grace};
  ai::EngagementRoster roster{
      .engaged_mask = 0x1, .engaged_count = 1, .limit = 1};
  CHECK(ai::TryEngageActor(host, 15, timers, &roster) ==
        std::optional<bool>{true});
  CHECK(timers.engaged_frames == (grace != 0 ? 32 : 1));
  CHECK(timers.grace_frames == grace);
  CHECK(roster.engaged_mask == 0x8001);
  CHECK(roster.engaged_count == 2);
  CHECK(host.cleared == 1);
  CHECK(host.linked == 1);
}

TEST_CASE("Engagement reports an engaged actor without reading the roster",
          "[ai][engagement]") {
  FakeEngagementHost host;
  host.busy = true;
  ai::EngagementTimers timers{.engaged_frames = 5, .cooldown_frames = 9};
  CHECK(ai::TryEngageActor(host, 4, timers, nullptr) ==
        std::optional<bool>{true});
  CHECK(timers.engaged_frames == 5);
  CHECK(host.cleared == 0);
  CHECK(host.linked == 0);
}

TEST_CASE(
    "Engagement declines a full room, a cooldown or a busy update without "
    "mutation",
    "[ai][engagement]") {
  const int blocked = GENERATE(0, 1, 2);
  FakeEngagementHost host;
  ai::EngagementTimers timers{};
  ai::EngagementRoster roster{.engaged_count = 1, .limit = 1};
  if (blocked == 0) roster.engaged_count = 2;
  if (blocked == 1) timers.cooldown_frames = 1;
  if (blocked == 2) host.busy = true;
  const auto saved_timers = timers;
  const auto saved_roster = roster;
  CHECK(ai::TryEngageActor(host, 4, timers, &roster) ==
        std::optional<bool>{false});
  CHECK(SameTimers(timers, saved_timers));
  CHECK(SameRoster(roster, saved_roster));
  CHECK(host.cleared == 0);
  CHECK(host.linked == 0);
}

TEST_CASE(
    "Engagement refuses bad IDs, a missing roster and a count past the mask",
    "[ai][engagement]") {
  const int refusal = GENERATE(0, 1, 2, 3);
  FakeEngagementHost host;
  int actor_id = 4;
  ai::EngagementTimers timers{};
  ai::EngagementRoster roster{.engaged_count = 16, .limit = 255};
  ai::EngagementRoster* room = &roster;
  if (refusal == 0) actor_id = -1;
  if (refusal == 1) actor_id = 16;
  if (refusal == 2) room = nullptr;
  if (refusal != 3) roster.engaged_count = 0;
  const auto saved_roster = roster;
  CHECK_FALSE(ai::TryEngageActor(host, actor_id, timers, room).has_value());
  CHECK(timers.engaged_frames == 0);
  CHECK(SameRoster(roster, saved_roster));
  CHECK(host.cleared == 0);
  CHECK(host.linked == 0);
}
