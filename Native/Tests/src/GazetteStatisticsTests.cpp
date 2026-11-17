#include <algorithm>
#include <array>
#include <bit>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <span>
#include <string_view>
#include <tuple>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Core/Math/ScalarMath.hpp"
#include "Core/String/GameText.hpp"
#include "Menu/Score/GazetteRows.hpp"
#include "Menu/Score/GazetteStatistics.hpp"
#include "Render/EncodedText.hpp"
#include "Render/GameFont.hpp"
#include "Render/UiPackets.hpp"

extern "C" {
#include "Render/Text/VariableFont.h"
}

namespace {
namespace score = VS::Menu::Score;
namespace fw = VS::Menu::Framework;
namespace render = VS::Render;
namespace math = VS::Core::Math;
using Bytes = std::vector<std::uint8_t>;
using Packet = std::vector<std::uint32_t>;
constexpr std::array<std::uint8_t, 3> kLockedTitle{0xA7, 0xA7, 0xE7};
constexpr std::array<std::uint8_t, 3> kLockedDescription{0x20, 0x21, 0xE7};

Bytes TextBytes(score::GazetteText text) {
  const auto end = std::ranges::find(text, std::uint8_t{0xE7});
  return {text.begin(), end == text.end() ? end : end + 1};
}

auto RowState(const score::GazetteRecordRow& row) {
  return std::tuple(row.selected, row.unlocked, row.highlight_step,
                    row.disabled, row.x, row.number, row.title.data(),
                    row.title.size(), row.description.data(),
                    row.description.size());
}

bool SameRows(std::span<const score::GazetteRecordRow> left,
              std::span<const score::GazetteRecordRow> right) {
  return std::ranges::equal(left, right, [](const auto& a, const auto& b) {
    return RowState(a) == RowState(b);
  });
}

class PacketQueue final : public render::PacketQueue {
 public:
  std::vector<Packet> packets;
  std::size_t capacity = 2048;
  std::uint32_t head = 0x123456;
  std::uint32_t ChainLink() const override { return head; }
  bool Emit(std::span<const std::uint32_t> packet) override {
    if (packets.size() >= capacity) return false;
    packets.emplace_back(packet.begin(), packet.end());
    head = 0x300000U + static_cast<std::uint32_t>(packets.size() - 1) * 40U;
    return true;
  }
};

class NumberTarget final : public score::TitleNumberTarget {
 public:
  PacketQueue queue;
  render::UiTexturedQuadPacket previous{};
  NumberTarget() { previous.fill(0xABCD9876); }
  bool DrawQuad(const render::UiTexturedQuad& quad) override {
    return queue.Emit(
        render::BuildUiTexturedQuadPacket(quad, previous, queue.ChainLink()));
  }
};

struct RowCall {
  fw::RowPlacement placement;
  Bytes text;
};

struct DrawCall {
  bool scrolling = false;
  fw::MenuRow row;
};

// Pin the only global font input consumed by these actual renderer checks.
struct FontGlobals {
  decltype(std::to_array(Render_variableFontGlyphWidths)) saved =
      std::to_array(Render_variableFontGlyphWidths);
  FontGlobals() { std::ranges::fill(Render_variableFontGlyphWidths, char{6}); }
  ~FontGlobals() { std::ranges::copy(saved, Render_variableFontGlyphWidths); }
  FontGlobals(const FontGlobals&) = delete;
  FontGlobals& operator=(const FontGlobals&) = delete;
};

score::GazetteText FontWidths() {
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
  return {reinterpret_cast<const std::uint8_t*>(Render_variableFontGlyphWidths),
          sizeof Render_variableFontGlyphWidths};
}

class ScrollRaster final : public score::ScrollableRowHost {
 public:
  PacketQueue queue;
  const fw::MenuRow* row = nullptr;
  std::size_t arrows = 0;
  std::size_t lines = 0;
  std::size_t shadows = 0;
  std::size_t reflections = 0;
  int refusals = 0;
  int FrameBufferIndex() override { return 1; }
  void DrawScrollArrow(const score::ScrollArrowSprite&) override { ++arrows; }
  int DrawGlyph(std::uint32_t character, int x) override {
    if (row == nullptr) return x;
    render::FontStyle style;
    render::SetMenuFontStyle(style, row->font_color);
    return render::DrawVariableWidthGlyph(queue, style, FontWidths(),
                                          character & 0x7FFFFFFFU, x, row->y);
  }
  std::uint32_t GradientStopColor(std::uint32_t, std::uint32_t,
                                  int brightness) override {
    return static_cast<std::uint32_t>(brightness) * 0x010101U;
  }
  void AddGradientLine(const score::GradientLine&) override { ++lines; }
  void AddShadowTile(std::uint32_t, std::uint32_t) override { ++shadows; }
  void AddReflection(const score::ReflectionSprite&) override { ++reflections; }
  void Refuse(const char*) override { ++refusals; }
};

class RecordHost final : public score::GazetteRecordHost {
 public:
  fw::MenuRow menu;
  std::vector<RowCall> creates;
  std::vector<DrawCall> draws;
  std::vector<Bytes> information;
  std::vector<std::array<int, 3>> cursors;
  std::vector<std::array<int, 4>> numbers;
  std::array<std::uint8_t, render::kVariableFontGlyphCount> widths{};
  std::int32_t cursor_reply = 257;
  int gets = 0;
  int refusals = 0;
  bool rasterize = false;
  ScrollRaster raster;
  score::ScrollableRowAnimation scroll_animation;
  NumberTarget digit_target;
  RecordHost() { widths.fill(6); }

