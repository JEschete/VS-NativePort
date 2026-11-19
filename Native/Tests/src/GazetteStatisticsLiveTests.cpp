#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <span>
#include <stdexcept>
#include <tuple>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Menu/GazetteStatisticsLiveFixture.hpp"
#include "Menu/Score/GazetteStatistics.hpp"
#include "Render/UiPackets.hpp"
#include "Render/UiPacketsLive.hpp"

namespace {
namespace live = gazette_statistics_live_tests;
namespace shared = gazette_times_live_tests;
namespace score = VS::Menu::Score;
namespace fw = VS::Menu::Framework;
namespace render = VS::Render;
using Text = shared::EncodedText;

template <std::size_t Count>
const char* LabelPointer(const u_short (&table)[Count], std::size_t index) {
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
  return reinterpret_cast<const char*>(table + table[index]);
}

auto RowFields(const Menu_Score_Row& row) {
  return std::tuple(row.selected, row.opaque1, row.animationState, row.disabled,
                    row.x, row.rowIndex, row.title, row.description);
}

bool SameRows(std::span<const Menu_Score_Row> left,
              std::span<const Menu_Score_Row> right) {
  return std::ranges::equal(left, right, [](const auto& a, const auto& b) {
    return RowFields(a) == RowFields(b);
  });
}

struct Glyph {
  std::uint8_t code;
  int x;
  friend bool operator==(const Glyph&, const Glyph&) = default;
};

using GlyphRun = std::vector<Glyph>;

std::vector<GlyphRun> GlyphRuns(const live::StatisticsFixture& fixture, int y) {
  constexpr std::uint32_t kVariableDrawMode = 0xE100002D;
  constexpr std::uint32_t kVariableSize = 0x000C000C;
  constexpr unsigned kCell = 12;
  constexpr unsigned kColumns = 21;
  std::vector<GlyphRun> result;
  bool continued = false;
  for (const auto& packet : fixture.Packets()) {
    if (packet.size() != render::kSpritePacketWords ||
        packet[render::kSpriteDrawModeWord] != kVariableDrawMode ||
        packet[render::kSpriteSizeWord] != kVariableSize) {
      continued = false;
      continue;
    }
    const auto position = packet[render::kSpritePositionWord];
    const int x = static_cast<std::int16_t>(position);
    if (static_cast<std::int16_t>(position >> 16) != y) {
      continued = false;
      continue;
    }
    const auto texture = packet[render::kSpriteTextureWord];
    const auto glyph =
        (texture & 255U) / kCell + ((texture >> 8) & 255U) / kCell * kColumns;
    if (!continued) result.emplace_back();
    result.back().push_back({static_cast<std::uint8_t>(glyph), x});
    continued = true;
  }
  for (auto& run : result) std::ranges::sort(run, {}, &Glyph::x);
  return result;
}

GlyphRun PrintedGlyphs(std::span<const std::uint8_t> text, int x) {
  constexpr std::uint8_t kSpacing = 0xFA;
  constexpr std::uint8_t kSpace = 0x8F;
  constexpr std::uint8_t kNonprinting = 0xE5;
  constexpr std::uint8_t kEnd = 0xE7;
  constexpr int kAdvance = 6;
  GlyphRun result;
  for (std::size_t index = 0; index < text.size(); ++index) {
    const auto code = text[index];
    if (code == kEnd) break;
    if (code == kSpacing && index + 1 < text.size()) {
      x += text[++index];
    } else if (code < kNonprinting) {
      if (code != kSpace) result.push_back({code, x});
      x += kAdvance;
    }
  }
  return result;
}

GlyphRun ValueRun(const live::StatisticsFixture& fixture, int y) {
  constexpr int kValueColumn = 158;
  auto runs = GlyphRuns(fixture, y);
  if (runs.size() != 2) {
    throw std::runtime_error(
        "Expected both actual statistic header and value glyph runs");
  }
  for (auto& run : runs) {
    if (run.front().x >= kValueColumn) return run;
  }
  throw std::runtime_error("Missing actual statistic value-column glyph run");
}

Text ValueGlyphs(const live::StatisticsFixture& fixture, int y) {
  Text result;
  for (const auto& glyph : ValueRun(fixture, y)) {
    result.push_back(glyph.code);
  }
  return result;
}

std::vector<shared::Packet> TitleDigits(
    const live::StatisticsFixture& fixture) {
  constexpr std::uint32_t kTitleCommand = 0x2E;
  auto packets = fixture.Packets();
  std::erase_if(packets, [](const auto& packet) {
    return packet.size() != render::kUiTexturedQuadWords ||
           (packet[1] >> 24) != kTitleCommand;
  });
  return packets;
}

void Progress() {
  Menu_Score_clearCount = 3;
  Menu_Score_mapCompletion = 180;
  Menu_Score_openedChestCount = 26;
  Audio_Seq_unusedScoredata.clearCount = 2;
  Audio_Seq_unusedScoredata.completionTimeMinutes = 60;
  Audio_Seq_unusedScoredata.mapCompletion = 361;
  Audio_Seq_unusedScoredata.openedChestCount = 52;
  Audio_Seq_unusedScoredata.maxChain = 65535;
  for (std::size_t index = 0;
       index < std::size(Audio_Seq_unusedScoredata.enemyKills); ++index) {
    Audio_Seq_unusedScoredata.enemyKills[index] =
        static_cast<u_short>(100 + index);
  }
  for (std::size_t index = 0;
       index < std::size(Audio_Seq_unusedScoredata.weaponAttacks); ++index) {
    Audio_Seq_unusedScoredata.weaponAttacks[index] =
        static_cast<u_short>(1000 + index);
  }
}

void CheckTitleGlyphs(const live::StatisticsFixture& fixture) {
  const auto& row = shared::Fixture::Row(0);
  CHECK(row.state == fw::RowState::kInactive);
  CHECK(std::array<int, 4>{row.x, row.y, row.w, row.is_scrollable} ==
        std::array<int, 4>{60, 168, 200, 0});
  const auto title = shared::Label(Menu_Score_titleText, 31);
  REQUIRE(title.size() > 1);
  CHECK(row.text[0] == title[0]);
  const auto glyphs = GlyphRuns(fixture, 168);
  REQUIRE(glyphs.size() == 1);
  CHECK(glyphs.front() == PrintedGlyphs(title, 66));
}

void CheckTitleInformationOwnership(live::StatisticsFixture& fixture) {
  const auto information = shared::Fixture::InformationText();
  CHECK(information == shared::Label(Menu_Score_titleDescriptions, 31));
  Menu_Score_titleDescriptions[Menu_Score_titleDescriptions[31]] = 0xE701;
  fixture.ResetFrame();
  score::RenderLiveTitles(8);
  CHECK(shared::Fixture::InformationText() == information);
}

void CheckDigitPayload(std::span<const u_long> units,
                       std::span<const u_long> tens, bool dimmed) {
  CHECK(units[1] == (dimmed ? 0x2E202020U : 0x2E808080U));
  CHECK(units[2] == 0x00440041U);
  CHECK(units[3] == (dimmed ? 0x37F7000CU : 0x37F4000CU));
  CHECK(units[4] == 0x00440047U);
  CHECK(units[5] == (dimmed ? 0x002C0012U : 0x000C0012U));
  CHECK(units[7] == 0xDEAD0A0CU);
  CHECK(units[9] == 0xDEAD0A12U);
  CHECK(tens[2] == 0x0044003CU);
  CHECK(tens[3] == (dimmed ? 0x37F70012U : 0x37F40012U));
}

void CheckSummaryGlyphs(const live::StatisticsFixture& fixture) {
  CHECK(ValueGlyphs(fixture, 56) == Text{3});
  CHECK(ValueRun(fixture, 56) == GlyphRun{{3, 282}});
  CHECK(ValueGlyphs(fixture, 72) == Text{0x1B, 3, 0, 1, 0x9E, 0, 0});
  CHECK(ValueGlyphs(fixture, 88) == Text{5, 0, 0x94});
  CHECK(ValueGlyphs(fixture, 104) == Text{1, 0, 0, 0x94});
  CHECK(ValueGlyphs(fixture, 120) == Text{4, 9, 0x94});
  CHECK(ValueGlyphs(fixture, 136) == Text{1, 0, 0, 0x94});
  CHECK(ValueGlyphs(fixture, 152) == Text{6, 5, 5, 3, 5});
}

void CheckWeaponPage(live::StatisticsFixture& fixture, std::uint8_t page,
                     std::size_t first_weapon) {
  fixture.ResetFrame();
  Battle_unusedMenu9CursorMemory.recordTimePage = page;
  score::RenderLiveStatistics(8);
  for (std::size_t index = 0; index < 6; ++index) {
    CHECK(ValueGlyphs(fixture, 72 + static_cast<int>(index) * 16) ==
          Text{1, 0, 0, static_cast<std::uint8_t>(index + first_weapon)});
  }
}
}  // namespace

