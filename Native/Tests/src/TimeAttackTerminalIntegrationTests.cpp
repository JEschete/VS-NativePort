#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Boot/SessionLive.hpp"
#include "Content/DiscLayout.h"
#include "Content/ModuleData.h"
#include "Menu/Framework/FrameworkLive.hpp"
#include "Menu/Framework/FrameworkLiveState.h"
#include "Menu/OverlayLoadFixture.hpp"
#include "Menu/TimeAttackTerminalLiveFixture.hpp"
#include "Render/Frame.h"

namespace {
namespace results = VS::Menu::Results;
namespace content = VS::Content;
namespace memory = VS::Core::Memory;
namespace render = VS::Render;
namespace tests = time_attack_terminal_live_tests;
using Asset = results::TimeAttackAsset;
constexpr std::array kAssets{Asset::kResults, Asset::kStart};

template <typename Entry>
Entry ModuleEntry(unsigned address, Entry expected) {
  auto* resolved = content::ResolveModuleAddress(
      {Content_moduleSymbols, Content_moduleSymbolCount}, "MENU/MENUF.PRG",
      address);
  CHECK(resolved == reinterpret_cast<void*>(expected));
  if (resolved == nullptr) {
    throw std::runtime_error("missing time-attack module entry");
  }
  return reinterpret_cast<Entry>(resolved);
}

void CheckUnbound(Asset asset) {
  const auto status = results::InspectLiveTimeAttackLoad(asset);
  CHECK_FALSE(status.request_bound);
  CHECK_FALSE(status.buffer_bound);
}

void CheckReleased(Asset asset) {
  const auto status = results::InspectLiveTimeAttackLoad(asset);
  CHECK_FALSE(status.owns_request);
  CHECK_FALSE(status.owns_buffer);
  CHECK_FALSE(status.owns_pages);
  CHECK(Content_discRequestCounts.waiting == 0);
}

void CheckModuleReset(Asset asset) {
  CheckUnbound(asset);
  CHECK(Menu_Results_screenTimer == 0);
  CHECK(Menu_Results_screenState == 0);
  CHECK(Menu_Results_newTimeSlot == 0);
  CHECK(Menu_Results_timeTrialTime == 0);
  CHECK(std::ranges::all_of(Menu_Results_clutBuf,
                            [](auto value) { return value == 0; }));
}

struct ReloadFixture : tests::Fixture {
  void Prime(Asset asset) {
    REQUIRE(tests::Initialize(asset) == 0);
    Menu_Results_screenTimer = 94;
    Menu_Results_newTimeSlot = 2;
    Menu_Results_timeTrialTime = 0x123456;
    Menu_Results_clutBuf[159] = 0x1234;
    Audio_Seq_unusedScoredata.bossTimeTrialScores[7][2].time = 123456;
    stats.strength = 77;
  }

  static void CheckUnchanged(Asset asset) {
    CHECK(Menu_Results_newTimeSlot == 2);
    CHECK(Menu_Results_timeTrialTime == 0x123456);
    CHECK(Menu_Results_clutBuf[159] == 0x1234);
    CHECK(results::InspectLiveTimeAttackLoad(asset).request_bound);
  }

  static void Reload(Asset asset) {
    {
      menu_overlay_tests::Load other(3, {VS_MENU2_PRG_LBA, VS_MENU2_PRG_SIZE});
      REQUIRE(other.Complete() == 1);
      CheckUnchanged(asset);
    }
    menu_overlay_tests::Load loaded(16, {VS_MENUF_PRG_LBA, VS_MENUF_PRG_SIZE});
    REQUIRE(loaded.Poll() == 0);
    CheckUnchanged(asset);
    REQUIRE(loaded.Complete() == 1);
  }

  void CheckResident() const {
    CHECK(Battle_unusedSubmenuState == 1);
    CHECK(Battle_characterState == &actor);
    CHECK(stats.strength == 77);
    CHECK(Audio_Seq_unusedScoredata.bossTimeTrialScores[7][2].time == 123456);
  }

