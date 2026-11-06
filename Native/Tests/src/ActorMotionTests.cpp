#include <algorithm>
#include <array>
#include <cstdint>
#include <iterator>
#include <memory>
#include <span>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Actors/ActorMotion.hpp"
#include "Actors/ActorMotionLive.hpp"
#include "Content/ModuleData.h"
#include "TestSupport/ChannelWarnings.hpp"

extern "C" {
#include "Actors/ActorRecord.h"
#include "Actors/Location.h"
#include "Actors/Movement.h"
#include "Battle/Actor.h"
#include "Battle/Damage.h"
}

namespace actors = VS::Actors;
using actors::ActorMotion;
using actors::MotionVector;
using actors::PackedTile;
using actors::SupportProbe;
using VS::test_support::ChannelWarnings;
using VS::test_support::Refused;

namespace {
constexpr auto kKeepFacing = static_cast<std::int16_t>(actors::kKeepFacing);
constexpr int kActor = 3;
constexpr int kOtherActor = 5;
constexpr int kEmptySlot = 6;

[[nodiscard]] std::string Pair(int first, int second) {
  return std::to_string(first) + "," + std::to_string(second);
}

[[nodiscard]] std::string Triple(const MotionVector& vector) {
  return Pair(vector.x, vector.y) + "," + std::to_string(vector.z);
}

[[nodiscard]] std::string Timed(bool over_time) {
  return over_time ? " timed" : "";
}

// Records each terrain query and answers with the configured heights.
struct FakeTerrain final : actors::MotionTerrainHost {
  int support_height = 0;
  int floor_height = 0;
  int gimmick_height = 0;
  std::vector<std::string> queries;

  int SupportHeight(const MotionVector& origin, SupportProbe probe) override {
    queries.push_back("support " + Pair(origin.x, origin.z) + " probe " +
                      std::to_string(static_cast<int>(probe)));
    return support_height;
  }
  int InterpolatedFloorHeight(int x, int z) override {
    queries.push_back("floor " + Pair(x, z));
    return floor_height;
  }
  int GimmickSupportHeight(int tile_x, int tile_z) override {
    queries.push_back("gimmick " + Pair(tile_x, tile_z));
    return gimmick_height;
  }
};

// A route whose animation leaves `route_after_animation` behind, as a played
// animation can rewrite the record; the calls are logged in order.
struct FakeRoute final : actors::ActorRouteHost {
  actors::ActorRoute route;
  actors::ActorRoute route_after_animation;
  int turn_frames = 0;
  int move_frames = 0;
  bool motion_locked = false;
  int travel_animation = 0;
  std::vector<std::string> calls;

  actors::ActorRoute Route() override { return route; }
  void SetRoutePhase(int phase) override {
    calls.push_back("phase " + std::to_string(phase));
  }
  int TurnFrames() override { return turn_frames; }
  int MoveFrames() override { return move_frames; }
  bool MotionLocked() override { return motion_locked; }
  int TravelAnimation() override { return travel_animation; }
  void PlayAnimation(int animation) override {
    calls.push_back("play " + std::to_string(animation));
    route = route_after_animation;
  }
  void TranslateTo(const MotionVector& target, int rate,
                   bool over_time) override {
    calls.push_back("translate " + Triple(target) + " rate " +
                    std::to_string(rate) + Timed(over_time));
  }
  void TurnTo(int target_yaw, int rate, bool over_time) override {
    calls.push_back("turn " + std::to_string(target_yaw) + " rate " +
                    std::to_string(rate) + Timed(over_time));
  }
};

struct FakeScriptedPath final : actors::ScriptedPathHost {
  int bypass = 0;
  int status = 0;
  std::vector<std::string> calls;

  int CollisionBypass() override { return bypass; }
  void SetCollisionBypass(int flags) override {
    bypass = flags;
    calls.push_back("bypass " + std::to_string(flags));
  }
  void SetNavigationOverride(int x, int z) override {
    calls.push_back("navigate " + Pair(x, z));
  }
  int NavigationStatus() override { return status; }
};

// An actor at the origin with move rate 3 and turn rate 4, on tile (2, 2).
[[nodiscard]] ActorMotion RatedActor() {
  ActorMotion actor;
  actor.move_rate = 3;
  actor.turn_rate = 4;
  actor.current_tile = {.x = 2, .layer = 0, .z = 2};
  return actor;
}

// Every motion field set to a distinct value within its record width.
[[nodiscard]] ActorMotion SampleMotion() {
  ActorMotion motion;
  motion.action_animation_state = 9;
  motion.movement_pending = true;
  motion.movement_mode = 5;
  motion.locomotion_state = 3;
  motion.motion_locked = true;
  motion.is_jumping = true;
  motion.pose_state = 6;
  motion.slot = 7;
  motion.jump_countdown = 8;
  motion.turn_frames = 10;
  motion.move_frames = 11;
  motion.position = {.x = 12, .y = -13, .z = 14, .w = 15};
  motion.rotation_x = 16;
  motion.facing = 17;
  motion.rotation_z = 18;
  motion.scale = {.x = 19, .y = 20, .z = 21};
  motion.root_motion = {.x = -22, .y = 23, .z = 24, .w = 25};
  motion.rotation_step_x = 26;
  motion.facing_delta = -27;
  motion.rotation_step_z = 28;
  motion.scale_delta = {.x = 29, .y = 30, .z = -31, .w = 32};
  motion.current_tile = {.x = 33, .layer = 34, .z = 35};
  motion.route_phase = 3;
  motion.route_uses_duration = true;
  motion.action_pose_active = true;
  motion.is_running = true;
  motion.weapon_pose_active = true;
  motion.jump_profile = 2;
  motion.run_speed = 36;
  motion.walk_speed = 37;
  motion.turn_rate = 38;
  motion.move_rate = 39;
  motion.route_move_rate = 40;
  motion.route_turn_rate = 41;
  motion.scale_frames = 42;
  motion.transition_frames = 43;
  motion.route_target = {.x = 44, .y = 45, .z = -46, .w = 47};
  motion.destination_x = 48;
  motion.destination_z = -49;
  motion.destination_mode = 50;
  motion.momentum_distance = 51;
  motion.run_frames = 52;
  motion.request_mode = 53;
  motion.request_direction = -54;
  motion.request_facing = 55;
  motion.request_distance = 56;
  return motion;
}

// The actor and battle actor tables, emptied for one test and then restored.
struct LiveMotionState {
  decltype(std::to_array(Actors_actorRecords)) saved_records =
      std::to_array(Actors_actorRecords);
  decltype(std::to_array(Battle_actors)) saved_battle_actors =
      std::to_array(Battle_actors);
  std::vector<std::unique_ptr<Actors_ActorRecord>> records;
  std::vector<std::unique_ptr<Battle_Actor>> battle_actors;

