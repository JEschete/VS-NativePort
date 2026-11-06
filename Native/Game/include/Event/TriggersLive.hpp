#pragma once

extern "C++" {
#include "Event/RoomTriggers.hpp"

namespace VS::Event {
bool EvaluateLiveActorRoomEventTrigger(int actor_id);
// Bounded import boundary while the legacy VM still owns the destinations.
[[nodiscard]] RoomEventLoad LoadLiveRoomEventSections(RoomEventInput input);
}  // namespace VS::Event
}