  static void Reenter(Asset asset) {
    render::AdvanceLiveTexturePageRetirements();
    render::AdvanceLiveTexturePageRetirements();
    Battle_unusedSubmenuState = 0;
    REQUIRE(tests::Initialize(asset) == 0);
    CHECK(results::InspectLiveTimeAttackLoad(asset).request_bound);
    CHECK(results::InspectLiveTimeAttackLoad(asset).buffer_bound);
  }
};

void BeginCountdown(tests::Fixture& fixture) {
  fixture.LoadThroughTextures(Asset::kStart);
  REQUIRE(results::LiveInitializeTimeAttackStart() == 1);
  Input_buttonsPressed.allButtons = 0x20;
  REQUIRE(results::LiveUpdateTimeAttackStart() == 0);
  REQUIRE(Menu_Results_screenState == 2);
  Input_buttonsPressed.allButtons = 0;
  fixture.BeginFrame();
  REQUIRE(results::LiveUpdateTimeAttackStart() == 0);
  REQUIRE(results::LiveTimeAttackOwnsCountdownControls());
  REQUIRE(render::HasLiveFramebufferEffectPackets());
  render::DrawLiveFramebufferEffect(fixture.frame.ordering.data());
  render::DrawLiveFramebufferEffect(fixture.frame.ordering.data());
}

void CheckAbandonedFrame() {
  CHECK_FALSE(render::HasLiveFramebufferEffectPackets());
  CHECK(render::LiveFramebufferEffect().phase ==
        render::FramebufferEffectPhase::kOff);
  CHECK(Render_legacyCurrentOrderingTable == nullptr);
  CHECK(Render_frameScratch.orderingTable == nullptr);
  CHECK(Render_frameScratch.overlayOrderingTable == nullptr);
  CHECK(Render_frameScratch.primitiveCursor == nullptr);
  CHECK(std::ranges::all_of(Render_legacyOrderingTables, [](const void* table) {
    return table == nullptr;
  }));
  CHECK(std::ranges::all_of(Render_legacyPrimitiveBuffers,
                            [](const void* data) { return data == nullptr; }));
}

void InitializeThroughModule(Asset asset, tests::Fixture& fixture) {
  const auto initialize =
      asset == Asset::kResults
          ? ModuleEntry(0x8010310C, &results::LiveInitializeTimeAttackResults)
          : ModuleEntry(0x80103530, &results::LiveInitializeTimeAttackStart);
  Audio_Seq_unusedStateFlags.timeAttackSecs = 30;
  Audio_Seq_unusedStateFlags.timeAttackHundredths = 50;
  Audio_Seq_unusedStateFlags.clearCount = 7;
  auto& row = Audio_Seq_unusedScoredata.bossTimeTrialScores[0];
  for (auto& score : row) score.time = 0x3B3B63;
  REQUIRE(initialize() == 0);
  static_cast<void>(tests::InjectLoaded(asset));
  REQUIRE(initialize() == 0);
  if (asset == Asset::kResults) fixture.CompleteMusicLoad();
  REQUIRE(initialize() == 1);
  CHECK(Battle_unusedSubmenuState == 0);
  CHECK_FALSE(results::InspectLiveTimeAttackLoad(asset).owns_buffer);
  CHECK_FALSE(results::InspectLiveTimeAttackLoad(asset).owns_request);
  if (asset == Asset::kResults) {
    CHECK(row[0].time == 0x1E32);
    CHECK(row[0].round == 7);
  }
}

void CheckFrameBindings(const Render_FrameScratch& before,
                        std::span<void* const> tables,
                        std::span<void* const> buffers) {
  CHECK(Render_frameScratch.orderingTable == before.orderingTable);
  CHECK(Render_frameScratch.overlayOrderingTable ==
        before.overlayOrderingTable);
  CHECK(Render_frameScratch.primitiveCursor == before.primitiveCursor);
  CHECK(std::ranges::equal(Render_legacyOrderingTables, tables));
  CHECK(std::ranges::equal(Render_legacyPrimitiveBuffers, buffers));
}
}  // namespace

TEST_CASE("Time-attack module entries execute real loading and rendering",
          "[time-attack-integration][time-attack-module]"
          "[F03011][F03012][F03016][F03017]") {
  for (const auto asset : kAssets) {
    tests::Fixture fixture;
    InitializeThroughModule(asset, fixture);
    const auto update =
        asset == Asset::kResults
            ? ModuleEntry(0x80103D88, &results::LiveUpdateTimeAttackResults)
            : ModuleEntry(0x8010412C, &results::LiveUpdateTimeAttackStart);
    Menu_Results_screenTimer = 64;
    fixture.BeginFrame();
    CHECK(update() == 0);
    CHECK(Menu_Results_screenTimer == 65);
    CHECK_FALSE(fixture.frame.Empty());
  }
}

