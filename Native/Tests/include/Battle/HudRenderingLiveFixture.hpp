#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <span>

#include "Battle/HudRendering.hpp"
extern "C" {
#include "Battle/Hud.h"
#include "Render/Display.h"
#include "Render/Frame.h"
#include "Render/Render.h"
#include "Render/UiPrimitives.h"
}

namespace hud_rendering_fixture {
inline constexpr std::size_t kHudOrderingWords = 0x2088 / sizeof(u_long);
inline constexpr std::size_t kHudPrimitiveWords = 0x20000 / sizeof(u_long);
inline constexpr std::size_t kHudSceneHead = 4;
inline constexpr std::size_t kHudOverlayHead = 0x2084 / sizeof(u_long);
inline constexpr std::uint32_t kEndLink = 0xFFFFFF;

class LiveFrame {
 public:
  std::array<u_long, kHudOrderingWords> ordering{};
  std::array<u_long, kHudPrimitiveWords> primitives{};

  LiveFrame()
      : saved_scratch_(Render_frameScratch),
        saved_frame_(Render_frameBuf),
        saved_ordering_(Render_legacyOrderingTables[0]),
        saved_primitives_(Render_legacyPrimitiveBuffers[0]),
        saved_slide_(Battle_hudSlideOffset) {
    CaptureLayout();
    ordering.fill(kEndLink);
    Render_frameBuf = 0;
    Render_legacyOrderingTables[0] = ordering.data();
    Render_legacyPrimitiveBuffers[0] = primitives.data();
    Render_frameScratch.primitiveCursor = primitives.data();
    Render_frameScratch.orderingTable = ordering.data() + kHudSceneHead;
    Render_frameScratch.overlayOrderingTable =
        ordering.data() + kHudOverlayHead;
    Battle_hudSlideOffset = 0;
    SeedLayout();
  }
  ~LiveFrame() {
    RestoreLayout();
    Battle_hudSlideOffset = saved_slide_;
    Render_frameScratch = saved_scratch_;
    Render_frameBuf = saved_frame_;
    Render_legacyOrderingTables[0] = saved_ordering_;
    Render_legacyPrimitiveBuffers[0] = saved_primitives_;
  }
  LiveFrame(const LiveFrame&) = delete;
  LiveFrame& operator=(const LiveFrame&) = delete;
  LiveFrame(LiveFrame&&) = delete;
  LiveFrame& operator=(LiveFrame&&) = delete;

  [[nodiscard]] std::size_t UsedWords() const {
    return static_cast<u_long*>(Render_frameScratch.primitiveCursor) -
           primitives.data();
  }
  void Disconnect() {
    Render_frameScratch.orderingTable = nullptr;
    Render_frameScratch.overlayOrderingTable = nullptr;
  }
  void FillBuffer() {
    Render_frameScratch.primitiveCursor = primitives.data() + primitives.size();
  }

 private:
  Render_FrameScratch saved_scratch_;
  int saved_frame_;
  void* saved_ordering_;
  void* saved_primitives_;
  int saved_slide_;
  std::array<int, VS::Battle::kHudGaugeCount> positions_{}, labels_{},
      palette_{};
  std::array<char, VS::Battle::kHudGaugeCount> widths_{}, textures_{};
  std::array<u_short, VS::Battle::kHudStatusIconCount> icons_{};

  static void SeedLayout() {
    auto positions = std::span(Battle_hudGaugePositions);
    auto labels = std::span(Battle_hudGaugeLabelPositions);
    auto widths = std::span(Battle_hudGaugeLabelWidths);
    auto textures = std::span(Battle_hudGaugeLabelTexU);
    auto palette = std::span(Render_kStatGaugeColors);
    for (std::size_t index = 0; index < VS::Battle::kHudGaugeCount; ++index) {
      positions[index] = 0x0012000A;
      labels[index] = 0x000A000A;
      widths[index] = static_cast<char>(0xFE);
      textures[index] = static_cast<char>(0xE1);
      palette[index] = 0x00123456;
    }
    std::ranges::fill(Battle_statusIconTexOffsets, u_short{0x1080});
  }

  void CaptureLayout() {
    std::ranges::copy(Battle_hudGaugePositions, positions_.begin());
    std::ranges::copy(Battle_hudGaugeLabelPositions, labels_.begin());
    std::ranges::copy(Render_kStatGaugeColors, palette_.begin());
    std::ranges::copy(Battle_hudGaugeLabelWidths, widths_.begin());
    std::ranges::copy(
        std::span(Battle_hudGaugeLabelTexU).first<VS::Battle::kHudGaugeCount>(),
        textures_.begin());
    std::ranges::copy(Battle_statusIconTexOffsets, icons_.begin());
  }
  void RestoreLayout() {
    std::ranges::copy(positions_, std::begin(Battle_hudGaugePositions));
    std::ranges::copy(labels_, std::begin(Battle_hudGaugeLabelPositions));
    std::ranges::copy(palette_, std::begin(Render_kStatGaugeColors));
    std::ranges::copy(widths_, std::begin(Battle_hudGaugeLabelWidths));
    std::ranges::copy(textures_, std::begin(Battle_hudGaugeLabelTexU));
    std::ranges::copy(icons_, std::begin(Battle_statusIconTexOffsets));
  }
};
}  // namespace hud_rendering_fixture
