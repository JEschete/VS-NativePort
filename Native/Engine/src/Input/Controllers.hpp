#pragma once
// The legacy port records shared by the copied code and the controller service
// (ControllerLiveState.hpp, ControllerLive.cpp). The original built with
// unsigned char, so every byte is std::uint8_t.
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
}  // namespace VS::Input
