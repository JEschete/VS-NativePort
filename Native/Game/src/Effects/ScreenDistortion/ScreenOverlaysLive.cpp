// Hand-maintained (tools/port/owned.csv).
#include "Effects/ScreenDistortion/ScreenOverlaysLive.hpp"

#include <cstdint>

#include "Core/Log.hpp"
#include "Effects/ScreenDistortion/ScreenOverlays.hpp"
#include "Render/TexturePagesLive.hpp"
#include "Render/UiPacketsLive.hpp"
extern "C" {
#include "Effects/ScreenEffects.h"
#include "Render/Display.h"
#include "Render/Render.h"
#include "Render/UiPrimitives.h"
}

namespace VS::Effects::ScreenDistortion {
namespace {
class LiveFeedbackTexturePages final : public FeedbackTexturePages {
 public:
  void Reserve(const TexturePageLease& lease) override {
    static_cast<void>(Render::ReserveLiveTexturePageRange(
        lease.first_page, lease.page_count, lease.owner_tag));
  }
  void Retire(const TexturePageLease& lease) override {
    static_cast<void>(Render::RetireLiveTexturePageRange(
        lease.first_page, lease.page_count, lease.owner_tag));
  }
};

ScreenOverlayControls ReadControls(const Effects_ScreenEffectOverlay& overlay) {
  return {.feedback_pages_reserved = overlay.opaque1,
          .feedback_layer = overlay.feedbackLayer,
          .radial_warp = {.scale = static_cast<std::uint16_t>(
                              overlay.radialWarpArg0),
                          .growth = overlay.radialWarpArg1},
          .vignette_enabled = overlay.vignetteEnabled};
}

void WriteControls(Effects_ScreenEffectOverlay& overlay,
                   const ScreenOverlayControls& controls) {
  overlay.opaque1 = controls.feedback_pages_reserved;
  overlay.feedbackLayer = controls.feedback_layer;
  overlay.radialWarpArg0 =
      static_cast<std::int16_t>(controls.radial_warp.scale);
  overlay.radialWarpArg1 = controls.radial_warp.growth;
  overlay.vignetteEnabled = controls.vignette_enabled;
}

// The active controller, or null after reporting `operation` as refused.
Effects_ScreenEffectOverlay* LiveOverlay(const char* operation) {
  if (Effects_screenEffectOverlay == nullptr) {
    VS_LOG_ERROR("effects", "screen overlay {} refused: no overlay controller",
                 operation);
  }
  return Effects_screenEffectOverlay;
}

template <typename Update>
void UpdateLiveControls(const char* operation, Update update) {
  Effects_ScreenEffectOverlay* overlay = LiveOverlay(operation);
  if (overlay == nullptr) {
    return;
  }
  ScreenOverlayControls controls = ReadControls(*overlay);
  update(controls);
  WriteControls(*overlay, controls);
}
}  // namespace

void SetLiveFeedbackLayer(int ordering_layer) {
  UpdateLiveControls("feedback layer", [&](ScreenOverlayControls& controls) {
    LiveFeedbackTexturePages pages;
    SetFeedbackLayer(controls, ordering_layer, pages);
  });
}

void DrawLiveFeedbackTiles() {
  const Effects_ScreenEffectOverlay* overlay = LiveOverlay("feedback tiles");
  if (overlay == nullptr) {
    return;
  }
  Render::LivePacketQueue scene_head(
      Render::LiveFrameOrderingEntry(Render::LiveOrderingBase::kScene, 0));
  Render::LivePacketQueue feedback_entry(Render::LiveFrameOrderingEntry(
      Render::LiveOrderingBase::kScene,
      FeedbackOrderingSlot(overlay->feedbackLayer)));
  DrawFeedbackTiles(Render_frameBuf, scene_head, feedback_entry);
}

void SetLiveRadialWarp(RadialWarpSettings settings) {
  UpdateLiveControls("radial warp", [&](ScreenOverlayControls& controls) {
    SetRadialWarp(controls, settings);
  });
}

void SetLiveRadialWarpWords(int scale_word, int growth_word) {
  SetLiveRadialWarp(RadialWarpFromWords(scale_word, growth_word));
}

void SetLiveVignetteEnabled(int enabled) {
  UpdateLiveControls("vignette", [&](ScreenOverlayControls& controls) {
    SetVignetteEnabled(controls, enabled);
  });
}

int* AppendLiveVignetteCorner(int packed_corner_xy, int first_angular_step,
                              int* packet_words, u_long* ordering_head) {
  if (packet_words == nullptr ||
      static_cast<void*>(packet_words) != Render_frameScratch.primitiveCursor) {
    VS_LOG_ERROR("effects",
                 "vignette corner refused: packets must start at the frame "
                 "primitive cursor");
    return packet_words;
  }
  Render::LivePacketQueue overlay(ordering_head);
  AppendVignetteCorner(static_cast<std::uint32_t>(packed_corner_xy),
                       first_angular_step, overlay);
  return static_cast<int*>(Render_frameScratch.primitiveCursor);
}

void DrawLiveVignette() {
  Render::LivePacketQueue overlay(
      Render::LiveFrameOrderingEntry(Render::LiveOrderingBase::kOverlay, 0));
  DrawVignette(overlay);
}
}  // namespace VS::Effects::ScreenDistortion
