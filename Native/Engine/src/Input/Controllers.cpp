// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Input/Controllers.hpp"

// decomp: vs_main_padBuffer (vs_main_padBuffer@SLUS_010.40)
std::array<VS::Input::PadPacket, VS::Input::kControllerPortCount> VS::Input::legacyPadResponseBytes{};

// decomp: _actParams (_actParams@SLUS_010.40)
VS::Input::ActuatorAlignment VS::Input::legacyActuatorAlignment = { 0, 1, 255, 255, 255, 255, 0,
    0, 0, 0, 0, 0 };