TEST_CASE("Results module dispatch entries use real healing and caller bytes",
          "[time-attack-integration][time-attack-module]"
          "[F03008][F03009][F03014]") {
  tests::Fixture fixture;
  Audio_Seq_unusedStateFlags.menuFSubmenu = 3;
  Audio_Seq_unusedScoredata.miscScore = 50;
  Audio_Seq_unusedScoredata.healCount = 2;
  CHECK(ModuleEntry(0x80102C54, &results::LiveInitializeResultsScreen)() == 1);
  CHECK(Audio_Seq_unusedScoredata.miscScore == 99850);
  CHECK(Audio_Seq_unusedScoredata.healCount == 0);
  CHECK(ModuleEntry(0x801037A0, &results::LiveUpdateResultsPresentation)() ==
        1);
  CHECK(Audio_Seq_unusedScoredata.miscScore == 99850);
  std::array<unsigned char, 2> state{2, 0xAA};
  CHECK(ModuleEntry(0x80102BB8,
                    &results::LiveUpdateResultsScreen)(state.data()) == 1);
  CHECK(state == std::array<unsigned char, 2>{0, 0xAA});
  CHECK(Audio_Seq_unusedScoredata.miscScore == 99850);
}

TEST_CASE(
    "Production framework dispatch consumes the actual results state byte",
    "[time-attack-integration][time-attack-framework][F03008]") {
  tests::Fixture fixture;
  const game_clear_live_tests::SavedGlobals<
      Menu_Framework_menuState, Menu_Framework_loadedSubMenu,
      Menu_Framework_openedWithoutMainMenu, Menu_Framework_actionRestrictions,
      Menu_Framework_submenuStates>
      saved;
  Menu_Framework_loadedSubMenu = 15;
  Menu_Framework_openedWithoutMainMenu = 1;
  Menu_Framework_menuState.currentMenu = 15;
  Menu_Framework_menuState.loading = 0;
  Menu_Framework_actionRestrictions = 0x200;
  Menu_Framework_submenuStates[15] = 2;
  Menu_Framework_submenuStates[14] = static_cast<char>(0xAA);
  Audio_Seq_unusedStateFlags.menuFSubmenu = 3;
  Audio_Seq_unusedScoredata.miscScore = 99;
  VS::Menu::Framework::UpdateLiveMainMenu(15);
  CHECK(Menu_Framework_submenuStates[15] == 0);
  CHECK(static_cast<unsigned char>(Menu_Framework_submenuStates[14]) == 0xAA);
  CHECK(Audio_Seq_unusedScoredata.miscScore == 99);
}

TEST_CASE("Completed MENUF resets time-attack views without losing owners",
          "[time-attack-integration][time-attack-reload][F03011][F03012]") {
  for (const auto asset : kAssets) {
    ReloadFixture fixture;
    fixture.Prime(asset);
    auto& request = tests::RequestFor(asset);
    const auto generation = content::LiveDiscRequestGeneration(&request);
    ReloadFixture::Reload(asset);
    CheckModuleReset(asset);
    fixture.CheckResident();
    CHECK(results::InspectLiveTimeAttackLoad(asset).owns_request);
    CHECK(results::InspectLiveTimeAttackLoad(asset).owns_buffer);
    CHECK(results::InspectLiveTimeAttackLoad(asset).owns_pages);
    CHECK(content::LiveDiscRequestGeneration(&request) == generation);
    REQUIRE(results::CleanupLiveTimeAttackOwnedState());
    ReloadFixture::Reenter(asset);
    CHECK(content::LiveDiscRequestGeneration(&request) != generation);
  }
}

TEST_CASE("Cold sessions cancel time-attack writes and permit fresh entry",
          "[time-attack-integration][time-attack-reload][F03011][F03012]") {
  for (const auto asset : kAssets) {
    ReloadFixture fixture;
    fixture.Prime(asset);
    auto& request = tests::RequestFor(asset);
    const auto generation = content::LiveDiscRequestGeneration(&request);
    void* const foreign = memory::AllocateLiveFromLowEnd(64);
    REQUIRE(foreign != nullptr);
    REQUIRE(render::ReserveLiveTexturePageRange(25, 1, 9) == 25);
    const auto entry = +[] { return 0; };
    REQUIRE(VS::Boot::RunLiveSessionLoop(entry, entry) == 0);
    CheckModuleReset(asset);
    CheckReleased(asset);
    fixture.CheckResident();
    CHECK(content::LiveDiscRequestGeneration(&request) == 0);
    CHECK(memory::LivePayloadCapacity(foreign).has_value());
    CHECK(render::LiveTexturePages()[25] ==
          render::TexturePage{render::TexturePageState::kOwned, 9});
    memory::FreeLiveFromLowEnd(foreign);
    REQUIRE(results::CleanupLiveTimeAttackOwnedState());
    ReloadFixture::Reenter(asset);
    CHECK(content::LiveDiscRequestGeneration(&request) != generation);
  }
}