  LiveMotionState() {
    std::fill(std::begin(Actors_actorRecords), std::end(Actors_actorRecords),
              nullptr);
    std::fill(std::begin(Battle_actors), std::end(Battle_actors), nullptr);
  }
  LiveMotionState(const LiveMotionState&) = delete;
  LiveMotionState& operator=(const LiveMotionState&) = delete;
  LiveMotionState(LiveMotionState&&) = delete;
  LiveMotionState& operator=(LiveMotionState&&) = delete;
  ~LiveMotionState() {
    std::ranges::copy(saved_records, std::begin(Actors_actorRecords));
    std::ranges::copy(saved_battle_actors, std::begin(Battle_actors));
  }

  Actors_ActorRecord& AddActor(int actor_id) {
    records.push_back(std::make_unique<Actors_ActorRecord>());
    Actors_ActorRecord& actor = *records.back();
    actor.instance.slot = static_cast<u_char>(actor_id);
    Actors_actorRecords[actor_id] = &actor;
    return actor;
  }

  Battle_Actor& AddBattleActor(int actor_id) {
    battle_actors.push_back(std::make_unique<Battle_Actor>());
    Battle_actors[actor_id] = battle_actors.back().get();
    return *battle_actors.back();
  }
};
}  // namespace

TEST_CASE("Stopping actor motion clears its modes, frames and root x and y",
          "[actors][actors-movement]") {
  ActorMotion actor;
  actor.jump_countdown = 5;
  actor.movement_mode = 3;
  actor.weapon_pose_active = true;
  actor.pose_state = 2;
  actor.locomotion_state = 1;
  actor.movement_pending = true;
  actor.is_jumping = true;
  actor.move_frames = 9;
  actor.turn_frames = 7;
  actor.root_motion = {.x = 10, .y = 20, .z = 30, .w = 40};

  ActorMotion cleared = actor;
  actors::ClearActorMovementModes(cleared);
  CHECK(cleared.move_frames == 9);
  CHECK(cleared.root_motion == actor.root_motion);

  actors::StopActorMotion(actor);
  ActorMotion expected;
  expected.is_jumping = true;
  expected.root_motion = {.x = 0, .y = 0, .z = 30, .w = 40};
  CHECK(actor == expected);
  CHECK(cleared.movement_mode == 0);
  CHECK(cleared.jump_countdown == 0);
}

TEST_CASE("Walk and run speeds drop the fraction and unlimit an equal run",
          "[actors][actors-movement]") {
  ActorMotion actor;
  actors::SetActorWalkRunSpeeds(actor, 0x2800, 0x5000);
  CHECK(actor.walk_speed == 2);
  CHECK(actor.run_speed == 5);
  actors::SetActorWalkRunSpeeds(actor, 0x5000, 0x5000);
  CHECK(actor.walk_speed == 5);
  CHECK(actor.run_speed == 0xFF);
  actors::SetActorWalkRunSpeeds(actor, 0x3000, 0x1000);
  CHECK(actor.run_speed == 0xFF);
}

TEST_CASE("Travel step counts turn a negative rate into 1 and read back",
          "[actors][actors-movement]") {
  ActorMotion actor;
  actors::SetActorTravelStepCounts(actor, -5, 32);
  auto counts = actors::ActorTravelStepCounts(actor);
  CHECK(counts.move_steps_per_tile == 1);
  CHECK(counts.turn_steps_per_revolution == 32);
  actors::SetActorTravelStepCounts(actor, 0, -1);
  counts = actors::ActorTravelStepCounts(actor);
  CHECK(counts.move_steps_per_tile == 0);
  CHECK(counts.turn_steps_per_revolution == 1);

  actor.momentum_distance = 40;
  actor.run_frames = 6;
  actor.is_running = true;
  actors::ResetActorRunMomentum(actor);
  CHECK(actor == ActorMotion{.turn_rate = 1});
}

TEST_CASE("Transition steps move each root share into the position",
          "[actors][actors-movement]") {
  ActorMotion actor;
  actor.transition_frames = 4;
  actor.root_motion = {.x = 100, .y = -50, .z = 7};
  actor.position = {.x = 1000, .y = 2000, .z = 3000};
  actors::StepActorTransitionDisplacement(actor);
  CHECK(actor.root_motion == MotionVector{.x = 75, .y = -38, .z = 6});
  CHECK(actor.position == MotionVector{.x = 1025, .y = 1988, .z = 3001});
  CHECK(actor.transition_frames == 3);
}

