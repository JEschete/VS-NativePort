#pragma once

#include <cstdint>

extern "C" {
#include "Input/ControllerLiveState.h"
}

namespace VS::Menu::Framework {
template <typename World>
class LiveDetailInputWorld : public World {
 public:
  std::uint32_t HeldButtons() override {
    return static_cast<std::uint32_t>(Input_buttonsState);
  }
  std::uint32_t RepeatButtons() override {
    return static_cast<std::uint32_t>(Input_buttonRepeat);
  }
  std::uint32_t PressedButtons() override {
    return Input_buttonsPressed.allButtons;
  }
};
}  // namespace VS::Menu::Framework
