#include "Event/RoomTriggerEntryLive.h"

#include <cstdint>
#include <limits>

#include "Event/TriggersLive.hpp"

namespace {

VS::Event::RoomTriggerContext TriggerContext(int actor,
                                             const std::uint8_t* context,
                                             std::int16_t kind) {
  if (kind == 0) {
    const auto other = reinterpret_cast<std::intptr_t>(context);
    if (other < std::numeric_limits<int>::min() ||
        other > std::numeric_limits<int>::max()) {
      return std::monostate{};
    }
    return VS::Event::TriggerActorPair{actor, static_cast<int>(other)};
  }
  if (kind == 1 && context != nullptr) {
    return VS::Event::TriggerTile{context[0], context[2]};
  }
  return std::monostate{};
}

}  // namespace

// decomp: BATTLE.PRG func_800BEC58 (src/BATTLE/BATTLE.PRG/4A0A8.c:4302).
int16_t Event_EvaluateRoomEventTriggers(int trigger_request,
                                        int event_or_actor_id,
                                        uint8_t* trigger_context,
                                        int context_kind) {
  const VS::Event::RoomTriggerQuery query{
      static_cast<VS::Event::RoomTriggerRequest>(
          static_cast<std::uint16_t>(trigger_request)),
      static_cast<std::int16_t>(event_or_actor_id),
      TriggerContext(event_or_actor_id, trigger_context,
                     static_cast<std::int16_t>(context_kind))};
  switch (VS::Event::EvaluateLiveRoomEventTriggers(query)) {
    case VS::Event::RoomTriggerResult::kStarted:
      return 1;
    case VS::Event::RoomTriggerResult::kBusy:
      return 2;
    default:
      return 0;
  }
}
