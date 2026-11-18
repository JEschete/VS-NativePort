#pragma once

#include <cstdint>

namespace VS::Event {

std::int16_t EvaluateRoomEventTriggersEntry(int trigger_request,
                                            int event_or_actor_id,
                                            std::uint8_t* trigger_context,
                                            int context_kind);

}  // namespace VS::Event