TEST_CASE("Transition steps with no frames left divide as the R3000 does",
          "[actors][actors-movement]") {
  // A zero divisor leaves -1 in LO, or 1 for a negative dividend.
  ActorMotion actor;
  actor.root_motion = {.x = 10, .y = -10, .z = 0};
  actors::StepActorTransitionDisplacement(actor);
  CHECK(actor.root_motion == MotionVector{.x = 11, .y = -11, .z = 1});
  CHECK(actor.position == MotionVector{.x = -1, .y = 1, .z = -1});
  CHECK(actor.transition_frames == 0xFFFF);
}

TEST_CASE("Fall destinations reach 63 units along each direction's sign",
          "[actors][actors-movement]") {
  ActorMotion actor;
  actor.position = {.x = 100, .y = 5, .z = 200};
  actors::SetActorFallDestination(actor, -3, 0);
  CHECK(actor.destination_x == 37);
  CHECK(actor.destination_z == 263);
  CHECK(actor.destination_mode == 1);
  actors::SetActorFallDestination(actor, 4, -1);
  CHECK(actor.destination_x == 163);
  CHECK(actor.destination_z == 137);
}

TEST_CASE("Movement requests clear to mode 0 for Ashley and 1 for others",
          "[actors][actors-movement]") {
  ActorMotion actor;
  actor.request_mode = 7;
  actor.request_direction = 5;
  actor.request_facing = 6;
  actor.request_distance = 99;
  actors::ClearActorMovementRequest(actor);
  CHECK(actor == ActorMotion{});

  actor.slot = kActor;
  actor.request_distance = 99;
  actors::ClearActorMovementRequest(actor);
  CHECK(actor.request_mode == 1);
  CHECK(actor.request_distance == 0);
}

TEST_CASE("Jump animation profiles rank action, jump, lock and pose states",
          "[actors][actors-movement]") {
  ActorMotion actor;
  actor.jump_profile = 2;
  CHECK(actors::ActorJumpAnimationProfile(actor) == 2);
  actor.action_pose_active = true;
  CHECK(actors::ActorJumpAnimationProfile(actor) == 6);
  actor.motion_locked = true;
  CHECK(actors::ActorJumpAnimationProfile(actor) == 5);
  actor.is_jumping = true;
  CHECK(actors::ActorJumpAnimationProfile(actor) == 4);
  actor.action_animation_state = 1;
  CHECK(actors::ActorJumpAnimationProfile(actor) == 3);
}

TEST_CASE("Facing deltas to a point wrap into a half turn and need a target",
          "[actors][actors-movement]") {
  ActorMotion actor;
  actor.facing = 0x100;
  const MotionVector east{.x = 100};
  const MotionVector west{.x = -100};
  CHECK(actors::ActorFacingDeltaToPoint(actor, &east) == -0x500);
  CHECK(actors::ActorFacingDeltaToPoint(actor, nullptr) == 0);
  const MotionVector here = actor.position;
  CHECK(actors::ActorFacingDeltaToPoint(actor, &here) == 0);
  actor.facing = 0xF00;
  CHECK(actors::ActorFacingDeltaToPoint(actor, &west) == 0x500);
}

TEST_CASE("Translations take their frames from the length and the move rate",
          "[actors][actors-movement]") {
  ActorMotion actor = RatedActor();
  actor.root_motion.w = 9;
  actors::SetActorTranslation(actor, {.x = 128, .w = 4}, -1);
  CHECK(actor.move_frames == 3);
  CHECK(actor.root_motion == MotionVector{.x = 128, .w = 9});
  actor.move_rate = 5;
  actors::SetActorMotion(actor, {.z = 64}, -1);
  CHECK(actor.move_frames == 3);
  actors::SetActorTranslation(actor, {.z = 64}, 7);
  CHECK(actor.move_frames == 7);
  CHECK(actor.position == MotionVector{});
}

TEST_CASE("Translations applied at once move the actor and its current tile",
          "[actors][actors-movement]") {
  ActorMotion actor = RatedActor();
  actor.move_frames = 4;
  actor.position = {.x = 100, .y = 0, .z = 500};
  actors::SetActorTranslation(actor, {.x = 200, .y = 10, .z = 100}, 0);
  CHECK(actor.move_frames == 0);
  CHECK(actor.position == MotionVector{.x = 300, .y = 10, .z = 600});
  CHECK(actor.current_tile == PackedTile{.x = 2, .layer = 0, .z = 4});
  // A length that rounds to no frames applies at once too.
  actor.move_rate = 0;
  actors::SetActorTranslation(actor, {.x = -300}, -1);
  CHECK(actor.position.x == 0);
  CHECK(actor.current_tile.x == 0);
}

TEST_CASE("Rate translations borrow the given move rate and restore it",
          "[actors][actors-movement]") {
  ActorMotion actor = RatedActor();
  actor.move_rate = 9;
  actors::TranslateActorByAtRate(actor, {.z = 64}, 5);
  CHECK(actor.move_frames == 3);
  CHECK(actor.move_rate == 9);
  actors::TranslateActorByAtRate(actor, {.z = 64}, -1);
  CHECK(actor.move_frames == 5);

  actor.position = {.x = 10, .y = 0, .z = 10};
  actors::TranslateActorToAtRate(actor, {.x = 138, .y = 4, .z = 10}, 0);
  CHECK(actor.position == MotionVector{.x = 138, .y = 4, .z = 10});
  CHECK(actor.current_tile.x == 1);
  CHECK(actor.move_rate == 9);
  actors::TranslateActorToOverTime(actor, {.x = 10, .y = 4, .z = 74}, 6);
  CHECK(actor.root_motion == MotionVector{.x = -128, .y = 0, .z = 64});
  CHECK(actor.move_frames == 6);
}

