#include <algorithm>
#include <array>
#include <bit>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Menu/Score/GazetteTimes.hpp"
#include "Render/GameFont.hpp"
#include "Render/Text/CompactFont.hpp"
#include "Render/UiPackets.hpp"

namespace {
namespace score = VS::Menu::Score;
namespace fw = VS::Menu::Framework;
namespace render = VS::Render;
using Bytes = std::vector<std::uint8_t>;
using Packet = std::vector<std::uint32_t>;
constexpr std::array<std::uint8_t, 4> kNoClear{0xA7, 0xA7, 0xE7, 0x55};

Bytes Fastest(const score::FastestClearTime& value) {
  std::array<std::uint8_t, score::kFastestClearTimeBytes> out{};
  const std::size_t written =
      score::FormatFastestClearTime(out, value, kNoClear);
  return {out.begin(), out.begin() + static_cast<std::ptrdiff_t>(written)};
}

struct TextCall {
  std::string text;
  int x = 0;
  int y = 0;
  std::uint32_t color = 0;
  friend bool operator==(const TextCall&, const TextCall&) = default;
};

class PacketQueue final : public render::PacketQueue {
 public:
  std::vector<Packet> packets;
  std::size_t capacity = 2048;

  std::uint32_t ChainLink() const override {
    return static_cast<std::uint32_t>(packets.size());
  }
  bool Emit(std::span<const std::uint32_t> packet) override {
    if (packets.size() >= capacity) return false;
    packets.emplace_back(packet.begin(), packet.end());
    return true;
  }
};

class TrialHost final : public score::TimeTrialHost {
 public:
  std::array<fw::MenuRow, score::kTimeTrialCount> menu{};
  std::vector<std::size_t> gets;
  std::vector<std::array<int, 5>> creates;
  std::vector<Bytes> titles;
  std::vector<Bytes> information;
  std::vector<std::array<int, 3>> cursors;
  std::vector<TextCall> texts;
  std::vector<std::array<int, 5>> panels;
  std::int32_t cursor_reply = 257;
  int refusals = 0;
  bool rasterize = false;
  bool raster_succeeded = true;
  PacketQueue queue;

  fw::MenuRow& Row(score::TimeTrialId trial) override {
    const auto index = static_cast<std::size_t>(trial);
    gets.push_back(index);
    return menu.at(index);
  }
  fw::MenuRow& CreateRow(const fw::RowPlacement& placement,
                         score::TimeTrialText title) override {
    creates.push_back({placement.row, placement.x, placement.y, placement.w,
                       placement.background_width});
    titles.emplace_back(title.begin(), title.end());
    return menu.at(static_cast<std::size_t>(placement.row));
  }
  void SetInformation(score::TimeTrialText description) override {
    information.emplace_back(description.begin(), description.end());
  }
  std::int32_t DrawCursor(std::uint8_t step, int x, int y) override {
    cursors.push_back({step, x, y});
    return cursor_reply;
  }
  void DrawText(std::string_view text, int x, int y,
                std::uint32_t color) override {
    texts.push_back({std::string(text), x, y, color});
    if (rasterize) {
      raster_succeeded =
          render::DrawCompactText(
              queue,
              {VS::Render::compactGlyphWidths, VS::Render::compactGlyphLeftTrim,
               VS::Render::compactGlyphIndices},
              text, std::bit_cast<std::int32_t>(fw::PackXy(x, y)), color) &&
          raster_succeeded;
    }
  }
  void DrawPanel(const score::PanelRect& rect, int gradient) override {
    panels.push_back({rect.x, rect.y, rect.width, rect.height, gradient});
  }
  void Refuse(std::string_view) override { ++refusals; }
};

struct TrialFixture {
  std::array<std::array<std::uint8_t, 2>, 18> strings{};
  std::array<score::TimeTrialRow, 9> rows{};
  score::TimeTrialScores scores{};
  score::TimeTrialLabels labels;
  score::TimeTrialAnimation animation;
  TrialHost host;

  TrialFixture() {
    for (std::size_t index = 0; index < strings.size(); ++index) {
      strings[index] = {static_cast<std::uint8_t>(index), 0xE7};
    }
    for (std::size_t index = 0; index < score::kTimeTrialCount; ++index) {
      labels.titles[index] = strings[index];
      labels.descriptions[index] = strings[index + 8];
      rows[index].title = labels.titles[index];
      rows[index].description = labels.descriptions[index];
      rows[index].row_number = static_cast<std::int16_t>(index + 100);
      host.menu[index].state = fw::RowState::kIdle;
    }
    labels.missing_title = strings[16];
    labels.missing_description = strings[17];
    rows.back().x = 1234;
    rows.back().entrance_delay = 77;
  }
  TrialFixture(const TrialFixture&) = delete;
  TrialFixture& operator=(const TrialFixture&) = delete;