TEST_CASE("Heap replacement cancels time-attack destinations before reuse",
          "[time-attack-integration][time-attack-heap][F03011][F03012]") {
  for (const auto asset : kAssets) {
    tests::Fixture fixture;
    REQUIRE(tests::Initialize(asset) == 0);
    const auto previous_heap = memory::GameHeap();
    VS::Boot::ResetLiveSessionHeap();
    CheckReleased(asset);
    CHECK(memory::GameHeap().Empty());
    CHECK(Battle_unusedSubmenuState == 1);
    // Keep a failing producer's retained owner cleanable by the fixture.
    if (results::InspectLiveTimeAttackLoad(asset).owns_buffer) {
      memory::GameHeap() = previous_heap;
    }
  }
}

TEST_CASE("Heap replacement abandons a real countdown frame before reuse",
          "[time-attack-integration][time-attack-heap][F03017]") {
  tests::Fixture fixture;
  BeginCountdown(fixture);
  const auto previous_heap = memory::GameHeap();
  const auto refusals = memory::RefusedLiveHeapRequestCount();
  VS::Boot::ResetLiveSessionHeap();
  CheckAbandonedFrame();
  CHECK_FALSE(results::LiveTimeAttackOwnsCountdownControls());
  CHECK(State_tickSpeed == 2);
  CHECK(memory::GameHeap().Empty());
  CHECK(memory::RefusedLiveHeapRequestCount() == refusals);
  if (render::HasLiveFramebufferEffectPackets()) {
    memory::GameHeap() = previous_heap;
  }
}

TEST_CASE("Quiescent frame abandonment frees packets without drawing",
          "[time-attack-integration][time-attack-quiescence][F03017]") {
  tests::Fixture fixture;
  BeginCountdown(fixture);
  REQUIRE(results::CleanupLiveTimeAttackOwnedState());
  void* const foreign = memory::AllocateLiveFromLowEnd(64);
  REQUIRE(foreign != nullptr);
  REQUIRE_FALSE(memory::GameHeap().Empty());
  render::GamePages().Fill({0, 0, 640, 240}, 64, 64, 64);
  const auto pixels = render::GamePages().Pixels();
  const std::vector<std::uint16_t> before(pixels.begin(), pixels.end());
  REQUIRE(render::AbandonLiveFrameBeforeHeapReset());
  CheckAbandonedFrame();
  CHECK(memory::LivePayloadCapacity(foreign).has_value());
  memory::FreeLiveFromLowEnd(foreign);
  CHECK(memory::GameHeap().Empty());
  CHECK(std::ranges::equal(render::GamePages().Pixels(), before));
  CHECK(render::AbandonLiveFrameBeforeHeapReset());
  CHECK(memory::GameHeap().Empty());
}

TEST_CASE("Frame abandonment refuses an unavailable heap without losing owners",
          "[time-attack-integration][time-attack-quiescence][F03017]") {
  tests::Fixture fixture;
  BeginCountdown(fixture);
  const auto effect = render::LiveFramebufferEffect();
  const auto frame = Render_frameScratch;
  const auto tables = std::to_array(Render_legacyOrderingTables);
  const auto buffers = std::to_array(Render_legacyPrimitiveBuffers);
  const auto refusals = render::RefusedLiveScreenOverlayCount();
  {
    const memory::ScopedLiveGameHeap unavailable(std::span<std::byte>{});
    CHECK_FALSE(render::AbandonLiveFrameBeforeHeapReset());
    CHECK(render::HasLiveFramebufferEffectPackets());
    CHECK(render::LiveFramebufferEffect() == effect);
    CheckFrameBindings(frame, tables, buffers);
  }
  CHECK(render::RefusedLiveScreenOverlayCount() == refusals + 1);
  REQUIRE(results::CleanupLiveTimeAttackOwnedState());
  REQUIRE(render::AbandonLiveFrameBeforeHeapReset());
  CHECK(memory::GameHeap().Empty());
}
