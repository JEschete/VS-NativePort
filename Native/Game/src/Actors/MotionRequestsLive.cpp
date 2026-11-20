// Hand-maintained (tools/port/owned.csv).
// Live binding for the typed motion requests (MotionRequests.cpp).
#include "Actors/MotionRequestsLive.hpp"

#include <array>
#include <bit>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <memory>

#include "Actors/ActorMotionLive.hpp"
#include "Actors/ActorMovementLive.hpp"
#include "Actors/ActorRecord.hpp"
#include "Actors/Animation.hpp"
#include "Actors/AnimationControlLive.hpp"
#include "Actors/Movement.hpp"
#include "Core/Diagnostics.hpp"

namespace VS::Actors {
namespace {
void Require(bool valid, const char* message) {
  if (!valid) {
    VS::Core::Errorf("actors", "%s", message);
    std::abort();
  }
}

VS::Actors::ActorRecord& Actor(int actor_id) {
  if (actor_id < 0 ||
      static_cast<unsigned>(actor_id) >= std::size(VS::Actors::actorRecords) ||
      VS::Actors::actorRecords[actor_id] == nullptr) {
    VS::Core::Errorf("actors", "Motion request requires actor %d", actor_id);
    std::abort();
  }
  return *VS::Actors::actorRecords[actor_id];
}

PackedTile Tile(const int* target) {
  Require(target != nullptr, "Motion request requires a packed target");
  return std::bit_cast<PackedTile>(*target);
}

MotionVector Vector(const void* source) {
  MotionVector value;
  static_assert(sizeof(value) == sizeof(SVECTOR));
  std::memcpy(&value, source, sizeof(value));
  return value;
}

MotionVector Route(const int* target) {
  Require(target != nullptr, "Motion request requires a packed target");
  return Vector(target);
}

MotionVector Step(const SVECTOR* step) {
  Require(step != nullptr, "Motion request requires a native step vector");
  return Vector(step);
}

SVECTOR Native(const MotionVector& value) {
  return SVECTOR{.vx = value.x, .vy = value.y, .vz = value.z, .pad = value.w};
}

const char* EntryName(JumpEntry entry) {
  switch (entry) {
    case JumpEntry::kAirborne:
      return "airborne";
    case JumpEntry::kNavigation:
      return "navigation";
    case JumpEntry::kPlain:
      break;
  }
  return "plain";
}

void LogJumpRequest(const VS::Actors::ActorRecord& actor, JumpEntry entry,
                    const PackedTile& target, int speed, std::int16_t facing) {
  VS::Core::Debugf(
      "actors.jump",
      "request actor=%d entry=%s target=[%d,%d,%d,%d] speed=%d facing=%d "
      "position=[%d,%d,%d] phase=%d locked=%d airborne=%d profile=%d",
      actor.instance.slot, EntryName(entry), target.x, target.layer, target.z,
      target.spare, speed, facing, actor.instance.position.vx,
      actor.instance.position.vy, actor.instance.position.vz,
      actor.instance.movement_mode, actor.instance.motion_locked,
      actor.instance.is_jumping, actor.jump_profile);
}

void LogJumpResult(const VS::Actors::ActorRecord& actor) {
  VS::Core::Debugf(
      "actors.jump",
      "result actor=%d phase=%d countdown=%d destination=[%d,%d] "
      "destination_mode=%d jump_frames=%d root=[%d,%d,%d] landing_pending=%d",
      actor.instance.slot, actor.instance.movement_mode,
      actor.instance.jump_countdown, actor.destination_x, actor.destination_z,
      actor.destination_mode, actor.jump_frames, actor.instance.root_motion.vx,
      actor.instance.root_motion.vy, actor.instance.root_motion.vz,
      actor.landing_pending);
}

void SetMode(VS::Actors::MovementRequest& request, MotionMode mode) {
  request.mode = static_cast<std::uint8_t>(mode);
  request.reserved1 = 0;
  request.reserved2 = 0;
  request.reserved3 = 0;
}

class LiveMotionActor final : public MotionActor {
 public:
  explicit LiveMotionActor(int actor_id)
      : id_(actor_id), actor_(Actor(actor_id)) {}

