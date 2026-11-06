#pragma once

extern "C++" {
#include "Event/RoomTriggers.hpp"

namespace VS::Event {
bool EvaluateLiveActorRoomEventTrigger(int actor_id);
[[nodiscard]] RoomTriggerResult EvaluateLiveRoomEventTriggers(
    const RoomTriggerQuery& query);
// Production import boundary: copies exact sections into native ownership.
[[nodiscard]] RoomEventLoad LoadLiveRoomEventSections(RoomEventInput input);
// Native VM import: prepare the entry source before committing room state;
// replacing/freeing the room does not revoke a mounted script generation.
[[nodiscard]] RoomEventLoad ImportLiveRoomScripts(RoomEventInput input);
void FreeLiveRoomScripts();
}  // namespace VS::Event
}