  bool Render(std::uint8_t selected = 0, std::int32_t step = 8) {
    return score::RenderTimeTrialRecords(
        rows, scores, static_cast<score::TimeTrialId>(selected), step,
        animation, host);
  }
};

// Pin every global compact-font input independently of earlier module tests.
struct GlyphTables {
  decltype(std::to_array(VS::Render::compactGlyphWidths)) widths =
      std::to_array(VS::Render::compactGlyphWidths);
  decltype(std::to_array(VS::Render::compactGlyphLeftTrim)) trim =
      std::to_array(VS::Render::compactGlyphLeftTrim);
  decltype(std::to_array(VS::Render::compactGlyphIndices)) indices =
      std::to_array(VS::Render::compactGlyphIndices);

  GlyphTables() {
    std::ranges::fill(VS::Render::compactGlyphWidths, std::uint8_t{0x66});
    std::ranges::fill(VS::Render::compactGlyphLeftTrim, std::uint8_t{0});
    for (std::size_t index = 0; index < indices.size(); ++index) {
      VS::Render::compactGlyphIndices[index] = static_cast<std::uint8_t>(index);
    }
  }
  ~GlyphTables() {
    std::ranges::copy(widths, VS::Render::compactGlyphWidths);
    std::ranges::copy(trim, VS::Render::compactGlyphLeftTrim);
    std::ranges::copy(indices, VS::Render::compactGlyphIndices);
  }
  GlyphTables(const GlyphTables&) = delete;
  GlyphTables& operator=(const GlyphTables&) = delete;
};
}  // namespace

TEST_CASE("F02828 FormatFastestClearTime copies only the no-clear label",
          "[gazette-times]") {
  CHECK(Fastest({0, INT_MAX, INT_MIN}) == Bytes{0xA7, 0xA7, 0xE7});
}

TEST_CASE("F02828 FormatFastestClearTime displays hours and minute boundaries",
          "[gazette-times]") {
  CHECK(Fastest({1, 0, 0}) == Bytes{0x1B, 1, 0xFA, 12, 0, 0, 0x9E, 0, 0, 0xE7});
  CHECK(Fastest({1, 9, 59}) ==
        Bytes{0x1B, 1, 0, 0xFA, 12, 0, 0, 0x9E, 5, 9, 0xE7});
  CHECK(Fastest({1, 99, 60}) ==
        Bytes{0x1B, 1, 0, 0, 0xFA, 12, 0, 1, 0x9E, 0, 0, 0xE7});
}

TEST_CASE("F02828 FormatFastestClearTime caps at 99 hours 59 minutes",
          "[gazette-times]") {
  const std::int32_t minutes = GENERATE(5999, 6000, 6001, INT_MAX);
  CHECK(Fastest({1, 0, minutes}) ==
        Bytes{0x1B, 1, 0xFA, 12, 9, 9, 0x9E, 5, 9, 0xE7});
  CHECK(Fastest({1, 0, 5998}) ==
        Bytes{0x1B, 1, 0xFA, 12, 9, 9, 0x9E, 5, 8, 0xE7});
}

TEST_CASE(
    "F02828 FormatFastestClearTime preserves negative formatting and word wrap",
    "[gazette-times]") {
  CHECK(Fastest({1, -2, -61}) ==
        Bytes{0x1B, 0xFD, 1, 0xFA, 12, 0xFD, 1, 0x9E, 0xFD, 1, 0xE7});
  CHECK(Fastest({1, INT_MAX, INT_MIN}) ==
        Bytes{0x1B, 0xFD, 2, 1, 4, 7, 4, 8, 3, 6,    4,    8, 0xFA, 12,
              0xFD, 3,    5, 7, 9, 1, 3, 9, 4, 0x9E, 0xFD, 8, 0xE7});
  CHECK(Fastest({1, -1, -1}) ==
        Bytes{0x1B, 0, 0xFA, 12, 0, 0, 0x9E, 0xFD, 1, 0xE7});
}