  fw::MenuRow& CreateRow(const fw::RowPlacement& placement,
                         score::GazetteText text) override {
    creates.push_back({placement, TextBytes(text)});
    menu = {};
    menu.state = fw::RowState::kIdle;
    menu.x = math::WrapSigned16(static_cast<std::uint32_t>(placement.x));
    menu.y = math::WrapSigned16(static_cast<std::uint32_t>(placement.y));
    menu.w = static_cast<std::uint8_t>(placement.w);
    menu.text.fill(0xFF);
    const Bytes bytes = TextBytes(text);
    const auto length = std::min(bytes.size(), menu.text.size());
    for (std::size_t index = 0; index < length; ++index) {
      menu.text[index] = bytes[index] == 0xE7 ? 0xFF : bytes[index];
    }
    return menu;
  }
  fw::MenuRow& Row() override {
    ++gets;
    return menu;
  }
  void SetInformation(score::GazetteText text) override {
    information.push_back(TextBytes(text));
  }
  std::int32_t DrawCursor(std::uint8_t step, int x, int y) override {
    cursors.push_back({step, x, y});
    return cursor_reply;
  }
  void DrawNumber(const score::TitleNumber& number) override {
    numbers.push_back(
        {number.x, number.y, number.value, number.dimmed ? 1 : 0});
    if (rasterize && !score::DrawTitleNumber(number, digit_target)) {
      ++refusals;
    }
  }
  void DrawRow(bool scrolling) override {
    draws.push_back({scrolling, menu});
    if (rasterize && scrolling) {
      raster.row = &menu;
      score::RenderScrollableRow(raster, scroll_animation, menu);
    }
  }
  std::int32_t MeasureText(score::GazetteText text) override {
    return render::MeasureEncodedTextLine(
        text, rasterize ? FontWidths() : score::GazetteText(widths));
  }
  void Refuse(std::string_view) override { ++refusals; }
};

struct RecordFixture {
  std::array<std::array<std::uint8_t, 3>, 64> strings{};
  std::array<score::GazetteRecordRow, 33> rows{};
  score::GazetteTitleLabels titles;
  score::GazetteStatisticLabels statistics;
  score::GazetteStatisticsValues values;
  score::GazetteRecordAnimation animation;
  RecordHost host;

