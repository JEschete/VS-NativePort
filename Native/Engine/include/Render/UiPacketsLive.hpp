#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstdint>
#include <span>

#include "Legacy/Psx.h"
#include "Render/UiPackets.hpp"

// The UI packets queued into the current frame's primitive buffer in front of
// a chain head or last in the overlay bucket, drawn at once, or moved to and
// from VRAM. A packet that does not fit, or that has no chain, is dropped and
// counted rather than written past the buffer (UiPacketsLive.cpp).
namespace VS::Render {
// Packets stored in the frame's primitive buffer, each linked in front of
// `chain` (addPrim).
class LivePacketQueue final : public PacketQueue {
 public:
  explicit LivePacketQueue(u_long* chain) : chain_(chain) {}

  [[nodiscard]] std::uint32_t ChainLink() const override;
  bool Emit(std::span<const std::uint32_t> packet) override;

 private:
  u_long* chain_;
};

// The frame's ordering tables (Boot/GameLoop.cpp): the scene table
// (Render_frameScratch.orderingTable) and the overlay table in front of it.
enum class LiveOrderingBase : std::uint8_t { kScene, kOverlay };
// Entry `slot` from the base's pointer, or null when it lies outside the
// current frame's table.
[[nodiscard]] u_long* LiveFrameOrderingEntry(LiveOrderingBase base, int slot);

// Splices the packet last into the current ordering table's overlay bucket. A
// broken chain, an unknown packet or no table leaves every tag untouched.
void AppendLiveOverlayPrimitive(void* primitive);
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
void DrawLiveNumericGlyph(int font, int xy, int digit, u_long* before);
// With no chain, the digits go 0x10 bytes before the frame's ordering table.
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
int DrawLiveIntegerRightAligned(int font, int xy, int value, u_long* before);
void DrawLiveStatGauge(int color_index, int w, u_long* before, int xy);
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
void InsertLiveThreeWordPrimitive(u_long* before, int rgb0, int xy, int wh);
void InsertLiveDrawModeCommand(int tpage, u_long* before);
// The caller's pixels must cover the packed w | h << 16 size.
void UploadLiveImageRectangle(int xy, const void* pixels, int wh);
// The caller's buffer must cover the packed w | h << 16 size.
void ReadLiveImageRectangle(int xy, void* pixels, int wh);
// Copies `source` to (x, y) when the overlay bucket is drawn.
void QueueLiveTextureRectangleMove(const ScreenRect& source, std::int32_t x,
                                   std::int32_t y);
void QueueLiveTextureRectangleMove(const RECT* source, int x, int y);
// The texture page's draw-mode command linked in front of entry `ot_slot` of
// the frame's ordering table. Returns the primitive cursor after it, or the
// unchanged cursor when the entry is outside the frame's table or the
// primitive buffer is full.
void* AppendLiveTexturePage(int tpage, int ot_slot);
// Draws the sprite at once; the texture page's high half dims it.
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
void DrawLiveImmediateSprite(int xy, int uv_clut, int wh, int tpage);
// Packets refused since start-up.
[[nodiscard]] std::uint64_t DroppedLiveUiPacketCount();
}  // namespace VS::Render
