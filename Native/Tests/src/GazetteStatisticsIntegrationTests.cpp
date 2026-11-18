#include <algorithm>
#include <array>
#include <cstddef>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Boot/SessionLive.hpp"
#include "Content/DiscLayout.h"
#include "Event/ScriptVm.h"
#include "Menu/GameClearLifecycleLiveFixture.hpp"
#include "Menu/GazetteStatisticsLiveFixture.hpp"
#include "Menu/OverlayLoadFixture.hpp"

namespace {
namespace live = gazette_statistics_live_tests;
namespace shared = gazette_times_live_tests;
namespace score = VS::Menu::Score;
namespace memory = VS::Core::Memory;

enum class Page { kTitles, kStatistics };

template <typename Entry>
Entry ModuleEntry(unsigned address, Entry expected) {
  auto* resolved = VS::Content::ResolveModuleAddress(
      {Content_moduleSymbols, Content_moduleSymbolCount}, "MENU/MENU9.PRG",
      address);
  CHECK(resolved == reinterpret_cast<void*>(expected));
  if (resolved != reinterpret_cast<void*>(expected)) {
    throw std::runtime_error("incorrect Gazette statistics module entry");
  }
  return reinterpret_cast<Entry>(resolved);
}

struct Fixture : live::StatisticsFixture {
  shared::Sprite DrawCursor(Page page) {
    ResetFrame();
    if (page == Page::kTitles) {
      score::RenderLiveTitles(8);
    } else {
      score::RenderLiveStatistics(8);
    }
    return Cursor();
  }

  std::array<shared::Sprite, 4> PrimeCursor(Page page) {
    Audio_Seq_unusedScoredata.titles = -1;
    Audio_Seq_unusedScoredata.completionTimeMinutes = 60;
    Menu_Score_clearCount = 3;
    Menu_Score_mapCompletion = 180;
    Menu_Score_openedChestCount = 26;
    if (page == Page::kTitles) {
      score::CreateLiveTitleRows();
    } else {
      score::CreateLiveStatisticRows();
    }
    std::array<shared::Sprite, 4> sequence{};
    for (auto& cursor : sequence) cursor = DrawCursor(page);
    score::ResetLiveGazetteStatisticsStatics();
    CHECK(DrawCursor(page) == sequence[0]);
    CHECK(DrawCursor(page) == sequence[1]);
    REQUIRE(sequence[0] != sequence[1]);
    return sequence;
  }
};

struct ResidentState {
  shared::ObservableState rows;
  std::vector<std::byte> cursor;

  explicit ResidentState(const Fixture& fixture) : rows(fixture.Observe()) {
    const auto bytes = shared::BytesOf(Battle_unusedMenu9CursorMemory);
    cursor.assign(bytes.begin(), bytes.end());
  }

  void Check() const {
    CHECK(std::ranges::equal(shared::BytesOf(Battle_unusedMenu9CursorMemory),
                             cursor));
    CHECK(Audio_Seq_unusedScoredata.titles == -1);
    CHECK(Audio_Seq_unusedScoredata.completionTimeMinutes == 60);
    CHECK(Menu_Score_clearCount == 3);
    CHECK(Menu_Score_mapCompletion == 180);
    CHECK(Menu_Score_openedChestCount == 26);
    for (std::size_t i = 0; i < rows.rows.size(); ++i) {
      CHECK(Fixture::Row(i).text == rows.rows[i].text);
      CHECK(Fixture::Row(i).x == rows.rows[i].x);
      CHECK(Fixture::Row(i).state == rows.rows[i].state);
    }
  }
};
void CheckTitleModuleEntries(const Fixture& fixture) {
  Audio_Seq_unusedScoredata.titles = -1;
  ModuleEntry(0x801056B8, &score::CreateLiveTitleRows)();
  CHECK(Menu_Score_gazetteRows[31].rowIndex == 32);
  ModuleEntry(0x8010579C, &score::RenderLiveTitles)(8);
  CHECK(fixture.Cursor().position == 0x00300030);
  CHECK(Fixture::InformationText() ==
        shared::Label(Menu_Score_titleDescriptions, 0));
}

void CheckStatisticModuleEntries(Fixture& fixture) {
  ModuleEntry(0x80106780, &score::CreateLiveStatisticRows)();
  CHECK(Menu_Score_gazetteRows[22].rowIndex == 23);
  fixture.ResetFrame();
  ModuleEntry(0x80106808, &score::RenderLiveStatistics)(8);
  CHECK(fixture.Cursor().position == 0x00300004);
  CHECK(Fixture::InformationText() ==
        shared::Label(Menu_Score_statDescriptions, 0));
}
}  // namespace

