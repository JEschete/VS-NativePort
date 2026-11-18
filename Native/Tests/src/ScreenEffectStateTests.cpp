#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <type_traits>

#include <catch2/catch_test_macros.hpp>

#include "Content/DiscLayout.h"
#include "Content/ModuleData.h"
#include "Content/OverlayWorkspaceLive.hpp"
#include "Core/Overlays.h"
#include "DiscQueue.h"
#include "Effects/ScreenDistortion/ScreenOverlaysLive.hpp"
#include "Effects/ScreenEffectState.hpp"
#include "Effects/ScreenEffects.h"
#include "Field/DoorMatchList.h"
#include "Legacy/Psx.h"

namespace {
struct ScreenStateFixture {
  Effects_ScreenEffectOverlay defaults = VS::Effects::screenEffectDefaults;
  Effects_ScreenEffectOverlay* active = Effects_screenEffectOverlay;
  decltype(std::to_array(Content_discRequestSlots)) requests =
      std::to_array(Content_discRequestSlots);
  Content_DiscRequestCounts counts = Content_discRequestCounts;
  char owner = VS::Content::overlayWorkspaceOwner;

  ScreenStateFixture() {
    std::fill_n(Content_discRequestSlots, requests.size(),
                Content_DiscQueueSlot{});
    Content_discRequestCounts = {};
    VS::Content::overlayWorkspaceOwner = 0;
    VS::Effects::ScreenDistortion::ResetLiveScreenOverlay();
  }

  ~ScreenStateFixture() {
    VS::Effects::screenEffectDefaults = defaults;
    Effects_screenEffectOverlay = active;
    std::copy(requests.begin(), requests.end(), Content_discRequestSlots);
    Content_discRequestCounts = counts;
    VS::Content::overlayWorkspaceOwner = owner;
  }
};

void CheckDefaultState() {
  CHECK(VS::Effects::screenEffectDefaults.loading == 0);
  CHECK(VS::Effects::screenEffectDefaults.opaque1 == 0);
  CHECK(VS::Effects::screenEffectDefaults.feedbackLayer == 0);
  CHECK(VS::Effects::screenEffectDefaults.radialWarpArg0 == 4096);
  CHECK(VS::Effects::screenEffectDefaults.radialWarpArg1 == 0);
  CHECK(VS::Effects::screenEffectDefaults.vignetteEnabled == 0);
  CHECK(VS::Effects::screenEffectDefaults.slot == nullptr);
}

void CheckQueuedRequest(const Content_DiscQueueSlot* request, int waiting) {
  CHECK(VS::Effects::screenEffectDefaults.loading == 1);
  CHECK(VS::Content::overlayWorkspaceOwner == 6);
  CHECK(request->state == Content_DiscRequestState_Enqueued);
  CHECK(request->cdFile.lba == VS_SCREFF2_PRG_LBA);
  CHECK(request->cdFile.size == VS_SCREFF2_PRG_SIZE);
  CHECK(request->vram == Core_overlaySlots[1]);
  CHECK(Content_discRequestCounts.waiting == waiting);
}
}  // namespace

TEST_CASE("Screen-effect defaults own the complete native overlay state",
          "[effects][screen-state]") {
  CHECK(sizeof VS::Effects::screenEffectDefaults >=
        sizeof(Effects_ScreenEffectOverlay));
  CHECK(alignof(decltype(VS::Effects::screenEffectDefaults)) >=
        alignof(Effects_ScreenEffectOverlay));
  CHECK(offsetof(Effects_ScreenEffectOverlay, slot) +
            sizeof(Content_DiscQueueSlot*) <=
        sizeof VS::Effects::screenEffectDefaults);
  CHECK((std::is_same_v<decltype(VS::Effects::screenEffectDefaults),
                        Effects_ScreenEffectOverlay>));
}

TEST_CASE("Screen-effect reset clears the entire native request pointer",
          "[effects][screen-state]") {
  const ScreenStateFixture fixture;
  VS::Effects::screenEffectDefaults = {
      1, 1, 7, -1, -2, 99, &Content_discRequestSlots[31]};
  Effects_screenEffectOverlay = &VS::Effects::screenEffectDefaults;
  VS::Effects::ScreenDistortion::ResetLiveScreenOverlay();
  CHECK(Effects_screenEffectOverlay == nullptr);
  CheckDefaultState();
  VS::Effects::ScreenDistortion::ResetLiveScreenOverlay();
  CheckDefaultState();
}

TEST_CASE("Screen-effect acquisition and reentry use one typed native state",
          "[effects][screen-state]") {
  const ScreenStateFixture fixture;
  for (int cycle = 0; cycle < 2; ++cycle) {
    VS::Effects::ScreenDistortion::AcquireLiveScreenOverlay();
    REQUIRE(Effects_screenEffectOverlay == &VS::Effects::screenEffectDefaults);
    auto* request = &Content_discRequestSlots[cycle];
    CHECK(VS::Effects::screenEffectDefaults.slot == request);
    CheckQueuedRequest(request, cycle + 1);
    VS::Effects::ScreenDistortion::AcquireLiveScreenOverlay();
    CHECK(VS::Effects::screenEffectDefaults.slot == request);
    CHECK(Content_discRequestCounts.waiting == cycle + 1);
    VS::Effects::ScreenDistortion::ReleaseLiveScreenOverlay();
    CHECK(Effects_screenEffectOverlay == nullptr);
    CHECK(VS::Content::overlayWorkspaceOwner == 0);
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
  VS::Effects::ScreenDistortion::LoadLiveScreenOverlay(1);
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
        &VS::Effects::screenEffectDefaults);
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
  CHECK(found->nativeSize == sizeof VS::Effects::screenEffectDefaults);
  CHECK(found->sourceElementSize == 0);
  CHECK(found->nativeElementSize == 0);
}

TEST_CASE("Screen-effect parameter setters retain the typed default controller",
          "[effects][screen-state]") {
  const ScreenStateFixture fixture;
  Effects_screenEffectOverlay = &VS::Effects::screenEffectDefaults;
  VS::Effects::ScreenDistortion::SetLiveRadialWarpWords(1234, -567);
  VS::Effects::ScreenDistortion::SetLiveVignetteEnabled(7);
  CHECK(VS::Effects::screenEffectDefaults.radialWarpArg0 == 1234);
  CHECK(VS::Effects::screenEffectDefaults.radialWarpArg1 == -567);
  CHECK(VS::Effects::screenEffectDefaults.vignetteEnabled == 7);
  CHECK(VS::Effects::screenEffectDefaults.slot == nullptr);
  VS::Effects::ScreenDistortion::ResetLiveScreenOverlay();
  CheckDefaultState();
}
