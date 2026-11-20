#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <type_traits>

#include <catch2/catch_test_macros.hpp>

#include "Content/DiscLayout.h"
#include "Content/DiscQueue.hpp"
#include "Content/ModuleData.hpp"
#include "Content/OverlayWorkspaceLive.hpp"
#include "Core/Overlays.hpp"
#include "Effects/ScreenDistortion/ScreenOverlaysLive.hpp"
#include "Effects/ScreenEffectState.hpp"
#include "Field/DoorMatchList.hpp"
#include "Legacy/Psx.h"

namespace {
struct ScreenStateFixture {
  VS::Effects::LiveScreenEffectOverlay defaults =
      VS::Effects::screenEffectDefaults;
  VS::Effects::LiveScreenEffectOverlay* active =
      VS::Effects::screenEffectOverlay;
  decltype(VS::Content::discRequestSlots) requests =
      VS::Content::discRequestSlots;
  VS::Content::DiscRequestCounts counts = VS::Content::discRequestCounts;
  char owner = VS::Content::overlayWorkspaceOwner;

  ScreenStateFixture() {
    std::fill_n(VS::Content::discRequestSlots.begin(), requests.size(),
                VS::Content::DiscQueueSlot{});
    VS::Content::discRequestCounts = {};
    VS::Content::overlayWorkspaceOwner = 0;
    VS::Effects::ScreenDistortion::ResetLiveScreenOverlay();
  }

  ~ScreenStateFixture() {
    VS::Effects::screenEffectDefaults = defaults;
    VS::Effects::screenEffectOverlay = active;
    std::copy(requests.begin(), requests.end(),
              VS::Content::discRequestSlots.begin());
    VS::Content::discRequestCounts = counts;
    VS::Content::overlayWorkspaceOwner = owner;
  }
};

void CheckDefaultState() {
  CHECK(VS::Effects::screenEffectDefaults.loading == 0);
  CHECK(VS::Effects::screenEffectDefaults.feedback_pages_reserved == 0);
  CHECK(VS::Effects::screenEffectDefaults.feedback_layer == 0);
  CHECK(VS::Effects::screenEffectDefaults.radial_warp_scale == 4096);
  CHECK(VS::Effects::screenEffectDefaults.radial_warp_growth == 0);
  CHECK(VS::Effects::screenEffectDefaults.vignette_enabled == 0);
  CHECK(VS::Effects::screenEffectDefaults.slot == nullptr);
}

void CheckQueuedRequest(const VS::Content::DiscQueueSlot* request,
                        int waiting) {
  CHECK(VS::Effects::screenEffectDefaults.loading == 1);
  CHECK(VS::Content::overlayWorkspaceOwner == 6);
  CHECK(request->state == VS::Content::DiscRequestState::kEnqueued);
  CHECK(request->file.lba == VS_SCREFF2_PRG_LBA);
  CHECK(request->file.size == VS_SCREFF2_PRG_SIZE);
  CHECK(request->destination == VS::Core::kOverlaySlots[1]);
  CHECK(VS::Content::discRequestCounts.waiting == waiting);
}
}  // namespace

TEST_CASE("Screen-effect defaults own the complete native overlay state",
          "[effects][screen-state]") {
  CHECK(sizeof VS::Effects::screenEffectDefaults >=
        sizeof(VS::Effects::LiveScreenEffectOverlay));
  CHECK(alignof(decltype(VS::Effects::screenEffectDefaults)) >=
        alignof(VS::Effects::LiveScreenEffectOverlay));
  CHECK(offsetof(VS::Effects::LiveScreenEffectOverlay, slot) +
            sizeof(VS::Content::DiscQueueSlot*) <=
        sizeof VS::Effects::screenEffectDefaults);
  CHECK((std::is_same_v<decltype(VS::Effects::screenEffectDefaults),
                        VS::Effects::LiveScreenEffectOverlay>));
}

TEST_CASE("Screen-effect reset clears the entire native request pointer",
          "[effects][screen-state]") {
  const ScreenStateFixture fixture;
  VS::Effects::screenEffectDefaults = {
      1, 1, 7, -1, -2, 99, &VS::Content::discRequestSlots[31]};
  VS::Effects::screenEffectOverlay = &VS::Effects::screenEffectDefaults;
  VS::Effects::ScreenDistortion::ResetLiveScreenOverlay();
  CHECK(VS::Effects::screenEffectOverlay == nullptr);
  CheckDefaultState();
  VS::Effects::ScreenDistortion::ResetLiveScreenOverlay();
  CheckDefaultState();
}

TEST_CASE("Screen-effect acquisition and reentry use one typed native state",
          "[effects][screen-state]") {
  const ScreenStateFixture fixture;
  for (int cycle = 0; cycle < 2; ++cycle) {
    VS::Effects::ScreenDistortion::AcquireLiveScreenOverlay();
    REQUIRE(VS::Effects::screenEffectOverlay ==
            &VS::Effects::screenEffectDefaults);
    auto* request = &VS::Content::discRequestSlots[cycle];
    CHECK(VS::Effects::screenEffectDefaults.slot == request);
    CheckQueuedRequest(request, cycle + 1);
    VS::Effects::ScreenDistortion::AcquireLiveScreenOverlay();
    CHECK(VS::Effects::screenEffectDefaults.slot == request);
    CHECK(VS::Content::discRequestCounts.waiting == cycle + 1);
    VS::Effects::ScreenDistortion::ReleaseLiveScreenOverlay();
    CHECK(VS::Effects::screenEffectOverlay == nullptr);
    CHECK(VS::Content::overlayWorkspaceOwner == 0);
    CheckDefaultState();
  }
}

TEST_CASE("Screen-effect queue pointer writes stay within native object guards",
          "[effects][screen-state]") {
  const ScreenStateFixture fixture;
  struct GuardedState {
    std::uint64_t prefix = 0x123456789ABCDEF0;
    VS::Effects::LiveScreenEffectOverlay state{};
    std::uint64_t suffix = 0x0FEDCBA987654321;
  } guarded;
  VS::Effects::screenEffectOverlay = &guarded.state;
  VS::Effects::ScreenDistortion::LoadLiveScreenOverlay(1);
  CHECK(guarded.state.slot == &VS::Content::discRequestSlots[0]);
  CHECK(guarded.state.loading == 1);
  CHECK(guarded.prefix == 0x123456789ABCDEF0);
  CHECK(guarded.suffix == 0x0FEDCBA987654321);
}

TEST_CASE("Screen-effect default binding retains original address provenance",
          "[effects][screen-state]") {
  const auto symbols = VS::Content::ModuleSymbols();
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x800F4ED8u) ==
        &VS::Effects::screenEffectDefaults);
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x800F4ED9u) == nullptr);
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x800F1D08u) ==
        &VS::Field::matchingDoorRecords);
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
  VS::Effects::screenEffectOverlay = &VS::Effects::screenEffectDefaults;
  VS::Effects::ScreenDistortion::SetLiveRadialWarpWords(1234, -567);
  VS::Effects::ScreenDistortion::SetLiveVignetteEnabled(7);
  CHECK(VS::Effects::screenEffectDefaults.radial_warp_scale == 1234);
  CHECK(VS::Effects::screenEffectDefaults.radial_warp_growth == -567);
  CHECK(VS::Effects::screenEffectDefaults.vignette_enabled == 7);
  CHECK(VS::Effects::screenEffectDefaults.slot == nullptr);
  VS::Effects::ScreenDistortion::ResetLiveScreenOverlay();
  CheckDefaultState();
}