TEST_CASE("Yaw turns take frames from the turn rate or recentre at once",
          "[actors][actors-movement]") {
  ActorMotion actor = RatedActor();
  actor.turn_rate = 6;
  actor.rotation_step_x = 3;
  actor.rotation_step_z = 4;
  actors::SetActorYawTurn(actor, 0x400, -1);
  CHECK(actor.turn_frames == 2);
  CHECK(actor.facing_delta == 0x400);
  CHECK(actor.rotation_step_x == 0);
  CHECK(actor.rotation_step_z == 0);
  actors::SetActorYawMotion(actor, 0x400, 5);
  CHECK(actor.turn_frames == 5);

  actor.facing = 0xF00;
  actors::SetActorYawTurn(actor, 0x200, 0);
  CHECK(actor.facing == 0x100);
  // The original's remainder keeps the sum's sign.
  actors::SetActorYawTurn(actor, -0x200, 0);
  CHECK(actor.facing == -0x100);
}

TEST_CASE("Turns to a yaw take the short way round",
          "[actors][actors-movement]") {
  ActorMotion actor = RatedActor();
  actor.facing = 0x100;
  actors::TurnActorToAtRate(actor, 0xF00, 8);
  CHECK(actor.facing_delta == -0x200);
  CHECK(actor.turn_frames == 1);
  CHECK(actor.turn_rate == 4);

  actor.facing = 0xF00;
  actors::TurnActorToOverTime(actor, 0x100, 0);
  CHECK(actor.facing == 0x100);
  CHECK(actor.turn_frames == 0);

  actor.turn_rate = 0;
  actors::TurnActorByAtRate(actor, 0x300, -1);
  CHECK(actor.facing == 0x400);
}

TEST_CASE("Angular speed turns round up and a zero speed turns at once",
          "[actors][actors-movement]") {
  ActorMotion actor = RatedActor();
  actors::TurnActorToAtAngularSpeed(actor, 0x300, 0x100);
  CHECK(actor.turn_frames == 3);
  actors::TurnActorToAtAngularSpeed(actor, 0x300, -0x200);
  CHECK(actor.turn_frames == 2);
  CHECK(actor.facing == 0);
  // The R3000 quotient of -1 plus the remainder's round-up gives 0 steps.
  actors::TurnActorToAtAngularSpeed(actor, 0x300, 0);
  CHECK(actor.turn_frames == 0);
  CHECK(actor.facing == 0x300);
}

TEST_CASE("Rotations take the largest axis for their frames and wrap angles",
          "[actors][actors-movement]") {
  ActorMotion actor = RatedActor();
  actor.rotation_x = 0xF80;
  actor.facing = 0x100;
  const MotionVector delta{.x = 0x100, .y = -0x300, .z = 0x200};
  actors::SetActorRotation(actor, delta, -1);
  CHECK(actor.turn_frames == 1);
  CHECK(actor.rotation_x == 0xF80);
  actors::SetActorRotationMotion(actor, delta, 6);
  CHECK(actor.turn_frames == 6);

  actors::SetActorRotation(actor, delta, 0);
  CHECK(actor.turn_frames == 0);
  CHECK(actor.rotation_x == 0x80);
  CHECK(actor.facing == 0xE00);
  CHECK(actor.rotation_z == 0x200);
}

TEST_CASE("Rotations to a target wrap each axis the short way round",
          "[actors][actors-movement]") {
  ActorMotion actor = RatedActor();
  actor.rotation_x = 0x100;
  actor.facing = 0xF00;
  actors::RotateActorToAtRate(actor, {.x = 0xF00, .y = 0x100, .z = 0x900}, 0);
  CHECK(actor.rotation_step_x == -0x200);
  CHECK(actor.facing_delta == 0x200);
  CHECK(actor.rotation_step_z == -0x700);
  CHECK(actor.rotation_x == 0xF00);
  CHECK(actor.facing == 0x100);
  CHECK(actor.rotation_z == 0x900);
  CHECK(actor.turn_rate == 4);

  actor.turn_rate = 2;
  actors::RotateActorByAtRate(actor, {.y = 0x400}, -1);
  CHECK(actor.turn_frames == 1);
}

TEST_CASE("Scales apply at once, tween, or stop when already at the target",
          "[actors][actors-movement]") {
  ActorMotion actor;
  actor.scale = {.x = 4096, .y = 4096, .z = 4096};
  const MotionVector target{.x = 2048, .y = 4096, .z = 8192, .w = 7};
  actors::ScaleActorToOverTime(actor, target, 10);
  CHECK(actor.scale_delta == MotionVector{.x = -2048, .y = 0, .z = 4096});
  CHECK(actor.scale_frames == 10);
  CHECK(actor.scale.x == 4096);

  actors::ScaleActorToOverTime(actor, target, 0);
  CHECK(actor.scale == MotionVector{.x = 2048, .y = 4096, .z = 8192});
  CHECK(actor.scale_frames == 0);
  actor.scale_frames = 5;
  actors::ScaleActorToOverTime(actor, target, 10);
  CHECK(actor.scale_frames == 0);
}

