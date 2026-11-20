#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <span>

#include "Battle/Hud.hpp"
#include "Battle/HudRendering.hpp"
#include "Render/Display.hpp"
#include "Render/Frame.hpp"
#include "Render/Render.hpp"
#include "Render/UiPrimitives.hpp"

namespace hud_rendering_fixture {
inline constexpr std::size_t kHudWordBytes = sizeof(std::uint32_t);
inline constexpr std::size_t kHudOrderingWords = 0x2088 / kHudWordBytes;
inline constexpr std::size_t kHudPrimitiveWords = 0x20000 / kHudWordBytes;
inline constexpr std::size_t kHudSceneHead = 4;
inline constexpr std::size_t kHudOverlayHead = 0x2084 / kHudWordBytes;
inline constexpr std::uint32_t kEndLink = 0xFFFFFF;

class LiveFrame {
 public:
  std::array<std::uint32_t, kHudOrderingWords> ordering{};
  std::array<std::uint32_t, kHudPrimitiveWords> primitives{};

  LiveFrame()
      : saved_scratch_(VS::Render::FrameScratchpad()),
        saved_frame_(VS::Render::frameBuf),
        saved_ordering_(VS::Render::legacyOrderingTables[0]),
        saved_primitives_(VS::Render::legacyPrimitiveBuffers[0]),
        saved_slide_(VS::Battle::hudSlideOffset) {
    CaptureLayout();
    ordering.fill(kEndLink);
    VS::Render::frameBuf = 0;
    VS::Render::legacyOrderingTables[0] = ordering.data();
    VS::Render::legacyPrimitiveBuffers[0] = primitives.data();
    VS::Render::FrameScratchpad().primitive_cursor = primitives.data();
    VS::Render::FrameScratchpad().ordering_table =
        ordering.data() + kHudSceneHead;
    VS::Render::FrameScratchpad().overlay_ordering_table =
        ordering.data() + kHudOverlayHead;
    VS::Battle::hudSlideOffset = 0;
    SeedLayout();
  }
  ~LiveFrame() {
    RestoreLayout();
    VS::Battle::hudSlideOffset = saved_slide_;
    VS::Render::FrameScratchpad() = saved_scratch_;
    VS::Render::frameBuf = saved_frame_;
    VS::Render::legacyOrderingTables[0] = saved_ordering_;
    VS::Render::legacyPrimitiveBuffers[0] = saved_primitives_;
  }
  LiveFrame(const LiveFrame&) = delete;
  LiveFrame& operator=(const LiveFrame&) = delete;
  LiveFrame(LiveFrame&&) = delete;
  LiveFrame& operator=(LiveFrame&&) = delete;

  [[nodiscard]] std::size_t UsedWords() const {
    return static_cast<std::uint32_t*>(
               VS::Render::FrameScratchpad().primitive_cursor) -
           primitives.data();
  }
  void Disconnect() {
    VS::Render::FrameScratchpad().ordering_table = nullptr;
    VS::Render::FrameScratchpad().overlay_ordering_table = nullptr;
  }
  void FillBuffer() {
    VS::Render::FrameScratchpad().primitive_cursor =
        primitives.data() + primitives.size();
  }

 private:
  VS::Render::FrameScratch saved_scratch_;
  int saved_frame_;
  void* saved_ordering_;
  void* saved_primitives_;
  int saved_slide_;
  std::array<int, VS::Battle::kHudGaugeCount> positions_{}, labels_{},
      palette_{};
  std::array<char, VS::Battle::kHudGaugeCount> widths_{}, textures_{};
  std::array<std::uint16_t, VS::Battle::kHudStatusIconCount> icons_{};

  static void SeedLayout() {
    auto positions = std::span(VS::Battle::hudGaugePositions);
    auto labels = std::span(VS::Battle::hudGaugeLabelPositions);
    auto widths = std::span(VS::Battle::hudGaugeLabelWidths);
    auto textures = std::span(VS::Battle::hudGaugeLabelTexU);
    auto palette = std::span(VS::Render::kStatGaugeColors);
    for (std::size_t index = 0; index < VS::Battle::kHudGaugeCount; ++index) {
      positions[index] = 0x0012000A;
      labels[index] = 0x000A000A;
      widths[index] = static_cast<char>(0xFE);
      textures[index] = static_cast<char>(0xE1);
      palette[index] = 0x00123456;
    }
    std::ranges::fill(VS::Battle::statusIconTexOffsets, std::uint16_t{0x1080});
  }

  void CaptureLayout() {
    std::ranges::copy(VS::Battle::hudGaugePositions, positions_.begin());
    std::ranges::copy(VS::Battle::hudGaugeLabelPositions, labels_.begin());
    std::ranges::copy(VS::Render::kStatGaugeColors, palette_.begin());
    std::ranges::copy(VS::Battle::hudGaugeLabelWidths, widths_.begin());
    std::ranges::copy(std::span(VS::Battle::hudGaugeLabelTexU)
                          .first<VS::Battle::kHudGaugeCount>(),
                      textures_.begin());
    std::ranges::copy(VS::Battle::statusIconTexOffsets, icons_.begin());
  }
  void RestoreLayout() {
    std::ranges::copy(positions_, std::begin(VS::Battle::hudGaugePositions));
    std::ranges::copy(labels_, std::begin(VS::Battle::hudGaugeLabelPositions));
    std::ranges::copy(palette_, std::begin(VS::Render::kStatGaugeColors));
    std::ranges::copy(widths_, std::begin(VS::Battle::hudGaugeLabelWidths));
    std::ranges::copy(textures_, std::begin(VS::Battle::hudGaugeLabelTexU));
    std::ranges::copy(icons_, std::begin(VS::Battle::statusIconTexOffsets));
  }
};
}  // namespace hud_rendering_fixture
