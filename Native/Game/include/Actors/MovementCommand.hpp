#pragma once

#include <cstdint>

// The movement request an actor's turn planner consumes. The actor table owns
// the request slots; the command policy only sees them through
// ActorMovementRequests, so it reaches no global table or PS1 address.
namespace VS::Actors {
inline constexpr std::uint8_t kMovementRequestIdle = 0;
inline constexpr std::uint8_t kMovementRequestTileTarget = 1;

// A queued request: the travel direction, the facing to finish on and the
// distance (or speed) to cover. Writing it clears the request's three
// reserved bytes and leaves its remaining fields alone.
struct MovementCommand {
  std::uint8_t mode = kMovementRequestIdle;
  std::int16_t direction = 0;
  std::int16_t facing = 0;
  std::uint32_t distance = 0;

  friend bool operator==(const MovementCommand&,
                         const MovementCommand&) = default;
};

// The actor movement request slots a command is written to.
class ActorMovementRequests {
 public:
  virtual ~ActorMovementRequests() = default;
  // Stores a command in an actor's request; returns false when the actor ID is
  // outside the table or the slot holds no actor.
  virtual bool StoreMovementRequest(int actor_id,
                                    const MovementCommand& command) = 0;
};

// The live actor table's movement requests.
[[nodiscard]] ActorMovementRequests& ActorRecordMovementRequests();

// Packs a tile-target request the way the original does: direction and facing
// share one 32-bit word, so direction bits above 16 also reach the facing.
[[nodiscard]] MovementCommand MakeTileTargetCommand(int direction, int distance,
                                                    int facing);

// Queues a tile target for an actor (func_800B64CC). A missing actor aborts.
void QueueNavigationTileTarget(ActorMovementRequests& requests, int actor_id,
                               int direction, int distance, int facing);

// Writes a direct-movement request: the tile-target packing with the request
// left idle (func_800B64A8). A missing actor aborts.
void SetDirectMovement(ActorMovementRequests& requests, int actor_id,
                       int direction, int speed);
}  // namespace VS::Actors