TEST_CASE("Tile translations aim at the tile centre's floor-only height",
          "[actors][actors-movement]") {
  FakeTerrain terrain;
  terrain.support_height = -40;
  ActorMotion actor = RatedActor();
  actor.position = {.x = 192, .y = -40, .z = 448};
  const PackedTile tile{.x = 2, .layer = 0, .z = 3};
  actors::SetActorTileTranslation(terrain, actor, tile, -1, 3);
  CHECK(actor.root_motion == MotionVector{.x = 128, .y = 0, .z = 0});
  CHECK(actor.move_frames == 3);
  CHECK(terrain.queries == std::vector<std::string>{"support 320,448 probe 4"});

  actors::SetActorTileTranslation(terrain, actor, tile, 9, 3);
  CHECK(actor.move_frames == 9);
  actors::SetActorTileTranslation(terrain, actor, tile, 0, 3);
  CHECK(actor.position == MotionVector{.x = 320, .y = -40, .z = 448});
  CHECK(actor.move_frames == 0);
  actor.move_frames = 7;
  actors::SetActorTileTranslation(terrain, actor, tile, 0, 3);
  CHECK(actor.move_frames == 7);
}

TEST_CASE("Tile translations at a rate or over time also take the tile",
          "[actors][actors-movement]") {
  FakeTerrain terrain;
  ActorMotion actor = RatedActor();
  actor.position = {.x = 192, .z = 448};
  const PackedTile tile{.x = 2, .layer = 1, .z = 3};
  actors::TranslateActorToTileAtRate(terrain, actor, tile, -1);
  CHECK(actor.move_frames == 3);
  CHECK(actor.current_tile == tile);
  actors::TranslateActorToTileAtRate(terrain, actor, tile, 5);
  CHECK(actor.move_frames == 5);
  actors::TranslateActorToTileOverTime(terrain, actor, tile, 6);
  CHECK(actor.move_frames == 6);
  CHECK(actor.position == MotionVector{.x = 192, .z = 448});
}

TEST_CASE("Tile turns face the tile's heading or its own facing in place",
          "[actors][actors-movement]") {
  ActorMotion actor = RatedActor();
  actor.turn_rate = 8;
  actor.current_tile = {.x = 1, .z = 1};
  actors::TurnActorTowardTileAtRate(actor, {.x = 2, .z = 1}, -1);
  CHECK(actor.facing_delta == -0x400);
  CHECK(actor.turn_frames == 2);
  actors::TurnActorTowardTileOverTime(actor, {.x = 2, .z = 1}, 4);
  CHECK(actor.turn_frames == 4);

  actors::SetActorTileTurn(actor, {.x = 1, .z = 1, .spare = 0x20}, 0, 8);
  CHECK(actor.facing == 0x200);
  actor.turn_frames = 9;
  actors::SetActorTileTurn(actor, {.x = 1, .z = 1, .spare = 0x20}, 0, 8);
  CHECK(actor.facing_delta == 0);
  CHECK(actor.turn_frames == 9);
}

TEST_CASE("Tile moves at a rate start a turning route to the tile centre",
          "[actors][actors-movement]") {
  FakeTerrain terrain;
  terrain.support_height = -40;
  ActorMotion actor = RatedActor();
  actor.turn_rate = 8;
  const PackedTile tile{.x = 2, .layer = 1, .z = 3, .spare = 0x10};
  actors::MoveActorToTileAtRate(terrain, actor, tile, 6, -1);
  CHECK(actor.route_target ==
        MotionVector{.x = 320, .y = -40, .z = 448, .w = 0x100});
  CHECK(actor.route_move_rate == 6);
  CHECK(actor.route_turn_rate == 8);
  CHECK(actor.facing_delta == -0x800);
  CHECK(actor.turn_frames == 4);
  CHECK(actor.route_phase == actors::kRouteTurning);
  CHECK(terrain.queries == std::vector<std::string>{"support 320,448 probe 1"});
  actors::MoveActorToTileAtRate(terrain, actor, {.spare = 0xFF}, 6, 4);
  CHECK(actor.route_target.w == kKeepFacing);
}

TEST_CASE("Tile moves with both rates 0 place the actor on the tile at once",
          "[actors][actors-movement]") {
  FakeTerrain terrain;
  terrain.support_height = -40;
  ActorMotion actor = RatedActor();
  actor.position = {.x = 320, .y = -40, .z = 320};
  actor.facing = 0x300;
  actor.route_phase = actors::kRouteWalking;
  const PackedTile tile{.x = 2, .layer = 1, .z = 3, .spare = 0x10};
  actors::MoveActorToTileAtRate(terrain, actor, tile, 0, 0);
  CHECK(actor.position == MotionVector{.x = 320, .y = -40, .z = 448});
  CHECK(actor.facing == 0x100);
  CHECK(actor.route_phase == actors::kRouteIdle);
  CHECK(actor.current_tile == PackedTile{.x = 2, .layer = 1, .z = 3});
  CHECK(terrain.queries == std::vector<std::string>{"support 320,448 probe 4"});
}

TEST_CASE("Timed tile moves record the move count and the duration flag",
          "[actors][actors-movement]") {
  FakeTerrain terrain;
  ActorMotion actor = RatedActor();
  const PackedTile tile{.x = 2, .layer = 1, .z = 3, .spare = 0x10};
  actors::MoveActorToTileOverTime(terrain, actor, tile, 12, 4);
  CHECK(actor.route_move_rate == 12);
  CHECK(actor.turn_frames == 4);
  CHECK(actor.route_phase == actors::kRouteTurning);
  CHECK(actor.route_uses_duration);
  CHECK(actor.current_tile == PackedTile{.x = 2, .layer = 1, .z = 3});
  CHECK(terrain.queries.empty());

  actors::MoveActorToTileOverTime(terrain, actor, tile, 0, 0);
  CHECK(actor.route_phase == actors::kRouteIdle);
  CHECK(actor.position == MotionVector{.x = 320, .z = 448});
}

