// Hand-maintained (tools/port/owned.csv).
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include "Render/DisplayControl.hpp"

// The display, frame and primitive services ENDING.PRG's staff roll
// (Credits/StaffRollCore.hpp) and clear-results screen (ClearResultsCore.hpp)
// share. Primitives are typed records: an ordering slot counts entries from
// the frame's scene ordering table, and the Live adapter (EndingLive.cpp)
// turns each record into its GPU packet.
namespace VS::Ending {
inline constexpr std::size_t kQuadCorners = 4;

struct ScreenPoint {
  std::int16_t x = 0;
  std::int16_t y = 0;

  friend bool operator==(const ScreenPoint&, const ScreenPoint&) = default;
};

struct TexelPoint {
  std::uint8_t u = 0;
  std::uint8_t v = 0;

  friend bool operator==(const TexelPoint&, const TexelPoint&) = default;
};

enum class TextureDepth : std::uint8_t { k4Bit = 0, k8Bit = 1, k16Bit = 2 };
enum class BlendMode : std::uint8_t {
  kAverage = 0,
  kAdd = 1,
  kSubtract = 2,
  kAddQuarter = 3
};

// getTPage: the draw-mode bits selecting a texture page at a VRAM origin.
[[nodiscard]] constexpr std::uint16_t TexturePageBits(TextureDepth depth,
                                                      BlendMode blend,
                                                      int origin_x,
                                                      int origin_y) noexcept {
  constexpr unsigned kFieldMask = 3;
  constexpr unsigned kDepthShift = 7;
  constexpr unsigned kBlendShift = 5;
  constexpr unsigned kRowHighBit = 0x100;
  constexpr unsigned kRowHighShift = 4;
  constexpr unsigned kColumnMask = 0x3FF;
  constexpr unsigned kColumnShift = 6;
  constexpr unsigned kRowTopBit = 0x200;
  constexpr unsigned kRowTopShift = 2;
  const auto x = static_cast<unsigned>(origin_x);
  const auto y = static_cast<unsigned>(origin_y);
  return static_cast<std::uint16_t>(
      ((static_cast<unsigned>(depth) & kFieldMask) << kDepthShift) |
      ((static_cast<unsigned>(blend) & kFieldMask) << kBlendShift) |
      ((y & kRowHighBit) >> kRowHighShift) |
      ((x & kColumnMask) >> kColumnShift) | ((y & kRowTopBit) << kRowTopShift));
}

// getClut: the palette word for a CLUT at a VRAM position.
[[nodiscard]] constexpr std::uint16_t PaletteBits(int x, int y) noexcept {
  constexpr int kRowShift = 6;
  constexpr int kColumnShift = 4;
  constexpr int kColumnMask = 0x3F;
  return static_cast<std::uint16_t>((y << kRowShift) |
                                    ((x >> kColumnShift) & kColumnMask));
}

// A textured rectangle (SPRT): flat brightness on every channel.
struct SpriteDraw {
  int ordering_slot = 0;
  ScreenPoint position{};
  TexelPoint texel{};
  std::int16_t width = 0;
  std::int16_t height = 0;
  std::uint16_t palette = 0;
  std::uint8_t brightness = 0;
  bool semi_transparent = false;

  friend bool operator==(const SpriteDraw&, const SpriteDraw&) = default;
};

// An 8x8 textured rectangle drawn unshaded (SPRT_8 with shade-texture set).
struct GlyphDraw {
  int ordering_slot = 0;
  ScreenPoint position{};
  TexelPoint texel{};
  std::uint16_t palette = 0;

  friend bool operator==(const GlyphDraw&, const GlyphDraw&) = default;
};

// A textured quad (POLY_FT4); corners and texels run top-left, top-right,
// bottom-left, bottom-right.
struct TexturedQuadDraw {
  int ordering_slot = 0;
  std::array<ScreenPoint, kQuadCorners> corners{};
  std::array<TexelPoint, kQuadCorners> texels{};
  std::uint16_t palette = 0;
  std::uint16_t texture_page = 0;
  std::uint8_t brightness = 0;
  bool semi_transparent = false;

  friend bool operator==(const TexturedQuadDraw&,
                         const TexturedQuadDraw&) = default;
};

// A grey Gouraud quad (POLY_G4), one brightness per corner.
struct GradientQuadDraw {
  int ordering_slot = 0;
  std::array<ScreenPoint, kQuadCorners> corners{};
  std::array<std::uint8_t, kQuadCorners> brightness{};
  bool semi_transparent = false;