  RecordFixture() {
    for (std::size_t index = 0; index < strings.size(); ++index) {
      strings[index] = {static_cast<std::uint8_t>(index), 0x10, 0xE7};
    }
    for (std::size_t index = 0; index < 32; ++index) {
      titles.titles[index] = strings[index];
      titles.descriptions[index] = strings[index + 32];
      rows[index] = {.selected = 2,
                     .unlocked = 3,
                     .highlight_step = 4,
                     .disabled = 5,
                     .x = static_cast<std::int16_t>(100 + index),
                     .number = static_cast<std::int16_t>(index + 1),
                     .title = strings[index],
                     .description = strings[index + 32]};
      if (index < 23) {
        statistics.titles[index] = strings[index];
        statistics.descriptions[index] = strings[index + 32];
      }
    }
    titles.locked_title = kLockedTitle;
    titles.locked_description = kLockedDescription;
    rows.back().x = 1234;
    rows.back().highlight_step = 77;
    values.current = {.map_completion = 180,
                      .opened_chest_count = 26,
                      .clear_count = 3,
                      .rank = {}};
    values.best.map_completion = 361;
    values.best.opened_chest_count = 52;
    values.best.completion_time_minutes = 60;
    values.best.max_chain = 65535;
    values.best_clear_round = 2;
    values.no_clear = kLockedTitle;
    for (std::size_t index = 0; index < 6; ++index) {
      values.best.enemy_kills[index] = static_cast<std::uint16_t>(100 + index);
    }
    for (std::size_t index = 0; index < 10; ++index) {
      values.best.weapon_attacks[index] =
          static_cast<std::uint16_t>(1000 + index);
    }
  }
  RecordFixture(const RecordFixture&) = delete;
  RecordFixture& operator=(const RecordFixture&) = delete;

  bool Titles(score::GazetteRecordCursor cursor = {}, std::int32_t step = 8) {
    return score::RenderTitles(rows, cursor, step, animation, host);
  }
  bool Statistics(score::GazetteRecordCursor cursor = {},
                  std::int32_t step = 8) {
    return score::RenderStatistics(rows, values, cursor, step, animation, host);
  }
  Bytes Value(std::size_t visible) const {
    const Bytes& bytes = host.creates.at(visible * 2 + 1).text;
    return {bytes.begin() + 2, bytes.end()};
  }
};
}  // namespace

TEST_CASE("F02814 title masks include the high bit and choose exact labels",
          "[gazette-statistics][F02814]") {
  RecordFixture fixture;
  const std::uint32_t mask = GENERATE(0U, 1U, 0x80000000U, 0xFFFFFFFFU);
  REQUIRE(score::CreateTitleRows(fixture.rows, mask, fixture.titles));
  for (std::size_t index = 0; index < 32; ++index) {
    const bool unlocked = (mask & (1U << index)) != 0;
    CHECK(fixture.rows[index].unlocked == (unlocked ? 1 : 0));
    CHECK(
        fixture.rows[index].title.data() ==
        (unlocked ? fixture.titles.titles[index].data() : kLockedTitle.data()));
    CHECK(fixture.rows[index].description.data() ==
          (unlocked ? fixture.titles.descriptions[index].data()
                    : kLockedDescription.data()));
  }
}

TEST_CASE("F02814 title construction resets exactly 32 rows",
          "[gazette-statistics][F02814]") {
  RecordFixture fixture;
  const auto extra = RowState(fixture.rows.back());
  REQUIRE(score::CreateTitleRows(fixture.rows, 0, fixture.titles));
  for (std::size_t index = 0; index < 32; ++index) {
    const auto& row = fixture.rows[index];
    CHECK(std::array<int, 6>{row.selected, row.unlocked, row.highlight_step,
                             row.disabled, row.x, row.number} ==
          std::array<int, 6>{0, 0, 0, 0, 60, static_cast<int>(index + 1)});
  }
  CHECK(RowState(fixture.rows.back()) == extra);
}