TEST_CASE("F02814 Live title creation borrows original tables and bit 31",
          "[gazette-statistics][gazette-statistics-live][F02814]") {
  live::StatisticsFixture fixture;
  Audio_Seq_unusedScoredata.titles = -2147483643;
  score::CreateLiveTitleRows();
  for (std::size_t index = 0; index < 32; ++index) {
    const auto& row = Menu_Score_gazetteRows[index];
    const bool unlocked = index == 0 || index == 2 || index == 31;
    CHECK(std::array<int, 6>{row.selected, row.opaque1, row.animationState,
                             row.disabled, row.x, row.rowIndex} ==
          std::array<int, 6>{0, unlocked ? 1 : 0, 0, 0, 60,
                             static_cast<int>(index + 1)});
    CHECK(row.title == (unlocked ? LabelPointer(Menu_Score_titleText, index)
                                 : LabelPointer(Menu_Score_miscInfo, 0)));
    CHECK(row.description ==
          (unlocked ? LabelPointer(Menu_Score_titleDescriptions, index)
                    : LabelPointer(Menu_Score_miscInfo, 1)));
  }
  Battle_unusedMenu9CursorMemory.titlePage = 24;
  Battle_unusedMenu9CursorMemory.titleRow = 7;
  score::RenderLiveTitles(8);
  CHECK(shared::Fixture::InformationText() ==
        shared::Label(Menu_Score_titleDescriptions, 31));
  CHECK(TitleDigits(fixture).size() == 16);
  CHECK_FALSE(GlyphRuns(fixture, 168).empty());
  CHECK(fixture.GuardsIntact());
}

