#pragma once

#include <expected>
#include <span>
#include <string>

#include "Content/NativeImage.hpp"
#include "Render/NativeTextLayout.hpp"
#include "Render/UiPackets.hpp"

namespace VS::Render {
struct NativeTextDraw {
  const TextLayout* layout = nullptr;
  int x = 0;
  int y = 0;
  bool clip_to_viewport = false;
};
// Compatibility backend only: source rectangles become textured quads.
// It does not parse string bytecode or consult mutable Game font state.
[[nodiscard]] std::expected<void, std::string> DrawNativeText(
    ImmediatePacketTarget& target, std::span<const NativeTextDraw> draws,
    const Content::NativeImage& atlas);
// Temporarily binds the owned PNG atlas to the save-font page and restores
// both pixels and palette before returning. No renderer state survives reset.
[[nodiscard]] std::expected<void, std::string> DrawLiveNativeText(
    std::span<const NativeTextDraw> draws, const Content::NativeImage& atlas);
// Gazette builds ordering tables; submit its owned native overlay after the
// table, not before queued backgrounds. No borrowed layout survives this call.
void QueueLiveNativeText(std::span<const NativeTextDraw> draws,
                         const Content::NativeImage& atlas);
void FlushLiveNativeText();
void ResetQueuedNativeText();
}  // namespace VS::Render