TEST_CASE("F02814 invalid chosen labels and short rows are atomic refusals",
          "[gazette-statistics][F02814]") {
  RecordFixture fixture;
  const auto before = fixture.rows;
  fixture.titles.descriptions.back() = {};
  CHECK_FALSE(
      score::CreateTitleRows(fixture.rows, 0x80000000U, fixture.titles));
  CHECK(SameRows(fixture.rows, before));
  CHECK_FALSE(score::CreateTitleRows(std::span(fixture.rows).first(31), 0,
                                     fixture.titles));
  fixture.titles.locked_title = {};
  CHECK_FALSE(score::CreateTitleRows(fixture.rows, 0, fixture.titles));
  CHECK(SameRows(fixture.rows, before));
}

TEST_CASE("F02814 unchosen malformed labels are never required",
          "[gazette-statistics][F02814]") {
  RecordFixture fixture;
  fixture.titles.titles.fill({});
  fixture.titles.descriptions.fill({});
  CHECK(score::CreateTitleRows(fixture.rows, 0, fixture.titles));
}

TEST_CASE("F02815 title selection updates information only on entry",
          "[gazette-statistics][F02815]") {
  RecordFixture fixture;
  REQUIRE(score::CreateTitleRows(fixture.rows, 1, fixture.titles));
  fixture.rows[1].highlight_step = 255;
  REQUIRE(fixture.Titles());
  CHECK(fixture.rows[0].selected == 1);
  CHECK(fixture.rows[0].highlight_step == 8);
  CHECK(fixture.rows[1].highlight_step == 254);
  CHECK(fixture.host.information ==
        std::vector<Bytes>{TextBytes(fixture.titles.descriptions[0])});
  REQUIRE(fixture.Titles());
  CHECK(fixture.host.information.size() == 1);
}

TEST_CASE("F02815 locked titles retain their selected fade and use dim text",
          "[gazette-statistics][F02815]") {
  RecordFixture fixture;
  REQUIRE(score::CreateTitleRows(fixture.rows, 0, fixture.titles));
  fixture.rows[1].highlight_step = 5;
  REQUIRE(fixture.Titles({0, 1}));
  CHECK(fixture.rows[1].selected == 1);
  CHECK(fixture.rows[1].highlight_step == 5);
  CHECK(fixture.host.draws[1].row.font_color == 3);
  CHECK(fixture.host.draws[1].row.gradient_state == 5);
  CHECK(fixture.host.information ==
        std::vector<Bytes>{Bytes{0x20, 0x21, 0xE7}});
}

TEST_CASE("F02815 title entrance clips each row and suppresses early cursors",
          "[gazette-statistics][F02815]") {
  RecordFixture fixture;
  REQUIRE(fixture.Titles({}, 4));
  CHECK(fixture.host.creates.size() == 8);
  CHECK(fixture.host.creates[0].placement.y == 56);
  CHECK(fixture.host.creates[1].placement.y == 72);
  CHECK(fixture.host.creates[2].placement.y == 76);
  CHECK(fixture.host.creates[7].placement.y == 76);
  CHECK(fixture.host.cursors.empty());
  CHECK(std::ranges::none_of(fixture.host.draws,
                             [](const auto& call) { return call.scrolling; }));
}

TEST_CASE("F02815 title row geometry and scroll boundaries are preserved",
          "[gazette-statistics][F02815]") {
  RecordFixture fixture;
  REQUIRE(score::CreateTitleRows(fixture.rows, 0xFFFFFFFFU, fixture.titles));
  const std::uint8_t page =
      GENERATE(std::uint8_t{0}, std::uint8_t{1}, std::uint8_t{24});
  REQUIRE(fixture.Titles({page, 0}));
  CHECK(fixture.host.creates.front().placement.w == 200);
  CHECK(fixture.host.numbers.front() ==
        std::array<int, 4>{46, 56, page + 1, page == 0 ? 0 : 1});
  CHECK(fixture.host.draws.front().scrolling == (page != 0));
  CHECK(fixture.host.draws.back().scrolling == (page != 24));
  CHECK(fixture.host.menu.state == fw::RowState::kInactive);
  CHECK(fixture.host.gets == 1);
}

