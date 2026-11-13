#include "Render/UiTexturedQuads.hpp"

#include <cstddef>
#include <cstdint>

#include "Render/UiPackets.hpp"

namespace VS::Render {
namespace {
constexpr std::uint32_t kQuadLength = 9;
constexpr std::uint32_t kQuadCode = 0x2C;
constexpr std::uint32_t kRawTexture = 1;
constexpr std::uint32_t kTranslucent = 2;
constexpr std::uint32_t kHalfMask = 0xFFFF;
constexpr std::uint32_t kColorMask = 0xFFFFFF;
constexpr int kByteShift = 8;
constexpr int kHalfShift = 16;
constexpr int kCommandShift = 24;
constexpr std::size_t kTopLeftPosition = 2;
constexpr std::size_t kTopLeftTexture = 3;
constexpr std::size_t kTopRightPosition = 4;
constexpr std::size_t kTopRightTexture = 5;
constexpr std::size_t kBottomLeftPosition = 6;
constexpr std::size_t kBottomLeftTexture = 7;
constexpr std::size_t kBottomRightPosition = 8;
constexpr std::size_t kBottomRightTexture = 9;

std::uint32_t Position(std::int16_t x, std::int16_t y) {
  return static_cast<std::uint16_t>(x) |
         (static_cast<std::uint32_t>(static_cast<std::uint16_t>(y))
          << kHalfShift);
}

std::uint32_t Texel(std::uint8_t u, std::uint8_t v) {
  return u | (static_cast<std::uint32_t>(v) << kByteShift);
}
}  // namespace

// FT4 fields used by func_80094E18/func_8009506C/func_800957D0/
// func_800970BC (src/BATTLE/BATTLE.PRG/2842C.c:2138-2208,2312-2330,2864-2879).
UiTexturedQuadPacket BuildUiTexturedQuadPacket(const UiTexturedQuad& quad,
                                               UiTexturedQuadPacket storage,
                                               std::uint32_t next_link) {
  const auto& texture = quad.texture;
  const auto right_u = static_cast<std::uint8_t>(texture.u + texture.width);
  const auto bottom_v = static_cast<std::uint8_t>(texture.v + texture.height);
  const auto code = kQuadCode | (quad.raw_texture ? kRawTexture : 0U) |
                    (quad.translucent ? kTranslucent : 0U);
  storage[0] = (kQuadLength << kCommandShift) | (next_link & kPacketLinkMask);
  storage[1] = (storage[1] & kColorMask) | (code << kCommandShift);
  if (quad.tint) {
    const auto& tint = quad.tint.value();
    storage[1] = tint.r | (static_cast<std::uint32_t>(tint.g) << kByteShift) |
                 (static_cast<std::uint32_t>(tint.b) << kHalfShift) |
                 (code << kCommandShift);
  }
  storage[kTopLeftPosition] = Position(quad.top_left.x, quad.top_left.y);
  storage[kTopRightPosition] = Position(quad.bottom_right.x, quad.top_left.y);
  storage[kBottomLeftPosition] = Position(quad.top_left.x, quad.bottom_right.y);
  storage[kBottomRightPosition] =
      Position(quad.bottom_right.x, quad.bottom_right.y);
  storage[kTopLeftTexture] =
      Texel(texture.u, texture.v) |
      (static_cast<std::uint32_t>(texture.clut) << kHalfShift);
  storage[kTopRightTexture] =
      Texel(right_u, texture.v) |
      (static_cast<std::uint32_t>(texture.texture_page) << kHalfShift);
  storage[kBottomLeftTexture] =
      (storage[kBottomLeftTexture] & ~kHalfMask) | Texel(texture.u, bottom_v);
  storage[kBottomRightTexture] =
      (storage[kBottomRightTexture] & ~kHalfMask) | Texel(right_u, bottom_v);
  return storage;
}
}  // namespace VS::Render
