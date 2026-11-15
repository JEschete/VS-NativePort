#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Effects/ScreenDistortion/RadialWarp.hpp"
#include "Effects/ScreenDistortion/ScreenOverlayLifetime.hpp"
#include "Effects/ScreenDistortion/ScreenOverlays.hpp"
#include "Effects/ScreenDistortion/ScreenOverlaysLive.hpp"
#include "Legacy/Psx.h"

extern "C" {
#include "Content/DiscLayout.h"
#include "Core/Overlays.h"
#include "DiscQueue.h"
#include "Effects/ScreenEffectState.h"
#include "Effects/ScreenEffects.h"
#include "OverlayWorkspaceLiveState.h"
}

namespace {
namespace sd = VS::Effects::ScreenDistortion;

// Records every port call in order as text.
class RecordingLoader final : public sd::ScreenOverlayLoader {
 public:
  void AcquireWorkspace(int owner_tag) override {
    calls.push_back("acquire " + std::to_string(owner_tag));
  }
  void ReleaseWorkspace(int owner_tag) override {
    calls.push_back("release workspace " + std::to_string(owner_tag));
  }
  Content_DiscQueueSlot* AllocateOverlayRead() override {
    calls.emplace_back("allocate");
    return refuse ? nullptr : &queued;
  }
  void EnqueueOverlayRead(Content_DiscQueueSlot* read) override {
    calls.emplace_back(read == &queued ? "enqueue queued" : "enqueue other");
  }
  Content_DiscQueueSlot* CompleteRefusedOverlayRead() override {
    calls.emplace_back("complete refused");
    return &fallback;
  }
  bool OverlayReadLoaded(const Content_DiscQueueSlot* read) override {
    calls.emplace_back(read == &queued ? "poll queued" : "poll other");
    return loaded;
  }
  void ReleaseOverlayRead(Content_DiscQueueSlot* read) override {
    calls.emplace_back(read == &queued ? "release queued" : "release other");
  }
  void WaitForOverlayReady() override { calls.emplace_back("wait"); }

  bool refuse = false;
  bool loaded = false;
  Content_DiscQueueSlot queued{};
  Content_DiscQueueSlot fallback{};
  std::vector<std::string> calls;
};

class RecordingPainter final : public sd::ScreenOverlayPainter {
 public:
  void WarpPolygons(sd::RadialWarpSettings settings) override {
    calls.push_back("warp " + std::to_string(settings.scale) + " " +
                    std::to_string(settings.growth));
  }
  void DrawFeedbackTiles(std::uint16_t feedback_layer) override {
    calls.push_back("tiles " + std::to_string(feedback_layer));
  }
  void DrawVignette() override { calls.emplace_back("vignette"); }

  std::vector<std::string> calls;
};

class RecordingPages final : public sd::FeedbackTexturePages {
 public:
  void Reserve(const sd::TexturePageLease& /*lease*/) override { ++reserved; }
  void Retire(const sd::TexturePageLease& lease) override {
    feedback_lease = lease.first_page == 0x1B && lease.page_count == 5 &&
                     lease.owner_tag == 6;
    ++retired;
  }

  int reserved = 0;
  int retired = 0;
  bool feedback_lease = false;
};

using Calls = std::vector<std::string>;

sd::ScreenOverlayLifetime Attached() {
  sd::ScreenOverlayLifetime lifetime;
  sd::ResetScreenOverlay(lifetime);
  lifetime.attached = true;
  return lifetime;
}
}  // namespace

TEST_CASE("Screen overlay reset detaches and restores the neutral warp",
          "[screen-overlay-lifetime]") {
  Content_DiscQueueSlot read{};
  sd::ScreenOverlayLifetime lifetime{
      .attached = true,
      .loading = 1,
      .controls = {.feedback_pages_reserved = 1,
                   .feedback_layer = 9,
                   .radial_warp = {.scale = 7, .growth = -3},
                   .vignette_enabled = 5},
      .read = &read};
  sd::ResetScreenOverlay(lifetime);
  CHECK_FALSE(lifetime.attached);
  CHECK(lifetime.loading == 0);
  CHECK(lifetime.controls.feedback_pages_reserved == 0);
  CHECK(lifetime.controls.feedback_layer == 0);
  CHECK(lifetime.controls.radial_warp.scale == 0x1000);
  CHECK(lifetime.controls.radial_warp.growth == 0);
  CHECK(lifetime.controls.vignette_enabled == 0);
  CHECK(lifetime.read == nullptr);
}

