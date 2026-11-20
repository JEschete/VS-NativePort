#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <string>

#include <catch2/catch_test_macros.hpp>

#include "Boot/SessionLive.hpp"
#include "Content/DiscLayout.h"
#include "Event/ScriptVmLiveState.hpp"
#include "Menu/GameClearLifecycleLiveFixture.hpp"
#include "Menu/GazetteTimesLiveFixture.hpp"
#include "Menu/OverlayLoadFixture.hpp"
#include "Menu/Score/GazetteStatisticsLive.hpp"
#include "Menu/Score/ScoreScreen.hpp"
#include "State/Progress.hpp"

namespace {
namespace live = gazette_times_live_tests;
namespace score = VS::Menu::Score;
namespace fw = VS::Menu::Framework;
namespace memory = VS::Core::Memory;

template <typename Entry>
Entry ModuleEntry(unsigned address, Entry expected) {
  auto* resolved = VS::Content::ResolveModuleAddress(
      VS::Content::ModuleSymbols(), "MENU/MENU9.PRG", address);
  CHECK(resolved == reinterpret_cast<void*>(expected));
  if (resolved != reinterpret_cast<void*>(expected)) {
    throw std::runtime_error("incorrect Gazette time module entry");
  }
  return reinterpret_cast<Entry>(resolved);
}

void CheckModuleFormatter() {
  std::array<char, 34> storage{};
  storage.fill('Q');
  score::clearCount = 1;
  const auto format =
      ModuleEntry(0x80106DE0, &score::FormatLiveFastestClearTime);
  const auto output = std::span(storage).subspan(1, 30);
  const live::EncodedText expected{0x1B, 1, 2,    8, 0xFA, 12,
                                   9,    9, 0x9E, 5, 9,    0xE7};
  REQUIRE(format(output, 127, 6000) == expected.size());
  CHECK(std::ranges::equal(
      output.first(expected.size()), expected, {},
      [](char value) { return static_cast<std::uint8_t>(value); }));
  CHECK(storage.front() == 'Q');
  CHECK(storage[31] == 'Q');
  CHECK(storage.back() == 'Q');
}

struct Fixture : live::Fixture {
  live::Sprite DrawCursor() {
    ResetFrame();
    score::RenderLiveTimeTrialRecords(8);
    return Cursor();
  }

  std::array<live::Sprite, 4> PrimeCursor() {
    SetScore(0, 0, 0x010203, 7);
    score::CreateLiveTimeTrialRows();
    SettleEntrance();
    std::array<live::Sprite, 4> sequence{};
    for (auto& cursor : sequence) cursor = DrawCursor();
    score::ResetLiveTimeTrialStatics();
    CHECK(DrawCursor() == sequence[0]);
    CHECK(DrawCursor() == sequence[1]);
    REQUIRE(sequence[0] != sequence[1]);
    return sequence;
  }
};

void LoadStatisticsLabels() {
  const auto image = live::ReadImage(R"(Native\data\disc\MENU\MENU9.PRG)");
  const auto entries = VS::Content::ModuleDataEntries();
  const std::array targets{static_cast<void*>(score::statHeaders),
                           static_cast<void*>(score::statDescriptions)};
  for (const auto target : targets) {
    const auto found = std::ranges::find(entries, target,
                                         &VS::Content::ModuleDataEntry::target);
    REQUIRE(found != entries.end());
    if (found == entries.end()) {
      throw std::runtime_error("missing statistics label owner");
    }
    REQUIRE(VS::Content::CopyModuleDataEntry(*found, image));
  }
}

void CheckResidentState(const live::ObservableState& before) {
  CHECK(Battle_unusedMenu9CursorMemory.gazette_page == 0);
  CHECK(Battle_unusedMenu9CursorMemory.record_time_page == 5);
  CHECK(Battle_unusedMenu9CursorMemory.record_time_row == 2);
  CHECK(VS::State::scoreData.boss_time_trial_scores[0][0].time == 0x010203);
  CHECK(score::clearCount == 3);
  for (std::size_t i = 0; i < score::kTimeTrialCount; ++i) {
    CHECK(Fixture::Row(i).text == before.rows[i].text);
    CHECK(Fixture::Row(i).x == before.rows[i].x);
    CHECK(Fixture::Row(i).state == before.rows[i].state);
  }
}
}  // namespace

