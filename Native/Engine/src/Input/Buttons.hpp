#pragma once
// The legacy stick and pressed-button views the copied code reads
// (ControllerLiveState.hpp). ControllerLive.cpp checks their PS1 layout.
#include <cstdint>

namespace VS::Input {
// decomp: vs_main_stickPos (src/SLUS_010.40/main.h:389). The l fields hold
// the physical right stick and the r fields the left one, as the original
// named them.
struct StickPosition {
  int l_stick_x;
  int l_stick_y;
  int r_stick_x;
  int r_stick_y;
};

// decomp: vs_main_buttonsPressed.ports[0].lowByte (src/SLUS_010.40/main.h:482):
// the first pad's L2, R2, L1, R1, Triangle, Circle, Cross and Square bits.
[[nodiscard]] constexpr std::uint8_t LowButtonByte(
    std::uint32_t buttons) noexcept {
  return static_cast<std::uint8_t>(buttons);
}
}  // namespace VS::Input
