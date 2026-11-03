#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstdint>

#include "Legacy/Psx.h"
#include "Render/TimImage.hpp"

// Live binding for the TIM parse (TimImage.cpp) behind the original
// TIM_IMAGE call shape. The callers hand over a TIM in a buffer whose size
// they do not pass, so the binding reads only the header words the TIM must
// have to learn its declared size and parses that many bytes. A null TIM, or
// one whose declared blocks are inconsistent or exceed VRAM, is refused,
// counted and reported: its block pointers are null, which the callers
// already skip.
namespace VS::Render {
// vs_battle_parseTim (146C.c:16935) and ENDING _parseTim (D4.c:2235). The
// TIM_IMAGE pointers point into `data`, which the callers patch in place.
void ParseLiveTim(void* data, TIM_IMAGE* tim);
// TIMs refused since start-up.
[[nodiscard]] std::uint64_t RefusedLiveTimCount();
}  // namespace VS::Render
