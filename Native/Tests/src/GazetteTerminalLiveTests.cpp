#include <algorithm>
#include <array>
#include <cstdint>
#include <span>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Content/BestiaryCatalog.hpp"
#include "Content/DiscLayout.hpp"
#include "Menu/GazetteTerminalLiveFixture.hpp"
#include "Menu/GazetteTerminalPreviewFixture.hpp"
#include "Menu/TextBoxStateLive.hpp"
#include "Render/NativeTextDraw.hpp"
#include "State/Progress.hpp"

namespace {
namespace live = gazette_terminal_live_tests;
namespace shared = gazette_times_live_tests;
namespace score = VS::Menu::Score;
namespace fw = VS::Menu::Framework;
namespace content = VS::Content;

void OpenEnemies(live::PreviewFixture& fixture) {
  fixture.LoadSourceActor();
  fixture.SelectPage(3);
  fixture.AdvanceTo(static_cast<int>(score::GazettePhase::kEnemies));
  REQUIRE(score::LiveBestiaryGeneration() != 0);
  REQUIRE(score::LiveBestiaryEnemies().size() >= 78);
}

TEST_CASE(
    "Native bestiary information detaches old text and honors its own "
    "visibility",
    "[bestiary][gazette-terminal-live][information]") {
  live::Fixture fixture;
  fixture.BorrowMon();
  VS::Input::settings.information = 1;
  fw::InitializeLiveInformationBox();
  const std::array<std::uint8_t, 3> old_text{0x10, 0x11, 0xE7};
  fw::SetLiveInformationMessageText(old_text);
  live::Fixture::UnlockAll();
  score::selectedEnemy = 0;
  score::ShowLiveEnemyDescription();
  auto box = VS::Menu::ReadLiveTextBox(VS::Menu::kInformationTextBox);
  CHECK(box.text == nullptr);
  CHECK_FALSE(score::LiveBestiaryInformationVisible());
  box.state = static_cast<std::int16_t>(box.open_duration + 1);
  VS::Menu::WriteLiveTextBox(VS::Menu::kInformationTextBox, box);
  score::hideUi = 1;
  CHECK(score::LiveBestiaryInformationVisible());
  VS::Input::settings.information = 0;
  CHECK_FALSE(score::LiveBestiaryInformationVisible());
  VS::Input::settings.information = 1;
  fw::DismissLiveInformationBox();
  CHECK_FALSE(score::LiveBestiaryInformationVisible());
}

TEST_CASE(
    "F02805 Live copies unlocked and locked descriptions through the real "
    "information box",
    "[gazette-terminal][gazette-terminal-live][F02805]") {
  live::Fixture fixture;
  fixture.BorrowMon();
  fw::InitializeLiveInformationBox();
  score::selectedEnemy = 0;
  live::Fixture::UnlockAll();
  score::ShowLiveEnemyDescription();
  CHECK(score::LiveBestiaryDescription() == "text.bestiary.zombie.description");
  std::ranges::fill(VS::State::scoreData.enemy_kill_flags, 0);
  score::InitializeLiveEnemyList();
  score::ShowLiveEnemyDescription();
  CHECK(score::LiveBestiaryDescription() == "text.bestiary.locked.description");
  CHECK(fixture.GuardsIntact());
}

void CheckDigitGeometry(std::span<const std::uint32_t> packet, int x) {
  CHECK(packet[2] == fw::PackXy(x, 26));
  CHECK(packet[4] == fw::PackXy(x + 6, 26));
  CHECK(packet[6] == fw::PackXy(x, 36));
  CHECK(packet[8] == fw::PackXy(x + 6, 36));
}
void CheckDigitPacket(std::span<const std::uint32_t> packet, std::size_t index,
                      bool dimmed) {
  const std::array<unsigned, 3> digits{1, 2, 7};
  REQUIRE(packet.size() == 10);
  CHECK(packet[0] == 0x09000000);
  CHECK(packet[1] == (dimmed ? 0x2E202020U : 0x2E808080U));
  CheckDigitGeometry(packet, 34 + static_cast<int>(index) * 5);
  CHECK(packet[3] ==
        ((dimmed ? 0x37F70000U : 0x37F40000U) | digits.at(index) * 6));
  CHECK(packet[5] ==
        ((dimmed ? 0x002C0000U : 0x000C0000U) | (digits.at(index) + 1) * 6));
}
void CheckUnlockedRing(std::span<const score::GazetteEnemy> rows) {
  REQUIRE(rows.size() >= 78);
  CHECK(rows[0].unlocked == 1);
  CHECK(rows[0].previous == 77);
  CHECK(rows[0].next == 1);
  CHECK(rows[1].next == 77);
  CHECK(rows[77].next == 0);
}
TEST_CASE(
    "F02817 Live queues three complete digit quads on the real overlay chain",
    "[gazette-terminal][gazette-terminal-live][F02817]") {
  live::Fixture fixture;
  const bool dimmed = GENERATE(false, true);
  const auto refusals = score::LiveGazetteTerminalRefusals();
  score::DrawLiveEnemyNumber(20, 30, 127, dimmed ? 1 : 0);
  REQUIRE(fixture.UsedWords() == 30);
  const auto packets = fixture.Packets();
  REQUIRE(packets.size() == 3);
  for (std::size_t i = 0; i < packets.size(); ++i) {
    CheckDigitPacket(packets[i], i, dimmed);
  }
  CHECK(score::LiveGazetteTerminalRefusals() == refusals);
}

TEST_CASE(
    "F02818 Live applies native defeat ranges without changing definitions",
    "[gazette-terminal][gazette-terminal-live][F02818]") {
  live::Fixture fixture;
  fixture.BorrowMon();
  const auto* store = content::DataStore::Get();
  REQUIRE(store != nullptr);
  REQUIRE(store->CatalogTables() != nullptr);
  const auto definitions = content::LoadBestiary(*store->CatalogTables());
  REQUIRE(definitions);
  std::ranges::fill(VS::State::scoreData.enemy_kill_flags, 0);
  VS::State::scoreData.enemy_kill_flags[0] = (1 << 2) | (1 << 6);
  VS::State::scoreData.enemy_kill_flags[4] = 1 << 8;
  score::InitializeLiveEnemyList();
  const auto rows = score::LiveBestiaryEnemies();
  CheckUnlockedRing(rows);
  CHECK(content::LoadBestiary(*store->CatalogTables()) == definitions);
}

TEST_CASE(
    "F02819 Live renders eight real rows digits cursor and copied information",
    "[gazette-terminal][gazette-terminal-live][F02819]") {
  live::Fixture fixture;
  fixture.BorrowMon();
  live::Fixture::UnlockAll();
  fw::InitializeLiveInformationBox();
  const auto refusals = score::LiveGazetteTerminalRefusals();
  score::RenderLiveEnemyList(8);
  CHECK(fixture.UsedWords() >= 240);
  CHECK(fixture.Cursor().position == fw::PackXy(48, 48));
  CHECK(score::LiveBestiaryDescription() == "text.bestiary.zombie.description");
  CHECK(score::LiveBestiaryEnemies()[0].selected == 1);
  CHECK(score::LiveBestiaryEnemies()[0].highlight_step == 8);
  CHECK(shared::Fixture::Row(0).state == fw::RowState::kInactive);
  CHECK(score::LiveGazetteTerminalRefusals() == refusals);
}

TEST_CASE(
    "F02820 Live viewer emits real font controls identification and panels",
    "[gazette-terminal][gazette-terminal-live][F02820]") {
  live::Fixture fixture;
  fixture.BorrowMon();
  live::Fixture::UnlockAll();
  score::selectedEnemy = 0;
  const auto projection = VS::Render::GetLiveProjectionScreenOffset();
  const auto refusals = score::LiveGazetteTerminalRefusals();
  score::RenderLiveEnemyViewer(8, 3);
  CHECK_FALSE(fixture.Packets().empty());
  CHECK(shared::Fixture::Row(0).state == fw::RowState::kInactive);
  CHECK(VS::Render::GetLiveProjectionScreenOffset().x == projection.x);
  CHECK(VS::Render::GetLiveProjectionScreenOffset().y == projection.y);
  CHECK(score::LiveGazetteTerminalRefusals() == refusals);
  CHECK(fixture.GuardsIntact());
}

TEST_CASE("F02834 Live navigation emits its real arrow and directional font",
          "[gazette-terminal][gazette-terminal-live][F02834]") {
  live::Fixture fixture;
  const auto direction = GENERATE(0, 1, 2, 3);
  score::hideUi = 0;
  score::DrawLiveEnemyNavigation(direction);
  CHECK_FALSE(fixture.Packets().empty());
  fixture.ResetFrame();
  score::hideUi = 1;
  score::DrawLiveEnemyNavigation(direction);
  CHECK(fixture.Packets().empty());
  CHECK(fixture.GuardsIntact());
}

TEST_CASE(
    "F02820 Live viewer draws a genuinely loaded enemy model and restores "
    "projection",
    "[gazette-terminal][gazette-terminal-live][gazette-terminal-preview]["
    "F02820]") {
  live::PreviewFixture fixture;
  fixture.BorrowMon();
  live::Fixture::UnlockAll();
  fixture.LoadEnemy(score::LiveBestiaryEnemies()[0].model_id);
  score::selectedEnemy = 0;
  const auto projection = VS::Render::GetLiveProjectionScreenOffset();
  score::RenderLiveEnemyViewer(8, 0);
  CHECK(VS::Actors::actorRecords[1] != nullptr);
  CHECK_FALSE(fixture.Packets().empty());
  CHECK(VS::Render::GetLiveProjectionScreenOffset().x == projection.x);
  CHECK(VS::Render::GetLiveProjectionScreenOffset().y == projection.y);
  CHECK(fixture.GuardsIntact());
}

TEST_CASE("F02803 Live prepares the real overview rank and actor preview",
          "[gazette-terminal][gazette-terminal-live][gazette-terminal-preview]["
          "F02803]") {
  live::PreviewFixture fixture;
  fixture.LoadSourceActor();
  fixture.SettleOverview();
  CHECK(score::menuState == static_cast<int>(score::GazettePhase::kOverview));
  CHECK(VS::Actors::actorRecords[1] != nullptr);
  CHECK_FALSE(live::Fixture::RankText().empty());
  CHECK(score::LiveBestiaryGeneration() == 0);
  CHECK(fixture.GuardsIntact());
}

TEST_CASE(
    "F02803 Live opens native bestiary and an actual preview through "
    "controller input",
    "[gazette-terminal][gazette-terminal-live][gazette-terminal-preview]["
    "F02803]") {
  live::PreviewFixture fixture;
  std::ranges::fill(VS::State::scoreData.enemy_kill_flags, -1);
  OpenEnemies(fixture);
  live::Fixture::Input(live::kCircle);
  fixture.TickGazette();
  live::Fixture::Input();
  fixture.AdvanceTo(static_cast<int>(score::GazettePhase::kViewer));
  CHECK(VS::Actors::actorRecords[1] != nullptr);
  CHECK(score::selectedEnemy == 0);
  CHECK(score::LiveBestiaryDescription() == "text.bestiary.zombie.description");
  live::Fixture::Input(live::kCross);
  fixture.TickGazette();
  live::Fixture::Input();
  fixture.AdvanceTo(static_cast<int>(score::GazettePhase::kEnemies));
  CHECK(score::LiveBestiaryGeneration() != 0);
  CHECK(fixture.GuardsIntact());
}

TEST_CASE(
    "F02803 Live cancellation releases the native snapshot and permits real "
    "reentry",
    "[gazette-terminal][gazette-terminal-live][gazette-terminal-preview]["
    "F02803]") {
  live::PreviewFixture fixture;
  OpenEnemies(fixture);
  const auto first = score::LiveBestiaryGeneration();
  live::Fixture::Input(live::kCross);
  fixture.TickGazette();
  live::Fixture::Input();
  fixture.AdvanceTo(static_cast<int>(score::GazettePhase::kPrepareOverview));
  CHECK(score::LiveBestiaryGeneration() == 0);
  CHECK(score::LiveBestiaryEnemies().empty());
  fixture.SelectPage(3);
  fixture.AdvanceTo(static_cast<int>(score::GazettePhase::kEnemies));
  CHECK(score::LiveBestiaryGeneration() > first);
  CHECK(score::LiveBestiaryEnemies()[0].highlight_step == 0);
  CHECK(fixture.GuardsIntact());
}

TEST_CASE(
    "F02803 Live draws each dependent Gazette page and returns through real "
    "controls",
    "[gazette-terminal][gazette-terminal-live][gazette-terminal-preview]["
    "F02803]") {
  live::PreviewFixture fixture;
  fixture.LoadSourceActor();
  const int page = GENERATE(0, 1, 2);
  fixture.SelectPage(page);
  const int phase = static_cast<int>(score::GazettePhase::kTitles) + page * 3;
  fixture.AdvanceTo(phase);
  fixture.TickGazette();
  CHECK_FALSE(fixture.Packets().empty());
  live::Fixture::Input(live::kCross);
  fixture.TickGazette();
  live::Fixture::Input();
  fixture.AdvanceTo(static_cast<int>(score::GazettePhase::kPrepareOverview));
  CHECK(score::LiveBestiaryGeneration() == 0);
  CHECK(fixture.GuardsIntact());
}

TEST_CASE(
    "Gazette terminal refuses a second native owner and resets both "
    "publication phases",
    "[gazette-terminal][gazette-terminal-live]") {
  live::Fixture fixture;
  const bool ready = GENERATE(false, true);
  REQUIRE(score::BeginLiveBestiary());
  if (ready) REQUIRE(score::FinishLiveBestiary());
  CHECK_FALSE(score::BeginLiveBestiary());
  CHECK(score::ResetLiveGazetteTerminalModuleState());
  CHECK(score::LiveBestiaryGeneration() == 0);
  CHECK(score::LiveBestiaryDescription().empty());
  REQUIRE(score::BeginLiveBestiary());
  CHECK(score::FinishLiveBestiary());
  CHECK_FALSE(score::FinishLiveBestiary());
}

TEST_CASE(
    "Gazette genuine fixture restores its borrowed source and runtime owners",
    "[gazette-terminal][gazette-terminal-live]") {
  const auto* store = content::DataStore::Get();
  const auto before = live::BytesOf(VS::Battle::menu9CursorMemory);
  const std::vector<std::byte> cursor(before.begin(), before.end());
  {
    live::Fixture fixture;
    fixture.BorrowMon();
    live::Fixture::UnlockAll();
    score::RenderLiveEnemyList(8);
    CHECK(score::LiveBestiaryGeneration() != 0);
  }
  CHECK(content::DataStore::Get() == store);
  CHECK(score::LiveBestiaryGeneration() == 0);
  CHECK(std::ranges::equal(before, cursor));
}

TEST_CASE("F02803 Live loads native bestiary with a full foreign disc queue",
          "[gazette-terminal][gazette-terminal-live][F02803]") {
  live::Fixture fixture;
  std::array<content::DiscQueueSlot*, 32> slots{};
  const content::DiscFile file{content::disc::Locate("SMALL/MON.BIN").lba,
                               2048};
  for (auto& slot : slots) {
    slot = content::AllocateLiveDiscRequest(&file);
    REQUIRE(slot != nullptr);
  }
  const auto bytes = live::BytesOf(content::discRequestSlots);
  const std::vector<std::byte> before(bytes.begin(), bytes.end());
  CHECK(score::BeginLiveBestiary());
  CHECK(score::FinishLiveBestiary());
  CHECK(score::ReleaseLiveGazetteEnemyData());
  CHECK(std::ranges::equal(bytes, before));
  for (auto* slot : slots) content::ReleaseLiveDiscRequest(slot);
  CHECK(fixture.GuardsIntact());
}

void PrepareGazetteScreen(live::PreviewFixture& fixture, std::uint8_t& phase) {
  REQUIRE(score::UpdateLiveGazetteScreen(&phase) == 0);
  REQUIRE(phase == 4);
  CHECK(VS::Battle::hudHidden == 0);
  CHECK(VS::Field::isFieldRenderingEnabled == 1);
  for (std::size_t tick = 0; tick < live::kFrameBudget && phase != 6; ++tick) {
    fixture.ResetFrame();
    live::fw::UpdateLiveScreen();
    REQUIRE(score::UpdateLiveGazetteScreen(&phase) == 0);
  }
  REQUIRE(phase == 6);
}
void CheckScreenProgress() {
  CHECK(score::score == 0);
  CHECK(score::characterRank == 15);
  CHECK(VS::Battle::hudHidden == 1);
  CHECK(VS::Battle::hudSlideOffset == 0x200000);
  CHECK(VS::Field::isFieldRenderingEnabled == 0);
}
void DrawActiveGazetteScreen(live::PreviewFixture& fixture,
                             std::uint8_t& phase) {
  for (int tick = 0; tick < 40; ++tick) {
    fixture.ResetFrame();
    live::fw::UpdateLiveScreen();
    REQUIRE(score::UpdateLiveGazetteScreen(&phase) == 0);
  }
  REQUIRE(score::menuState == 2);
  CHECK(VS::Menu::Framework::backgroundFadeStep > 0);
}
void RequestScreenExit(live::PreviewFixture& fixture, std::uint8_t& phase) {
  live::Fixture::Input(live::kTriangle);
  fixture.ResetFrame();
  live::fw::UpdateLiveScreen();
  REQUIRE(score::UpdateLiveGazetteScreen(&phase) == 0);
  live::Fixture::Input();
  for (std::size_t tick = 0; tick < live::kFrameBudget && phase != 7; ++tick) {
    fixture.ResetFrame();
    live::fw::UpdateLiveScreen();
    REQUIRE(score::UpdateLiveGazetteScreen(&phase) == 0);
  }
  REQUIRE(phase == 7);
}
void CheckScreenRestoredField() {
  CHECK(VS::Actors::actorRecords[1] == nullptr);
  CHECK(VS::Battle::hudHidden == 0);
  CHECK(VS::Battle::hudSlideOffset == 0);
  CHECK(VS::Field::isFieldRenderingEnabled == 1);
}
void BeginScreenFade(std::uint8_t& phase) {
  REQUIRE(score::UpdateLiveGazetteScreen(&phase) == 0);
  REQUIRE(phase == 8);
  const int initial_fade = VS::Menu::Framework::backgroundFadeStep;
  REQUIRE(initial_fade > 0);
}
void FinishScreenFade(live::PreviewFixture& fixture, std::uint8_t& phase) {
  int result = 0;
  for (std::size_t tick = 0; tick < live::kFrameBudget && result == 0; ++tick) {
    fixture.ResetFrame();
    live::fw::UpdateLiveScreen();
    result = score::UpdateLiveGazetteScreen(&phase);
  }
  CHECK(result == 1);
  CHECK(phase == 0);
  CHECK(VS::Menu::Framework::backgroundFadeStep == 0);
  CHECK(VS::Menu::Framework::menuState.control.current_menu == 9);
  CHECK(VS::Menu::Framework::hideMenu == 0);
}
TEST_CASE(
    "F04156 Live calculates progress and leaves through real fade and "
    "controller services",
    "[gazette-terminal][gazette-terminal-live][gazette-terminal-screen]["
    "F04156]") {
  live::PreviewFixture fixture;
  fixture.LoadSourceActor();
  VS::Field::isFieldRenderingEnabled = 1;
  VS::Battle::hudHidden = 0;
  std::uint8_t phase = 3;
  const auto refusals = score::LiveGazetteTerminalRefusals();
  PrepareGazetteScreen(fixture, phase);
  CheckScreenProgress();
  DrawActiveGazetteScreen(fixture, phase);
  RequestScreenExit(fixture, phase);
  CheckScreenRestoredField();
  BeginScreenFade(phase);
  FinishScreenFade(fixture, phase);
  CHECK(score::LiveGazetteTerminalRefusals() == refusals);
  CHECK(fixture.GuardsIntact());
}
}  // namespace