  [[nodiscard]] bool Jumping() const override {
    return actor_.instance.is_jumping != 0;
  }
  [[nodiscard]] bool MotionLocked() const override {
    return actor_.instance.motion_locked != 0;
  }
  void SetJumpFlags(bool motion_locked, bool jumping) override {
    actor_.instance.motion_locked = motion_locked ? 1 : 0;
    actor_.instance.is_jumping = jumping ? 1 : 0;
  }
  void SetJumpProfile(int profile) override {
    actor_.jump_profile = static_cast<std::uint8_t>(profile);
  }
  void PlayAnimation(int animation, int blend_frames) override {
    PlayLiveActorAnimation(id_, animation, 0, blend_frames);
  }
  void JumpToTile(JumpEntry entry, PackedTile target, int speed,
                  std::int16_t facing) override {
    LogJumpRequest(actor_, entry, target, speed, facing);
    BeginLiveActorJumpToTile(&actor_, target, speed, facing);
    LogJumpResult(actor_);
  }

  [[nodiscard]] bool Reacting() const override {
    return actor_.instance.reaction_state != 0;
  }
  void StartTeleport(const MotionVector& route) override {
    actor_.instance.turn_frames = 0;
    actor_.route_target = Native(route);
    actor_.instance.reaction_state = 1;
    actor_.instance.pose_state = 0;
    actor_.instance.was_drawn = 0;
  }
  [[nodiscard]] MotionActor* TeleportCompanion() override {
    if (actor_.companion_slot < 2) return nullptr;
    companion_ = std::make_unique<LiveMotionActor>(actor_.companion_slot);
    return companion_.get();
  }
  void StartTravel(const MotionVector& route, TravelPose pose) override {
    actor_.route_target = Native(route);
    actor_.instance.pose_state = static_cast<std::uint8_t>(pose);
  }

  void QueueGroundOffset(const MotionVector& step) override {
    actor_.turn_state.step = Native(step);
    SetMode(actor_.turn_state, MotionMode::kGround);
  }
  void QueueMovement(const QueuedMovement& movement) override {
    actor_.turn_state.distance = movement.distance;
    actor_.turn_state.facing = movement.facing;
    actor_.turn_state.step = Native(movement.step);
    actor_.instance.motion_locked = 1;
    SetMode(actor_.turn_state, movement.mode);
  }

  [[nodiscard]] unsigned CollisionRadius() const override {
    return actor_.collision_radius;
  }
  void TurnAtRate(std::int16_t facing, int rate) override {
    TurnLiveActorToAtRate(id_, facing, rate);
  }
  void TurnAtAngularSpeed(std::int16_t facing, int speed) override {
    TurnLiveActorToAtAngularSpeed(id_, facing, speed);
  }
  void PlayTurnAnimation(int facing) override {
    PlayLiveActorTurnAnimation(id_, facing);
  }

