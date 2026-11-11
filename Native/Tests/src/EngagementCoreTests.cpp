#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
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

namespace {
using Poses = std::vector<std::pair<int, unsigned>>;
constexpr unsigned kAshleyAlly = 2;

std::size_t Slot(int actor_id) { return static_cast<std::size_t>(actor_id); }

class FakeEngagedList final : public ai::EngagedList {
 public:
  [[nodiscard]] std::optional<int> Head() const override { return head; }
  void SetHead(std::optional<int> actor_id) override { head = actor_id; }
  [[nodiscard]] std::optional<int> Tail() const override { return tail; }
  void SetTail(std::optional<int> actor_id) override { tail = actor_id; }
  [[nodiscard]] std::optional<int> Next(int actor_id) const override {
    return next.at(Slot(actor_id));
  }
  void SetNext(int actor_id, std::optional<int> successor) override {
    next.at(Slot(actor_id)) = successor;
  }
  [[nodiscard]] ai::EngagementPhase Phase(int actor_id) const override {
    return phases.at(Slot(actor_id));
  }
  void SetPhase(int actor_id, ai::EngagementPhase phase) override {
    phases.at(Slot(actor_id)) = phase;
  }
  void SetPose(int actor_id, unsigned pose) override {
    poses.emplace_back(actor_id, pose);
  }
  [[nodiscard]] unsigned AshleyClassification(int actor_id) const override {
    return ashley.at(Slot(actor_id));
  }

  // Links `ids` in order, with the tail on the last member.
  void Link(std::initializer_list<int> ids) {
    std::optional<int> previous;
    for (const int id : ids) {
      if (previous.has_value()) {
        next.at(Slot(*previous)) = id;
      } else {
        head = id;
      }
      previous = id;
    }
    tail = previous;
  }

  [[nodiscard]] std::vector<int> Order() const {
    std::vector<int> order;
    for (std::optional<int> node = head;
         node.has_value() && order.size() <= next.size();
         node = next.at(Slot(*node))) {
      order.push_back(*node);
    }
    return order;
  }

  std::optional<int> head;
  std::optional<int> tail;
  std::array<std::optional<int>, ai::kEngagementActorSlots> next{};
  std::array<ai::EngagementPhase, ai::kEngagementActorSlots> phases{};
  std::array<unsigned, ai::kEngagementActorSlots> ashley{};
  Poses poses;
};

bool SamePhase(const ai::EngagementPhase& a, const ai::EngagementPhase& b) {
  return a.phase == b.phase && a.step == b.step;
}

class FakeBuildupHost final : public ai::EngagementBuildupHost {
 public:
  void SetMarker(int actor_id, int marker) override {
    markers.emplace_back(actor_id, marker);
  }
  [[nodiscard]] bool CandidateEngagedThisUpdate() const override {
    return false;
  }
  void ClearEngagementRequest() override {}
  void LinkEngagedActor() override {}
  void UnlinkEngagedActor() override { ++unlinked; }
  [[nodiscard]] std::optional<bool> HasSingleOpponent() const override {
    return false;
  }
  void RememberActionTarget() override { ++remembered; }

  int unlinked = 0;
  int remembered = 0;
  MarkerCalls markers;
};
}  // namespace

TEST_CASE("Engaged-list removal unlinks any member and moves its cursor on",
          "[ai][engagement]") {
  const auto [leaving, head, tail] =
      GENERATE(table<int, int, int>({{3, 5, 5}, {5, 3, 7}, {7, 3, 3}}));
  FakeEngagedList list;
  list.Link({3, 5, 7});
  list.tail = leaving;
  list.phases.at(Slot(leaving)) = {.phase = 9, .step = -1};
  CHECK(ai::UnlinkEngagedActor(list, leaving));
  std::vector<int> remaining{3, 5, 7};
  std::erase(remaining, leaving);
  CHECK(list.Order() == remaining);
  CHECK(list.head == head);
  CHECK(list.tail == tail);
  CHECK(SamePhase(list.phases.at(Slot(leaving)), {.phase = 0, .step = -1}));
  CHECK(list.poses == Poses{{leaving, ai::kEngagementPoseFalling}});
}

