#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Actors/MotionRequests.hpp"

namespace {
namespace Actors = VS::Actors;
using Actors::JumpEntry;
using Actors::MotionVector;
using Actors::PackedTile;

struct JumpCall {
  JumpEntry entry;
  PackedTile target;
  int speed;
  std::int16_t facing;
};

class FakeMotionActor final : public Actors::MotionActor {
 public:
  bool jumping = false;
  bool locked = false;
  int profile = 0;
  std::vector<std::string> events;
  std::vector<JumpCall> jumps;
  bool reacting = false;
  std::optional<MotionVector> route;
  std::optional<Actors::TravelPose> pose;
  int teleports = 0;
  FakeMotionActor* companion = nullptr;
  std::optional<MotionVector> ground_step;
  std::optional<Actors::QueuedMovement> queued;
  unsigned radius = 0;
  int turn_rate = 0;
  int angular_speed = 0;
  std::int16_t turn_facing = 0;
  std::optional<int> turn_animation;

  [[nodiscard]] bool Jumping() const override { return jumping; }
  [[nodiscard]] bool MotionLocked() const override { return locked; }
  void SetJumpFlags(bool motion_locked, bool airborne) override {
    locked = motion_locked;
    jumping = airborne;
    events.emplace_back("flags");
  }
  void SetJumpProfile(int value) override {
    profile = value;
    events.emplace_back("profile");
  }
  void PlayAnimation(int animation, int blend_frames) override {
    events.push_back("animation " + std::to_string(animation) + " " +
                     std::to_string(blend_frames));
  }
  void JumpToTile(JumpEntry entry, PackedTile target, int speed,
                  std::int16_t facing) override {
    jumps.push_back({entry, target, speed, facing});
    events.emplace_back("jump");
  }
  [[nodiscard]] bool Reacting() const override { return reacting; }
  void StartTeleport(const MotionVector& value) override {
    route = value;
    reacting = true;
    ++teleports;
  }
  [[nodiscard]] Actors::MotionActor* TeleportCompanion() override {
    return companion;
  }
  void StartTravel(const MotionVector& value,
                   Actors::TravelPose travel) override {
    route = value;
    pose = travel;
  }
  void QueueGroundOffset(const MotionVector& step) override {
    ground_step = step;
  }
  void QueueMovement(const Actors::QueuedMovement& movement) override {
    queued = movement;
  }
  [[nodiscard]] unsigned CollisionRadius() const override { return radius; }
  void TurnAtRate(std::int16_t facing, int rate) override {
    turn_facing = facing;
    turn_rate = rate;
    events.emplace_back("turn");
  }
  void TurnAtAngularSpeed(std::int16_t facing, int speed) override {
    turn_facing = facing;
    angular_speed = speed;
    events.emplace_back("turn");
  }
  void PlayTurnAnimation(int facing) override {
    turn_animation = facing;
    events.emplace_back("turn animation");
  }
};

constexpr PackedTile kTile{.x = 9, .layer = 1, .z = 11, .spare = 227};
}  // namespace

TEST_CASE("Facings narrow to their low halfword as the original stores them",
          "[actors][motion-request]") {
  CHECK(Actors::NarrowFacing(0x18001) == -32767);
  CHECK(Actors::NarrowFacing(-1) == -1);
  CHECK(Actors::NarrowFacing(0x10003) == 3);
  CHECK(Actors::NarrowFacing(0x7FFF) == 0x7FFF);
}

TEST_CASE("Grounded snapshots keep the saved anchor while on a support",
          "[actors][motion-request]") {
  Actors::GroundedPose pose{.tile = kTile,
                            .position = {-4, 7, 300},
                            .facing = -2048,
                            .support_layer = 0};
  auto history = Actors::SaveActorGroundedTransform(pose);
  CHECK(history.previous_tile == kTile);
  CHECK(history.position == pose.position);
  CHECK(history.previous_facing == -2048);
  CHECK(history.saved_placement == kTile);

  pose.support_layer = 3;
  history = Actors::SaveActorGroundedTransform(pose);
  CHECK(history.previous_tile == kTile);
  CHECK_FALSE(history.saved_placement.has_value());
}

namespace {
void CheckAirborneJump(const FakeMotionActor& actor) {
  CHECK(actor.jumps.at(0).entry == JumpEntry::kAirborne);
  CHECK_FALSE(actor.locked);
  CHECK(actor.jumping);
  CHECK(actor.profile == 0);
}

void CheckNavigationJump(const FakeMotionActor& actor) {
  CHECK(actor.jumps.at(0).entry == JumpEntry::kNavigation);
  CHECK(actor.locked);
  CHECK(actor.jumping);
  CHECK(actor.events ==
        std::vector<std::string>{"flags", "animation 31 8", "jump"});
}

void CheckPlainJump(const FakeMotionActor& actor) {
  CHECK(actor.jumps.at(0).entry == JumpEntry::kPlain);
  CHECK(actor.profile == Actors::kPlainJumpProfile);
  CHECK(actor.events == std::vector<std::string>{"profile", "jump"});
  CHECK_FALSE(actor.jumping);
}
}  // namespace

TEST_CASE("Tile jumps prefer the airborne entry, then navigation, then plain",
          "[actors][motion-request]") {
  const bool jumping = GENERATE(false, true);
  const bool locked = GENERATE(false, true);
  FakeMotionActor actor;
  actor.jumping = jumping;
  actor.locked = locked;

  Actors::BeginTileJump(actor, kTile, 40, 0x18001);

  REQUIRE(actor.jumps.size() == 1);
  CHECK(actor.jumps[0].target == kTile);
  CHECK(actor.jumps[0].speed == 40);
  CHECK(actor.jumps[0].facing == -32767);
  if (jumping) {
    CheckAirborneJump(actor);
  } else if (locked) {
    CheckNavigationJump(actor);
  } else {
    CheckPlainJump(actor);
  }
}