TEST_CASE("F02814 Live malformed chosen label leaves the actual rows intact",
          "[gazette-statistics][gazette-statistics-live][F02814]") {
  live::StatisticsFixture fixture;
  Audio_Seq_unusedScoredata.titles = -1;
  score::CreateLiveTitleRows();
  const auto before = std::to_array(Menu_Score_gazetteRows);
  const auto refused = score::LiveGazetteStatisticsRefusals();
  Menu_Score_titleText[31] =
      static_cast<u_short>(std::size(Menu_Score_titleText));
  score::CreateLiveTitleRows();
  CHECK(score::LiveGazetteStatisticsRefusals() == refused + 1);
  CHECK(SameRows(Menu_Score_gazetteRows, before));
  CHECK(fixture.UsedWords() == 0);
}

TEST_CASE(
    "F02815 Live renders selected title text cursor and dimmed edge digits",
    "[gazette-statistics][gazette-statistics-live][F02815]") {
  live::StatisticsFixture fixture;
  Audio_Seq_unusedScoredata.titles = -1;
  score::CreateLiveTitleRows();
  Battle_unusedMenu9CursorMemory.titlePage = 24;
  Battle_unusedMenu9CursorMemory.titleRow = 7;
  const auto refused = score::LiveGazetteStatisticsRefusals();
  score::RenderLiveTitles(8);
  CHECK(score::LiveGazetteStatisticsRefusals() == refused);
  CHECK(Menu_Score_gazetteRows[31].selected == 1);
  CHECK(Menu_Score_gazetteRows[31].animationState == 8);
  CHECK(fixture.Cursor().position == 0x00A00030);
  CheckTitleGlyphs(fixture);
  const auto digits = TitleDigits(fixture);
  CHECK(digits.size() == 16);
  CHECK(std::ranges::count_if(digits, [](const auto& packet) {
          return packet[1] == 0x2E202020U && (packet[5] >> 16) == 0x2CU;
        }) == 2);
  CheckTitleInformationOwnership(fixture);
  CHECK(fixture.GuardsIntact());
}

TEST_CASE("F02815 Live refuses an out-of-owner visible title before drawing",
          "[gazette-statistics][gazette-statistics-live][F02815]") {
  live::StatisticsFixture fixture;
  Audio_Seq_unusedScoredata.titles = -1;
  score::CreateLiveTitleRows();
  std::array<char, 2> outside{1, static_cast<char>(0xE7)};
  Menu_Score_gazetteRows[0].title = outside.data();
  Battle_unusedMenu9CursorMemory.titlePage = 0;
  Battle_unusedMenu9CursorMemory.titleRow = 0;
  const auto before = std::to_array(Menu_Score_gazetteRows);
  const auto refused = score::LiveGazetteStatisticsRefusals();
  score::RenderLiveTitles(8);
  CHECK(score::LiveGazetteStatisticsRefusals() == refused + 1);
  CHECK(SameRows(Menu_Score_gazetteRows, before));
  CHECK(fixture.UsedWords() == 0);
  CHECK(shared::Fixture::InformationText().empty());
}

