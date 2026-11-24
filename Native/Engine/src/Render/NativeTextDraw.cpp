#include "Render/NativeTextDraw.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include "Render/UiTexturedQuads.hpp"

namespace VS::Render {
namespace {
constexpr int kPaletteX = 896, kPaletteY = 222;
constexpr std::uint16_t kPage = 0x2D;
constexpr std::uint16_t kPalette = (kPaletteY << 6) | (kPaletteX >> 4);
constexpr int kAtlasWidth = 256, kAtlasHeight = 220;
constexpr int kPaletteColors = 16, kLastTexel = 255;
constexpr int kScreenWidth = 320, kScreenHeight = 224;

std::expected<std::vector<UiTexturedQuadPacket>, std::string> Packets(
    std::span<const NativeTextDraw> draws, const Content::NativeImage& atlas) {
  if (atlas.width > kAtlasWidth || atlas.height > kAtlasHeight ||
      atlas.depth != Content::ImageDepth::kIndexed4 ||
      atlas.palettes.size() != 1 || atlas.PaletteEntries() != kPaletteColors) {
    return std::unexpected(
        "native text backend: atlas exceeds save-page binding");
  }
  if (auto valid = Content::ValidateImage(atlas); !valid) {
    return std::unexpected(valid.error());
  }
  std::vector<UiTexturedQuadPacket> packets;
  for (const auto& draw : draws) {
    if (draw.layout == nullptr) {
      return std::unexpected("native text backend: missing layout");
    }
    for (const auto& glyph : draw.layout->glyphs) {
      const auto& source = glyph.source;
      const double left = draw.x + glyph.x, top = draw.y + glyph.y;
      const double right = left + glyph.width, bottom = top + glyph.height;
      if (!std::isfinite(left) || !std::isfinite(top) ||
          !std::isfinite(right) || !std::isfinite(bottom) || left < 0 ||
          top < 0 || right > kScreenWidth || bottom > kScreenHeight ||
          right <= left || bottom <= top || source.x < 0 || source.y < 0 ||
          source.width <= 0 || source.height <= 0 ||
          static_cast<std::int64_t>(source.x) + source.width > atlas.width ||
          static_cast<std::int64_t>(source.y) + source.height > atlas.height ||
          static_cast<std::int64_t>(source.x) + source.width > kLastTexel ||
          static_cast<std::int64_t>(source.y) + source.height > kLastTexel) {
        return std::unexpected(
            "native text backend: glyph rectangle out of bounds");
      }
      const UiTexturedQuad quad{
          .top_left = {static_cast<std::int16_t>(std::lround(left)),
                       static_cast<std::int16_t>(std::lround(top))},
          .bottom_right = {static_cast<std::int16_t>(std::lround(right)),
                           static_cast<std::int16_t>(std::lround(bottom))},
          .texture = {static_cast<std::uint8_t>(source.x),
                      static_cast<std::uint8_t>(source.y),
                      static_cast<std::uint8_t>(source.width),
                      static_cast<std::uint8_t>(source.height), kPage,
                      kPalette},
          .translucent = true};
      packets.push_back(BuildUiTexturedQuadPacket(quad, {}, kPacketLinkMask));
    }
  }
  return packets;
}

}  // namespace

std::expected<void, std::string> DrawNativeText(
    ImmediatePacketTarget& target, std::span<const NativeTextDraw> draws,
    const Content::NativeImage& atlas) {
  auto packets = Packets(draws, atlas);
  if (!packets) return std::unexpected(packets.error());
  for (const auto& packet : *packets) target.Submit(packet);
  return {};
}

}  // namespace VS::Render