TEST_CASE("F02815 cursor results truncate to a byte at the settled step",
          "[gazette-statistics][F02815]") {
  RecordFixture fixture;
  fixture.animation.cursor_step = 255;
  fixture.host.cursor_reply = 513;
  REQUIRE(fixture.Titles({0, 3}));
  CHECK(fixture.host.cursors == std::vector<std::array<int, 3>>{{255, 91, 96}});
  CHECK(fixture.animation.cursor_step == 1);
}

TEST_CASE("F02815 byte selections outside the list select no row",
          "[gazette-statistics][F02815]") {
  RecordFixture fixture;
  REQUIRE(fixture.Titles({24, 255}));
  CHECK(fixture.host.information.empty());
  CHECK(fixture.host.cursors.empty());
  CHECK(std::ranges::all_of(std::span(fixture.rows).first(32),
                            [](const auto& row) { return row.selected == 0; }));
  CHECK(fixture.rows.back().highlight_step == 77);
}

TEST_CASE("F02815 invalid page span or required text refuses before mutation",
          "[gazette-statistics][F02815]") {
  RecordFixture fixture;
  const int failure = GENERATE(0, 1, 2, 3);
  std::span<score::GazetteRecordRow> rows(fixture.rows);
  score::GazetteRecordCursor cursor{};
  if (failure == 0) cursor.first_row = 25;
  if (failure == 1) rows = rows.first(31);
  if (failure == 2) fixture.rows[7].title = {};
  if (failure == 3) {
    fixture.rows[0].selected = 0;
    fixture.rows[0].description = {};
  }
  const auto before = fixture.rows;
  CHECK_FALSE(
      score::RenderTitles(rows, cursor, 8, fixture.animation, fixture.host));
  CHECK(SameRows(fixture.rows, before));
  CHECK(fixture.host.creates.empty());
  CHECK(fixture.host.refusals == 1);
}

TEST_CASE("F02815 signed presentation offsets keep original wrapping",
          "[gazette-statistics][F02815]") {
  RecordFixture fixture;
  const std::int32_t step = GENERATE(-1, INT_MIN, INT_MAX);
  REQUIRE(fixture.Titles({}, step));
  CHECK(fixture.host.creates.front().placement.y == (step == -1 ? -39 : 56));
  CHECK(fixture.host.cursors.empty());
}

TEST_CASE("F02816 title digits use real FT4 packets in right-to-left order",
          "[gazette-statistics][F02816]") {
  NumberTarget target;
  const int value = GENERATE(0, 1, 9, 10, 31, 32, 99);
  REQUIRE(score::DrawTitleNumber({100, 50, value, false}, target));
  REQUIRE(target.queue.packets.size() == 2);
  CHECK(target.queue.packets[0][2] == 0x002E0077U);
  CHECK(target.queue.packets[1][2] == 0x002E0072U);
  CHECK(target.queue.packets[0][3] ==
        (0x37F40000U | static_cast<std::uint32_t>(value % 10 * 6)));
  CHECK(target.queue.packets[1][3] ==
        (0x37F40000U | static_cast<std::uint32_t>(value / 10 * 6)));
  CHECK(target.queue.packets[0][1] == 0x2E808080U);
  CHECK(target.queue.packets[0][8] == 0x0038007DU);
}

