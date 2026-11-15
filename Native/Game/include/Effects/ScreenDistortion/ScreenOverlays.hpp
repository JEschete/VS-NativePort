#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstdint>

#include "Effects/ScreenDistortion/RadialWarp.hpp"
#include "Render/UiPackets.hpp"

// The SCREFF2 controls that select the screen overlays, and the two 2D
// overlays drawn from them: the framebuffer feedback tiles and the elliptical
// vignette. The cores read and write a typed copy of the controller words and
// link packets through Render::PacketQueue chains, so they reach no overlay
// controller, ordering table or primitive cursor (ScreenOverlaysLive.cpp).
namespace VS::Effects::ScreenDistortion {
// The controller words SCREFF2 owns (D_800EB9B4_t +1 to +0xB).
struct ScreenOverlayControls {
  // +1: nonzero while the feedback texture pages are lent.
  std::uint8_t feedback_pages_reserved{};
  // +2: the feedback layer; 0 disables the feedback tiles.
  std::uint16_t feedback_layer{};
  // +4 and +6: the radial warp words func_800F9BD8 reads.
  RadialWarpSettings radial_warp{};
  // +8: nonzero draws the vignette.
  std::int32_t vignette_enabled{};
};

// A range of lendable texture pages and the owner tag it is lent to.
struct TexturePageLease {
  std::uint32_t first_page{};
  std::uint32_t page_count{};
  int owner_tag{};
};

// Pages 27-31 (VRAM x 704-1023, y 256-511), lent to owner 6 while feedback
// is enabled: {first_page, page_count, owner_tag}.
inline constexpr TexturePageLease kFeedbackTexturePages{0x1B, 5, 6};

class FeedbackTexturePages {
 public:
  FeedbackTexturePages() = default;
  FeedbackTexturePages(const FeedbackTexturePages&) = delete;
  FeedbackTexturePages& operator=(const FeedbackTexturePages&) = delete;
  FeedbackTexturePages(FeedbackTexturePages&&) = delete;
  FeedbackTexturePages& operator=(FeedbackTexturePages&&) = delete;
  virtual ~FeedbackTexturePages() = default;

  virtual void Reserve(const TexturePageLease& lease) = 0;
  virtual void Retire(const TexturePageLease& lease) = 0;
};

// func_800F9800: stores the layer's low halfword. The full word decides
// whether feedback is on: the first nonzero layer lends the feedback pages and
// a zero layer while they are lent retires them.
void SetFeedbackLayer(ScreenOverlayControls& controls,
                      std::int32_t ordering_layer, FeedbackTexturePages& pages);

// The settings the original's two halfword stores keep from argument words
// (the event camera passes whole words).
[[nodiscard]] RadialWarpSettings RadialWarpFromWords(
    std::int32_t scale_word, std::int32_t growth_word) noexcept;
// func_800F9BC0.
void SetRadialWarp(ScreenOverlayControls& controls,
                   RadialWarpSettings settings) noexcept;
// func_800F9DE8.
void SetVignetteEnabled(ScreenOverlayControls& controls,
                        std::int32_t enabled) noexcept;

// The scene ordering-table slot, relative to the scene table head, that the
// feedback capture packet is linked in front of: the layer less 3 (negative
// below layer 3, as in the original).
[[nodiscard]] std::int32_t FeedbackOrderingSlot(
    std::uint16_t feedback_layer) noexcept;

// func_800F986C: 315 sprites that blend the captured frame back from the
// feedback pages at (704, 256), then the capture packet in front of
// `feedback_entry` (which redirects drawing to the capture area and clears
// it) and the packet that restores the display buffer's drawing area in front
// of `scene_head`. `frame_buffer` is the frame's buffer index; the restored
// area starts at x 320 for buffer 0 and x 0 otherwise. The two queues may be
// the same chain.
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
void DrawFeedbackTiles(std::int32_t frame_buffer,
                       Render::PacketQueue& scene_head,
                       Render::PacketQueue& feedback_entry);

// func_800F9DF8: the packed screen point (x in the low halfword, y in the
// high) at `angular_step` (64 per turn) on an ellipse of `radius` around
// (160, 112), widened 11/8 horizontally and scalloped every 16 steps.
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
[[nodiscard]] std::uint32_t EllipsePoint(std::int32_t angular_step,
                                         std::int32_t radius) noexcept;

// func_800F9EBC: four shaded triangles fanning from the screen corner
// `packed_corner_xy` to the outer ellipse, starting at `first_angular_step`.
void AppendVignetteCorner(std::uint32_t packed_corner_xy,
                          std::int32_t first_angular_step,
                          Render::PacketQueue& overlay);

// func_800F9FB8: the four corner fans, then 64 shaded quads between the outer
// and inner ellipses, all in front of the overlay chain.
void DrawVignette(Render::PacketQueue& overlay);
}  // namespace VS::Effects::ScreenDistortion
