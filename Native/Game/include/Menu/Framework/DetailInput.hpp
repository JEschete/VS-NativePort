#pragma once

#include <cstdint>

namespace VS::Menu::Framework {
class DetailInputWorld {
 public:
  DetailInputWorld() = default;
  DetailInputWorld(const DetailInputWorld&) = delete;
  DetailInputWorld& operator=(const DetailInputWorld&) = delete;
  DetailInputWorld(DetailInputWorld&&) = delete;
  DetailInputWorld& operator=(DetailInputWorld&&) = delete;
  virtual ~DetailInputWorld() = default;

  [[nodiscard]] virtual std::uint32_t HeldButtons() = 0;
  [[nodiscard]] virtual std::uint32_t RepeatButtons() = 0;
  [[nodiscard]] virtual std::uint32_t PressedButtons() = 0;
  [[nodiscard]] virtual std::int32_t ItemCount() = 0;
};
}  // namespace VS::Menu::Framework
