#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Content/StringTables.hpp"
#include "Menu/GazetteTimesLiveFixture.hpp"

namespace menu9_misc_info = VS::Content::text::menu9_prg::misc_info;

namespace {
namespace live = gazette_times_live_tests;
namespace score = VS::Menu::Score;
namespace fw = VS::Menu::Framework;

void CheckFastestClearFormat(std::span<char> output,
                             const live::EncodedText& expected) {
  CHECK(score::FormatLiveFastestClearTime(output, 127, 6000) ==
        expected.size());
  CHECK(std::ranges::equal(
      output.first(expected.size()), expected, {},
      [](char value) { return static_cast<std::uint8_t>(value); }));
}

void CheckCreatedRows(const live::EncodedText& minotaur) {
  for (std::size_t i = 0; i < score::kTimeTrialCount; ++i) {
    const auto& row = score::gazetteRows[i];
    CHECK(row.x == 194);
    CHECK(row.selected == 0);
    CHECK(row.unlocked == 1);
    CHECK(static_cast<std::uint8_t>(row.animation_state) == i + 1);
    CHECK(row.disabled == (i == 0 ? 0 : 1));
    CHECK(row.row_index == static_cast<std::int16_t>(20 + i));
    CHECK(live::BorrowedText(row.title) ==
          (i == 0 ? minotaur
                  : live::Label(score::miscInfo,
                                menu9_misc_info::index::kNoneBossRush)));
    CHECK(live::BorrowedText(row.description) ==
          (i == 0 ? live::Label(score::timeAttackDescriptions, 0)
                  : live::Label(score::miscInfo,
                                menu9_misc_info::index::kNoneBossRushDesc)));
  }
}

void CheckLeavingRows(
    const std::array<std::uint8_t, fw::kRowTextBytes>& previous_text) {
  CHECK(live::Fixture::Row(0).state == fw::RowState::kSlideX);
  CHECK(live::Fixture::Row(0).target0 == 320);
  CHECK(live::Fixture::Row(0).text == previous_text);
  for (std::size_t i = 1; i < score::kTimeTrialCount; ++i) {
    CHECK(static_cast<std::uint8_t>(score::gazetteRows[i].animation_state) ==
          255);
    CHECK(live::Fixture::Row(i).state == fw::RowState::kInactive);
  }
}

void CheckRenderedRows(const live::ObservableState& state) {
  for (std::size_t i = 0; i < state.rows.size(); ++i) {
    const auto& row = state.rows[i];
    CHECK(row.state == fw::RowState::kIdle);
    CHECK(row.x == 194);
    CHECK(row.y == 50 + static_cast<int>(i) * 16);
    CHECK(row.delay == 0);
    CHECK(row.selected == (i == 2 ? 1 : 0));
    CHECK(row.disabled == (i == 2 ? 0 : 1));
    auto expected = live::BorrowedText(score::gazetteRows[i].title);
    expected.back() = 0xFF;
    CHECK(std::ranges::equal(std::span(row.text).first(expected.size()),
                             expected));
  }
}

void CheckRecordGlyphs(const live::Fixture& fixture) {
  CHECK(fixture.Text("255:255:255", 50).X() == 44);
  CHECK(fixture.Text("255:255:255", 50).Right() == 133);
  CHECK(fixture.Text("00:01:02", 80).X() == 57);
  CHECK(fixture.Text("00:01:02", 80).Right() == 119);
  CHECK(fixture.Text("--:--:--", 110).X() == 63);
  CHECK(fixture.Text("--:--:--", 110).Right() == 113);
  CHECK((fixture.Text("--:--:--", 110).glyphs.front().color &
         live::kLinkMask) == 0x505050);
  CHECK((fixture.Text("255:255:255", 50).glyphs.front().color &
         live::kLinkMask) == 0x808080);
}

void CheckHeadingGlyphs(const live::Fixture& fixture) {
  CHECK(fixture.Text("PLAYER TIME", 32).X() == 8);
  CHECK(fixture.Text("1ST", 50).X() == 24);
  CHECK(fixture.Text("ROUND    128", 62).X() == 68);
  CHECK(fixture.Text("REFERENCE TIME", 140).X() == 8);
  CHECK(fixture.Text("00:40:00", 158).X() == 57);
  CHECK(fixture.Text("00:40:00", 158).Right() == 119);
}

void CheckPanels(const live::ObservableState& state) {
  std::vector<live::Packet> shaded;
  for (const auto& packet : state.packets) {
    if (packet.size() == 9 && (packet[1] >> 24) == 0x38) {
      shaded.push_back(packet);
    }
  }
  const std::vector<live::Packet> expected_panels{
      {0x08000000, 0x38663040, fw::PackXy(16, 158), 0x200808,
       fw::PackXy(143, 158), 0x663040, fw::PackXy(16, 169), 0x200808,
       fw::PackXy(143, 169)},
      {0x08000000, 0x386C8219, fw::PackXy(0, 140), 0x262801,
       fw::PackXy(111, 140), 0x6C8219, fw::PackXy(0, 151), 0x262801,
       fw::PackXy(111, 151)},
      {0x08000000, 0x38663040, fw::PackXy(16, 50), 0x200808,
       fw::PackXy(143, 50), 0x663040, fw::PackXy(16, 133), 0x200808,
       fw::PackXy(143, 133)},
      {0x08000000, 0x386C8219, fw::PackXy(0, 32), 0x262801, fw::PackXy(95, 32),
       0x6C8219, fw::PackXy(0, 43), 0x262801, fw::PackXy(95, 43)}};
  CHECK(shaded == expected_panels);
  CHECK(std::ranges::count_if(state.packets, [](const auto& packet) {
          return packet.size() == 6 && packet[1] == 0x28000000;
        }) == 4);
}

TEST_CASE(
    "F02828 live formatter copies original no-clear text and bounded time",
    "[gazette-times][gazette-times-live]") {
  live::Fixture fixture;
  const auto refusals = score::LiveTimeTrialRefusals();
  std::array<char, score::kFastestClearTimeBytes + 2> storage{};
  storage.fill('Q');
  const auto output = std::span(storage).subspan(1, storage.size() - 2);
  score::clearCount = 0;
  const auto missing =
      live::Label(score::miscInfo, menu9_misc_info::index::kNoneClear);
  REQUIRE(missing ==
          live::EncodedText{0x8F, 0x8F, 0x17, 0x18, 0x17, 0x0E, 0xE7});
  CheckFastestClearFormat(output, missing);
  score::clearCount = 1;
  const live::EncodedText completed{0x1B, 1, 2,    8, 0xFA, 12,
                                    9,    9, 0x9E, 5, 9,    0xE7};
  CheckFastestClearFormat(output, completed);
  CHECK(storage.front() == 'Q');
  CHECK(storage.back() == 'Q');
  CHECK(output[completed.size()] == 'Q');
  CHECK(score::LiveTimeTrialRefusals() == refusals);
}

TEST_CASE("F02829 live create selects genuine MENU9 labels from first place",
          "[gazette-times][gazette-times-live]") {
  live::Fixture fixture;
  const auto refusals = score::LiveTimeTrialRefusals();
  live::Fixture::SetScore(0, 0, 0x010203, 127, true);
  live::Fixture::SetScore(1, 1, 0x000102);
  score::gazetteRows[8].x = -123;
  for (std::size_t i = 0; i < score::kTimeTrialCount; ++i) {
    score::gazetteRows[i].row_index = static_cast<std::int16_t>(20 + i);
  }
  score::CreateLiveTimeTrialRows();
  const auto minotaur = live::Label(score::timeAttacks, 0);
  REQUIRE(minotaur == live::EncodedText{0x16, 0x2C, 0x31, 0x32, 0x37, 0x24,
                                        0x38, 0x35, 0xE7});
  CheckCreatedRows(minotaur);
  CHECK(score::gazetteRows[8].x == -123);
  CHECK(score::LiveTimeTrialRefusals() == refusals);
}

TEST_CASE("F02830 live leave moves a real row and cancels pending entrances",
          "[gazette-times][gazette-times-live]") {
  live::Fixture fixture;
  const auto refusals = score::LiveTimeTrialRefusals();
  live::Fixture::SetScore(0, 0, 0x010203);
  score::CreateLiveTimeTrialRows();
  score::RenderLiveTimeTrialRecords(1);
  fw::UpdateAndDrawLiveRows();
  REQUIRE(live::Fixture::Row(0).x < 320);
  REQUIRE(live::Fixture::Row(0).x > 194);
  const auto text = live::Fixture::Row(0).text;
  score::LeaveLiveTimeTrialMenu();
  CheckLeavingRows(text);
  fixture.ResetFrame();
  score::RenderLiveTimeTrialRecords(0);
  CHECK(static_cast<std::uint8_t>(score::gazetteRows[1].animation_state) ==
        254);
  for (int tick = 0; tick < 16; ++tick) {
    fixture.ResetFrame();
    fw::UpdateAndDrawLiveRows();
  }
  CHECK(live::Fixture::Row(0).x == 320);
  CHECK(live::Fixture::Row(0).state == fw::RowState::kIdle);
  CHECK(fixture.GuardsIntact());
  CHECK(score::LiveTimeTrialRefusals() == refusals);
}

TEST_CASE(
    "F02831 live render produces resource-backed rows text panels and help",
    "[gazette-times][gazette-times-live]") {
  live::Fixture fixture;
  const auto refusals = score::LiveTimeTrialRefusals();
  live::Fixture::SetScore(2, 0, 0xFFFFFF, 127, true);
  live::Fixture::SetScore(2, 1, 0x000102, 0);
  VS::Battle::menu9CursorMemory.gazette_page = 2;
  score::CreateLiveTimeTrialRows();
  fixture.SettleEntrance();
  fixture.ResetFrame();
  score::RenderLiveTimeTrialRecords(8);
  const auto state = fixture.Observe();
  CheckRenderedRows(state);
  CHECK(state.information == live::Label(score::timeAttackDescriptions, 2));
  CHECK(VS::Menu::textBoxes[live::kInformationBox].flags.speaker_texture == 32);
  CHECK(VS::Menu::textBoxes[live::kInformationBox].speed != 0);
  CheckRecordGlyphs(fixture);
  CheckHeadingGlyphs(fixture);
  CHECK(fixture.Cursor().position == fw::PackXy(182, 74));
  CHECK(fixture.Cursor().size == fw::PackXy(16, 16));
  CheckPanels(state);
  CHECK(fixture.UsedWords() > 100);
  CHECK(fixture.GuardsIntact());
  CHECK(score::LiveTimeTrialRefusals() == refusals);
}

TEST_CASE("F02831 live cursor phase resets through genuine rendered output",
          "[gazette-times][gazette-times-live]") {
  live::Fixture fixture;
  live::Fixture::SetScore(0, 0, 0x010203);
  score::CreateLiveTimeTrialRows();
  fixture.SettleEntrance();
  fixture.ResetFrame();
  score::RenderLiveTimeTrialRecords(8);
  const auto first = fixture.Cursor();
  CHECK((first.color & live::kLinkMask) == 0x808080);
  fixture.ResetFrame();
  score::RenderLiveTimeTrialRecords(8);
  const auto second = fixture.Cursor();
  CHECK((second.color & live::kLinkMask) == 0xB8B8B8);
  CHECK(second != first);
  const auto before_reset = fixture.Observe();
  score::ResetLiveTimeTrialStatics();
  fixture.ResetFrame();
  score::RenderLiveTimeTrialRecords(8);
  CHECK(fixture.Cursor() == first);
  CHECK(fixture.Observe().rows == before_reset.rows);
  CHECK(fixture.InformationText() == before_reset.information);
  CHECK(fixture.GuardsIntact());
}

TEST_CASE(
    "F02831 live fixture restores all borrowed data font and frame inputs",
    "[gazette-times][gazette-times-live]") {
  const auto before = live::SnapshotBorrowedInputs();
  {
    live::Fixture fixture;
    live::Fixture::SetScore(0, 0, 0x010203);
    score::CreateLiveTimeTrialRows();
    fixture.SettleEntrance();
    fixture.ResetFrame();
    score::RenderLiveTimeTrialRecords(8);
    REQUIRE_FALSE(fixture.Observe().packets.empty());
    REQUIRE(live::SnapshotBorrowedInputs() != before);
  }
  CHECK(live::SnapshotBorrowedInputs() == before);
}
}  // namespace