TEST_CASE("Position moves take the floor height and the gimmick at tile x, x",
          "[actors][actors-movement]") {
  FakeTerrain terrain;
  // The floor height keeps its low 15 bits, sign-extended.
  terrain.floor_height = 0x1FFF0;
  ActorMotion actor = RatedActor();
  const MotionVector target{.x = 300, .y = kKeepFacing, .z = 700, .w = 0x234};
  actors::MoveActorToPositionAtRate(terrain, actor, target, 6, 4);
  CHECK(actor.route_target ==
        MotionVector{.x = 300, .y = -16, .z = 700, .w = 0x234});
  CHECK(terrain.queries ==
        std::vector<std::string>{"floor 300,700", "gimmick 2,2"});
  CHECK(actor.route_move_rate == 6);
  CHECK(actor.route_turn_rate == 4);
  CHECK(actor.route_phase == actors::kRouteTurning);

  terrain.gimmick_height = -96;
  actors::MoveActorToPositionAtRate(terrain, actor, target, -1, -1);
  CHECK(actor.route_target.y == -96);
  CHECK(actor.route_move_rate == 3);
}

TEST_CASE("Position moves turn towards the target and keep a given height",
          "[actors][actors-movement]") {
  FakeTerrain terrain;
  ActorMotion actor = RatedActor();
  const MotionVector target{.x = 300, .y = 50, .z = 0, .w = kKeepFacing};
  actors::MoveActorToPositionAtRate(terrain, actor, target, 6, 4);
  CHECK(actor.facing_delta == -0x400);
  CHECK(actor.turn_frames == 1);
  CHECK(actor.route_target == target);
  CHECK(terrain.queries.empty());

  // On the target with a kept facing there is nothing to do.
  actor.position = target;
  const ActorMotion before = actor;
  actors::MoveActorToPositionAtRate(terrain, actor, target, 6, 4);
  CHECK(actor == before);
}

TEST_CASE("Position moves at once face the travel heading or the target's",
          "[actors][actors-movement]") {
  FakeTerrain terrain;
  ActorMotion actor = RatedActor();
  actor.facing = 0x123;
  actor.turn_frames = 5;
  actors::MoveActorToPositionAtRate(
      terrain, actor, {.x = 200, .y = 30, .z = 0, .w = kKeepFacing}, 0, 0);
  CHECK(actor.facing == 0xC00);
  CHECK(actor.turn_frames == 0);
  CHECK(actor.position == MotionVector{.x = 200, .y = 30, .z = 0});
  CHECK(actor.current_tile.x == 1);
  CHECK(actor.move_rate == 3);

  actors::MoveActorToPositionAtRate(terrain, actor,
                                    {.x = 0, .y = 0, .z = 0, .w = 0x300}, 0, 0);
  CHECK(actor.facing == 0x300);
  CHECK(actor.position == MotionVector{});
}

TEST_CASE("Routes wait for the turn, then walk with the route read again",
          "[actors][actors-movement]") {
  FakeRoute host;
  host.route.phase = actors::kRouteTurning;
  host.turn_frames = 2;
  actors::UpdateActorRoute(host);
  CHECK(host.calls.empty());

  host.turn_frames = 0;
  host.travel_animation = 7;
  host.route_after_animation = {.phase = actors::kRouteTurning,
                                .uses_duration = true,
                                .target = {.x = 10, .y = 20, .z = 30},
                                .move_rate = 9};
  actors::UpdateActorRoute(host);
  CHECK(host.calls ==
        std::vector<std::string>{"play 7", "translate 10,20,30 rate 9 timed",
                                 "phase 2"});
  host.calls.clear();
  host.motion_locked = true;
  actors::UpdateActorRoute(host);
  CHECK(host.calls.front() == "play 31");
}

TEST_CASE("Routes wait for the walk, then idle and face the target's facing",
          "[actors][actors-movement]") {
  FakeRoute host;
  host.route.phase = actors::kRouteWalking;
  host.move_frames = 3;
  actors::UpdateActorRoute(host);
  CHECK(host.calls.empty());

  host.move_frames = 0;
  host.route_after_animation = {.target = {.w = 0x1234}, .turn_rate = 5};
  actors::UpdateActorRoute(host);
  CHECK(host.calls ==
        std::vector<std::string>{"play 1", "turn 564 rate 5", "phase 0"});
  host.calls.clear();
  host.route = {.phase = actors::kRouteWalking};
  host.route_after_animation = {.target = {.w = kKeepFacing}};
  actors::UpdateActorRoute(host);
  CHECK(host.calls == std::vector<std::string>{"play 1", "phase 0"});
  host.calls.clear();
  host.route = {.phase = actors::kRouteIdle};
  actors::UpdateActorRoute(host);
  host.route = {.phase = 3};
  actors::UpdateActorRoute(host);
  CHECK(host.calls.empty());
}

