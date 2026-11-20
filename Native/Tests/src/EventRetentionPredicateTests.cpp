#include <algorithm>
#include <array>
#include <type_traits>

#include <catch2/catch_test_macros.hpp>

#include "Actors/ActorStateLive.hpp"
#include "Battle/Actor.hpp"
#include "Battle/Damage.hpp"
#include "Battle/Encounter.hpp"
#include "Event/RoomTriggerEntryLive.hpp"
#include "Event/ScriptVmLive.hpp"
#include "Event/ScriptVmLiveState.hpp"
#include "Event/Triggers.hpp"
#include "Legacy/Psx.h"

static_assert(
    std::is_same_v<decltype(&VS::Actors::IsLiveActorRetainedAfterDefeat),
                   char (*)(int)>);

namespace {
constexpr int kActorSlot = 2;
constexpr unsigned kTriggerOffset = 32;

struct RetentionPredicateFixture {
  VS::Battle::Actor* saved_actor = VS::Battle::actors[kActorSlot];
  VS::Event::RoomEventLease saved_room = VS::Event::LiveRoomScriptSections();
  decltype(VS::Event::playbackPhase) saved_phase = VS::Event::playbackPhase;
  decltype(VS::Event::eventId) saved_id = VS::Event::eventId;
  decltype(VS::Event::eventSource) saved_source = VS::Event::eventSource;
  decltype(VS::Event::isDirectEventRequest) saved_direct =
      VS::Event::isDirectEventRequest;
  std::array<u_char, 64> triggers{};
  VS::Battle::Actor actor{};
  VS::Battle::ActorStats stats{};

  explicit RetentionPredicateFixture(bool test_health = false) {
    triggers.fill(0xFF);
    triggers[0] = kTriggerOffset;
    triggers[1] = 0;
    const std::array<u_char, 7> retained{8,          0,    0x80, 16,
                                         kActorSlot, 0x10, 0xFF};
    const std::array<u_char, 13> retained_dead{
        8, 0,          0x80, 16, kActorSlot, 0x10, 0xFE,
        7, kActorSlot, 0x10, 0,  0,          0xFF};
    if (test_health) {
      std::copy(retained_dead.begin(), retained_dead.end(),
                triggers.begin() + kTriggerOffset);
    } else {
      std::copy(retained.begin(), retained.end(),
                triggers.begin() + kTriggerOffset);
    }
    const auto room =
        VS::Event::RoomEventSections::Copy({.triggers = triggers});
    REQUIRE(room);
    VS::Event::PublishLiveRoomScriptSections(*room);
    VS::Event::playbackPhase = 0;
    VS::Event::eventId = 0;
    VS::Event::eventSource = 0;
    VS::Event::isDirectEventRequest = 0;
    VS::Battle::actors[kActorSlot] = nullptr;
    actor.stats = &stats;
    stats.max_hp = 100;
  }

  ~RetentionPredicateFixture() {
    VS::Event::PublishLiveRoomScriptSections(saved_room);
    VS::Battle::actors[kActorSlot] = saved_actor;
    VS::Event::playbackPhase = saved_phase;
    VS::Event::eventId = saved_id;
    VS::Event::eventSource = saved_source;
    VS::Event::isDirectEventRequest = saved_direct;
  }
};
}  // namespace

TEST_CASE("Absent actor cannot satisfy a retained-actor room trigger",
          "[events][retention-predicate]") {
  RetentionPredicateFixture fixture;
  CHECK(VS::Actors::IsLiveActorRetainedAfterDefeat(kActorSlot) == 0);
  CHECK(VS::Event::EvaluateRoomEventTriggersEntry(8, 0, nullptr, 0) == 0);
  CHECK(VS::Event::playbackPhase == 0);
}

TEST_CASE("Room death predicates reject absent and nonretained actors",
          "[events][retention-predicate]") {
  RetentionPredicateFixture fixture(true);
  SECTION("No combat actor") {
    CHECK(VS::Event::EvaluateRoomEventTriggersEntry(8, 0, nullptr, 0) == 0);
  }
  SECTION("Existing actor without retention") {
    VS::Battle::actors[kActorSlot] = &fixture.actor;
    CHECK(VS::Event::EvaluateRoomEventTriggersEntry(8, 0, nullptr, 0) == 0);
  }
  CHECK(VS::Event::playbackPhase == 0);
}

TEST_CASE("Room death predicates distinguish retained dead and living actors",
          "[events][retention-predicate]") {
  RetentionPredicateFixture fixture(true);
  fixture.actor.retain_after_defeat = 1;
  VS::Battle::actors[kActorSlot] = &fixture.actor;
  SECTION("Retained dead actor activates the original local event") {
    CHECK(VS::Event::EvaluateRoomEventTriggersEntry(8, 0, nullptr, 0) == 1);
    CHECK(VS::Event::playbackPhase == 1);
    CHECK(VS::Event::eventSource == 2);
    CHECK(VS::Event::eventId == 0);
  }
  SECTION("A living one-percent actor is not dead") {
    fixture.stats.current_hp = 1;
    fixture.stats.max_hp = 1000;
    CHECK(VS::Event::EvaluateRoomEventTriggersEntry(8, 0, nullptr, 0) == 0);
    CHECK(VS::Event::playbackPhase == 0);
  }
}

TEST_CASE("Room-entry scenes keep the event id; Lab entry restores boot value",
          "[events][retention-predicate][lab-reentry]") {
  RetentionPredicateFixture fixture;
  const auto saved_entry = VS::Event::hasRoomEntryScript;
  VS::Event::eventId = 8;
  VS::Event::hasRoomEntryScript = 1;
  CHECK(VS::Event::EvaluateRoomEventTriggersEntry(1, 0, nullptr, 0) == 1);
  CHECK(VS::Event::eventSource == 1);
  CHECK(VS::Event::eventId == 8);
  VS::Game::RestoreBootEventSelector();
  CHECK(VS::Event::eventId == 0);
  CHECK(VS::Event::eventSource == 1);
  CHECK(VS::Event::playbackPhase == 1);
  VS::Event::hasRoomEntryScript = saved_entry;
}
