#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstdint>

#include "Legacy/Psx.h"
#include "Render/UiPackets.hpp"

// The HUD packets queued into the current frame's primitive buffer in front of
// a chain head. A packet that does not fit, or that has no chain, is dropped
// and counted rather than written past the buffer (UiPacketsLive.cpp).
namespace VS::Render {
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
// Packets refused since start-up.
[[nodiscard]] std::uint64_t DroppedLiveUiPacketCount();
}  // namespace VS::Render
