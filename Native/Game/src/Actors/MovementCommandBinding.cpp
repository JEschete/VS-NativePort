// Hand-maintained (tools/port/owned.csv).
#include <iterator>

#include "Actors/ActorRecord.hpp"
#include "Actors/Movement.hpp"
#include "Actors/MovementCommand.hpp"
#include "Legacy/Psx.h"

namespace {
class ActorTableMovementRequests final
    : public VS::Actors::ActorMovementRequests {
 public:
  bool StoreMovementRequest(
      int actor_id, const VS::Actors::MovementCommand& command) override {
    if (actor_id < 0 ||
        actor_id >= static_cast<int>(std::size(VS::Actors::actorRecords))) {
      return false;
    }
    VS::Actors::ActorRecord* actor = VS::Actors::actorRecords[actor_id];
    if (actor == nullptr) return false;
    VS::Actors::MovementRequest& request = actor->turn_state;
    request.mode = command.mode;
    request.reserved1 = 0;
    request.reserved2 = 0;
    request.reserved3 = 0;
    request.direction = command.direction;
    request.facing = command.facing;
    request.distance = command.distance;
    return true;
  }
};
}  // namespace

VS::Actors::ActorMovementRequests& VS::Actors::ActorRecordMovementRequests() {
  static ActorTableMovementRequests requests;
  return requests;
}
