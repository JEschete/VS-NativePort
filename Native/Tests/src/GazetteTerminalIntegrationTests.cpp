#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Boot/SessionLive.hpp"
#include "Content/DiscLayout.h"
#include "Event/ScriptVmLiveState.hpp"
#include "Menu/GameClearLifecycleLiveFixture.hpp"
#include "Menu/GazetteTerminalLiveFixture.hpp"
#include "Menu/GazetteTerminalPreviewFixture.hpp"
#include "Menu/OverlayLoadFixture.hpp"
#include "State/Progress.hpp"

namespace {
namespace live = gazette_terminal_live_tests;
namespace shared = gazette_times_live_tests;
namespace score = VS::Menu::Score;
namespace fw = VS::Menu::Framework;
namespace content = VS::Content;
namespace memory = VS::Core::Memory;
constexpr int kGazetteScreen = static_cast<int>(fw::Submenu::kScore) + 1;

template <typename Entry>
Entry ModuleEntry(unsigned address, Entry expected) {
  auto* resolved = content::ResolveModuleAddress(VS::Content::ModuleSymbols(),
                                                 "MENU/MENU9.PRG", address);
  CHECK(resolved == reinterpret_cast<void*>(expected));
  if (resolved != reinterpret_cast<void*>(expected)) {
    throw std::runtime_error("incorrect Gazette terminal module entry");
  }
  return reinterpret_cast<Entry>(resolved);
}

struct MonOwner {
  void* buffer;
  VS::Content::DiscQueueSlot* request;
  std::uint64_t generation;

  void CheckReleased() const {
    CHECK(score::monBinData == nullptr);
    CHECK(score::monBinCdQueueSlot == nullptr);
    CHECK(score::enemyDescriptions == nullptr);
    CHECK(score::menuState == 0);
    CHECK_FALSE(memory::LivePayloadCapacity(buffer).has_value());
    CHECK(content::LiveDiscRequestGeneration(request) != generation);
  }
};

MonOwner BeginMon(live::PreviewFixture& fixture, bool loaded) {
  fixture.LoadSourceActor();
  std::ranges::fill(VS::State::scoreData.enemy_kill_flags, -1);
  fixture.SelectPage(3, false);
  fixture.AdvanceTo(static_cast<int>(score::GazettePhase::kWaitForEnemies),
                    false);
  const MonOwner owner{
      score::monBinData, score::monBinCdQueueSlot,
      content::LiveDiscRequestGeneration(score::monBinCdQueueSlot)};
  REQUIRE(owner.buffer != nullptr);
  REQUIRE(owner.request != nullptr);
  REQUIRE(owner.generation != 0);
  if (loaded) {
    fixture.AdvanceTo(static_cast<int>(score::GazettePhase::kEnemies));
  } else {
    REQUIRE(content::BeginLiveDiscRead(
        score::kMonBinFile.lba,
        static_cast<std::uint32_t>(score::kMonBinFile.size), owner.buffer));
    REQUIRE(VS::Content::discDriveState.destination == owner.buffer);
  }
  return owner;
}

struct ForeignOwner {
  std::vector<std::byte> cursor;
  void* actor = VS::Actors::actorRecords[0];
  void* allocation = memory::AllocateLiveFromLowEnd(64);
  VS::Content::DiscQueueSlot* request = nullptr;
  std::uint64_t generation = 0;

  ForeignOwner() {
    const auto bytes = live::BytesOf(VS::Battle::menu9CursorMemory);
    cursor.assign(bytes.begin(), bytes.end());
    const VS::Content::DiscFile file{score::kMonBinFile.lba, 2048};
    request = content::AllocateLiveDiscRequest(&file);
    generation = content::LiveDiscRequestGeneration(request);
    if (allocation == nullptr || request == nullptr || generation == 0) {
      throw std::runtime_error("terminal foreign-owner control unavailable");
    }
  }
  ~ForeignOwner() {
    if (content::LiveDiscRequestGeneration(request) == generation) {
      content::ReleaseLiveDiscRequest(request);
    }
    if (memory::LivePayloadCapacity(allocation)) {
      memory::FreeLiveFromLowEnd(allocation);
    }
  }
  ForeignOwner(const ForeignOwner&) = delete;
  ForeignOwner& operator=(const ForeignOwner&) = delete;

