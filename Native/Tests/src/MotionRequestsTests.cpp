#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Actors/MotionRequests.h"

extern "C" {
#include "Actors/ActorRecord.h"
#include "Actors/Movement.h"
#include "Battle/Actor.h"
}

namespace {
static_assert(sizeof(SVECTOR) == 2 * sizeof(int));
static_assert(offsetof(Battle_Actor, commandArg1) ==
              offsetof(Battle_Actor, commandArg0) + sizeof(int));

std::array<int, 8> TransitionState(const Actors_ActorRecord& actor) {
  const auto& route = actor.routeTarget;
  const auto& instance = actor.instance;
  return {route.vx,
          route.vy,
          route.vz,
          route.pad,
          instance.reactionState,
          instance.poseState,
          instance.wasDrawn,
          instance.turnFrames};
}

struct MotionFixture {
  Actors_ActorRecord* saved = Actors_actorRecords[2];
  Actors_ActorRecord actor{};

  MotionFixture() {
    Actors_actorRecords[2] = &actor;
    actor.instance.facing = 0xF00;
    actor.turnRate = 17;
    actor.animationId = 0x76;
  }

  ~MotionFixture() { Actors_actorRecords[2] = saved; }
};

struct LinkedMotionFixture : MotionFixture {
  Actors_ActorRecord* saved_linked = Actors_actorRecords[3];
  Actors_ActorRecord linked{};

  LinkedMotionFixture() { Actors_actorRecords[3] = &linked; }
  ~LinkedMotionFixture() { Actors_actorRecords[3] = saved_linked; }
};
}  // namespace

TEST_CASE("Ground offset requests preserve vectors, flags and unrelated fields",
          "[actors][motion-request]") {
  MotionFixture fixture;
  auto& request = fixture.actor.turnState;
  std::memset(&request, 0x55, sizeof(request));
  fixture.actor.instance.motionLocked = 1;
  const auto original = request;
  SVECTOR step{-120, 31, 124, 0x4567};
  Actors_QueueGroundOffset(2, &step);
  std::uint32_t mode_bytes;
  std::memcpy(&mode_bytes, &request, sizeof(mode_bytes));
  CHECK(mode_bytes == 2);
  CHECK(std::memcmp(&request.step, &step, sizeof(step)) == 0);
  CHECK((std::array<int, 4>{request.direction, request.facing,
                            static_cast<int>(request.distance),
                            request.reservedC} ==
         std::array<int, 4>{original.direction, original.facing,
                            static_cast<int>(original.distance),
                            original.reservedC}));
  CHECK(fixture.actor.instance.motionLocked == 1);
}

TEST_CASE("Flight and vector requests retain packed facing and complete step",
          "[actors][motion-request]") {
  const bool flight = GENERATE(false, true);
  MotionFixture fixture;
  fixture.actor.turnState.direction = 19;
  SVECTOR step{-8, -16, 127, -1};
  if (flight) {
    Actors_QueueFlightMovement(2, &step, -10, 0x10003);
  } else {
    Actors_QueueMovementVector(2, &step, 0x10003);
  }
  CHECK(fixture.actor.turnState.mode == (flight ? 3 : 4));
  CHECK(fixture.actor.turnState.facing == 3);
  CHECK(fixture.actor.turnState.direction == 19);
  CHECK(fixture.actor.turnState.distance ==
        (flight ? static_cast<unsigned>(-10) : 0x10003u));
  CHECK(std::memcmp(&fixture.actor.turnState.step, &step, sizeof(step)) == 0);
  CHECK(fixture.actor.instance.motionLocked == 1);
}

TEST_CASE("Queued movement accepts input aliased with native request storage",
          "[actors][motion-request]") {
  MotionFixture fixture;
  fixture.actor.turnState.step = {-16, 17, -18, 19};
  Actors_QueueFlightMovement(2, &fixture.actor.turnState.step, 77, -200);
  CHECK(fixture.actor.turnState.step.vx == -16);
  CHECK(fixture.actor.turnState.step.vy == 17);
  CHECK(fixture.actor.turnState.step.vz == -18);
  CHECK(fixture.actor.turnState.step.pad == 19);
  CHECK(fixture.actor.turnState.facing == -200);
}

TEST_CASE("Navigation turns use the original radius threshold and shortest yaw",
          "[actors][motion-request]") {
  const unsigned radius = GENERATE(0u, 127u, 128u, 65535u);
  MotionFixture fixture;
  fixture.actor.collisionRadius = static_cast<std::uint16_t>(radius);
  Actors_TurnNavigationActor(2, 0x100);
  CHECK(fixture.actor.instance.facingDelta == 512);
  CHECK(fixture.actor.instance.turnFrames == (radius < 128 ? 4 : 16));
  CHECK(fixture.actor.turnRate == 17);
  CHECK(fixture.actor.turnState.mode == 6);
}

TEST_CASE("Speed turns use the existing angular-speed consumer and animation",
          "[actors][motion-request]") {
  const int speed = GENERATE(128, -128, 200);
  MotionFixture fixture;
  Actors_TurnActorAtSpeed(2, 0x100, speed);
  CHECK(fixture.actor.instance.facingDelta == 512);
  CHECK(fixture.actor.instance.turnFrames == (speed == 200 ? 3 : 4));
  CHECK(fixture.actor.turnRate == 17);
  CHECK(fixture.actor.turnState.mode == 6);
}

