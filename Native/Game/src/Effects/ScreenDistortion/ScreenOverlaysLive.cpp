// Hand-maintained (tools/port/owned.csv).
#include "Effects/ScreenDistortion/ScreenOverlaysLive.hpp"

#include <cstdint>

#include "Boot/SessionLive.hpp"
#include "Content/CdDriveLive.hpp"
#include "Content/DiscLayout.h"
#include "Content/DiscQueue.hpp"
#include "Content/OverlayWorkspaceLive.hpp"
#include "Core/Log.hpp"
#include "Core/Overlays.hpp"
#include "Effects/ScreenDistortion/RadialWarpLive.hpp"
#include "Effects/ScreenDistortion/ScreenOverlayLifetime.hpp"
#include "Effects/ScreenDistortion/ScreenOverlays.hpp"
#include "Effects/ScreenEffectState.hpp"
#include "Render/Display.hpp"
#include "Render/Render.hpp"
#include "Render/TexturePagesLive.hpp"
#include "Render/UiPacketsLive.hpp"
#include "Render/UiPrimitives.hpp"

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

ScreenOverlayControls ReadControls(const LiveScreenEffectOverlay& overlay) {
  return {.feedback_pages_reserved = overlay.feedback_pages_reserved,
          .feedback_layer = overlay.feedback_layer,
          .radial_warp = {.scale = static_cast<std::uint16_t>(
                              overlay.radial_warp_scale),
                          .growth = overlay.radial_warp_growth},
          .vignette_enabled = overlay.vignette_enabled};
}

void WriteControls(LiveScreenEffectOverlay& overlay,
                   const ScreenOverlayControls& controls) {
  overlay.feedback_pages_reserved = controls.feedback_pages_reserved;
  overlay.feedback_layer = controls.feedback_layer;
  overlay.radial_warp_scale =
      static_cast<std::int16_t>(controls.radial_warp.scale);
  overlay.radial_warp_growth = controls.radial_warp.growth;
  overlay.vignette_enabled = controls.vignette_enabled;
}

// The active controller, or null after reporting `operation` as refused.
LiveScreenEffectOverlay* LiveOverlay(const char* operation) {
  if (screenEffectOverlay == nullptr) {
    VS::Core::LogError("effects",
                       "screen overlay {} refused: no overlay controller",
                       operation);
  }
  return screenEffectOverlay;
}

template <typename Update>
void UpdateLiveControls(const char* operation, Update update) {
  LiveScreenEffectOverlay* overlay = LiveOverlay(operation);
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
  const LiveScreenEffectOverlay* overlay = LiveOverlay("feedback tiles");
  if (overlay == nullptr) {
    return;
  }
  Render::LivePacketQueue scene_head(
      Render::LiveFrameOrderingEntry(Render::LiveOrderingBase::kScene, 0));
  Render::LivePacketQueue feedback_entry(Render::LiveFrameOrderingEntry(
      Render::LiveOrderingBase::kScene,
      FeedbackOrderingSlot(overlay->feedback_layer)));
  DrawFeedbackTiles(VS::Render::frameBuf, scene_head, feedback_entry);
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
                              int* packet_words, std::uint32_t* ordering_head) {
  if (packet_words == nullptr ||
      static_cast<void*>(packet_words) !=
          VS::Render::FrameScratchpad().primitive_cursor) {
    VS::Core::LogError(
        "effects",
        "vignette corner refused: packets must start at the frame "
        "primitive cursor");
    return packet_words;
  }
  Render::LivePacketQueue overlay(ordering_head);
  AppendVignetteCorner(static_cast<std::uint32_t>(packed_corner_xy),
                       first_angular_step, overlay);
  return static_cast<int*>(VS::Render::FrameScratchpad().primitive_cursor);
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
  VS::Content::DiscQueueSlot* AllocateOverlayRead() override {
    const VS::Content::DiscFile file = OverlayFile();
    return Content::AllocateLiveDiscRequest(&file);
  }
  void EnqueueOverlayRead(VS::Content::DiscQueueSlot* read) override {
    Content::EnqueueLiveDiscRequest(read, VS::Core::kOverlaySlots[1]);
  }
  VS::Content::DiscQueueSlot* CompleteRefusedOverlayRead() override {
    const VS::Content::DiscFile file = OverlayFile();
    return Content::CompleteLiveRefusedDiscRequest(&file,
                                                   VS::Core::kOverlaySlots[1]);
  }
  bool OverlayReadLoaded(const VS::Content::DiscQueueSlot* read) override {
    if (read == nullptr) {
      VS::Core::LogError("effects",
                         "screen overlay load poll: no read request");
      return false;
    }
    return read->state == VS::Content::DiscRequestState::kLoaded;
  }
  void ReleaseOverlayRead(VS::Content::DiscQueueSlot* read) override {
    Content::ReleaseLiveDiscRequest(read);
  }
  void WaitForOverlayReady() override { Boot::WaitForOverlayReadyLive(); }

 private:
  static VS::Content::DiscFile OverlayFile() {
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
LiveScreenEffectOverlay& LiveLifetimeRecord() {
  return screenEffectOverlay != nullptr ? *screenEffectOverlay
                                        : screenEffectDefaults;
}

ScreenOverlayLifetime ReadLiveLifetime() {
  const LiveScreenEffectOverlay& record = LiveLifetimeRecord();
  return {.attached = screenEffectOverlay != nullptr,
          .loading = record.loading,
          .controls = ReadControls(record),
          .read = record.slot};
}

// Attaching points D_800EB9B4 at the defaults; a detached lifetime is
// written to the defaults, as the original's reset cleared them.
void WriteLiveLifetime(const ScreenOverlayLifetime& lifetime) {
  if (!lifetime.attached) {
    screenEffectOverlay = nullptr;
  } else if (screenEffectOverlay == nullptr) {
    screenEffectOverlay = &screenEffectDefaults;
  }
  LiveScreenEffectOverlay& record = LiveLifetimeRecord();
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
