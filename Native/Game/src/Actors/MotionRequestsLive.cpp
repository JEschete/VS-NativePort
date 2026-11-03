// Hand-maintained (tools/port/owned.csv).
// Live binding for the typed motion requests (MotionRequests.cpp).
#include "Actors/MotionRequestsLive.hpp"

#include <array>
#include <bit>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <memory>

extern "C" {
#include "Actors/ActorRecord.h"
#include "Actors/Animation.h"
#include "Actors/Movement.h"
#include "Core/Diagnostics.h"
}

namespace VS::Actors {
namespace {
void Require(bool valid, const char* message) {
  if (!valid) {
    Core_Debugf("actors", "%s", message);
    std::abort();
  }
}

Actors_ActorRecord& Actor(int actor_id) {
  if (actor_id < 0 ||
      static_cast<unsigned>(actor_id) >= std::size(Actors_actorRecords) ||
      Actors_actorRecords[actor_id] == nullptr) {
    Core_Debugf("actors", "Motion request requires actor %d", actor_id);
    std::abort();
  }
  return *Actors_actorRecords[actor_id];
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

void LogJumpRequest(const Actors_ActorRecord& actor, JumpEntry entry,
                    const PackedTile& target, int speed, std::int16_t facing) {
  Core_Debugf(
      "actors.jump",
      "request actor=%d entry=%s target=[%d,%d,%d,%d] speed=%d facing=%d "
      "position=[%d,%d,%d] phase=%d locked=%d airborne=%d profile=%d",
      actor.instance.slot, EntryName(entry), target.x, target.layer, target.z,
      target.spare, speed, facing, actor.instance.position.vx,
      actor.instance.position.vy, actor.instance.position.vz,
      actor.instance.movementMode, actor.instance.motionLocked,
      actor.instance.isJumping, actor.jumpProfile);
}

void LogJumpResult(const Actors_ActorRecord& actor) {
  Core_Debugf(
      "actors.jump",
      "result actor=%d phase=%d countdown=%d destination=[%d,%d] "
      "destination_mode=%d jump_frames=%d root=[%d,%d,%d] landing_pending=%d",
      actor.instance.slot, actor.instance.movementMode,
      actor.instance.jumpCountdown, actor.destinationX, actor.destinationZ,
      actor.destinationMode, actor.jumpFrames, actor.instance.rootMotion.vx,
      actor.instance.rootMotion.vy, actor.instance.rootMotion.vz,
      actor.landingPending);
}

void SetMode(Actors_MovementRequest& request, MotionMode mode) {
  request.mode = static_cast<u_char>(mode);
  request.reserved1 = 0;
  request.reserved2 = 0;
  request.reserved3 = 0;
}

class LiveMotionActor final : public MotionActor {
 public:
  explicit LiveMotionActor(int actor_id)
      : id_(actor_id), actor_(Actor(actor_id)) {}

  [[nodiscard]] bool Jumping() const override {
    return actor_.instance.isJumping != 0;
  }
  [[nodiscard]] bool MotionLocked() const override {
    return actor_.instance.motionLocked != 0;
  }
  void SetJumpFlags(bool motion_locked, bool jumping) override {
    actor_.instance.motionLocked = motion_locked ? 1 : 0;
    actor_.instance.isJumping = jumping ? 1 : 0;
  }
  void SetJumpProfile(int profile) override {
    actor_.jumpProfile = static_cast<u_char>(profile);
  }
  void PlayAnimation(int animation, int blend_frames) override {
    Actors_PlayActorAnimation(id_, animation, 0, blend_frames);
  }
  void JumpToTile(JumpEntry entry, PackedTile target, int speed,
                  std::int16_t facing) override {
    LogJumpRequest(actor_, entry, target, speed, facing);
    auto bytes = std::bit_cast<std::array<u_char, sizeof(target)>>(target);
    Actors_BeginActorJumpToTile(&actor_, bytes.data(), speed, facing);
    LogJumpResult(actor_);
  }

  [[nodiscard]] bool Reacting() const override {
    return actor_.instance.reactionState != 0;
  }
  void StartTeleport(const MotionVector& route) override {
    actor_.instance.turnFrames = 0;
    actor_.routeTarget = Native(route);
    actor_.instance.reactionState = 1;
    actor_.instance.poseState = 0;
    actor_.instance.wasDrawn = 0;
  }
  [[nodiscard]] MotionActor* TeleportCompanion() override {
    if (actor_.companionSlot < 2) return nullptr;
    companion_ = std::make_unique<LiveMotionActor>(actor_.companionSlot);
    return companion_.get();
  }
  void StartTravel(const MotionVector& route, TravelPose pose) override {
    actor_.routeTarget = Native(route);
    actor_.instance.poseState = static_cast<u_char>(pose);
  }

  void QueueGroundOffset(const MotionVector& step) override {
    actor_.turnState.step = Native(step);
    SetMode(actor_.turnState, MotionMode::kGround);
  }
  void QueueMovement(const QueuedMovement& movement) override {
    actor_.turnState.distance = movement.distance;
    actor_.turnState.facing = movement.facing;
    actor_.turnState.step = Native(movement.step);
    actor_.instance.motionLocked = 1;
    SetMode(actor_.turnState, movement.mode);
  }

  [[nodiscard]] unsigned CollisionRadius() const override {
    return actor_.collisionRadius;
  }
  void TurnAtRate(std::int16_t facing, int rate) override {
    Actors_TurnActorToAtRate(id_, facing, rate);
  }
  void TurnAtAngularSpeed(std::int16_t facing, int speed) override {
    Actors_TurnActorToAtAngularSpeed(id_, facing, speed);
  }
  void PlayTurnAnimation(int facing) override {
    Actors_PlayActorTurnAnimation(id_, facing);
  }

 private:
  int id_;
  Actors_ActorRecord& actor_;
  std::unique_ptr<LiveMotionActor> companion_;
};
}  // namespace

// F00989 func_800ACF54, BATTLE/BATTLE.PRG/40564.c:2548-2560.
void SaveLiveActorGroundedTransform(Actors_ActorRecord* actor) {
  Require(actor != nullptr, "Grounded transform requires an actor");
  auto& instance = actor->instance;
  const auto history = SaveActorGroundedTransform(
      {.tile = {instance.currentTileX, instance.currentTileLayer,
                instance.currentTileZ, instance.reserved5F},
       .position = {instance.position.vx, instance.position.vy,
                    instance.position.vz},
       .facing = instance.facing,
       .support_layer = instance.supportLayer});
  instance.previousTileX = history.previous_tile.x;
  instance.previousTileLayer = history.previous_tile.layer;
  instance.previousTileZ = history.previous_tile.z;
  instance.reserved63 = history.previous_tile.spare;
  // Original 0x4C/0x50 history packs X/Y then Z; 0x52 stores facing.
  instance.supportOffset = std::bit_cast<std::int32_t>(
      std::array{history.position[0], history.position[1]});
  instance.reserved50 = history.position[2];
  if (history.saved_placement) {
    actor->savedPlacement =
        std::bit_cast<std::int32_t>(*history.saved_placement);
  }
  instance.previousFacing = history.previous_facing;
}

// decomp: BATTLE.PRG func_800B63AC (src/BATTLE/BATTLE.PRG/4A0A8.c:340)
void BeginLiveTileJump(int actor_id, int* target, int speed, int facing) {
  LiveMotionActor actor(actor_id);
  BeginTileJump(actor, Tile(target), speed, facing);
}

// decomp: BATTLE.PRG func_800B6404 (src/BATTLE/BATTLE.PRG/4A0A8.c:368)
void BeginLiveAirborneTileJump(int actor_id, int* target, int speed,
                               int facing) {
  LiveMotionActor actor(actor_id);
  BeginAirborneTileJump(actor, Tile(target), speed, facing);
}

// decomp: BATTLE.PRG func_800B6438 (src/BATTLE/BATTLE.PRG/4A0A8.c:383)
void BeginLiveNavigationJump(int actor_id, int* target, int speed, int facing) {
  LiveMotionActor actor(actor_id);
  BeginNavigationJump(actor, Tile(target), speed, facing);
}

// decomp: BATTLE.PRG func_800B6580 (src/BATTLE/BATTLE.PRG/4A0A8.c:490)
void BeginLiveTeleport(int actor_id, int* target, int facing) {
  LiveMotionActor actor(actor_id);
  BeginTeleport(actor, Tile(target), facing);
}

// decomp: BATTLE.PRG func_800B65E8 (src/BATTLE/BATTLE.PRG/4A0A8.c:522)
void BeginLiveTileTravel(int actor_id, int* target, int facing) {
  LiveMotionActor actor(actor_id);
  BeginTileTravel(actor, Route(target), facing);
}

// decomp: BATTLE.PRG func_800B65F4 (src/BATTLE/BATTLE.PRG/4A0A8.c:531)
void BeginLiveTimedTravel(int actor_id, int* target, int facing) {
  LiveMotionActor actor(actor_id);
  BeginTimedTravel(actor, Route(target), facing);
}

// decomp: BATTLE.PRG func_800B6500 (src/BATTLE/BATTLE.PRG/4A0A8.c:445)
void QueueLiveGroundOffset(int actor_id, SVECTOR* step) {
  const auto value = Step(step);
  LiveMotionActor actor(actor_id);
  QueueGroundOffset(actor, value);
}

// decomp: BATTLE.PRG func_800B6530 (src/BATTLE/BATTLE.PRG/4A0A8.c:459)
void QueueLiveMovementVector(int actor_id, SVECTOR* step, int facing) {
  const auto value = Step(step);
  LiveMotionActor actor(actor_id);
  QueueMovementVector(actor, value, facing);
}

// decomp: BATTLE.PRG func_800B653C (src/BATTLE/BATTLE.PRG/4A0A8.c:467)
void QueueLiveFlightMovement(int actor_id, SVECTOR* step, int speed,
                             int facing) {
  const auto value = Step(step);
  LiveMotionActor actor(actor_id);
  QueueFlightMovement(actor, value, speed, facing);
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
    Core_Debugf("actors", "Actor %d turn has zero angular speed", actor_id);
    std::abort();
  }
}
}  // namespace VS::Actors
