#pragma once

#include <cstdint>

#include "Input/ControllerLiveState.hpp"

namespace VS::Menu::Framework {
template <typename World>
class LiveDetailInputWorld : public World {
 public:
  std::uint32_t HeldButtons() override {
    return static_cast<std::uint32_t>(VS::Input::buttonsState);
  }
  std::uint32_t RepeatButtons() override {
    return static_cast<std::uint32_t>(VS::Input::buttonRepeat);
  }
  std::uint32_t PressedButtons() override { return VS::Input::buttonsPressed; }
};
}  // namespace VS::Menu::Framework