TEST_CASE(
    "Engaged-list removal keeps another member's cursor and clears an emptied "
    "list",
    "[ai][engagement]") {
  FakeEngagedList list;
  list.Link({3, 5, 7});
  CHECK(ai::UnlinkEngagedActor(list, 5));
  CHECK(list.Order() == std::vector<int>{3, 7});
  CHECK(list.tail == 7);

  FakeEngagedList single;
  single.Link({4});
  CHECK(ai::UnlinkEngagedActor(single, 4));
  CHECK_FALSE(single.head.has_value());
  CHECK_FALSE(single.tail.has_value());
  CHECK(single.poses == Poses{{4, ai::kEngagementPoseFalling}});
}

TEST_CASE(
    "Engaged-list removal of an absent actor changes only its phase and pose",
    "[ai][engagement]") {
  FakeEngagedList list;
  list.Link({3, 5});
  // A stale successor outside the slots would be refused if it were read.
  list.next.at(9) = 99;
  list.phases.at(9) = {.phase = 6, .step = 1};
  CHECK(ai::UnlinkEngagedActor(list, 9));
  CHECK(list.Order() == std::vector<int>{3, 5});
  CHECK(list.tail == 5);
  CHECK(SamePhase(list.phases.at(9), {.phase = 0, .step = 1}));
  CHECK(list.poses == Poses{{9, ai::kEngagementPoseFalling}});

  FakeEngagedList empty;
  CHECK(ai::UnlinkEngagedActor(empty, 9));
  CHECK_FALSE(empty.tail.has_value());
}

TEST_CASE(
    "Engaged-list removal refuses bad IDs, a revisiting list and a bad "
    "successor without changes",
    "[ai][engagement]") {
  const int refusal = GENERATE(0, 1, 2, 3);
  FakeEngagedList list;
  int leaving = 3;
  list.Link({3, 5});
  if (refusal == 0) leaving = -1;
  if (refusal == 1) leaving = 16;
  if (refusal == 2) {
    list.next.at(5) = 3;
    leaving = 7;
  }
  if (refusal == 3) list.next.at(3) = 16;
  const auto saved = list.next;
  list.phases.fill({.phase = 4, .step = 1});
  CHECK_FALSE(ai::UnlinkEngagedActor(list, leaving));
  CHECK(list.head == 3);
  CHECK(list.tail == 5);
  CHECK(list.next == saved);
  CHECK(SamePhase(list.phases.at(3), {.phase = 4, .step = 1}));
  CHECK(list.poses.empty());
}

TEST_CASE("Engagement buildup shows a marker step every eight frames",
          "[ai][engagement]") {
  const auto [before, marker] = GENERATE(table<std::uint16_t, int>(
      {{1, 1}, {6, 1}, {7, 2}, {15, 3}, {23, 4}, {119, 4}}));
  FakeBuildupHost host;
  ai::EngagementTimers timers{
      .engaged_frames = before, .cooldown_frames = 5, .grace_frames = 3};
  ai::EngagementRoster roster{.engaged_mask = 0x40, .engaged_count = 1};
  CHECK(ai::UpdateEngagementBuildup(host, 6, timers, &roster));
  CHECK(timers.engaged_frames == before + 1);
  CHECK(timers.cooldown_frames == 5);
  CHECK(timers.grace_frames == 3);
  CHECK(host.markers == MarkerCalls{{6, marker}});
  CHECK(host.remembered == 0);
  CHECK(roster.engaged_count == 1);
}

TEST_CASE(
    "Engagement buildup gives up after 120 frames and starts the rejoin "
    "countdown",
    "[ai][engagement]") {
  FakeBuildupHost host;
  ai::EngagementTimers timers{.engaged_frames = 120};
  ai::EngagementRoster roster{.engaged_mask = 0x41, .engaged_count = 2};
  CHECK(ai::UpdateEngagementBuildup(host, 6, timers, &roster));
  CHECK(host.remembered == 1);
  CHECK(host.unlinked == 1);
  CHECK(roster.engaged_mask == 0x01);
  CHECK(roster.engaged_count == 1);
  CHECK(timers.engaged_frames == 0);
  CHECK(timers.grace_frames == ai::kReengagementGraceFrames - 1);
  CHECK(timers.cooldown_frames == ai::kEngagementLeaveCooldownFrames - 1);
  CHECK(host.markers == MarkerCalls{{6, 0}, {6, ai::kGraceAwarenessMarker}});
}

