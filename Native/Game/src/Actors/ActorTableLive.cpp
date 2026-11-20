#include "Actors/ActorTableLive.hpp"

#include <iterator>

#include "Actors/ActorRecord.hpp"
#include "Core/Diagnostics.hpp"

namespace VS::Actors {
void WarnActorRefused(const char* operation, const char* reason) {
  VS::Core::Warnf("actors", "%s refused: %s", operation, reason);
}

bool IsActorTableId(int actor_id) {
  return static_cast<unsigned>(actor_id) < std::size(VS::Actors::actorRecords);
}

VS::Actors::ActorRecord* LiveActorRecord(int actor_id, const char* operation) {
  if (!IsActorTableId(actor_id)) {
    VS::Core::Warnf("actors",
                    "%s refused: actor id %d is outside the actor table",
                    operation, actor_id);
    return nullptr;
  }
  return VS::Actors::actorRecords[actor_id];
}
}  // namespace VS::Actors