TEST_CASE("Scripted paths bypass collision until arrival or no tracking",
          "[actors][actors-movement]") {
  FakeScriptedPath host;
  host.bypass = 4;
  actors::BeginScriptedActorPath(host, {.x = 300, .y = 5, .z = 700});
  CHECK(host.calls == std::vector<std::string>{"bypass 2", "navigate 300,700"});

  host.calls.clear();
  host.status = 1;
  CHECK_FALSE(actors::IsScriptedActorPathFinished(host));
  host.status = 2;
  CHECK(actors::IsScriptedActorPathFinished(host));
  host.status = 3;
  CHECK_FALSE(actors::IsScriptedActorPathFinished(host));
  CHECK(host.calls.empty());
  host.status = 0;
  host.bypass = 7;
  CHECK(actors::IsScriptedActorPathFinished(host));
  CHECK(host.bypass == 5);
}

TEST_CASE("Movement resets keep the locked command state and allow absences",
          "[actors][actors-movement]") {
  ActorMotion actor;
  actor.movement_mode = 3;
  actor.move_frames = 4;
  std::uint8_t command_state = 5;
  actors::ResetActorMovement(&command_state, &actor);
  CHECK(command_state == 0);
  CHECK(actor == ActorMotion{});

  command_state = 0x20;
  actors::ResetActorMovement(&command_state, nullptr);
  CHECK(command_state == 0x20);
  actor.turn_frames = 6;
  actors::ResetActorMovement(nullptr, &actor);
  CHECK(actor.turn_frames == 0);
  actors::ResetActorMovement(nullptr, nullptr);
}

TEST_CASE("Live actor motion stores and loads every record field",
          "[actors][actors-movement]") {
  const ActorMotion sample = SampleMotion();
  auto record = std::make_unique<Actors_ActorRecord>();
  record->travelAnimation = 7;
  actors::StoreLiveActorMotion(sample, *record);
  CHECK(actors::LoadLiveActorMotion(*record) == sample);
  CHECK(record->instance.currentAnimation == sample.move_frames);
  CHECK(record->instance.currentTileLayer == sample.current_tile.layer);
  CHECK(record->turnState.distance == sample.request_distance);
  CHECK(record->routeTarget.pad == sample.route_target.w);
  CHECK(record->travelAnimation == 7);
}

TEST_CASE("Live adapters run the motion rules on the actor table's record",
          "[actors][actors-movement]") {
  LiveMotionState state;
  Actors_ActorRecord& actor = state.AddActor(kActor);
  actors::SetLiveActorTravelStepCounts(kActor, -1, 32);
  int move_steps = 0;
  int turn_steps = 0;
  actors::LiveActorTravelStepCounts(kActor, &move_steps, &turn_steps);
  CHECK(move_steps == 1);
  CHECK(turn_steps == 32);
  actors::SetLiveActorWalkRunSpeeds(kActor, 0x2000, 0x1000);
  CHECK(actor.walkSpeed == 2);
  CHECK(actor.runSpeed == 0xFF);

  actor.instance.position = SVECTOR{.vx = 100, .vy = 0, .vz = 200, .pad = 0};
  SVECTOR step{.vx = 200, .vy = 10, .vz = 100, .pad = 0};
  actors::SetLiveActorTranslation(kActor, &step, 0);
  CHECK(actor.instance.position.vx == 300);
  CHECK(actor.instance.currentTileZ == 2);
  actors::StopLiveActorMotion(kActor);
  CHECK(actor.instance.rootMotion.vx == 0);
}

TEST_CASE("Live turns and scales write the record's angles and scale",
          "[actors][actors-movement]") {
  LiveMotionState state;
  Actors_ActorRecord& actor = state.AddActor(kActor);
  actor.turnRate = 4;
  actor.instance.facing = 0x100;
  actors::TurnLiveActorToAtRate(kActor, 0xF00, 8);
  CHECK(actor.instance.facingDelta == -0x200);
  CHECK(actor.instance.turnFrames == 1);
  CHECK(actor.turnRate == 4);

  actor.instance.currentTileX = 1;
  actor.instance.currentTileZ = 1;
  Actors_TileTarget tile{.tileX = 2, .tileLayer = 0, .tileZ = 1, .facing = 0};
  actors::TurnLiveActorTowardTileAtRate(kActor, &tile, 0);
  CHECK(actor.instance.facing == 0xC00);

  SVECTOR scale{.vx = 2048, .vy = 4096, .vz = 8192, .pad = 0};
  actors::ScaleLiveActorToOverTime(kActor, &scale, 0);
  CHECK(actor.instance.scaleX == 2048);
  CHECK(actor.instance.scaleZ == 8192);
}

TEST_CASE("Live adapters skip an empty slot and refuse ids past the table",
          "[actors][actors-movement]") {
  LiveMotionState state;
  Actors_ActorRecord& actor = state.AddActor(kActor);
  actor.moveRate = 9;
  actors::SetLiveActorTravelStepCounts(kEmptySlot, 4, 4);
  CHECK(actor.moveRate == 9);
  int move_steps = -5;
  int turn_steps = -5;
  actors::LiveActorTravelStepCounts(kEmptySlot, &move_steps, &turn_steps);
  CHECK(move_steps == -5);

  ChannelWarnings warnings{"actors"};
  const int past_table = static_cast<int>(std::size(Actors_actorRecords));
  actors::StopLiveActorMotion(past_table);
  actors::LiveActorTravelStepCounts(-1, &move_steps, &turn_steps);
  CHECK(turn_steps == -5);
  CHECK(
      warnings.messages ==
      std::vector<std::string>{
          Refused("Actor motion", "actor id 17 lies outside the actor table"),
          Refused("Actor motion", "actor id -1 lies outside the actor table")});
}

