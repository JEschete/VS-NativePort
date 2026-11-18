// Hand-maintained (tools/port/owned.csv).
#include "Effects/ScreenDistortion/ScreenOverlaysLive.hpp"

#include <cstdint>

#include "Boot/SessionLive.hpp"
#include "Content/CdDriveLive.hpp"
#include "Content/DiscLayout.h"
#include "Content/OverlayWorkspaceLive.hpp"
#include "Core/Log.hpp"
#include "Core/Overlays.h"
#include "DiscQueue.h"
#include "Effects/ScreenDistortion/RadialWarpLive.hpp"
#include "Effects/ScreenDistortion/ScreenOverlayLifetime.hpp"
#include "Effects/ScreenDistortion/ScreenOverlays.hpp"
#include "Effects/ScreenEffectState.hpp"
#include "Effects/ScreenEffects.h"
#include "Render/Display.h"
#include "Render/Render.h"
#include "Render/TexturePagesLive.hpp"
#include "Render/UiPacketsLive.hpp"
#include "Render/UiPrimitives.h"

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

namespace {
class LiveScreenOverlayLoader final : public ScreenOverlayLoader {
 public:
  void AcquireWorkspace(int owner_tag) override {
    static_cast<void>(Content::AcquireLiveOverlayWorkspace(owner_tag));
  }
  void ReleaseWorkspace(int owner_tag) override {
    static_cast<void>(Content::ReleaseLiveOverlayWorkspace(owner_tag));
  }
  Content_DiscQueueSlot* AllocateOverlayRead() override {
    const Content_DiscFile file = OverlayFile();
    return Content::AllocateLiveDiscRequest(&file);
  }
  void EnqueueOverlayRead(Content_DiscQueueSlot* read) override {
    Content::EnqueueLiveDiscRequest(read, Core_overlaySlots[1]);
  }
  Content_DiscQueueSlot* CompleteRefusedOverlayRead() override {
    const Content_DiscFile file = OverlayFile();
    return Content::CompleteLiveRefusedDiscRequest(&file, Core_overlaySlots[1]);
  }
  bool OverlayReadLoaded(const Content_DiscQueueSlot* read) override {
    if (read == nullptr) {
      VS_LOG_ERROR("effects", "screen overlay load poll: no read request");
      return false;
    }
    return read->state == Content_DiscRequestState_Loaded;
  }
  void ReleaseOverlayRead(Content_DiscQueueSlot* read) override {
    Content::ReleaseLiveDiscRequest(read);
  }
  void WaitForOverlayReady() override { Boot::WaitForOverlayReadyLive(); }

 private:
  static Content_DiscFile OverlayFile() {
    return {.lba = VS_SCREFF2_PRG_LBA, .size = VS_SCREFF2_PRG_SIZE};
  }
};

class LiveScreenOverlayPainter final : public ScreenOverlayPainter {
 public:
  // The live draws read the same words from the attached controller.
  void WarpPolygons(RadialWarpSettings /*settings*/) override {
    WarpLivePolygonVertices();
  }
  void DrawFeedbackTiles(std::uint16_t /*feedback_layer*/) override {
    DrawLiveFeedbackTiles();
  }
  void DrawVignette() override { DrawLiveVignette(); }
};

// The attached controller, or the defaults the next attach points at.
Effects_ScreenEffectOverlay& LiveLifetimeRecord() {
  return Effects_screenEffectOverlay != nullptr ? *Effects_screenEffectOverlay
                                                : screenEffectDefaults;
}

ScreenOverlayLifetime ReadLiveLifetime() {
  const Effects_ScreenEffectOverlay& record = LiveLifetimeRecord();
  return {.attached = Effects_screenEffectOverlay != nullptr,
          .loading = record.loading,
          .controls = ReadControls(record),
          .read = record.slot};
}

// Attaching points D_800EB9B4 at the defaults; a detached lifetime is
// written to the defaults, as the original's reset cleared them.
void WriteLiveLifetime(const ScreenOverlayLifetime& lifetime) {
  if (!lifetime.attached) {
    Effects_screenEffectOverlay = nullptr;
  } else if (Effects_screenEffectOverlay == nullptr) {
    Effects_screenEffectOverlay = &screenEffectDefaults;
  }
  Effects_ScreenEffectOverlay& record = LiveLifetimeRecord();
  record.loading = lifetime.loading;
  WriteControls(record, lifetime.controls);
  record.slot = lifetime.read;
}

template <typename Update>
void UpdateLiveLifetime(Update update) {
  ScreenOverlayLifetime lifetime = ReadLiveLifetime();
  update(lifetime);
  WriteLiveLifetime(lifetime);
}
}  // namespace

void ResetLiveScreenOverlay() {
  UpdateLiveLifetime(
      [](ScreenOverlayLifetime& lifetime) { ResetScreenOverlay(lifetime); });
}

void LoadLiveScreenOverlay(int begin_load) {
  if (LiveOverlay("load") == nullptr) {
    return;
  }
  UpdateLiveLifetime([&](ScreenOverlayLifetime& lifetime) {
    LiveScreenOverlayLoader loader;
    LoadScreenOverlay(lifetime, begin_load, loader);
  });
}

void DrawLiveScreenOverlay() {
  UpdateLiveLifetime([](ScreenOverlayLifetime& lifetime) {
    LiveScreenOverlayLoader loader;
    LiveScreenOverlayPainter painter;
    DrawScreenOverlay(lifetime, loader, painter);
  });
}

void AcquireLiveScreenOverlay() {
  UpdateLiveLifetime([](ScreenOverlayLifetime& lifetime) {
    LiveScreenOverlayLoader loader;
    AcquireScreenOverlay(lifetime, loader);
  });
}

void ReleaseLiveScreenOverlay() {
  UpdateLiveLifetime([](ScreenOverlayLifetime& lifetime) {
    LiveScreenOverlayLoader loader;
    LiveFeedbackTexturePages pages;
    ReleaseScreenOverlay(lifetime, loader, pages);
  });
}
}  // namespace VS::Effects::ScreenDistortion