TEST_CASE("Airborne jumps unlock motion and never play the navigation clip",
          "[actors][motion-request]") {
  FakeMotionActor actor;
  actor.locked = true;
  Actors::BeginAirborneTileJump(actor, kTile, 18, 0);
  CHECK_FALSE(actor.locked);
  CHECK(actor.jumping);
  CHECK(actor.events == std::vector<std::string>{"flags", "jump"});
  CHECK(actor.jumps.at(0).entry == JumpEntry::kAirborne);
}

TEST_CASE("Teleports clear the route depth and carry the narrowed facing",
          "[actors][motion-request]") {
  FakeMotionActor actor;
  Actors::BeginTeleport(actor, kTile, 0x18001);
  REQUIRE(actor.route.has_value());
  const auto route = actor.route.value_or(MotionVector{});
  CHECK(route.x == 0x0109);
  CHECK(route.y == static_cast<std::int16_t>(0xE30B));
  CHECK(route.z == 0);
  CHECK(route.w == -32767);
  CHECK(actor.teleports == 1);
}

TEST_CASE("Teleports start idle companions and stop at the first busy actor",
          "[actors][motion-request]") {
  FakeMotionActor first;
  FakeMotionActor second;
  FakeMotionActor third;
  first.companion = &second;
  second.companion = &third;
  third.reacting = true;

  Actors::BeginTeleport(first, kTile, 5);
  CHECK(first.teleports == 1);
  CHECK(second.teleports == 1);
  CHECK(third.teleports == 0);
  CHECK(second.route == first.route);

  FakeMotionActor busy;
  FakeMotionActor idle;
  busy.reacting = true;
  busy.companion = &idle;
  Actors::BeginTeleport(busy, kTile, 5);
  CHECK(busy.teleports == 0);
  CHECK(idle.teleports == 0);
}

TEST_CASE("Cyclic teleport companions start each actor once",
          "[actors][motion-request]") {
  FakeMotionActor first;
  FakeMotionActor second;
  first.companion = &second;
  second.companion = &first;
  Actors::BeginTeleport(first, kTile, -1);
  CHECK(first.teleports == 1);
  CHECK(second.teleports == 1);
}

TEST_CASE("Travel keeps the full route and selects the tile or timed pose",
          "[actors][motion-request]") {
  const bool timed = GENERATE(false, true);
  FakeMotionActor actor;
  const MotionVector target{.x = 1, .y = -2, .z = 3, .w = 4};
  if (timed) {
    Actors::BeginTimedTravel(actor, target, 0x1ABCD);
  } else {
    Actors::BeginTileTravel(actor, target, 0x1ABCD);
  }
  REQUIRE(actor.route.has_value());
  CHECK(*actor.route ==
        MotionVector{
            .x = 1, .y = -2, .z = 3, .w = static_cast<std::int16_t>(0xABCD)});
  CHECK(actor.pose ==
        (timed ? Actors::TravelPose::kTimed : Actors::TravelPose::kTile));
  CHECK(actor.teleports == 0);
}

TEST_CASE("Queued movement requests select their mode, distance and facing",
          "[actors][motion-request]") {
  FakeMotionActor actor;
  const MotionVector step{.x = 10, .y = -20, .z = 30, .w = -40};

  Actors::QueueGroundOffset(actor, step);
  CHECK(actor.ground_step == step);
  CHECK_FALSE(actor.queued.has_value());

  Actors::QueueFlightMovement(actor, step, -10, 0x10003);
  CHECK(actor.queued ==
        Actors::QueuedMovement{.mode = Actors::MotionMode::kFlight,
                               .step = step,
                               .distance = static_cast<std::uint32_t>(-10),
                               .facing = 3});

  Actors::QueueMovementVector(actor, step, 0x10003);
  CHECK(actor.queued ==
        Actors::QueuedMovement{.mode = Actors::MotionMode::kVector,
                               .step = step,
                               .distance = 0x10003,
                               .facing = 3});
}

TEST_CASE("Navigation turns pick the rate from the collision radius",
          "[actors][motion-request]") {
  const auto [radius, rate] =
      GENERATE(std::pair{0U, 32}, std::pair{127U, 32}, std::pair{128U, 128},
               std::pair{0xFFFFU, 128});
  FakeMotionActor actor;
  actor.radius = radius;
  Actors::TurnNavigationActor(actor, 0x10100);
  CHECK(actor.turn_rate == rate);
  CHECK(actor.turn_facing == 0x100);
  CHECK(actor.turn_animation == 0x10100);
  CHECK(actor.events == std::vector<std::string>{"turn", "turn animation"});
}

TEST_CASE("Speed turns refuse a zero angular speed without turning",
          "[actors][motion-request]") {
  FakeMotionActor actor;
  CHECK_FALSE(Actors::TurnActorAtSpeed(actor, 0x100, 0));
  CHECK(actor.events.empty());

  CHECK(Actors::TurnActorAtSpeed(actor, -0x100, -7));
  CHECK(actor.angular_speed == -7);
  CHECK(actor.turn_facing == -0x100);
  CHECK(actor.turn_animation == -0x100);
  CHECK(actor.events == std::vector<std::string>{"turn", "turn animation"});
}