TEST_CASE("Gazette statistics module entries reach real rows glyphs and digits",
          "[gazette-statistics-integration][gazette-statistics-module]"
          "[F02814][F02815][F02816][F02824][F02825]") {
  Fixture fixture;
  const auto refusals = score::LiveGazetteStatisticsRefusals();
  CheckTitleModuleEntries(fixture);
  fixture.ResetFrame();
  ModuleEntry(0x80105A0C, &score::DrawLiveTitleNumber)(46, 72, 32, 0);
  CHECK(fixture.UsedWords() == 20);
  CHECK(fixture.Packets().size() == 2);
  CheckStatisticModuleEntries(fixture);
  CHECK(fixture.GuardsIntact());
  CHECK(score::LiveGazetteStatisticsRefusals() == refusals);
}

TEST_CASE("Completed MENU9 restores both statistics cursors without row loss",
          "[gazette-statistics-integration][gazette-statistics-reload]"
          "[F02815][F02825]") {
  const auto page = GENERATE(Page::kTitles, Page::kStatistics);
  Fixture fixture;
  const auto sequence = fixture.PrimeCursor(page);
  const ResidentState before(fixture);
  {
    menu_overlay_tests::Load other(3, {VS_MENU2_PRG_LBA, VS_MENU2_PRG_SIZE});
    REQUIRE(other.Complete() == 1);
    CHECK(fixture.DrawCursor(page) == sequence[2]);
  }
  {
    menu_overlay_tests::Load loaded(10, {VS_MENU9_PRG_LBA, VS_MENU9_PRG_SIZE});
    REQUIRE(loaded.Poll() == 0);
    CHECK(fixture.DrawCursor(page) == sequence[3]);
    REQUIRE(loaded.Complete() == 1);
  }
  CHECK(fixture.DrawCursor(page) == sequence[0]);
  before.Check();
  CHECK(Fixture::InformationText() == before.rows.information);
  CHECK(fixture.GuardsIntact());
}

TEST_CASE("Cold session restores both statistics cursors with foreign owners",
          "[gazette-statistics-integration][gazette-statistics-reload]"
          "[F02815][F02825]") {
  const auto page = GENERATE(Page::kTitles, Page::kStatistics);
  const game_clear_live_tests::SavedModuleData saved_module;
  const game_clear_live_tests::SavedGlobals<
      Menu_textBoxSelector, Battle_stringContext, Event_isTextBoxAwaitingClose,
      Menu_sceneNotificationBuffer, Menu_sceneNotificationTimer,
      Menu_sceneNotificationTimed>
      saved_ui;
  Fixture fixture;
  const auto sequence = fixture.PrimeCursor(page);
  const ResidentState before(fixture);
  void* const foreign = memory::AllocateLiveFromLowEnd(64);
  REQUIRE(foreign != nullptr);
  const auto entry = +[] { return 0; };
  REQUIRE(VS::Boot::RunLiveSessionLoop(entry, entry) == 0);
  CHECK(fixture.DrawCursor(page) == sequence[0]);
  before.Check();
  CHECK(Menu_textBoxes[shared::kInformationBox].string == nullptr);
  CHECK(memory::LivePayloadCapacity(foreign).has_value());
  memory::FreeLiveFromLowEnd(foreign);
  CHECK(fixture.GuardsIntact());
}