TEST_CASE("Screen overlay load queues the SCREFF2 read and marks loading",
          "[screen-overlay-lifetime]") {
  RecordingLoader loader;
  sd::ScreenOverlayLifetime lifetime = Attached();
  sd::LoadScreenOverlay(lifetime, 0x100, loader);
  CHECK(lifetime.loading == 1);
  CHECK(lifetime.read == &loader.queued);
  CHECK(loader.calls == Calls{"allocate", "enqueue queued"});
}

TEST_CASE("Screen overlay load completes a refused disc request at once",
          "[screen-overlay-lifetime]") {
  RecordingLoader loader;
  loader.refuse = true;
  sd::ScreenOverlayLifetime lifetime = Attached();
  sd::LoadScreenOverlay(lifetime, -1, loader);
  CHECK(lifetime.loading == 1);
  CHECK(lifetime.read == &loader.fallback);
  CHECK(loader.calls == Calls{"allocate", "complete refused"});
  loader.calls.clear();
  loader.loaded = true;
  sd::LoadScreenOverlay(lifetime, 0, loader);
  CHECK(lifetime.loading == 0);
  CHECK(loader.calls == Calls{"poll other", "release other", "wait"});
}

TEST_CASE("Screen overlay load poll waits for the read to land",
          "[screen-overlay-lifetime]") {
  RecordingLoader loader;
  sd::ScreenOverlayLifetime lifetime = Attached();
  lifetime.loading = 1;
  lifetime.read = &loader.queued;
  sd::LoadScreenOverlay(lifetime, 0, loader);
  CHECK(lifetime.loading == 1);
  CHECK(loader.calls == Calls{"poll queued"});
  loader.calls.clear();
  loader.loaded = true;
  sd::LoadScreenOverlay(lifetime, 0, loader);
  CHECK(lifetime.loading == 0);
  // The released request stays recorded, as in the original.
  CHECK(lifetime.read == &loader.queued);
  CHECK(loader.calls == Calls{"poll queued", "release queued", "wait"});
}

TEST_CASE("Screen overlay draw polls while loading and draws nothing",
          "[screen-overlay-lifetime]") {
  RecordingLoader loader;
  RecordingPainter painter;
  sd::ScreenOverlayLifetime lifetime = Attached();
  lifetime.loading = 2;
  lifetime.read = &loader.queued;
  lifetime.controls = {.feedback_layer = 4,
                       .radial_warp = {.scale = 1, .growth = 1},
                       .vignette_enabled = 1};
  sd::DrawScreenOverlay(lifetime, loader, painter);
  CHECK(loader.calls == Calls{"poll queued"});
  CHECK(painter.calls.empty());
  loader.loaded = true;
  loader.calls.clear();
  sd::DrawScreenOverlay(lifetime, loader, painter);
  CHECK(lifetime.loading == 0);
  CHECK(loader.calls == Calls{"poll queued", "release queued", "wait"});
  CHECK(painter.calls.empty());
}

TEST_CASE("Screen overlay draw selects the warp tiles and vignette in order",
          "[screen-overlay-lifetime]") {
  RecordingLoader loader;
  RecordingPainter painter;
  sd::ScreenOverlayLifetime lifetime = Attached();
  sd::DrawScreenOverlay(lifetime, loader, painter);
  CHECK(painter.calls.empty());

  lifetime.controls.radial_warp.growth = -1;
  lifetime.controls.feedback_layer = 0x8000;
  lifetime.controls.vignette_enabled = -0x10000;
  sd::DrawScreenOverlay(lifetime, loader, painter);
  CHECK(painter.calls == Calls{"warp 4096 -1", "tiles 32768", "vignette"});

  painter.calls.clear();
  lifetime.controls = {.radial_warp = {.scale = 0x1001}};
  sd::DrawScreenOverlay(lifetime, loader, painter);
  CHECK(painter.calls == Calls{"warp 4097 0"});

  painter.calls.clear();
  lifetime.attached = false;
  lifetime.loading = 1;
  sd::DrawScreenOverlay(lifetime, loader, painter);
  CHECK(painter.calls.empty());
  CHECK(loader.calls.empty());
}

