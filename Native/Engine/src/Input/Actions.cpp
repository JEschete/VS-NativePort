#include "Input/Actions.hpp"

#include <optional>
#include <string_view>

namespace VS::Input {

std::string_view ActionName(Action action) noexcept {
  switch (action) {
    case Action::kL2:
      return "L2";
    case Action::kR2:
      return "R2";
    case Action::kL1:
      return "L1";
    case Action::kR1:
      return "R1";
    case Action::kTriangle:
      return "Triangle";
    case Action::kCircle:
      return "Circle";
    case Action::kCross:
      return "Cross";
    case Action::kSquare:
      return "Square";
    case Action::kSelect:
      return "Select";
    case Action::kL3:
      return "L3";
    case Action::kR3:
      return "R3";
    case Action::kStart:
      return "Start";
    case Action::kUp:
      return "Up";
    case Action::kRight:
      return "Right";
    case Action::kDown:
      return "Down";
    case Action::kLeft:
      return "Left";
  }
  return {};
}

std::optional<Action> ParseAction(std::string_view name) noexcept {
  for (const Action action : kActions) {
    if (ActionName(action) == name) {
      return action;
    }
  }
  return std::nullopt;
}

}  // namespace VS::Input