TEST_CASE("Live movement resets drop the battle command unless locked",
          "[actors][actors-movement]") {
  LiveMotionState state;
  Actors_ActorRecord& actor = state.AddActor(kActor);
  Battle_Actor& battle_actor = state.AddBattleActor(kActor);
  battle_actor.commandState = 5;
  actor.instance.movementMode = 3;
  actor.instance.turnFrames = 4;
  actors::ResetLiveActorMovement(kActor);
  CHECK(battle_actor.commandState == 0);
  CHECK(actor.instance.movementMode == 0);
  CHECK(actor.instance.turnFrames == 0);

  battle_actor.commandState = 0x20;
  actors::ResetLiveActorMovement(kActor);
  CHECK(battle_actor.commandState == 0x20);
  // A battle actor without an actor record still drops its command.
  Battle_Actor& other = state.AddBattleActor(kOtherActor);
  other.commandState = 7;
  actors::ResetLiveActorMovement(kOtherActor);
  CHECK(other.commandState == 0);
}

TEST_CASE("Live record entries apply the fall, request and transition rules",
          "[actors][actors-movement]") {
  auto record = std::make_unique<Actors_ActorRecord>();
  record->instance.slot = kActor;
  record->instance.position = SVECTOR{.vx = 100, .vy = 5, .vz = 200, .pad = 0};
  Actors_FallDirection direction{
      .directionX = -3, .reserved2 = 0, .directionZ = 4};
  actors::SetLiveActorFallDestination(record.get(), &direction);
  CHECK(record->destinationX == 37);
  CHECK(record->destinationZ == 263);
  record->turnState.distance = 99;
  actors::ClearLiveActorMovementRequest(record.get());
  CHECK(record->turnState.mode == 1);
  CHECK(record->turnState.distance == 0);

  record->transitionFrames = 4;
  record->instance.rootMotion.vx = 100;
  actors::StepLiveActorTransitionDisplacement(record.get());
  CHECK(record->instance.position.vx == 125);
  CHECK(record->transitionFrames == 3);
  record->instance.isJumping = 1;
  CHECK(actors::LiveActorJumpAnimationProfile(record.get()) == 4);
}

TEST_CASE("Live record entries refuse a missing record with a warning",
          "[actors][actors-movement]") {
  ChannelWarnings warnings{"actors"};
  Actors_FallDirection direction{};
  SVECTOR point{};
  actors::StepLiveActorTransitionDisplacement(nullptr);
  actors::SetLiveActorFallDestination(nullptr, &direction);
  actors::ClearLiveActorMovementRequest(nullptr);
  CHECK(actors::LiveActorFacingDeltaToPoint(nullptr, &point) == 0);
  actors::UpdateLiveActorRoute(kActor, nullptr);
  CHECK(warnings.messages ==
        std::vector<std::string>{
            Refused("StepActorTransitionDisplacement", "no actor record"),
            Refused("SetActorFallDestination", "no actor record"),
            Refused("ClearActorMovementRequest", "no actor record"),
            Refused("ActorFacingDeltaToPoint", "no actor record"),
            Refused("UpdateActorRoute", "no actor record")});
}

TEST_CASE("Live routes wait on the record's turn and walk frames",
          "[actors][actors-movement]") {
  auto record = std::make_unique<Actors_ActorRecord>();
  record->routePhase = actors::kRouteTurning;
  record->instance.turnFrames = 3;
  actors::UpdateLiveActorRoute(kActor, record.get());
  CHECK(record->routePhase == actors::kRouteTurning);

  record->routePhase = actors::kRouteWalking;
  record->instance.currentAnimation = 5;
  actors::UpdateLiveActorRoute(kActor, record.get());
  CHECK(record->routePhase == actors::kRouteWalking);
  record->routePhase = actors::kRouteIdle;
  actors::UpdateLiveActorRoute(kActor, record.get());
  CHECK(record->routePhase == actors::kRouteIdle);
}

TEST_CASE("Promoted actor motion leaves no module address binding",
          "[actors][actors-movement][redirect-binding]") {
  const auto symbols =
      std::span(Content_moduleSymbols, Content_moduleSymbolCount);
  // No module image stores these functions' addresses; callers use the
  // typed API by name. ActorJumpAnimationProfile (func_800AC690_getProfile)
  // is a decomp-local helper with no address of its own: 0x800AC690 is
  // UpdateActorJumpAndFall, which the movement slice retires.
  constexpr std::array<std::uint32_t, 40> kRetired{
      0x8007CFF8u, 0x8007D03Cu, 0x8007D260u, 0x8009DC38u, 0x8009DC8Cu,
      0x8009E180u, 0x800A0ABCu, 0x800A0AFCu, 0x800A0B5Cu, 0x800A0BA0u,
      0x800A2FBCu, 0x800A3054u, 0x800A525Cu, 0x800A97ECu, 0x800A9988u,
      0x800A9C54u, 0x800A9CDCu, 0x800A9D24u, 0x800A9D90u, 0x800A9E38u,
      0x800A9EB4u, 0x800A9F64u, 0x800A9FD0u, 0x800AA108u, 0x800AA218u,
      0x800AA290u, 0x800AA454u, 0x800AA490u, 0x800AA600u, 0x800AA620u,
      0x800AA698u, 0x800AA82Cu, 0x800AA850u, 0x800AA8D0u, 0x800AA984u,
      0x800AAA68u, 0x800AAA88u, 0x800AABD0u, 0x800AC540u, 0x800AF960u};
  for (const std::uint32_t address : kRetired) {
    CAPTURE(address);
    CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                            address) == nullptr);
  }
}
