#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

#include "Render/ScreenOverlay.hpp"
#include "Render/TextureRegionDraw.hpp"

namespace VS::Render {
struct UiTexturedQuad {
  ScreenVertex top_left{};
  ScreenVertex bottom_right{};
  TextureRegion texture{};
  std::optional<QuadColor> tint = std::nullopt;
  bool raw_texture = true;
  bool translucent = false;
};

inline constexpr std::size_t kUiTexturedQuadWords = 10;
using UiTexturedQuadPacket = std::array<std::uint32_t, kUiTexturedQuadWords>;

// The raw-texture form leaves RGB unchanged. Both forms preserve the unused
// high halves of the final two UV words in the current primitive storage.
[[nodiscard]] UiTexturedQuadPacket BuildUiTexturedQuadPacket(
    const UiTexturedQuad& quad, UiTexturedQuadPacket storage,
    std::uint32_t next_link);
}  // namespace VS::Render
