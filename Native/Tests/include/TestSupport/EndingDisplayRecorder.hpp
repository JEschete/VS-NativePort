#pragma once

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "Ending/DisplayHost.hpp"
#include "Render/TimImage.hpp"

namespace VS::test_support {
// One TIM block: its VRAM rectangle and the byte every pixel holds.
struct TimBlockSpec {
  Render::ScreenRect area{};
  std::byte fill{};
};

inline void AppendLittleEndian(std::vector<std::byte>& bytes,
                               std::uint32_t value, std::size_t count) {
  for (std::size_t index = 0; index < count; ++index) {
    bytes.push_back(static_cast<std::byte>((value >> (8 * index)) & 0xFFU));
  }
}

inline void AppendTimBlock(std::vector<std::byte>& bytes,
                           const TimBlockSpec& block) {
  const std::size_t pixels = static_cast<std::size_t>(block.area.width) *
                             static_cast<std::size_t>(block.area.height) * 2;
  AppendLittleEndian(
      bytes, static_cast<std::uint32_t>(Render::kTimBlockHeaderBytes + pixels),
      4);
  for (const std::int16_t field :
       {block.area.x, block.area.y, block.area.width, block.area.height}) {
    AppendLittleEndian(bytes, static_cast<std::uint16_t>(field), 2);
  }
  bytes.insert(bytes.end(), pixels, block.fill);
}

// A TIM's bytes: the header, the CLUT block when one is given (a 4-bit mode)
// or none (a 16-bit mode), then the image block.
[[nodiscard]] inline std::vector<std::byte> TimBytes(
    const TimBlockSpec& image,
    const std::optional<TimBlockSpec>& clut = std::nullopt) {
  constexpr std::uint32_t kTimId = 0x10;
  constexpr std::uint32_t kDirectColorMode = 2;
  std::vector<std::byte> bytes;
  AppendLittleEndian(bytes, kTimId, 4);
  AppendLittleEndian(bytes, clut ? Render::kTimClutFlag : kDirectColorMode, 4);
  if (clut) AppendTimBlock(bytes, *clut);
  AppendTimBlock(bytes, image);
  return bytes;
}

struct FrameBufferAllocation {
  Ending::FrameBufferKind kind = Ending::FrameBufferKind::kPrimitives;
  int buffer = 0;
  std::size_t bytes = 0;

  friend bool operator==(const FrameBufferAllocation&,
                         const FrameBufferAllocation&) = default;
};

// Records every DisplayHost call an ENDING.PRG core makes; Base is the core's
// host interface (DisplayHost or one derived from it).
template <typename Base>
class EndingDisplayRecorder : public Base {
 public:
  std::vector<Ending::SpriteDraw> sprites;
  std::vector<Ending::GlyphDraw> glyphs;
  std::vector<Ending::TexturedQuadDraw> textured_quads;
  std::vector<Ending::GradientQuadDraw> gradient_quads;
  std::vector<Ending::TileDraw> tiles;
  std::vector<Ending::TexturePageChange> page_changes;
  // Display, environment and frame calls in order.
  std::vector<std::string> calls;
  std::vector<Ending::DrawEnvironment> environments;
  std::vector<Render::ScreenRect> cleared_areas;
  std::vector<Render::ScreenRect> loaded_areas;
  std::vector<std::vector<std::byte>> loaded_pixels;
  std::vector<Ending::OrderingTableLayout> ordering_layouts;
  std::vector<FrameBufferAllocation> allocations;
  std::vector<int> drawn_slots;
  Ending::DisplayLayout layout{};
  std::uint32_t buttons = 0;
  int frame_buffer = 0;
  int advanced_frames = 0;
  int presented_frames = 0;
  int controller_updates = 0;
  int vertical_blank = 0;
  // VerticalBlankCounter advances by this much per call.
  int vertical_blank_step = 0;
  int vertical_blank_waits = 0;

  void DrawSprite(const Ending::SpriteDraw& sprite) override {
    sprites.push_back(sprite);
  }
  void DrawGlyph(const Ending::GlyphDraw& glyph) override {
    glyphs.push_back(glyph);
  }
  void DrawTexturedQuad(const Ending::TexturedQuadDraw& quad) override {
    textured_quads.push_back(quad);
  }
  void DrawGradientQuad(const Ending::GradientQuadDraw& quad) override {
    gradient_quads.push_back(quad);
  }
  void DrawTile(const Ending::TileDraw& tile) override {
    tiles.push_back(tile);
  }
  void ChangeTexturePage(const Ending::TexturePageChange& change) override {
    page_changes.push_back(change);
  }

  void SetDisplayEnabled(bool enabled) override {
    calls.emplace_back(enabled ? "display on" : "display off");
  }
  void SetGeometryOffset(int x, int y) override {
    calls.push_back("offset " + std::to_string(x) + " " + std::to_string(y));
  }
  void SetProjectionDistance(int distance) override {
    calls.push_back("projection " + std::to_string(distance));
  }
  void ApplyDisplayLayout(const Ending::DisplayLayout& applied) override {
    layout = applied;
    calls.emplace_back("layout");
  }
  void ApplyDrawEnvironment(
      int buffer, const Ending::DrawEnvironment& environment) override {
    environments.push_back(environment);
    calls.push_back("environment " + std::to_string(buffer));
  }
  void SetInterlaced(int buffer, bool interlaced) override {
    calls.push_back("interlace " + std::to_string(buffer) +
                    (interlaced ? " on" : " off"));
  }
  void ClearVram(const Render::ScreenRect& area,
                 Render::ClearColor /*color*/) override {
    cleared_areas.push_back(area);
  }
  void LoadImage(const Render::ScreenRect& area,
                 std::span<const std::byte> pixels) override {
    loaded_areas.push_back(area);
    loaded_pixels.emplace_back(pixels.begin(), pixels.end());
  }
  void ShowFrameBuffer(int buffer) override {
    calls.push_back("show " + std::to_string(buffer));
  }
  [[nodiscard]] int FrameBuffer() const override { return frame_buffer; }
  void SetFrameBuffer(int buffer) override { frame_buffer = buffer; }
  void DrawOrderingTable(int ordering_slot) override {
    drawn_slots.push_back(ordering_slot);
  }
  int AdvanceFrameClock() override { return ++advanced_frames; }
  void ResetPresentedFrameCount() override {
    presented_frames = 0;
    calls.emplace_back("reset presented");
  }
  void CountPresentedFrame() override { ++presented_frames; }
  void UpdateControllers() override { ++controller_updates; }
  [[nodiscard]] std::uint32_t ButtonsPressed() const override {
    return buttons;
  }
  void BeginOrderingTables(const Ending::OrderingTableLayout& tables) override {
    ordering_layouts.push_back(tables);
  }
  void ResetHeap() override { calls.emplace_back("reset heap"); }
  void AllocateFrameBuffer(Ending::FrameBufferKind kind, int buffer,
                           std::size_t bytes) override {
    allocations.push_back({kind, buffer, bytes});
  }
  [[nodiscard]] int VerticalBlankCounter() override {
    vertical_blank += vertical_blank_step;
    return vertical_blank;
  }
  void WaitForVerticalBlank() override { ++vertical_blank_waits; }
};
}  // namespace VS::test_support