TEST_CASE(
    "Teleport requests admit only idle reactions and clear the whole state "
    "byte",
    "[actors][motion-request][transition-request]") {
  const int reaction = GENERATE(0, 1, 7, 15);
  MotionFixture fixture;
  auto& actor = fixture.actor;
  actor.routeTarget = {10, 11, 12, 13};
  actor.instance.reactionState = static_cast<u_char>(reaction);
  actor.instance.poseState = 7;
  actor.instance.wasDrawn = 1;
  actor.instance.turnFrames = 44;
  const auto before = TransitionState(actor);
  std::array<int, 2> target{0x4321FFFE, 0x7ABCDEFF};
  const auto original = target;

  Actors_BeginTeleport(2, target.data(), 0x18001);

  const std::array<int, 8> expected{-2, 0x4321, 0, -32767, 1, 0, 0, 0};
  CHECK(TransitionState(actor) == (reaction == 0 ? expected : before));
  CHECK(target == original);
  CHECK(actor.instance.facing == 0xF00);
  CHECK(actor.animationId == 0x76);
}

TEST_CASE("Teleport companion propagation respects busy state and cyclic links",
          "[actors][motion-request][transition-request]") {
  const bool linked_busy = GENERATE(false, true);
  const int linked_slot = GENERATE(0, 1, 2, 3);
  LinkedMotionFixture fixture;
  fixture.actor.companionSlot = static_cast<u_char>(linked_slot);
  fixture.linked.companionSlot = 2;
  fixture.linked.routeTarget = {21, 22, 23, 24};
  fixture.linked.instance.reactionState = linked_busy ? 3 : 0;
  fixture.linked.instance.poseState = 7;
  fixture.linked.instance.wasDrawn = 1;
  fixture.linked.instance.turnFrames = 99;
  const auto before = TransitionState(fixture.linked);
  std::array<int, 2> target{0x0009002B, 0x77777777};

  Actors_BeginTeleport(2, target.data(), -1);

  const std::array<int, 8> expected{43, 9, 0, -1, 1, 0, 0, 0};
  CHECK(TransitionState(fixture.actor) == expected);
  CHECK(TransitionState(fixture.linked) ==
        (linked_slot == 3 && !linked_busy ? expected : before));
}

TEST_CASE("A busy teleport parent never starts its idle companion",
          "[actors][motion-request][transition-request]") {
  LinkedMotionFixture fixture;
  fixture.actor.companionSlot = 3;
  fixture.actor.instance.reactionState = 1;
  fixture.actor.routeTarget = {1, 2, 3, 4};
  const auto parent = TransitionState(fixture.actor);
  const auto linked = TransitionState(fixture.linked);
  std::array<int, 2> target{0x0009002B, 0};
  Actors_BeginTeleport(2, target.data(), -1);
  CHECK(TransitionState(fixture.actor) == parent);
  CHECK(TransitionState(fixture.linked) == linked);
}

TEST_CASE(
    "Travel request modes preserve reaction and drawn bits and both target "
    "words",
    "[actors][motion-request][transition-request]") {
  const bool timed = GENERATE(false, true);
  const int reaction = GENERATE(0, 9, 15);
  const bool drawn = GENERATE(false, true);
  MotionFixture fixture;
  auto& actor = fixture.actor;
  actor.instance.reactionState = static_cast<u_char>(reaction);
  actor.instance.wasDrawn = drawn;
  actor.instance.poseState = 7;
  actor.instance.turnFrames = 71;
  const SVECTOR position{-300, 32767, -32768, 0x1234};
  std::array<int, 2> target{};
  std::memcpy(target.data(), &position, sizeof(position));
  const auto original = target;

  if (timed) {
    Actors_BeginTimedTravel(2, target.data(), 0x1ABCD);
  } else {
    Actors_BeginTileTravel(2, target.data(), 0x1ABCD);
  }

  const std::array<int, 8> expected{
      -300, 32767, -32768, -21555, reaction, timed ? 4 : 1, drawn ? 1 : 0, 71};
  CHECK(TransitionState(actor) == expected);
  CHECK(target == original);
  CHECK(actor.instance.facing == 0xF00);
  CHECK(actor.animationId == 0x76);
}

TEST_CASE("Travel captures aliased target storage before replacing facing",
          "[actors][motion-request][transition-request]") {
  const bool timed = GENERATE(false, true);
  MotionFixture fixture;
  fixture.actor.routeTarget = {-120, 121, -122, 123};
  auto* target = reinterpret_cast<int*>(&fixture.actor.routeTarget);
  if (timed) {
    Actors_BeginTimedTravel(2, target, 0x1FFFF);
  } else {
    Actors_BeginTileTravel(2, target, 0x1FFFF);
  }
  const std::array<int, 8> expected{-120, 121,           -122, -1,
                                    0,    timed ? 4 : 1, 0,    0};
  CHECK(TransitionState(fixture.actor) == expected);
}

TEST_CASE("Teleport captures an aliased tile before clearing its route tail",
          "[actors][motion-request][transition-request]") {
  MotionFixture fixture;
  fixture.actor.routeTarget = {-120, 121, -122, 123};
  Actors_BeginTeleport(2, reinterpret_cast<int*>(&fixture.actor.routeTarget),
                       0x1FFFF);
  const std::array<int, 8> expected{-120, 121, 0, -1, 1, 0, 0, 0};
  CHECK(TransitionState(fixture.actor) == expected);
}
