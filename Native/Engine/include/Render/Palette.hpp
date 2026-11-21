#pragma once
// A GPU color word: red, green and blue with the primitive code byte.
#include <cstddef>
#include <cstdint>

namespace VS::Render {
// decomp: D_800F4538 (src/BATTLE/BATTLE.PRG/entity.h:524; was
// D_800F4538_unk58_2)
struct ColorCode {
  std::uint8_t red = 0;
  std::uint8_t green = 0;
  std::uint8_t blue = 0;
  std::uint8_t code = 0;

  friend bool operator==(const ColorCode&, const ColorCode&) = default;
};

namespace layout {
inline constexpr std::size_t kColorCodeBytes = 0x4;
static_assert(sizeof(ColorCode) == kColorCodeBytes);
}  // namespace layout
}  // namespace VS::Render
