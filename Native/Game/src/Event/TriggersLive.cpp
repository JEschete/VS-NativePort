#include "Event/TriggersLive.hpp"

#include <cstdint>

extern "C" {
#include "Core/Diagnostics.h"
#include "Event/ScriptVm.h"
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

RoomEventLoad LoadLiveRoomEventSections(RoomEventInput input) {
  constexpr std::int16_t kWaitingForRoom = 1;
  if (Event_roomChangeState == kWaitingForRoom) {
    input.entry_script = {};
  }
  if (const auto error = ValidateRoomEventInput(input)) {
    Core_Warnf("event", "Room event import refused: section %d, %zu bytes",
               static_cast<int>(error->reason), error->bytes);
    return std::unexpected(*error);
  }
  const int result = Event_LoadRoomEventSections(
      const_cast<std::uint8_t*>(input.triggers.data()),
      static_cast<std::uint16_t>(input.triggers.size()),
      const_cast<std::uint8_t*>(input.entry_script.data()),
      static_cast<std::uint16_t>(input.entry_script.size()),
      const_cast<std::uint8_t*>(input.doors.data()),
      static_cast<std::uint16_t>(input.doors.size()),
      input.first_entry ? 1 : 0);
  return result == 0 ? RoomEventLoadResult::kReady
                     : RoomEventLoadResult::kContinuingEvent;
}
}  // namespace VS::Event