 private:
  int id_;
  VS::Actors::ActorRecord& actor_;
  std::unique_ptr<LiveMotionActor> companion_;
};
}  // namespace

// F00989 func_800ACF54, BATTLE/BATTLE.PRG/40564.c:2548-2560.
void SaveLiveActorGroundedTransform(VS::Actors::ActorRecord* actor) {
  Require(actor != nullptr, "Grounded transform requires an actor");
  auto& instance = actor->instance;
  const auto history = SaveActorGroundedTransform(
      {.tile = {instance.current_tile_x, instance.current_tile_layer,
                instance.current_tile_z, instance.reserved5f},
       .position = {instance.position.vx, instance.position.vy,
                    instance.position.vz},
       .facing = instance.facing,
       .support_layer = instance.support_layer});
  instance.previous_tile_x = history.previous_tile.x;
  instance.previous_tile_layer = history.previous_tile.layer;
  instance.previous_tile_z = history.previous_tile.z;
  instance.reserved63 = history.previous_tile.spare;
  // Original 0x4C/0x50 history packs X/Y then Z; 0x52 stores facing.
  instance.support_offset = std::bit_cast<std::int32_t>(
      std::array{history.position[0], history.position[1]});
  instance.reserved50 = history.position[2];
  if (history.saved_placement) {
    actor->saved_placement =
        std::bit_cast<std::int32_t>(*history.saved_placement);
  }
  instance.previous_facing = history.previous_facing;
}

// decomp: BATTLE.PRG func_800B63AC (src/BATTLE/BATTLE.PRG/4A0A8.c:340)
void BeginLiveTileJump(int actor_id, int* target, int speed, int facing) {
  BeginLiveTileJump(actor_id, Tile(target), speed, facing);
}

void BeginLiveTileJump(int actor_id, PackedTile target, int speed, int facing) {
  LiveMotionActor actor(actor_id);
  BeginTileJump(actor, target, speed, facing);
}

// decomp: BATTLE.PRG func_800B6404 (src/BATTLE/BATTLE.PRG/4A0A8.c:368)
void BeginLiveAirborneTileJump(int actor_id, int* target, int speed,
                               int facing) {
  BeginLiveAirborneTileJump(actor_id, Tile(target), speed, facing);
}

void BeginLiveAirborneTileJump(int actor_id, PackedTile target, int speed,
                               int facing) {
  LiveMotionActor actor(actor_id);
  BeginAirborneTileJump(actor, target, speed, facing);
}

// decomp: BATTLE.PRG func_800B6438 (src/BATTLE/BATTLE.PRG/4A0A8.c:383)
void BeginLiveNavigationJump(int actor_id, int* target, int speed, int facing) {
  BeginLiveNavigationJump(actor_id, Tile(target), speed, facing);
}

void BeginLiveNavigationJump(int actor_id, PackedTile target, int speed,
                             int facing) {
  LiveMotionActor actor(actor_id);
  BeginNavigationJump(actor, target, speed, facing);
}

// decomp: BATTLE.PRG func_800B6580 (src/BATTLE/BATTLE.PRG/4A0A8.c:490)
void BeginLiveTeleport(int actor_id, int* target, int facing) {
  BeginLiveTeleport(actor_id, Tile(target), facing);
}

void BeginLiveTeleport(int actor_id, PackedTile target, int facing) {
  LiveMotionActor actor(actor_id);
  BeginTeleport(actor, target, facing);
}

// decomp: BATTLE.PRG func_800B65E8 (src/BATTLE/BATTLE.PRG/4A0A8.c:522)
void BeginLiveTileTravel(int actor_id, int* target, int facing) {
  BeginLiveTileTravel(actor_id, Route(target), facing);
}

void BeginLiveTileTravel(int actor_id, const MotionVector& route, int facing) {
  LiveMotionActor actor(actor_id);
  BeginTileTravel(actor, route, facing);
}

// decomp: BATTLE.PRG func_800B65F4 (src/BATTLE/BATTLE.PRG/4A0A8.c:531)
void BeginLiveTimedTravel(int actor_id, int* target, int facing) {
  BeginLiveTimedTravel(actor_id, Route(target), facing);
}

void BeginLiveTimedTravel(int actor_id, const MotionVector& route, int facing) {
  LiveMotionActor actor(actor_id);
  BeginTimedTravel(actor, route, facing);
}

// decomp: BATTLE.PRG func_800B6500 (src/BATTLE/BATTLE.PRG/4A0A8.c:445)
void QueueLiveGroundOffset(int actor_id, SVECTOR* step) {
  QueueLiveGroundOffset(actor_id, Step(step));
}

void QueueLiveGroundOffset(int actor_id, const MotionVector& step) {
  LiveMotionActor actor(actor_id);
  QueueGroundOffset(actor, step);
}

// decomp: BATTLE.PRG func_800B6530 (src/BATTLE/BATTLE.PRG/4A0A8.c:459)
void QueueLiveMovementVector(int actor_id, SVECTOR* step, int facing) {
  QueueLiveMovementVector(actor_id, Step(step), facing);
}

void QueueLiveMovementVector(int actor_id, const MotionVector& step,
                             int facing) {
  LiveMotionActor actor(actor_id);
  QueueMovementVector(actor, step, facing);
}

// decomp: BATTLE.PRG func_800B653C (src/BATTLE/BATTLE.PRG/4A0A8.c:467)
void QueueLiveFlightMovement(int actor_id, SVECTOR* step, int speed,
                             int facing) {
  QueueLiveFlightMovement(actor_id, Step(step), speed, facing);
}

void QueueLiveFlightMovement(int actor_id, const MotionVector& step, int speed,
                             int facing) {
  LiveMotionActor actor(actor_id);
  QueueFlightMovement(actor, step, speed, facing);
}

// decomp: BATTLE.PRG func_800B6634 (src/BATTLE/BATTLE.PRG/4A0A8.c:551)
void TurnLiveNavigationActor(int actor_id, int facing) {
  LiveMotionActor actor(actor_id);
  TurnNavigationActor(actor, facing);
}

// decomp: BATTLE.PRG func_800B668C (src/BATTLE/BATTLE.PRG/4A0A8.c:578)
void TurnLiveActorAtSpeed(int actor_id, std::int16_t facing, int speed) {
  LiveMotionActor actor(actor_id);
  if (!TurnActorAtSpeed(actor, facing, speed)) {
    VS::Core::Errorf("actors", "Actor %d turn has zero angular speed",
                     actor_id);
    std::abort();
  }
}
}  // namespace VS::Actors