TEST_CASE("F02816 Live digits use the real primitive owner and ordering links",
          "[gazette-statistics][gazette-statistics-live][F02816]") {
  live::StatisticsFixture fixture;
  const bool dimmed = GENERATE(false, true);
  auto* head =
      render::LiveFrameOrderingEntry(render::LiveOrderingBase::kOverlay, 0);
  REQUIRE(head != nullptr);
  const auto prior = *head & shared::kLinkMask;
  const auto refused = score::LiveGazetteStatisticsRefusals();
  score::DrawLiveTitleNumber(46, 72, 32, dimmed ? 1 : 0);
  CHECK(score::LiveGazetteStatisticsRefusals() == refused);
  CHECK(fixture.UsedWords() == 20);
  const auto* units = fixture.primitives.data() + 1;
  const auto* tens = units + render::kUiTexturedQuadWords;
  CHECK(units[0] == (0x09000000U | prior));
  CHECK(tens[0] == (0x09000000U | LegacyGpu_Handle(units)));
  CHECK(*head == LegacyGpu_Handle(tens));
  CheckDigitPayload({units, render::kUiTexturedQuadWords},
                    {tens, render::kUiTexturedQuadWords}, dimmed);
  CHECK(fixture.Packets().size() == 2);
  CHECK(fixture.GuardsIntact());
}

TEST_CASE("F02816 Live refuses absent capacity without changing a real chain",
          "[gazette-statistics][gazette-statistics-live][F02816]") {
  live::StatisticsFixture fixture;
  auto* head =
      render::LiveFrameOrderingEntry(render::LiveOrderingBase::kOverlay, 0);
  REQUIRE(head != nullptr);
  const auto previous = *head;
  VS::Render::FrameScratchpad().primitive_cursor =
      fixture.primitives.data() + shared::kPrimitiveWords - 8;
  const auto words = fixture.UsedWords();
  const auto refused = score::LiveGazetteStatisticsRefusals();
  score::DrawLiveTitleNumber(0, 0, 1, 0);
  CHECK(score::LiveGazetteStatisticsRefusals() == refused + 1);
  CHECK(fixture.UsedWords() == words);
  CHECK(*head == previous);
  CHECK(fixture.GuardsIntact());
}

TEST_CASE(
    "F02824 Live statistics initialize their own rows and preserve titles",
    "[gazette-statistics][gazette-statistics-live][F02824]") {
  live::StatisticsFixture fixture;
  Audio_Seq_unusedScoredata.titles = -1;
  score::CreateLiveTitleRows();
  const auto before = std::to_array(Menu_Score_gazetteRows);
  score::CreateLiveStatisticRows();
  for (std::size_t index = 0; index < 23; ++index) {
    const auto& row = Menu_Score_gazetteRows[index];
    CHECK(std::array<int, 6>{row.selected, row.opaque1, row.animationState,
                             row.disabled, row.x, row.rowIndex} ==
          std::array<int, 6>{0, 1, 0, 0, 16, static_cast<int>(index + 1)});
    CHECK(row.title == LabelPointer(Menu_Score_statHeaders, index));
    CHECK(row.description == LabelPointer(Menu_Score_statDescriptions, index));
  }
  CHECK(SameRows(std::span(Menu_Score_gazetteRows).subspan(23),
                 std::span(before).subspan(23)));
  Progress();
  Battle_unusedMenu9CursorMemory.recordTimePage = 0;
  Battle_unusedMenu9CursorMemory.recordTimeRow = 0;
  score::RenderLiveStatistics(8);
  CHECK(shared::Fixture::InformationText() ==
        shared::Label(Menu_Score_statDescriptions, 0));
  CHECK(ValueGlyphs(fixture, 56) == Text{3});
  CHECK(fixture.GuardsIntact());
}

TEST_CASE("F02824 Live invalid final description is an atomic native refusal",
          "[gazette-statistics][gazette-statistics-live][F02824]") {
  live::StatisticsFixture fixture;
  score::CreateLiveStatisticRows();
  const auto before = std::to_array(Menu_Score_gazetteRows);
  const auto refused = score::LiveGazetteStatisticsRefusals();
  Menu_Score_statDescriptions[22] =
      static_cast<u_short>(std::size(Menu_Score_statDescriptions));
  score::CreateLiveStatisticRows();
  CHECK(score::LiveGazetteStatisticsRefusals() == refused + 1);
  CHECK(SameRows(Menu_Score_gazetteRows, before));
  CHECK(fixture.UsedWords() == 0);
}