TEST_CASE(
    "Engagement buildup wraps without leaving and counts the rejoin timers "
    "down",
    "[ai][engagement]") {
  const std::uint16_t engaged = GENERATE(0, 0xFFFF);
  FakeBuildupHost host;
  ai::EngagementTimers timers{
      .engaged_frames = engaged, .cooldown_frames = 2, .grace_frames = 2};
  CHECK(ai::UpdateEngagementBuildup(host, 6, timers, nullptr));
  CHECK(timers.engaged_frames == 0);
  CHECK(timers.cooldown_frames == 1);
  CHECK(timers.grace_frames == 1);
  CHECK(ai::UpdateEngagementBuildup(host, 6, timers, nullptr));
  CHECK(timers.cooldown_frames == 0);
  CHECK(host.markers == MarkerCalls{{6, ai::kGraceAwarenessMarker}, {6, 0}});
  CHECK(host.remembered == 0);
}

TEST_CASE(
    "Engagement buildup keeps counting an actor missing from the engaged mask "
    "and refuses a leave without a roster",
    "[ai][engagement]") {
  FakeBuildupHost host;
  ai::EngagementTimers timers{.engaged_frames = 130};
  ai::EngagementRoster roster{.engaged_mask = 0x1, .engaged_count = 1};
  CHECK(ai::UpdateEngagementBuildup(host, 6, timers, &roster));
  CHECK(timers.engaged_frames == 131);
  CHECK(host.remembered == 1);
  CHECK(roster.engaged_count == 1);
  CHECK_FALSE(ai::UpdateEngagementBuildup(host, 6, timers, nullptr));
  CHECK(timers.engaged_frames == 131);
  CHECK(host.remembered == 1);
  CHECK_FALSE(ai::UpdateEngagementBuildup(host, 16, timers, &roster));
}

TEST_CASE(
    "Engagement phase steps members that are not Ashley's allies until one "
    "turns",
    "[ai][engagement]") {
  FakeEngagedList list;
  list.Link({3, 5, 7});
  list.ashley.at(3) = kAshleyAlly;
  list.phases.at(3) = {.phase = 0, .step = 1};
  list.phases.at(5) = {.phase = 0, .step = -1};
  list.phases.at(7) = {.phase = 8, .step = 1};
  CHECK(ai::AnimateEngagementPhase(list));
  CHECK(SamePhase(list.phases.at(3), {.phase = 0, .step = 1}));
  CHECK(SamePhase(list.phases.at(5), {.phase = 1, .step = 1}));
  CHECK(SamePhase(list.phases.at(7), {.phase = 8, .step = 1}));
  CHECK(list.poses == Poses{{5, ai::kEngagementPoseRising}});
}

TEST_CASE("Engagement phase turns back at sixteen and wraps as a signed byte",
          "[ai][engagement]") {
  FakeEngagedList list;
  list.Link({3, 5, 7});
  list.phases.at(3) = {.phase = 127, .step = 1};
  list.phases.at(5) = {.phase = 3, .step = -1};
  list.phases.at(7) = {.phase = 16, .step = 1};
  CHECK(ai::AnimateEngagementPhase(list));
  CHECK(SamePhase(list.phases.at(3), {.phase = -128, .step = 1}));
  CHECK(SamePhase(list.phases.at(5), {.phase = 2, .step = -1}));
  CHECK(SamePhase(list.phases.at(7), {.phase = 15, .step = -1}));
  CHECK(list.poses == Poses{{7, ai::kEngagementPoseFalling}});
}

TEST_CASE(
    "Engagement phase refuses a revisiting or out-of-slot list without "
    "changes",
    "[ai][engagement]") {
  const bool revisits = GENERATE(false, true);
  FakeEngagedList list;
  list.Link({3, 5});
  list.next.at(5) = revisits ? 3 : 16;
  list.phases.fill({.phase = 4, .step = 1});
  CHECK_FALSE(ai::AnimateEngagementPhase(list));
  CHECK(SamePhase(list.phases.at(3), {.phase = 4, .step = 1}));
  CHECK(SamePhase(list.phases.at(5), {.phase = 4, .step = 1}));
  CHECK(list.poses.empty());

  FakeEngagedList empty;
  CHECK(ai::AnimateEngagementPhase(empty));
}