TEST_CASE("F02816 dim digits retain links and untouched UV high halves",
          "[gazette-statistics][F02816]") {
  NumberTarget target;
  REQUIRE(score::DrawTitleNumber({0, 0, 12, true}, target));
  REQUIRE(target.queue.packets.size() == 2);
  const auto& first = target.queue.packets[0];
  CHECK(first[0] == 0x09123456U);
  CHECK(first[1] == 0x2E202020U);
  CHECK(first[3] == 0x37F7000CU);
  CHECK(first[5] == 0x002C0012U);
  CHECK(first[7] == 0xABCD0A0CU);
  CHECK(first[9] == 0xABCD0A12U);
  CHECK(target.queue.packets[1][0] == 0x09300000U);
}

TEST_CASE("F02816 extreme coordinates wrap at the original halfword stores",
          "[gazette-statistics][F02816]") {
  NumberTarget target;
  REQUIRE(score::DrawTitleNumber({INT_MAX, INT_MIN, 1, false}, target));
  CHECK(target.queue.packets[0][2] == 0xFFFC0012U);
  CHECK(target.queue.packets[0][8] == 0x00060018U);
}

TEST_CASE("F02816 refuses non-two-digit values and bounded target exhaustion",
          "[gazette-statistics][F02816]") {
  NumberTarget target;
  CHECK_FALSE(score::DrawTitleNumber({0, 0, -1, false}, target));
  CHECK_FALSE(score::DrawTitleNumber({0, 0, 100, false}, target));
  CHECK(target.queue.packets.empty());
  target.queue.capacity = 1;
  CHECK_FALSE(score::DrawTitleNumber({0, 0, 32, false}, target));
  CHECK(target.queue.packets.size() == 1);
}

TEST_CASE("F02824 statistic rows reset all fields and preserve the tail",
          "[gazette-statistics][F02824]") {
  RecordFixture fixture;
  const auto before = fixture.rows;
  REQUIRE(score::CreateStatisticRows(fixture.rows, fixture.statistics));
  for (std::size_t index = 0; index < 23; ++index) {
    const auto& row = fixture.rows[index];
    CHECK(std::array<int, 6>{row.selected, row.unlocked, row.highlight_step,
                             row.disabled, row.x, row.number} ==
          std::array<int, 6>{0, 1, 0, 0, 16, static_cast<int>(index + 1)});
    CHECK(row.title.data() == fixture.statistics.titles[index].data());
    CHECK(row.description.data() ==
          fixture.statistics.descriptions[index].data());
  }
  CHECK(SameRows(std::span(fixture.rows).subspan(23),
                 std::span(before).subspan(23)));
}

TEST_CASE("F02824 short rows and malformed final labels are atomic refusals",
          "[gazette-statistics][F02824]") {
  RecordFixture fixture;
  const auto before = fixture.rows;
  CHECK_FALSE(score::CreateStatisticRows(std::span(fixture.rows).first(22),
                                         fixture.statistics));
  fixture.statistics.descriptions.back() = {};
  CHECK_FALSE(score::CreateStatisticRows(fixture.rows, fixture.statistics));
  CHECK(SameRows(fixture.rows, before));
}

TEST_CASE("F02825 first statistics show current progress and saved time",
          "[gazette-statistics][F02825]") {
  RecordFixture fixture;
  REQUIRE(fixture.Statistics());
  CHECK(fixture.Value(0) == Bytes{3, 0xE7});
  CHECK(fixture.Value(1) == Bytes{0x1B, 3, 0xFA, 12, 0, 1, 0x9E, 0, 0, 0xE7});
  CHECK(fixture.Value(2) == Bytes{5, 0, 0x94, 0xE7});
  CHECK(fixture.Value(3) == Bytes{1, 0, 0, 0x94, 0xE7});
  CHECK(fixture.Value(4) == Bytes{4, 9, 0x94, 0xE7});
  CHECK(fixture.Value(5) == Bytes{1, 0, 0, 0x94, 0xE7});
  CHECK(fixture.Value(6) == Bytes{6, 5, 5, 3, 5, 0xE7});
  CHECK(fixture.Value(7) == Bytes{1, 0, 0, 0xE7});
}