TEST_CASE("F02828 FormatFastestClearTime honors exact capacity and guard bytes",
          "[gazette-times]") {
  std::array<std::uint8_t, 12> storage{};
  storage.fill(0x55);
  const auto out = std::span(storage).subspan(1, 10);
  CHECK(score::FormatFastestClearTime(out, {1, 0, 60}, {}) == 10);
  CHECK(storage.front() == 0x55);
  CHECK(storage.back() == 0x55);
  const auto saved = storage;
  CHECK(score::FormatFastestClearTime(out.first(9), {1, 0, 60}, {}) == 0);
  CHECK(storage == saved);
}

TEST_CASE(
    "F02828 FormatFastestClearTime refuses malformed or oversized placeholders",
    "[gazette-times]") {
  std::array<std::uint8_t, 2> destination{0x55, 0x55};
  const std::array<std::uint8_t, 2> unterminated{1, 2};
  CHECK(score::FormatFastestClearTime(destination, {}, unterminated) == 0);
  CHECK(score::FormatFastestClearTime(destination, {}, kNoClear) == 0);
  CHECK(score::FormatFastestClearTime({}, {1, 0, 60}, {}) == 0);
  CHECK(destination == std::array<std::uint8_t, 2>{0x55, 0x55});
}

TEST_CASE(
    "F02829 CreateTimeTrialRows selects first-place labels and staggers eight "
    "rows",
    "[gazette-times]") {
  TrialFixture fixture;
  fixture.scores[1][0].time = score::TimeTrialTime{128, 0, 1};
  fixture.scores[0][1].time = score::TimeTrialTime{0, 1, 2};
  CHECK(
      score::CreateTimeTrialRows(fixture.rows, fixture.scores, fixture.labels));
  CHECK(fixture.rows[0].title.data() == fixture.labels.missing_title.data());
  CHECK(fixture.rows[0].description.data() ==
        fixture.labels.missing_description.data());
  CHECK(fixture.rows[0].disabled == 1);
  CHECK(fixture.rows[1].title.data() == fixture.labels.titles[1].data());
  CHECK(fixture.rows[1].description.data() ==
        fixture.labels.descriptions[1].data());
  CHECK(fixture.rows[1].disabled == 0);
}

TEST_CASE("F02829 CreateTimeTrialRows preserves row numbers and the ninth row",
          "[gazette-times]") {
  TrialFixture fixture;
  CHECK(
      score::CreateTimeTrialRows(fixture.rows, fixture.scores, fixture.labels));
  for (std::size_t index = 0; index < 8; ++index) {
    const auto& row = fixture.rows[index];
    CHECK(row.x == 194);
    CHECK(row.row_number == static_cast<int>(index + 100));
    CHECK(row.selected == 0);
    CHECK(row.enabled_style == 1);
    CHECK(row.entrance_delay == index + 1);
  }
  CHECK(fixture.rows.back().x == 1234);
  CHECK(fixture.rows.back().entrance_delay == 77);
}

TEST_CASE(
    "F02829 CreateTimeTrialRows refuses short rows and labels before writes",
    "[gazette-times]") {
  TrialFixture fixture;
  fixture.rows[0].x = -123;
  CHECK_FALSE(score::CreateTimeTrialRows(std::span(fixture.rows).first(7),
                                         fixture.scores, fixture.labels));
  fixture.labels.missing_description = {};
  CHECK_FALSE(
      score::CreateTimeTrialRows(fixture.rows, fixture.scores, fixture.labels));
  CHECK(fixture.rows[0].x == -123);
  CHECK(fixture.rows[0].entrance_delay == 0);
}

TEST_CASE(
    "F02830 LeaveTimeTrialMenu slides active rows and cancels pending rows",
    "[gazette-times]") {
  TrialFixture fixture;
  fixture.rows[1].entrance_delay = 1;
  fixture.rows[2].entrance_delay = 128;
  fixture.rows[3].entrance_delay = 255;
  fixture.host.menu[1].target0 = -44;
  CHECK(score::LeaveTimeTrialMenu(fixture.rows, fixture.host));
  CHECK(fixture.host.gets == std::vector<std::size_t>{0, 4, 5, 6, 7});
  CHECK(fixture.host.menu[0].state == fw::RowState::kSlideX);
  CHECK(fixture.host.menu[0].target0 == 320);
  CHECK(fixture.host.menu[1].target0 == -44);
  CHECK(fixture.rows[1].entrance_delay == 255);
  CHECK(fixture.rows[2].entrance_delay == 255);
  CHECK(fixture.rows[3].entrance_delay == 255);
}

