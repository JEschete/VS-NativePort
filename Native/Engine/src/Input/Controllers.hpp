#pragma once
// The legacy port records and libpad buffers shared by the copied code and
// libpad (ControllerLiveState.hpp, ControllerLive.cpp). The original built
// with unsigned char, so every byte is std::uint8_t.
#include <array>
#include <cstddef>
#include <cstdint>

#include "Input/Controller.hpp"

namespace VS::Input {
// decomp: portInfo_t (src/SLUS_010.40/main.h:19), 0xB bytes. right_stick_x/y
// hold the physical left stick and left_stick_x/y the right one, as the
// original named them.
struct LegacyControllerPort {
  std::uint8_t controller_type;
  std::uint8_t extended_type;
  std::uint8_t button_bits;
  std::uint8_t right_stick_x;
  std::uint8_t right_stick_y;
  std::uint8_t left_stick_x;
  std::uint8_t left_stick_y;
  std::uint8_t mode_lock;
  std::array<std::uint8_t, kMotorCount> actuator_levels;
  std::uint8_t connected;
};

// decomp: _actParams: the motor alignment table libpad reads.
inline constexpr std::size_t kActuatorAlignmentBytes = 12;
using ActuatorAlignment = std::array<std::uint8_t, kActuatorAlignmentBytes>;

// decomp: vs_main_padBuffer: libpad's receive buffer for each port.
extern std::array<PadPacket, kControllerPortCount> legacyPadResponseBytes;
extern ActuatorAlignment legacyActuatorAlignment;
}  // namespace VS::Input
