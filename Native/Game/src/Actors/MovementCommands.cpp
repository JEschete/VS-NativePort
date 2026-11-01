// Hand-maintained (tools/port/owned.csv).
#include <cstdint>
#include <cstdlib>

#include "Actors/MovementCommand.hpp"
#include "Core/Diagnostics.h"

namespace VS::Actors {
namespace {
constexpr int kOrientationBits = 16;

void Store(ActorMovementRequests& requests, int actor_id,
           const MovementCommand& command) {
  if (!requests.StoreMovementRequest(actor_id, command)) {
    Core_Errorf("actors", "Movement command references missing actor %d",
                actor_id);
    std::abort();
  }
}
}  // namespace

MovementCommand MakeTileTargetCommand(int direction, int distance, int facing) {
  const auto orientation =
      static_cast<std::uint32_t>(direction) |
      (static_cast<std::uint32_t>(facing) << kOrientationBits);
  return {kMovementRequestTileTarget, static_cast<std::int16_t>(orientation),
          static_cast<std::int16_t>(orientation >> kOrientationBits),
          static_cast<std::uint32_t>(distance)};
}

// decomp: BATTLE.PRG func_800B64CC (src/BATTLE/BATTLE.PRG/4A0A8.c:428).
void QueueNavigationTileTarget(ActorMovementRequests& requests, int actor_id,
                               int direction, int distance, int facing) {
  Store(requests, actor_id, MakeTileTargetCommand(direction, distance, facing));
}

// decomp: BATTLE.PRG func_800B64A8 (src/BATTLE/BATTLE.PRG/4A0A8.c:415). The
// original queues a tile target and then clears its mode; nothing observes the
// request in between, so one idle write is equivalent.
void SetDirectMovement(ActorMovementRequests& requests, int actor_id,
                       int direction, int speed) {
  MovementCommand command = MakeTileTargetCommand(direction, speed, direction);
  command.mode = kMovementRequestIdle;
  Store(requests, actor_id, command);
}
}  // namespace VS::Actors