TEST_CASE(
    "F02825 Live reads current progress saved time and actual font widths",
    "[gazette-statistics][gazette-statistics-live][F02825]") {
  live::StatisticsFixture fixture;
  Progress();
  score::CreateLiveStatisticRows();
  VS::Render::variableFontGlyphWidths[3] = 12;
  Battle_unusedMenu9CursorMemory.recordTimePage = 0;
  Battle_unusedMenu9CursorMemory.recordTimeRow = 0;
  const auto refused = score::LiveGazetteStatisticsRefusals();
  score::RenderLiveStatistics(8);
  CHECK(score::LiveGazetteStatisticsRefusals() == refused);
  CheckSummaryGlyphs(fixture);
  CHECK(fixture.Cursor().position == 0x00300004);
  CHECK(Menu_Score_gazetteRows[0].selected == 1);
  CHECK(Menu_Score_gazetteRows[0].animationState == 8);
  CHECK(shared::Fixture::Row(0).state == fw::RowState::kInactive);
  CHECK(fixture.GuardsIntact());
}

TEST_CASE("F02825 Live enemy and weapon fields reach genuine rendered glyphs",
          "[gazette-statistics][gazette-statistics-live][F02825]") {
  live::StatisticsFixture fixture;
  Progress();
  score::CreateLiveStatisticRows();
  Battle_unusedMenu9CursorMemory.recordTimePage = 6;
  Battle_unusedMenu9CursorMemory.recordTimeRow = 3;
  score::RenderLiveStatistics(8);
  for (std::size_t index = 0; index < 6; ++index) {
    CHECK(ValueGlyphs(fixture, 72 + static_cast<int>(index) * 16) ==
          Text{1, 0, static_cast<std::uint8_t>(index)});
  }
  CHECK(fixture.Cursor().position == 0x00600004);
  CHECK(shared::Fixture::InformationText() ==
        shared::Label(Menu_Score_statDescriptions, 9));
  CheckWeaponPage(fixture, 12, 1);
  Battle_unusedMenu9CursorMemory.recordTimeRow = 7;
  CheckWeaponPage(fixture, 15, 4);
  CHECK(ValueGlyphs(fixture, 168) == Text{1, 0, 0, 0});
  CHECK(shared::Fixture::Row(0).is_scrollable == 0);
  CHECK(fixture.GuardsIntact());
}

TEST_CASE("F02825 Live no-clear text and invalid page retain bounded owners",
          "[gazette-statistics][gazette-statistics-live][F02825]") {
  live::StatisticsFixture fixture;
  Progress();
  Menu_Score_clearCount = 0;
  score::CreateLiveStatisticRows();
  Battle_unusedMenu9CursorMemory.recordTimePage = 0;
  Battle_unusedMenu9CursorMemory.recordTimeRow = 1;
  score::RenderLiveStatistics(8);
  const auto no_clear = shared::Label(Menu_Score_miscInfo, 0);
  Text expected;
  for (const auto& glyph : PrintedGlyphs(no_clear, 0)) {
    expected.push_back(glyph.code);
  }
  CHECK(ValueGlyphs(fixture, 72) == expected);
  fixture.ResetFrame();
  Battle_unusedMenu9CursorMemory.recordTimePage = 16;
  const auto before = std::to_array(Menu_Score_gazetteRows);
  const auto refused = score::LiveGazetteStatisticsRefusals();
  const auto information = shared::Fixture::InformationText();
  score::RenderLiveStatistics(8);
  CHECK(score::LiveGazetteStatisticsRefusals() == refused + 1);
  CHECK(SameRows(Menu_Score_gazetteRows, before));
  CHECK(shared::Fixture::InformationText() == information);
  CHECK(fixture.UsedWords() == 0);
}

TEST_CASE("Gazette statistics Live fixture restores all borrowed owners",
          "[gazette-statistics][gazette-statistics-live][fixture]") {
  const auto before = live::SnapshotBorrowedInputs();
  {
    live::StatisticsFixture fixture;
    Progress();
    Audio_Seq_unusedScoredata.titles = -1;
    score::CreateLiveTitleRows();
    Battle_unusedMenu9CursorMemory.titlePage = 1;
    Battle_unusedMenu9CursorMemory.titleRow = 0;
    score::RenderLiveTitles(8);
    score::DrawLiveTitleNumber(0, 0, 99, 1);
    score::CreateLiveStatisticRows();
    Battle_unusedMenu9CursorMemory.recordTimePage = 1;
    Battle_unusedMenu9CursorMemory.recordTimeRow = 0;
    score::RenderLiveStatistics(8);
    CHECK(fixture.GuardsIntact());
    CHECK_FALSE(fixture.Packets().empty());
  }
  CHECK(live::SnapshotBorrowedInputs() == before);
}