TEST_CASE("F02825 all six enemy-class counters retain their order",
          "[gazette-statistics][F02825]") {
  RecordFixture fixture;
  REQUIRE(fixture.Statistics({5, 0}));
  for (std::size_t index = 0; index < 6; ++index) {
    CHECK(fixture.Value(index + 2) ==
          Bytes{1, 0, static_cast<std::uint8_t>(index), 0xE7});
  }
}

TEST_CASE("F02825 weapon counters rotate class zero to the final row",
          "[gazette-statistics][F02825]") {
  RecordFixture fixture;
  REQUIRE(fixture.Statistics({15, 7}));
  for (std::size_t index = 0; index < 7; ++index) {
    CHECK(fixture.Value(index) ==
          Bytes{1, 0, 0, static_cast<std::uint8_t>(index + 3), 0xE7});
  }
  CHECK(fixture.Value(7) == Bytes{1, 0, 0, 0, 0xE7});
  CHECK_FALSE(fixture.host.draws.back().scrolling);
}

TEST_CASE("F02825 saved percentages wrap unsigned products before division",
          "[gazette-statistics][F02825]") {
  RecordFixture fixture;
  fixture.values.best.opened_chest_count = 0xFFFFFFFFU;
  fixture.values.best.map_completion = 0x80000004U;
  REQUIRE(fixture.Statistics());
  CHECK(fixture.Value(3) == Bytes{8, 2, 5, 9, 5, 5, 2, 3, 0x94, 0xE7});
  CHECK(fixture.Value(5) == Bytes{1, 0x94, 0xE7});
}

TEST_CASE("F02825 no-clear label and signed time reuse the bounded formatter",
          "[gazette-statistics][F02825]") {
  RecordFixture fixture;
  fixture.values.current.clear_count = 0;
  REQUIRE(fixture.Statistics());
  CHECK(fixture.Value(1) == Bytes{0xA7, 0xA7, 0xE7});
  fixture.host.creates.clear();
  fixture.values.current.clear_count = 1;
  fixture.values.best_clear_round = 0xFFFFFFFEU;
  fixture.values.best.completion_time_minutes = 0xFFFFFFC3U;
  REQUIRE(fixture.Statistics());
  CHECK(fixture.Value(1) ==
        Bytes{0x1B, 0xFD, 1, 0xFA, 12, 0xFD, 1, 0x9E, 0xFD, 1, 0xE7});
}

TEST_CASE("F02825 statistics selection and value highlights are complementary",
          "[gazette-statistics][F02825]") {
  RecordFixture fixture;
  REQUIRE(score::CreateStatisticRows(fixture.rows, fixture.statistics));
  fixture.rows[1].highlight_step = 255;
  REQUIRE(fixture.Statistics());
  CHECK(fixture.host.information.size() == 1);
  CHECK(fixture.host.draws[0].row.gradient_state == 8);
  CHECK(fixture.host.draws[1].row.gradient_state == 0);
  CHECK(fixture.host.draws[2].row.gradient_state == 254);
  CHECK(fixture.host.draws[3].row.gradient_state == 10);
  CHECK(fixture.rows[23].selected == 2);
}

TEST_CASE("F02825 statistics entrance and paired scroll boundaries agree",
          "[gazette-statistics][F02825]") {
  RecordFixture fixture;
  REQUIRE(score::CreateStatisticRows(fixture.rows, fixture.statistics));
  REQUIRE(fixture.Statistics({1, 0}, 4));
  CHECK(fixture.host.creates.size() == 16);
  CHECK(fixture.host.creates[0].placement.y == 148);
  CHECK(fixture.host.creates[15].placement.y == 168);
  CHECK(fixture.host.creates[1].placement.x == 158);
  CHECK(fixture.host.draws[0].row.is_scrollable == 1);
  CHECK(fixture.host.draws[1].row.is_scrollable == 1);
  CHECK(fixture.host.cursors.empty());
}