TEST_CASE(
    "F02830 LeaveTimeTrialMenu is repeatable and leaves unrelated row state "
    "alone",
    "[gazette-times]") {
  TrialFixture fixture;
  fixture.rows[0].entrance_delay = 7;
  fixture.rows[0].selected = 2;
  CHECK(score::LeaveTimeTrialMenu(fixture.rows, fixture.host));
  CHECK(score::LeaveTimeTrialMenu(fixture.rows, fixture.host));
  CHECK(fixture.rows[0].entrance_delay == 255);
  CHECK(fixture.rows[0].selected == 2);
  CHECK(fixture.rows[0].row_number == 100);
  CHECK(fixture.rows.back().entrance_delay == 77);
  CHECK(fixture.host.texts.empty());
}

TEST_CASE("F02830 LeaveTimeTrialMenu refuses fewer than eight rows atomically",
          "[gazette-times]") {
  TrialFixture fixture;
  CHECK_FALSE(score::LeaveTimeTrialMenu(std::span(fixture.rows).first(7),
                                        fixture.host));
  CHECK(fixture.host.refusals == 1);
  CHECK(fixture.host.gets.empty());
  CHECK(fixture.host.menu[0].state == fw::RowState::kIdle);
}

TEST_CASE(
    "F02831 RenderTimeTrialRecords creates entering rows at the original "
    "bounds",
    "[gazette-times]") {
  TrialFixture fixture;
  fixture.rows[0].entrance_delay = 1;
  fixture.rows[0].disabled = 3;
  fixture.rows[1].entrance_delay = 255;
  CHECK(fixture.Render());
  CHECK(fixture.host.creates ==
        std::vector<std::array<int, 5>>{{0, 320, 50, 136, 0}});
  CHECK(fixture.host.titles == std::vector<Bytes>{{0, 0xE7}});
  CHECK(fixture.host.menu[0].unselectable == 3);
  CHECK(fixture.host.menu[0].state == fw::RowState::kSlideX);
  CHECK(fixture.host.menu[0].target0 == 194);
  CHECK(fixture.rows[1].entrance_delay == 254);
  CHECK(fixture.host.cursors.empty());
}

TEST_CASE(
    "F02831 RenderTimeTrialRecords waits for all rows and presentation step "
    "eight",
    "[gazette-times]") {
  TrialFixture fixture;
  fixture.rows[0].selected = 2;
  CHECK(fixture.Render(0, 7));
  CHECK(fixture.host.cursors.empty());
  CHECK(fixture.rows[0].selected == 2);
  CHECK(fixture.host.texts.size() == 8);
  fixture.host.menu[7].state = fw::RowState::kSlideX;
  CHECK(fixture.Render());
  CHECK(fixture.host.cursors.empty());
  fixture.host.menu[7].state = fw::RowState::kIdle;
  CHECK(fixture.Render());
  CHECK(fixture.host.cursors.size() == 1);
}

TEST_CASE(
    "F02831 RenderTimeTrialRecords selects once and truncates cursor results "
    "to a byte",
    "[gazette-times]") {
  TrialFixture fixture;
  fixture.animation.cursor_step = 255;
  fixture.rows[0].selected = 1;
  CHECK(fixture.Render(7));
  CHECK(fixture.host.information == std::vector<Bytes>{{15, 0xE7}});
  CHECK(fixture.host.cursors ==
        std::vector<std::array<int, 3>>{{255, 182, 154}});
  CHECK(fixture.animation.cursor_step == 1);
  CHECK(fixture.rows[0].selected == 0);
  CHECK(fixture.host.menu[7].selected == 1);
  CHECK(fixture.Render(7));
  CHECK(fixture.host.information.size() == 1);
}

TEST_CASE(
    "F02831 RenderTimeTrialRecords formats full-width fields and seven-bit "
    "rounds",
    "[gazette-times]") {
  TrialFixture fixture;
  fixture.scores[0][0] = {score::TimeTrialTime{0, 9, 10}, 0};
  fixture.scores[0][2] = {score::TimeTrialTime{255, 255, 255}, 127};
  CHECK(fixture.Render());
  REQUIRE(fixture.host.texts.size() == 11);
  CHECK(fixture.host.texts[4] == TextCall{"$00:09:10", 88, 50, 0x808080});
  CHECK(fixture.host.texts[5] == TextCall{"ROUND    1", 68, 62, 0x808080});
  CHECK(fixture.host.texts[6] == TextCall{"$--:--:--", 88, 80, 0x505050});
  CHECK(fixture.host.texts[7] == TextCall{"$255:255:255", 88, 110, 0x808080});
  CHECK(fixture.host.texts[8] == TextCall{"ROUND    128", 68, 122, 0x808080});
}

