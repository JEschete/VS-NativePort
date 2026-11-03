// Hand-maintained (tools/port/owned.csv).
// Live binding for the scaled font's glyph quads (ScaledFont.cpp): the
// current frame's primitive buffer.
#include "Render/ScaledFontLive.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "Legacy/Psx.h"
#include "Render/ScaledFont.hpp"
#include "Render/UiPackets.hpp"
#include "Render/UiPacketsLive.hpp"

namespace VS::Render {
namespace {
static_assert(sizeof(POLY_FT4) % sizeof(std::uint32_t) == 0);
constexpr std::size_t kQuadWords = sizeof(POLY_FT4) / sizeof(std::uint32_t);
}  // namespace

void LiveScaledGlyphTarget::Draw(const ScaledGlyphQuad& quad) {
  POLY_FT4 primitive{};
  setPolyFT4(&primitive);
  std::memcpy(&primitive.r0, &quad.color, sizeof quad.color);
  setXY4(&primitive, quad.left, quad.top, quad.right, quad.top, quad.left,
         quad.bottom, quad.right, quad.bottom);
  const unsigned u = quad.u;
  const unsigned v = quad.v;
  const unsigned cell = kScaledFontGlyphCellSize;
  setUV4(&primitive, u, v, u + cell, v, u, v + cell, u + cell, v + cell);
  primitive.clut = quad.clut;
  primitive.tpage = quad.texture_page;
  std::array<std::uint32_t, kQuadWords> words{};
  std::memcpy(words.data(), &primitive, sizeof primitive);
  LivePacketQueue queue(layer_);
  // addPrim: the packet keeps its length byte and links to the layer's chain.
  words[0] =
      (words[0] & ~kPacketLinkMask) | (queue.ChainLink() & kPacketLinkMask);
  queue.Emit(words);
}
}  // namespace VS::Render
