#include <algorithm>
#include <array>
#include <type_traits>

#include <catch2/catch_test_macros.hpp>

#include "Legacy/Psx.h"

extern "C" {
#include "Actors/ActorState.h"
#include "Battle/Actor.h"
#include "Battle/Damage.h"
#include "Event/ScriptVm.h"
#include "Event/Triggers.h"
}

static_assert(std::is_same_v<decltype(&Actors_IsActorRetainedAfterDefeat),
                             char (*)(int)>);

namespace {
constexpr int kActorSlot = 2;
constexpr unsigned kTriggerOffset = 32;

struct RetentionPredicateFixture {
  Battle_Actor* saved_actor = Battle_actors[kActorSlot];
  decltype(Event_roomEventTriggers) saved_triggers = Event_roomEventTriggers;
  decltype(Event_playbackPhase) saved_phase = Event_playbackPhase;
  decltype(Event_eventId) saved_id = Event_eventId;
  decltype(Event_eventSource) saved_source = Event_eventSource;
  decltype(Event_isDirectEventRequest) saved_direct =
      Event_isDirectEventRequest;
  std::array<u_char, 64> triggers{};
  Battle_Actor actor{};
  Battle_ActorStats stats{};

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
    Event_roomEventTriggers = triggers.data();
    Event_playbackPhase = 0;
    Event_eventId = 0;
    Event_eventSource = 0;
    Event_isDirectEventRequest = 0;
    Battle_actors[kActorSlot] = nullptr;
    actor.stats = &stats;
    stats.maxHp = 100;
  }

  ~RetentionPredicateFixture() {
    Battle_actors[kActorSlot] = saved_actor;
    Event_roomEventTriggers = saved_triggers;
    Event_playbackPhase = saved_phase;
    Event_eventId = saved_id;
    Event_eventSource = saved_source;
    Event_isDirectEventRequest = saved_direct;
  }
};
}  // namespace

TEST_CASE("Absent actor cannot satisfy a retained-actor room trigger",
          "[events][retention-predicate]") {
  RetentionPredicateFixture fixture;
  CHECK(Actors_IsActorRetainedAfterDefeat(kActorSlot) == 0);
  CHECK(Event_EvaluateRoomEventTriggers(8, 0, nullptr, 0) == 0);
  CHECK(Event_playbackPhase == 0);
}

TEST_CASE("Room death predicates reject absent and nonretained actors",
          "[events][retention-predicate]") {
  RetentionPredicateFixture fixture(true);
  SECTION("No combat actor") {
    CHECK(Event_EvaluateRoomEventTriggers(8, 0, nullptr, 0) == 0);
  }
  SECTION("Existing actor without retention") {
    Battle_actors[kActorSlot] = &fixture.actor;
    CHECK(Event_EvaluateRoomEventTriggers(8, 0, nullptr, 0) == 0);
  }
  CHECK(Event_playbackPhase == 0);
}

TEST_CASE("Room death predicates distinguish retained dead and living actors",
          "[events][retention-predicate]") {
  RetentionPredicateFixture fixture(true);
  fixture.actor.retainAfterDefeat = 1;
  Battle_actors[kActorSlot] = &fixture.actor;
  SECTION("Retained dead actor activates the original local event") {
    CHECK(Event_EvaluateRoomEventTriggers(8, 0, nullptr, 0) == 1);
    CHECK(Event_playbackPhase == 1);
    CHECK(Event_eventSource == 2);
    CHECK(Event_eventId == 0);
  }
  SECTION("A living one-percent actor is not dead") {
    fixture.stats.currentHp = 1;
    fixture.stats.maxHp = 1000;
    CHECK(Event_EvaluateRoomEventTriggers(8, 0, nullptr, 0) == 0);
    CHECK(Event_playbackPhase == 0);
  }
}