TEST_CASE("Gazette time module entries use bounded text and real row services",
          "[gazette-times-integration][gazette-times-module]"
          "[F02828][F02829][F02830][F02831]") {
  Fixture fixture;
  const auto refusals = score::LiveTimeTrialRefusals();
  CheckModuleFormatter();
  Fixture::SetScore(0, 0, 0x010203);
  ModuleEntry(0x80106F9C, &score::CreateLiveTimeTrialRows)();
  CHECK(score::gazetteRows[0].disabled == 0);
  CHECK(score::gazetteRows[1].disabled == 1);
  ModuleEntry(0x80107120, &score::RenderLiveTimeTrialRecords)(1);
  fw::UpdateAndDrawLiveRows();
  CHECK(Fixture::Row(0).x < 320);
  CHECK(Fixture::Row(0).target0 == 194);
  ModuleEntry(0x80107090, &score::LeaveLiveTimeTrialMenu)();
  CHECK(Fixture::Row(0).target0 == 320);
  CHECK(static_cast<std::uint8_t>(score::gazetteRows[1].animation_state) ==
        255);
  CHECK(fixture.GuardsIntact());
  CHECK(score::LiveTimeTrialRefusals() == refusals);
}

TEST_CASE("Production statistics caller supplies the remaining time text span",
          "[gazette-times-integration][F02828]") {
  const game_clear_live_tests::SavedModuleData saved_module;
  Fixture fixture;
  LoadStatisticsLabels();
  score::clearCount = 1;
  VS::State::scoreData.clear_count = 127;
  VS::State::scoreData.completion_time_minutes = 6000;
  Battle_unusedMenu9CursorMemory.record_time_page = 0;
  Battle_unusedMenu9CursorMemory.record_time_row = 0;
  score::CreateLiveStatisticRows();
  const auto refusals = score::LiveTimeTrialRefusals();
  score::RenderLiveStatistics(7);
  const auto first = fixture.Packets();
  REQUIRE_FALSE(first.empty());
  fixture.ResetFrame();
  score::RenderLiveStatistics(7);
  CHECK(fixture.Packets() == first);
  VS::State::scoreData.completion_time_minutes = 60;
  fixture.ResetFrame();
  score::RenderLiveStatistics(7);
  CHECK(fixture.Packets() != first);
  CHECK(fixture.GuardsIntact());
  CHECK(score::LiveTimeTrialRefusals() == refusals);
}

TEST_CASE("Completed MENU9 resets only the time cursor before fresh rendering",
          "[gazette-times-integration][gazette-times-reload][F02831]") {
  Fixture fixture;
  const auto sequence = fixture.PrimeCursor();
  Battle_unusedMenu9CursorMemory.record_time_page = 5;
  Battle_unusedMenu9CursorMemory.record_time_row = 2;
  score::clearCount = 3;
  const auto before = fixture.Observe();
  {
    menu_overlay_tests::Load other(3, {VS_MENU2_PRG_LBA, VS_MENU2_PRG_SIZE});
    REQUIRE(other.Complete() == 1);
    CHECK(fixture.DrawCursor() == sequence[2]);
  }
  {
    menu_overlay_tests::Load loaded(10, {VS_MENU9_PRG_LBA, VS_MENU9_PRG_SIZE});
    REQUIRE(loaded.Poll() == 0);
    CHECK(fixture.DrawCursor() == sequence[3]);
    REQUIRE(loaded.Complete() == 1);
  }
  CHECK(fixture.DrawCursor() == sequence[0]);
  CheckResidentState(before);
  CHECK(Fixture::InformationText() == before.information);
  CHECK(fixture.GuardsIntact());
}

TEST_CASE("Actual cold session resets the time cursor without replacing rows",
          "[gazette-times-integration][gazette-times-reload][F02831]") {
  const game_clear_live_tests::SavedModuleData saved_module;
  const game_clear_live_tests::SavedGlobals<
      VS::Menu::textBoxSelector, VS::Battle::stringContext,
      VS::Event::isTextBoxAwaitingClose, VS::Menu::sceneNotificationBuffer,
      VS::Menu::sceneNotificationTimer, VS::Menu::sceneNotificationTimed>
      saved_ui;
  Fixture fixture;
  const auto sequence = fixture.PrimeCursor();
  Battle_unusedMenu9CursorMemory.record_time_page = 5;
  Battle_unusedMenu9CursorMemory.record_time_row = 2;
  score::clearCount = 3;
  const auto before = fixture.Observe();
  void* const foreign = memory::AllocateLiveFromLowEnd(64);
  REQUIRE(foreign != nullptr);
  const auto entry = +[] { return 0; };
  REQUIRE(VS::Boot::RunLiveSessionLoop(entry, entry) == 0);
  CHECK(fixture.DrawCursor() == sequence[0]);
  CheckResidentState(before);
  CHECK(VS::Menu::textBoxes[live::kInformationBox].text == nullptr);
  CHECK(memory::LivePayloadCapacity(foreign).has_value());
  memory::FreeLiveFromLowEnd(foreign);
  CHECK(fixture.GuardsIntact());
}
