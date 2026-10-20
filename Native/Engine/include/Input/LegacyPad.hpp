#pragma once

// Host side of the libpad shim (C++ only): the desktop loop hands the sampled
// controller state to the legacy pad buffers once per logical tick, before
// resuming the game fiber.

#include "Input/Pad.hpp"

namespace VS::Legacy {

struct ActuatorState {
  u16 low_frequency = 0;
  u16 high_frequency = 0;
};

// `connected` false reports an empty port. The state is written in the original
// receive-buffer format.
void FeedPad(int port, const Input::PadState& state, bool connected);
ActuatorState ReadActuators(int port);

}  // namespace VS::Legacy
