#pragma once
// The screen offset a framebuffer effect applies while it copies the frame.
#include <cstddef>
#include <cstdint>

namespace VS::Render {
// decomp: D_800F1A68_t (src/BATTLE/BATTLE.PRG/146C.h:726)
struct FramebufferEffectTranslation {
  std::int32_t x;
  std::int32_t y;
  std::int32_t reserved08;
  std::int32_t reserved0c;
};

namespace layout {
inline constexpr std::size_t kFramebufferEffectTranslationBytes = 0x10;
static_assert(sizeof(FramebufferEffectTranslation) ==
              kFramebufferEffectTranslationBytes);
}  // namespace layout
}  // namespace VS::Render
