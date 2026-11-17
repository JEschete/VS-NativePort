#pragma once

#include <cstddef>
#include <cstdint>

#include "Menu/TextBoxState.hpp"

namespace VS::Menu {
inline constexpr std::uint8_t kTextBoxOrderingBufferCount = 3;
inline constexpr std::size_t kTextBoxOrderingTags = 34;

class TextBoxUpdateHost {
 public:
  virtual ~TextBoxUpdateHost() = default;
  [[nodiscard]] virtual TextBox ReadBox(std::size_t index) const = 0;
  virtual void WriteBox(std::size_t index, const TextBox& box) = 0;
  [[nodiscard]] virtual std::int32_t TickSpeed() const = 0;
  virtual void BlendText(std::size_t index, std::int32_t scale) = 0;
  virtual void DrawInformationBackdrop(std::size_t index) = 0;
  virtual void DrawFrame(std::size_t index) = 0;
  virtual void DrawProgressiveText(std::size_t index) = 0;
  virtual void DrawScaledText(std::size_t index, std::int32_t scale) = 0;
  virtual void SubmitOrdering() = 0;
  [[nodiscard]] virtual std::uint8_t OrderingBufferIndex() const = 0;
  virtual void BeginOrderingBuffer(std::uint8_t index) = 0;
  [[nodiscard]] virtual bool TimerVisible() const = 0;
  virtual void DrawTimer() = 0;
  virtual void DrawOverlay() = 0;
  virtual void AdvanceRandomBits() = 0;
};

enum class TextBoxUpdateStatus { kComplete, kInvalidOrderingBuffer };

// F01392 func_800CB83C, src/BATTLE/BATTLE.PRG/5BF94.c:3936-4054,
// original instructions 800CB83C-800CBBCC (end exclusive).
[[nodiscard]] TextBoxUpdateStatus UpdateTextBoxes(TextBoxUpdateHost& host);
}  // namespace VS::Menu
