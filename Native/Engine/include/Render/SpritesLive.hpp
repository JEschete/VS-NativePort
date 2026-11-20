#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstddef>
#include <cstdint>

#include "Legacy/Psx.hpp"
#include "Render/Sprites.hpp"

// Sprites queued into the current frame's primitive buffer. A packet that does
// not fit in the buffer, or that has no buffer or chain, is not linked and the
// caller receives discard storage, so its texture and draw-mode patches stay
// in bounds (#265).
namespace VS::Render {
// Room for `words` words at the cursor of the current frame's primitive
// buffer, or null; the caller advances the cursor past what it writes.
[[nodiscard]] std::uint32_t* ReserveLivePrimitiveWords(std::size_t words);
std::uint32_t* QueueLiveDefaultSprite(int wh, int xy);
std::uint32_t* QueueLiveSpriteWithTexturePage(int brightness_transparency,
                                              int xy, int wh,
                                              std::uint32_t* next_prim);
std::uint32_t* QueueLiveSprite(int brightness_transparency, int xy, int wh,
                               std::uint32_t* next_prim);
void LoadLiveFrameViewMatrix();
// Sprites refused since start-up.
[[nodiscard]] std::uint64_t DroppedLiveSpriteCount();
}  // namespace VS::Render