TEST_CASE(
    "F02831 RenderTimeTrialRecords uses the selected reference and disabled "
    "tint",
    "[gazette-times]") {
  TrialFixture fixture;
  CHECK(fixture.Render(7));
  REQUIRE_FALSE(fixture.host.texts.empty());
  CHECK(fixture.host.texts.back() == TextCall{"$01:25:00", 88, 158, 0x808080});
  fixture.rows[7].disabled = 1;
  CHECK(fixture.Render(7));
  CHECK(fixture.host.texts.back() == TextCall{"$--:--:--", 88, 158, 0x505050});
}

TEST_CASE("F02831 RenderTimeTrialRecords emits the four exact panel rectangles",
          "[gazette-times]") {
  TrialFixture fixture;
  CHECK(fixture.Render());
  CHECK(fixture.host.panels ==
        std::vector<std::array<int, 5>>{{0, 32, 96, 12, 1},
                                        {16, 50, 128, 84, 2},
                                        {0, 140, 112, 12, 1},
                                        {16, 158, 128, 12, 2}});
  CHECK(fixture.host.texts.front() == TextCall{"PLAYER TIME", 8, 32, 0x808080});
}

TEST_CASE(
    "F02831 RenderTimeTrialRecords wraps presentation arithmetic without "
    "overflow",
    "[gazette-times]") {
  TrialFixture fixture;
  CHECK(fixture.Render(0, INT_MAX));
  CHECK(fixture.host.texts.front() ==
        TextCall{"PLAYER TIME", -136, 32, 0x808080});
  CHECK(fixture.host.panels.front() == std::array<int, 5>{-144, 32, 96, 12, 1});
  CHECK(fixture.Render(0, INT_MIN));
  CHECK(fixture.host.texts.back() ==
        TextCall{"REFERENCE TIME", -120, 140, 0x808080});
}

TEST_CASE(
    "F02831 RenderTimeTrialRecords refuses invalid selection and row extent",
    "[gazette-times]") {
  TrialFixture fixture;
  CHECK_FALSE(fixture.Render(8));
  CHECK_FALSE(fixture.Render(255));
  CHECK_FALSE(score::RenderTimeTrialRecords(
      std::span(fixture.rows).first(7), fixture.scores, score::TimeTrialId{}, 8,
      fixture.animation, fixture.host));
  CHECK(fixture.host.refusals == 3);
  CHECK(fixture.host.gets.empty());
  CHECK(fixture.host.texts.empty());
  CHECK(fixture.rows[0].selected == 0);
}

TEST_CASE(
    "F02831 RenderTimeTrialRecords refuses malformed text and rounds without "
    "mutation",
    "[gazette-times]") {
  TrialFixture fixture;
  fixture.rows[0].entrance_delay = 1;
  fixture.rows[0].title = {};
  CHECK_FALSE(fixture.Render());
  CHECK(fixture.rows[0].entrance_delay == 1);
  fixture.rows[0].title = fixture.labels.titles[0];
  fixture.rows[0].description = {};
  CHECK_FALSE(fixture.Render());
  fixture.rows[0].description = fixture.labels.descriptions[0];
  fixture.scores[0][1] = {score::TimeTrialTime{}, 128};
  CHECK_FALSE(fixture.Render());
  CHECK(fixture.host.texts.empty());
  CHECK(fixture.host.creates.empty());
}

TEST_CASE(
    "F02831 RenderTimeTrialRecords produces actual bounded compact glyph "
    "packets",
    "[gazette-times]") {
  GlyphTables glyphs;
  TrialFixture fixture;
  fixture.host.rasterize = true;
  CHECK(fixture.Render());
  CHECK(fixture.host.raster_succeeded);
  REQUIRE(fixture.host.queue.packets.size() == 75);
  const auto& first = fixture.host.queue.packets.front();
  REQUIRE(first.size() == 5);
  CHECK(first[0] == 0x04000000);
  CHECK(first[1] == 0x64808080);
  CHECK(first[2] == fw::PackXy(68, 32));
  CHECK(first[4] == 0x000A0006);
}

TEST_CASE("F02831 RenderTimeTrialRecords cannot overrun a full renderer queue",
          "[gazette-times]") {
  GlyphTables glyphs;
  TrialFixture fixture;
  fixture.host.rasterize = true;
  fixture.host.queue.capacity = 1;
  CHECK(fixture.Render());
  CHECK_FALSE(fixture.host.raster_succeeded);
  CHECK(fixture.host.queue.packets.size() == 1);
}