TEST_CASE("Screen overlay acquire attaches once and starts the read",
          "[screen-overlay-lifetime]") {
  RecordingLoader loader;
  sd::ScreenOverlayLifetime lifetime;
  sd::ResetScreenOverlay(lifetime);
  lifetime.controls.vignette_enabled = 3;
  sd::AcquireScreenOverlay(lifetime, loader);
  CHECK(lifetime.attached);
  CHECK(lifetime.loading == 1);
  CHECK(lifetime.read == &loader.queued);
  CHECK(lifetime.controls.vignette_enabled == 3);
  CHECK(loader.calls == Calls{"acquire 6", "allocate", "enqueue queued"});
  loader.calls.clear();
  lifetime.loading = 0;
  sd::AcquireScreenOverlay(lifetime, loader);
  CHECK(lifetime.loading == 0);
  CHECK(loader.calls.empty());
}

namespace {
void CheckReleasedLifetime(const sd::ScreenOverlayLifetime& released) {
  CHECK_FALSE(released.attached);
  CHECK(released.loading == 0);
  CHECK(released.read == nullptr);
  CHECK(released.controls.radial_warp.scale == 0x1000);
  CHECK(released.controls.feedback_layer == 0);
}
}  // namespace

TEST_CASE("Screen overlay release retires pages only for reservation byte one",
          "[screen-overlay-lifetime]") {
  for (const std::uint8_t reserved : {std::uint8_t{0}, std::uint8_t{1},
                                      std::uint8_t{2}, std::uint8_t{0xFF}}) {
    RecordingLoader loader;
    RecordingPages pages;
    sd::ScreenOverlayLifetime lifetime = Attached();
    lifetime.loading = 1;
    lifetime.read = &loader.queued;
    lifetime.controls = {.feedback_pages_reserved = reserved,
                         .feedback_layer = 6,
                         .radial_warp = {.scale = 3, .growth = 4},
                         .vignette_enabled = 1};
    sd::ReleaseScreenOverlay(lifetime, loader, pages);
    CHECK(loader.calls == Calls{"release workspace 6"});
    CHECK(pages.retired == (reserved == 1 ? 1 : 0));
    CHECK(pages.feedback_lease == (reserved == 1));
    CHECK(pages.reserved == 0);
    CheckReleasedLifetime(lifetime);
  }
}

TEST_CASE("Screen overlay release ignores a detached controller",
          "[screen-overlay-lifetime]") {
  RecordingLoader loader;
  RecordingPages pages;
  sd::ScreenOverlayLifetime detached;
  detached.controls.feedback_pages_reserved = 1;
  detached.controls.vignette_enabled = 7;
  sd::ReleaseScreenOverlay(detached, loader, pages);
  CHECK(loader.calls.empty());
  CHECK(pages.retired == 0);
  CHECK(detached.controls.vignette_enabled == 7);
}

namespace {
struct LiveLifetimeFixture {
  Effects_ScreenEffectOverlay defaults = Effects_screenEffectDefaults;
  Effects_ScreenEffectOverlay* active = Effects_screenEffectOverlay;
  decltype(std::to_array(Content_discRequestSlots)) requests =
      std::to_array(Content_discRequestSlots);
  Content_DiscRequestCounts counts = Content_discRequestCounts;
  char owner = Content_overlayWorkspaceOwner;

  LiveLifetimeFixture() {
    std::fill_n(Content_discRequestSlots, requests.size(),
                Content_DiscQueueSlot{});
    Content_discRequestCounts = {};
    Content_overlayWorkspaceOwner = 0;
    sd::ResetLiveScreenOverlay();
  }
  LiveLifetimeFixture(const LiveLifetimeFixture&) = delete;
  LiveLifetimeFixture& operator=(const LiveLifetimeFixture&) = delete;
  LiveLifetimeFixture(LiveLifetimeFixture&&) = delete;
  LiveLifetimeFixture& operator=(LiveLifetimeFixture&&) = delete;

