#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

#include "Render/NativeTextDraw.hpp"

namespace VS::Render {
namespace {
struct OwnedDraw {
  TextLayout layout;
  int x;
  int y;
  bool clip;
};
struct Overlay {
  Content::NativeImage atlas;
  std::vector<OwnedDraw> draws;
};
std::optional<Overlay>& Queued() {
  static std::optional<Overlay> queued;
  return queued;
}
}  // namespace
void ResetQueuedNativeText() { Queued().reset(); }
void QueueLiveNativeText(std::span<const NativeTextDraw> draws,
                         const Content::NativeImage& atlas) {
  Overlay overlay{atlas, {}};
  for (const auto& draw : draws) {
    if (draw.layout == nullptr) {
      throw std::runtime_error("native overlay: missing layout");
    }
    overlay.draws.push_back(
        {*draw.layout, draw.x, draw.y, draw.clip_to_viewport});
  }
  Queued() = std::move(overlay);
}
void FlushLiveNativeText() {
  auto overlay = std::exchange(Queued(), std::nullopt);
  if (!overlay) return;
  std::vector<NativeTextDraw> draws;
  for (const auto& draw : overlay->draws) {
    draws.push_back({&draw.layout, draw.x, draw.y, draw.clip});
  }
  if (auto result = DrawLiveNativeText(draws, overlay->atlas); !result) {
    throw std::runtime_error(result.error());
  }
}
}  // namespace VS::Render