  friend bool operator==(const GradientQuadDraw&,
                         const GradientQuadDraw&) = default;
};

// A flat grey rectangle (TILE).
struct TileDraw {
  int ordering_slot = 0;
  ScreenPoint position{};
  std::int16_t width = 0;
  std::int16_t height = 0;
  std::uint8_t brightness = 0;
  bool semi_transparent = false;

  friend bool operator==(const TileDraw&, const TileDraw&) = default;
};

// A draw-mode command selecting texture_page for the primitives queued after
// it in the same slot.
struct TexturePageChange {
  int ordering_slot = 0;
  std::uint16_t texture_page = 0;

  friend bool operator==(const TexturePageChange&,
                         const TexturePageChange&) = default;
};

// The two buffers' draw and display areas, the screen window, the viewport
// and the clear colour InitializeDisplay sets up.
struct DisplayLayout {
  std::int16_t width = 0;
  std::int16_t viewport_height = 0;
  std::array<ScreenPoint, 2> draw_origins{};
  std::array<ScreenPoint, 2> display_origins{};
  Render::ScreenRect screen{};
  Render::ClearColor background{};
};

// The draw-environment fields the staff roll overrides (_setDrawEnv).
struct DrawEnvironment {
  bool clear_background = false;
  bool dither = false;
  bool draw_on_display = false;
  Render::ClearColor background{};
  std::uint16_t texture_page = 0;
};

// The frame's ordering tables: `entries` are cleared in reverse, the overlay
// table starts overlay_offset entries in and the scene table scene_offset
// entries in.
struct OrderingTableLayout {
  int buffer = 0;
  int entries = 0;
  int overlay_offset = 0;
  int scene_offset = 0;
};

enum class FrameBufferKind : std::uint8_t { kPrimitives, kOrderingTable };

class DisplayHost {
 public:
  virtual ~DisplayHost() = default;

  virtual void DrawSprite(const SpriteDraw& sprite) = 0;
  virtual void DrawGlyph(const GlyphDraw& glyph) = 0;
  virtual void DrawTexturedQuad(const TexturedQuadDraw& quad) = 0;
  virtual void DrawGradientQuad(const GradientQuadDraw& quad) = 0;
  virtual void DrawTile(const TileDraw& tile) = 0;
  virtual void ChangeTexturePage(const TexturePageChange& change) = 0;

  virtual void SetDisplayEnabled(bool enabled) = 0;
  virtual void SetGeometryOffset(int x, int y) = 0;
  virtual void SetProjectionDistance(int distance) = 0;
  virtual void ApplyDisplayLayout(const DisplayLayout& layout) = 0;
  virtual void ApplyDrawEnvironment(int buffer,
                                    const DrawEnvironment& environment) = 0;
  virtual void SetInterlaced(int buffer, bool interlaced) = 0;
  // Fills the VRAM area with the colour.
  virtual void ClearVram(const Render::ScreenRect& area,
                         Render::ClearColor color) = 0;
  // Uploads the halfword pixels covering the VRAM area.
  virtual void LoadImage(const Render::ScreenRect& area,
                         std::span<const std::byte> pixels) = 0;
  // Makes the buffer's display and draw environments current.
  virtual void ShowFrameBuffer(int buffer) = 0;
  [[nodiscard]] virtual int FrameBuffer() const = 0;
  virtual void SetFrameBuffer(int buffer) = 0;
  // Draws the frame's ordering table from the scene slot.
  virtual void DrawOrderingTable(int ordering_slot) = 0;
  // Advances play time and pending loads by one frame; returns the vsync
  // result.
  virtual int AdvanceFrameClock() = 0;
  virtual void ResetPresentedFrameCount() = 0;
  virtual void CountPresentedFrame() = 0;
  virtual void UpdateControllers() = 0;
  [[nodiscard]] virtual std::uint32_t ButtonsPressed() const = 0;
  virtual void BeginOrderingTables(const OrderingTableLayout& layout) = 0;
  virtual void ResetHeap() = 0;
  virtual void AllocateFrameBuffer(FrameBufferKind kind, int buffer,
                                   std::size_t bytes) = 0;
  // VSync(1): the vertical-blank counter.
  [[nodiscard]] virtual int VerticalBlankCounter() = 0;
  // VSync(0): waits for the next vertical blank.
  virtual void WaitForVerticalBlank() = 0;
};
}  // namespace VS::Ending