  ~LiveLifetimeFixture() {
    Effects_screenEffectDefaults = defaults;
    Effects_screenEffectOverlay = active;
    std::copy(requests.begin(), requests.end(), Content_discRequestSlots);
    Content_discRequestCounts = counts;
    Content_overlayWorkspaceOwner = owner;
  }
};

void CheckQueuedScreff2Read(const Content_DiscQueueSlot& request) {
  CHECK(request.state == Content_DiscRequestState_Enqueued);
  CHECK(request.cdFile.lba == VS_SCREFF2_PRG_LBA);
  CHECK(request.cdFile.size == VS_SCREFF2_PRG_SIZE);
  CHECK(request.vram == Core_overlaySlots[1]);
}
}  // namespace

TEST_CASE("Live screen overlay acquire queues the SCREFF2 read",
          "[screen-overlay-lifetime]") {
  const LiveLifetimeFixture fixture;
  REQUIRE(Effects_screenEffectOverlay == nullptr);

  sd::AcquireLiveScreenOverlay();
  REQUIRE(Effects_screenEffectOverlay == &Effects_screenEffectDefaults);
  const Content_DiscQueueSlot* const request = &Content_discRequestSlots[0];
  CHECK(Effects_screenEffectDefaults.slot == request);
  CHECK(Effects_screenEffectDefaults.loading == 1);
  CHECK(Content_overlayWorkspaceOwner == 6);
  CheckQueuedScreff2Read(*request);
}

TEST_CASE("Live screen overlay acquire is ignored while attached",
          "[screen-overlay-lifetime]") {
  const LiveLifetimeFixture fixture;
  sd::AcquireLiveScreenOverlay();
  sd::AcquireLiveScreenOverlay();
  CHECK(Content_discRequestCounts.waiting == 1);
  CHECK(Content_discRequestSlots[1].state == 0);
}

TEST_CASE("Live screen overlay draw polls the read until loaded",
          "[screen-overlay-lifetime]") {
  const LiveLifetimeFixture fixture;
  sd::AcquireLiveScreenOverlay();
  Content_DiscQueueSlot* const request = &Content_discRequestSlots[0];
  REQUIRE(Effects_screenEffectDefaults.slot == request);

  sd::DrawLiveScreenOverlay();
  CHECK(Effects_screenEffectDefaults.loading == 1);
  request->state = Content_DiscRequestState_Loaded;
  sd::DrawLiveScreenOverlay();
  CHECK(Effects_screenEffectDefaults.loading == 0);
  CHECK(request->state != Content_DiscRequestState_Loaded);
}

TEST_CASE("Live screen overlay release restores the defaults",
          "[screen-overlay-lifetime]") {
  const LiveLifetimeFixture fixture;
  sd::AcquireLiveScreenOverlay();
  REQUIRE(Effects_screenEffectOverlay == &Effects_screenEffectDefaults);
  Effects_screenEffectDefaults.vignetteEnabled = 4;
  sd::ReleaseLiveScreenOverlay();
  CHECK(Effects_screenEffectOverlay == nullptr);
  CHECK(Content_overlayWorkspaceOwner == 0);
  CHECK(Effects_screenEffectDefaults.vignetteEnabled == 0);
  CHECK(Effects_screenEffectDefaults.radialWarpArg0 == 0x1000);
  CHECK(Effects_screenEffectDefaults.slot == nullptr);
}

TEST_CASE("Live screen overlay load refuses a missing controller",
          "[screen-overlay-lifetime]") {
  const LiveLifetimeFixture fixture;
  sd::LoadLiveScreenOverlay(1);
  CHECK(Effects_screenEffectOverlay == nullptr);
  CHECK(Effects_screenEffectDefaults.loading == 0);
  CHECK(Content_discRequestCounts.waiting == 0);
  CHECK(Content_discRequestSlots[0].state == 0);
}

TEST_CASE("Live screen overlay load writes the attached controller",
          "[screen-overlay-lifetime]") {
  const LiveLifetimeFixture fixture;
  Effects_ScreenEffectOverlay other{};
  other.vignetteEnabled = 2;
  Effects_screenEffectOverlay = &other;
  sd::LoadLiveScreenOverlay(1);
  CHECK(other.loading == 1);
  CHECK(other.slot == &Content_discRequestSlots[0]);
  CHECK(other.vignetteEnabled == 2);
  CHECK(Effects_screenEffectDefaults.loading == 0);
  // A missing request is reported and stays pending.
  other.slot = nullptr;
  sd::LoadLiveScreenOverlay(0);
  CHECK(other.loading == 1);
}
