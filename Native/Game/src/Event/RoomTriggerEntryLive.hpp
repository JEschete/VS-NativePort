#pragma once

#include <stdint.h>

int16_t Event_EvaluateRoomEventTriggersEntry(int trigger_request,
                                             int event_or_actor_id,
                                             uint8_t* trigger_context,
                                             int context_kind);