TEST_CASE("F02825 settled statistics cursor and row cleanup retain byte rules",
          "[gazette-statistics][F02825]") {
  RecordFixture fixture;
  REQUIRE(score::CreateStatisticRows(fixture.rows, fixture.statistics));
  fixture.animation.cursor_step = 255;
  fixture.host.cursor_reply = -1;
  REQUIRE(fixture.Statistics({1, 7}));
  CHECK(fixture.host.cursors == std::vector<std::array<int, 3>>{{255, 4, 160}});
  CHECK(fixture.animation.cursor_step == 255);
  CHECK(fixture.host.draws.front().scrolling);
  CHECK(fixture.host.draws.back().scrolling);
  CHECK(fixture.host.menu.state == fw::RowState::kInactive);
  CHECK(fixture.host.gets == 1);
}

TEST_CASE("F02825 invalid rows page and oversized text are atomic refusals",
          "[gazette-statistics][F02825]") {
  RecordFixture fixture;
  const int failure = GENERATE(0, 1, 2, 3);
  std::array<std::uint8_t, 31> long_label{};
  long_label.back() = 0xE7;
  score::GazetteRecordCursor cursor{};
  std::span<score::GazetteRecordRow> rows(fixture.rows);
  if (failure == 0) cursor.first_row = 16;
  if (failure == 1) rows = rows.first(22);
  if (failure == 2) fixture.rows[7].title = {};
  if (failure == 3) {
    fixture.values.current.clear_count = 0;
    fixture.values.no_clear = long_label;
  }
  const auto before = fixture.rows;
  CHECK_FALSE(score::RenderStatistics(rows, fixture.values, cursor, 8,
                                      fixture.animation, fixture.host));
  CHECK(SameRows(fixture.rows, before));
  CHECK(fixture.host.creates.empty());
  CHECK(fixture.host.information.empty());
}

TEST_CASE("F02825 byte selection can leave all statistics unselected",
          "[gazette-statistics][F02825]") {
  RecordFixture fixture;
  REQUIRE(fixture.Statistics({15, 255}, -1));
  CHECK(fixture.host.creates.front().placement.y == 263);
  CHECK(fixture.host.information.empty());
  CHECK(fixture.host.cursors.empty());
}

TEST_CASE("F02815 title output reaches existing scrolling and glyph renderers",
          "[gazette-statistics][F02815][renderer]") {
  FontGlobals fonts;
  RecordFixture fixture;
  REQUIRE(score::CreateTitleRows(fixture.rows, 0xFFFFFFFFU, fixture.titles));
  fixture.host.rasterize = true;
  REQUIRE(fixture.Titles({1, 0}));
  CHECK(fixture.host.raster.arrows == 2);
  CHECK(fixture.host.raster.lines == 24);
  CHECK(fixture.host.raster.reflections == 24);
  CHECK(fixture.host.raster.queue.packets.size() == 4);
  CHECK(fixture.host.digit_target.queue.packets.size() == 16);
  CHECK(fixture.host.raster.refusals == 0);
}

TEST_CASE("F02825 alignment uses actual pinned font widths and scroll renderer",
          "[gazette-statistics][F02825][renderer]") {
  FontGlobals fonts;
  Render_variableFontGlyphWidths[3] = 12;
  RecordFixture fixture;
  REQUIRE(score::CreateStatisticRows(fixture.rows, fixture.statistics));
  fixture.host.rasterize = true;
  REQUIRE(fixture.Statistics());
  CHECK(fixture.host.creates[1].text[0] == 0xFA);
  CHECK(fixture.host.creates[1].text[1] == 118);
  CHECK(fixture.host.raster.arrows == 1);
  CHECK(fixture.host.raster.lines == 24);
  CHECK(fixture.host.raster.queue.packets.size() == 5);
  CHECK(fixture.host.raster.refusals == 0);
}
