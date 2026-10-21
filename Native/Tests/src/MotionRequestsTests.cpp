#include <array>
#include <cstdint>
#include <cstring>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Actors/MotionRequests.h"

extern "C" {
#include "Actors/ActorRecord.h"
#include "Actors/Movement.h"
}

namespace {
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
