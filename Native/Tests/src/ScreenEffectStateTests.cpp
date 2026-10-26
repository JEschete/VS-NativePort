#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <type_traits>

#include <catch2/catch_test_macros.hpp>

#include "Content/ModuleData.h"
#include "Legacy/Psx.h"

extern "C" {
#include "CdDrive.h"
#include "Content/DiscLayout.h"
#include "Core/Overlays.h"
#include "DiscQueue.h"
#include "Effects/ScreenDistortion/ScreenDistortion.h"
#include "Effects/ScreenEffectState.h"
#include "Effects/ScreenEffects.h"
#include "Field/DoorMatches.h"
#include "OverlayWorkspace.h"
}

namespace {
struct ScreenStateFixture {
  Effects_ScreenEffectOverlay defaults = Effects_screenEffectDefaults;
  Effects_ScreenEffectOverlay* active = Effects_screenEffectOverlay;
  decltype(std::to_array(Content_discRequestSlots)) requests =
      std::to_array(Content_discRequestSlots);
  Content_DiscRequestCounts counts = Content_discRequestCounts;
  char owner = Content_overlayWorkspaceOwner;

  ScreenStateFixture() {
    std::fill_n(Content_discRequestSlots, requests.size(),
                Content_DiscQueueSlot{});
    Content_discRequestCounts = {};
    Content_overlayWorkspaceOwner = 0;
    Effects_ResetScreenEffectState();
  }

  ~ScreenStateFixture() {
    Effects_screenEffectDefaults = defaults;
    Effects_screenEffectOverlay = active;
    std::copy(requests.begin(), requests.end(), Content_discRequestSlots);
    Content_discRequestCounts = counts;
    Content_overlayWorkspaceOwner = owner;
  }
};

void CheckDefaultState() {
  CHECK(Effects_screenEffectDefaults.loading == 0);
  CHECK(Effects_screenEffectDefaults.opaque1 == 0);
  CHECK(Effects_screenEffectDefaults.feedbackLayer == 0);
  CHECK(Effects_screenEffectDefaults.radialWarpArg0 == 4096);
  CHECK(Effects_screenEffectDefaults.radialWarpArg1 == 0);
  CHECK(Effects_screenEffectDefaults.vignetteEnabled == 0);
  CHECK(Effects_screenEffectDefaults.slot == nullptr);
}

void CheckQueuedRequest(const Content_DiscQueueSlot* request, int waiting) {
  CHECK(Effects_screenEffectDefaults.loading == 1);
  CHECK(Content_overlayWorkspaceOwner == 6);
  CHECK(request->state == Content_DiscRequestState_Enqueued);
  CHECK(request->cdFile.lba == VS_SCREFF2_PRG_LBA);
  CHECK(request->cdFile.size == VS_SCREFF2_PRG_SIZE);
  CHECK(request->vram == Core_overlaySlots[1]);
  CHECK(Content_discRequestCounts.waiting == waiting);
}
}  // namespace

TEST_CASE("Screen-effect defaults own the complete native overlay state",
          "[effects][screen-state]") {
  CHECK(sizeof Effects_screenEffectDefaults >=
        sizeof(Effects_ScreenEffectOverlay));
  CHECK(alignof(decltype(Effects_screenEffectDefaults)) >=
        alignof(Effects_ScreenEffectOverlay));
  CHECK(offsetof(Effects_ScreenEffectOverlay, slot) +
            sizeof(Content_DiscQueueSlot*) <=
        sizeof Effects_screenEffectDefaults);
  CHECK((std::is_same_v<decltype(Effects_screenEffectDefaults),
                        Effects_ScreenEffectOverlay>));
}

TEST_CASE("Screen-effect reset clears the entire native request pointer",
          "[effects][screen-state]") {
  const ScreenStateFixture fixture;
  Effects_screenEffectDefaults = {
      1, 1, 7, -1, -2, 99, &Content_discRequestSlots[31]};
  Effects_screenEffectOverlay = &Effects_screenEffectDefaults;
  Effects_ResetScreenEffectState();
  CHECK(Effects_screenEffectOverlay == nullptr);
  CheckDefaultState();
  Effects_ResetScreenEffectState();
  CheckDefaultState();
}

TEST_CASE("Screen-effect acquisition and reentry use one typed native state",
          "[effects][screen-state]") {
  const ScreenStateFixture fixture;
  for (int cycle = 0; cycle < 2; ++cycle) {
    Effects_AcquireScreenEffectOverlay();
    REQUIRE(Effects_screenEffectOverlay == &Effects_screenEffectDefaults);
    auto* request = &Content_discRequestSlots[cycle];
    CHECK(Effects_screenEffectDefaults.slot == request);
    CheckQueuedRequest(request, cycle + 1);
    Effects_AcquireScreenEffectOverlay();
    CHECK(Effects_screenEffectDefaults.slot == request);
    CHECK(Content_discRequestCounts.waiting == cycle + 1);
    Effects_ReleaseScreenEffectOverlay();
    CHECK(Effects_screenEffectOverlay == nullptr);
    CHECK(Content_overlayWorkspaceOwner == 0);
    CheckDefaultState();
  }
}

TEST_CASE("Screen-effect queue pointer writes stay within native object guards",
          "[effects][screen-state]") {
  const ScreenStateFixture fixture;
  struct GuardedState {
    std::uint64_t prefix = 0x123456789ABCDEF0;
    Effects_ScreenEffectOverlay state{};
    std::uint64_t suffix = 0x0FEDCBA987654321;
  } guarded;
  Effects_screenEffectOverlay = &guarded.state;
  Effects_LoadScreenEffectOverlay(1);
  CHECK(guarded.state.slot == &Content_discRequestSlots[0]);
  CHECK(guarded.state.loading == 1);
  CHECK(guarded.prefix == 0x123456789ABCDEF0);
  CHECK(guarded.suffix == 0x0FEDCBA987654321);
}

TEST_CASE("Screen-effect default binding retains original address provenance",
          "[effects][screen-state]") {
  const auto symbols =
      std::span(Content_moduleSymbols, Content_moduleSymbolCount);
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x800F4ED8u) ==
        &Effects_screenEffectDefaults);
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x800F4ED9u) == nullptr);
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x800F1D08u) ==
        &Field_matchingDoorRecords);
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x800F1D09u) == nullptr);
  const auto found = std::ranges::find_if(symbols, [](const auto& symbol) {
    return std::string_view(symbol.module) == "BATTLE/BATTLE.PRG" &&
           symbol.address == 0x800F4ED8u;
  });
  REQUIRE(found != symbols.end());
  CHECK(found->size == 16);
  CHECK(found->nativeSize == sizeof Effects_screenEffectDefaults);
  CHECK(found->sourceElementSize == 0);
  CHECK(found->nativeElementSize == 0);
}

TEST_CASE("Screen-effect parameter setters retain the typed default controller",
          "[effects][screen-state]") {
  const ScreenStateFixture fixture;
  Effects_screenEffectOverlay = &Effects_screenEffectDefaults;
  Effects_ScreenDistortion_SetRadialWarp(1234, -567);
  Effects_ScreenDistortion_SetVignetteEnabled(7);
  CHECK(Effects_screenEffectDefaults.radialWarpArg0 == 1234);
  CHECK(Effects_screenEffectDefaults.radialWarpArg1 == -567);
  CHECK(Effects_screenEffectDefaults.vignetteEnabled == 7);
  CHECK(Effects_screenEffectDefaults.slot == nullptr);
  Effects_ResetScreenEffectState();
  CheckDefaultState();
}
