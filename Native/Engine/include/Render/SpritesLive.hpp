#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstdint>

#include "Legacy/Psx.h"
#include "Render/Sprites.hpp"

// Sprites queued into the current frame's primitive buffer. A packet that does
// not fit in the buffer, or that has no buffer or chain, is not linked and the
// caller receives discard storage, so its texture and draw-mode patches stay
// in bounds (#265).
namespace VS::Render {
u_long* QueueLiveDefaultSprite(int wh, int xy);
u_long* QueueLiveSpriteWithTexturePage(int brightness_transparency, int xy,
                                       int wh, u_long* next_prim);
u_long* QueueLiveSprite(int brightness_transparency, int xy, int wh,
                        u_long* next_prim);
void LoadLiveFrameViewMatrix();
// Sprites refused since start-up.
[[nodiscard]] std::uint64_t DroppedLiveSpriteCount();
}  // namespace VS::Render
