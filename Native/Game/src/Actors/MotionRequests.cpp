// Hand-maintained (tools/port/owned.csv).
// C counterparts: rood-boi pc/src/battle_motion.c:64-85,112-123,
// revision 3be5ba38b008cad919a2b1ca196432c8d4817335 (CC0-1.0).
#include "Actors/MotionRequests.h"

#include <bit>
#include <cstdint>
#include <cstdlib>
#include <iterator>

extern "C" {
#include "Actors/ActorRecord.h"
#include "Actors/Animation.h"
#include "Actors/Movement.h"
}

namespace {
enum class MotionMode : u_char { kGround = 2, kFlight = 3, kVector = 4 };
constexpr int kSmallTurnRate = 32;
constexpr int kLargeTurnRate = 128;
constexpr unsigned kLargeActorRadius = 128;

Actors_ActorRecord& Actor(int actor_id) {
  if (actor_id < 0 ||
      static_cast<unsigned>(actor_id) >= std::size(Actors_actorRecords) ||
      Actors_actorRecords[actor_id] == nullptr) {
    Core_Debugf("actors", "Motion request requires actor %d", actor_id);
    std::abort();
  }
  return *Actors_actorRecords[actor_id];
}

std::int16_t Half(int value) {
  return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(value));
}

SVECTOR Step(const SVECTOR* step) {
  if (step == nullptr) {
    Core_Debugf("actors", "Motion request requires a native step vector");
    std::abort();
  }
  return *step;
}

void SetMode(Actors_MovementRequest& request, MotionMode mode) {
  request.mode = static_cast<u_char>(mode);
  request.reserved1 = 0;
  request.reserved2 = 0;
  request.reserved3 = 0;
}

void QueueFlight(Actors_ActorRecord& actor, const SVECTOR& step, int speed,
                 int facing, MotionMode mode) {
  actor.turnState.distance = static_cast<unsigned>(speed);
  actor.turnState.facing = Half(facing);
  actor.turnState.step = step;
  actor.instance.motionLocked = 1;
  SetMode(actor.turnState, mode);
}
}  // namespace

// decomp: func_800B6500 (src/BATTLE/BATTLE.PRG/4A0A8.c:445).
void Actors_QueueGroundOffset(int actor_id, SVECTOR* step) {
  const auto value = Step(step);
  auto& request = Actor(actor_id).turnState;
  request.step = value;
  SetMode(request, MotionMode::kGround);
}

// decomp: func_800B653C (src/BATTLE/BATTLE.PRG/4A0A8.c:467).
void Actors_QueueFlightMovement(int actor_id, SVECTOR* step, int speed,
                                int facing) {
  const auto value = Step(step);
  QueueFlight(Actor(actor_id), value, speed, facing, MotionMode::kFlight);
}

// decomp: func_800B6530 (src/BATTLE/BATTLE.PRG/4A0A8.c:459).
void Actors_QueueMovementVector(int actor_id, SVECTOR* step, int facing) {
  const auto value = Step(step);
  QueueFlight(Actor(actor_id), value, facing, facing, MotionMode::kVector);
}

// decomp: func_800B6634 (src/BATTLE/BATTLE.PRG/4A0A8.c:551).
void Actors_TurnNavigationActor(int actor_id, int facing) {
  const auto& actor = Actor(actor_id);
  Actors_TurnActorToAtRate(actor_id, Half(facing),
                           actor.collisionRadius < kLargeActorRadius
                               ? kSmallTurnRate
                               : kLargeTurnRate);
  Actors_PlayActorTurnAnimation(actor_id, facing);
}

// decomp: func_800B668C (src/BATTLE/BATTLE.PRG/4A0A8.c:578).
void Actors_TurnActorAtSpeed(int actor_id, std::int16_t facing, int speed) {
  Actor(actor_id);
  if (speed == 0) {
    Core_Debugf("actors", "Actor %d turn has zero angular speed", actor_id);
    std::abort();
  }
  Actors_TurnActorToAtAngularSpeed(actor_id, facing, speed);
  Actors_PlayActorTurnAnimation(actor_id, facing);
}