  void Check() const {
    CHECK(content::LiveDiscRequestGeneration(request) == generation);
    CHECK(memory::LivePayloadCapacity(allocation).has_value());
    CHECK(VS::Actors::actorRecords[0] == actor);
    CHECK(std::ranges::equal(live::BytesOf(VS::Battle::menu9CursorMemory),
                             cursor));
    CHECK(VS::Content::discRequestCounts.reading == 0);
    CHECK(VS::Content::discRequestCounts.waiting == 0);
  }
};

TEST_CASE("Gazette enemy module entries use real links descriptions and digits",
          "[gazette-terminal-integration][gazette-terminal-module]"
          "[F02805][F02817][F02818]") {
  live::Fixture fixture;
  fixture.BorrowMon();
  std::ranges::fill(VS::State::scoreData.enemy_kill_flags, -1);
  ModuleEntry(0x80105D8C, &score::InitializeLiveEnemyList)();
  fw::InitializeLiveInformationBox();
  score::selectedEnemy = 0;
  ModuleEntry(0x80104B40, &score::ShowLiveEnemyDescription)();
  CHECK(fixture.InformationText() == live::Fixture::Description(0));
  fixture.ResetFrame();
  ModuleEntry(0x80105BCC, &score::DrawLiveEnemyNumber)(32, 48, 78, 0);
  CHECK(fixture.Packets().size() == 3);
  CHECK(fixture.GuardsIntact());
}

TEST_CASE(
    "Gazette render module entries emit production list viewer and arrows",
    "[gazette-terminal-integration][gazette-terminal-module]"
    "[F02819][F02820][F02834]") {
  live::Fixture fixture;
  fixture.BorrowMon();
  live::Fixture::UnlockAll();
  fw::InitializeLiveInformationBox();
  const auto refusals = score::LiveGazetteTerminalRefusals();
  ModuleEntry(0x80105F00, &score::RenderLiveEnemyList)(8);
  CHECK(fixture.InformationText() == live::Fixture::Description(0));
  fixture.ResetFrame();
  ModuleEntry(0x801061F8, &score::RenderLiveEnemyViewer)(8, 3);
  CHECK_FALSE(fixture.Packets().empty());
  fixture.ResetFrame();
  ModuleEntry(0x80107A98, &score::DrawLiveEnemyNavigation)(0);
  CHECK_FALSE(fixture.Packets().empty());
  CHECK(score::LiveGazetteTerminalRefusals() == refusals);
  CHECK(fixture.GuardsIntact());
}

TEST_CASE("Gazette terminal module entries initialize real preview and screen",
          "[gazette-terminal-integration][gazette-terminal-module]"
          "[F02803][F04156]") {
  live::PreviewFixture fixture;
  fixture.LoadSourceActor();
  CHECK(ModuleEntry(0x8010337C, &score::UpdateLiveGazette)() == 0);
  CHECK(score::menuState == 1);
  CHECK(ModuleEntry(0x8010337C, &score::UpdateLiveGazette)() == 0);
  CHECK(score::menuState == 2);
  CHECK(VS::Actors::actorRecords[1] != nullptr);
  std::array<std::uint8_t, 2> phase{3, 0xAA};
  CHECK(ModuleEntry(0x80102FF8,
                    &score::UpdateLiveGazetteScreen)(phase.data()) == 0);
  CHECK(phase == std::array<std::uint8_t, 2>{4, 0xAA});
  CHECK(fixture.GuardsIntact());
}

TEST_CASE(
    "Production framework dispatch consumes the actual Gazette state byte",
    "[gazette-terminal-integration][gazette-terminal-framework]"
    "[F04156]") {
  live::Fixture fixture;
  const game_clear_live_tests::SavedGlobals<
      VS::Menu::Framework::menuState, VS::Menu::Framework::loadedSubMenu,
      VS::Menu::Framework::openedWithoutMainMenu,
      VS::Menu::Framework::actionRestrictions,
      VS::Menu::Framework::submenuStates>
      saved;
  VS::Menu::Framework::loadedSubMenu = kGazetteScreen;
  VS::Menu::Framework::openedWithoutMainMenu = 1;
  VS::Menu::Framework::menuState.control.current_menu = kGazetteScreen;
  VS::Menu::Framework::menuState.control.loading = 0;
  VS::Menu::Framework::actionRestrictions = 0;
  VS::Menu::Framework::submenuStates[kGazetteScreen] = 3;
  VS::Menu::Framework::submenuStates[kGazetteScreen - 1] =
      static_cast<char>(0xAA);
  score::score = 12345;
  score::characterRank = 0xFF;
  fw::UpdateLiveMainMenu(kGazetteScreen);
  CHECK(VS::Menu::Framework::submenuStates[kGazetteScreen] == 4);
  CHECK(static_cast<unsigned char>(
            VS::Menu::Framework::submenuStates[kGazetteScreen - 1]) == 0xAA);
  CHECK(VS::Menu::Framework::backgroundFadeStep > 0);
  fw::UpdateLiveMainMenu(kGazetteScreen);
  CHECK(score::score == 0);
  CHECK(score::characterRank == 15);
  CHECK(fixture.GuardsIntact());
}

TEST_CASE("Only completed MENU9 drains the real pending or consumed MON owner",
          "[gazette-terminal-integration][gazette-terminal-reload]"
          "[F02803][F04156]") {
  const bool loaded = GENERATE(false, true);
  live::PreviewFixture fixture;
  const auto owner = BeginMon(fixture, loaded);
  const ForeignOwner foreign;
  {
    menu_overlay_tests::Load other(3, {VS_MENU2_PRG_LBA, VS_MENU2_PRG_SIZE});
    REQUIRE(other.Complete() == 1);
    CHECK(score::monBinData == owner.buffer);
  }
  {
    menu_overlay_tests::Load menu(kGazetteScreen,
                                  {VS_MENU9_PRG_LBA, VS_MENU9_PRG_SIZE});
    REQUIRE(menu.Poll() == 0);
    CHECK(score::monBinData == owner.buffer);
    REQUIRE(menu.Complete() == 1);
  }
  owner.CheckReleased();
  foreign.Check();
  CHECK(VS::Content::discDriveState.destination == nullptr);
  CHECK(fixture.GuardsIntact());
}

TEST_CASE("Actual cold session drains the real pending or consumed MON owner",
          "[gazette-terminal-integration][gazette-terminal-reload]"
          "[F02803][F04156]") {
  const bool loaded = GENERATE(false, true);
  const game_clear_live_tests::SavedModuleData saved_module;
  const game_clear_live_tests::SavedGlobals<
      VS::Menu::textBoxSelector, VS::Battle::stringContext,
      VS::Event::isTextBoxAwaitingClose, VS::Menu::sceneNotificationBuffer,
      VS::Menu::sceneNotificationTimer, VS::Menu::sceneNotificationTimed>
      saved_ui;
  live::PreviewFixture fixture;
  const auto owner = BeginMon(fixture, loaded);
  const ForeignOwner foreign;
  const auto entry = +[] { return 0; };
  CHECK(VS::Boot::RunLiveSessionLoop(entry, entry) == 0);
  owner.CheckReleased();
  foreign.Check();
  CHECK(VS::Content::discDriveState.destination == nullptr);
  CHECK(fixture.GuardsIntact());
}

TEST_CASE("Actual pre-heap boundary drains MON before replacing heap identity",
          "[gazette-terminal-integration][gazette-terminal-heap]"
          "[F02803][F04156]") {
  const bool loaded = GENERATE(false, true);
  live::PreviewFixture fixture;
  const auto owner = BeginMon(fixture, loaded);
  static_cast<void>(content::ReleaseLiveActorModel(0));
  const auto refusals = memory::RefusedLiveHeapRequestCount();
  VS::Boot::ResetLiveSessionHeap();
  owner.CheckReleased();
  CHECK(VS::Content::discDriveState.destination == nullptr);
  CHECK(VS::Content::discRequestCounts.reading == 0);
  CHECK(VS::Content::discRequestCounts.waiting == 0);
  CHECK(memory::GameHeap().Empty());
  CHECK(memory::RefusedLiveHeapRequestCount() == refusals);
}

void ResetAtBoundary(bool cold) {
  if (cold) {
    const auto entry = +[] { return 0; };
    REQUIRE(VS::Boot::RunLiveSessionLoop(entry, entry) == 0);
  } else {
    menu_overlay_tests::Load menu(kGazetteScreen,
                                  {VS_MENU9_PRG_LBA, VS_MENU9_PRG_SIZE});
    REQUIRE(menu.Complete() == 1);
  }
}

TEST_CASE("Actual Gazette boundaries drain pending ZUD before its MON owner",
          "[gazette-terminal-integration][gazette-terminal-reload]"
          "[F02803][F04156]") {
  const bool cold = GENERATE(false, true);
  const game_clear_live_tests::SavedModuleData saved_module;
  const game_clear_live_tests::SavedGlobals<
      VS::Menu::textBoxSelector, VS::Battle::stringContext,
      VS::Event::isTextBoxAwaitingClose, VS::Menu::sceneNotificationBuffer,
      VS::Menu::sceneNotificationTimer, VS::Menu::sceneNotificationTimed>
      saved_ui;
  live::PreviewFixture fixture;
  const auto owner = BeginMon(fixture, true);
  REQUIRE(fixture.TickGazette() == 0);
  live::Fixture::Input(live::kCircle);
  REQUIRE(fixture.TickGazette(false) == 0);
  const auto preview = score::LiveEnemyPreviewState();
  REQUIRE(preview.file != content::AssetAddress::kNone);
  REQUIRE(preview.model_buffer != content::AssetAddress::kNone);
  const ForeignOwner foreign;
  ResetAtBoundary(cold);
  owner.CheckReleased();
  CHECK(score::LiveEnemyPreviewState() == score::EnemyPreviewState{});
  CHECK_FALSE(memory::LivePayloadCapacity(reinterpret_cast<void*>(preview.file))
                  .has_value());
  CHECK_FALSE(
      memory::LivePayloadCapacity(reinterpret_cast<void*>(preview.model_buffer))
          .has_value());
  foreign.Check();
}

shared::Sprite DrawEnemyCursor(live::PreviewFixture& fixture) {
  fixture.ResetFrame();
  score::RenderLiveEnemyList(8);
  return fixture.Cursor();
}

TEST_CASE(
    "Actual Gazette reset restores the private cursor through real reentry",
    "[gazette-terminal-integration][gazette-terminal-reload]"
    "[F02819][F02803]") {
  const bool cold = GENERATE(false, true);
  const game_clear_live_tests::SavedModuleData saved_module;
  const game_clear_live_tests::SavedGlobals<
      VS::Menu::textBoxSelector, VS::Battle::stringContext,
      VS::Event::isTextBoxAwaitingClose, VS::Menu::sceneNotificationBuffer,
      VS::Menu::sceneNotificationTimer, VS::Menu::sceneNotificationTimed>
      saved_ui;
  live::PreviewFixture fixture;
  const auto owner = BeginMon(fixture, true);
  std::array<shared::Sprite, 4> cursors{};
  for (auto& cursor : cursors) cursor = DrawEnemyCursor(fixture);
  REQUIRE(cursors[0] != cursors[1]);
  ResetAtBoundary(cold);
  owner.CheckReleased();
  fixture.SelectPage(3);
  fixture.AdvanceTo(static_cast<int>(score::GazettePhase::kEnemies));
  CHECK(DrawEnemyCursor(fixture) == cursors[0]);
  CHECK(DrawEnemyCursor(fixture) == cursors[1]);
  CHECK(fixture.GuardsIntact());
}
}  // namespace
