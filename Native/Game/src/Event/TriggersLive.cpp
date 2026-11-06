#include "Event/TriggersLive.hpp"

#include <cstdint>

extern "C" {
#include "Event/Triggers.h"
}

namespace VS::Event {
namespace {
constexpr int kActorInteractionTriggerRequest = 4;
}

bool EvaluateLiveActorRoomEventTrigger(int actor_id) {
  const auto context =
      reinterpret_cast<u_char*>(static_cast<std::intptr_t>(actor_id));
  return Event_EvaluateRoomEventTriggers(kActorInteractionTriggerRequest, 0,
                                         context, 0) == 1;
}
}  // namespace VS::Event
