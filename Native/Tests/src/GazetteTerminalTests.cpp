#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Menu/GazetteTerminalFixture.hpp"

namespace {
namespace fixture = VS::tests::gazette;
namespace score = VS::Menu::Score;
namespace fw = VS::Menu::Framework;
using fixture::Call;
using fixture::Calls;

std::vector<std::int64_t> Kinds(const Calls& calls) {
  std::vector<std::int64_t> result;
  for (const auto& call : calls) {
    if (!call.empty()) result.push_back(call.front());
  }
  return result;
}

const Call& Find(const Calls& calls, std::int64_t kind) {
  const auto found = std::ranges::find_if(calls, [kind](const auto& call) {
    return !call.empty() && call[0] == kind;
  });
  static const Call empty;
  return found == calls.end() ? empty : *found;
}

// The rank box's little-endian X halfword.
int BoxX(std::span<const std::uint8_t> box) {
  return box[fixture::kBoxX] | (box[fixture::kBoxX + 1] << 8);
}

// The call kinds of a replacement preview that fails: the navigation and
// viewer draws (47, 47, 48), freeing the old preview when the switch was
// starting (44), the failed load (43), showing a hidden menu again (46), then
// the rank box move (25) and the list description (49).
std::vector<std::int64_t> ReplacementFailureKinds(bool starting, bool hidden) {
  std::vector<std::int64_t> kinds{47, 47, 48};
  if (starting) kinds.push_back(44);
  kinds.push_back(43);
  if (hidden) kinds.push_back(46);
  kinds.insert(kinds.end(), {25, 49});
  return kinds;
}

bool List(fixture::EnemyData& data, fixture::EncyclopediaHost& host,
          score::GazetteRecordCursor cursor, int step,
          score::GazetteRecordAnimation& animation) {
  return score::RenderEnemyList(data.enemies, cursor, step, animation,
                                data.locked_name, data.locked_description,
                                host);
}
}  // namespace

TEST_CASE("F02805 chooses the selected unlocked description or locked label",
          "[gazette-terminal][F02805]") {
  fixture::EnemyData data;
  fixture::EncyclopediaHost host;
  data.enemies[77].unlocked = static_cast<std::uint8_t>(GENERATE(0, 1, 255));
  REQUIRE(score::ShowEnemyDescription(data.enemies, 77, data.locked_description,
                                      host));
  const Calls expected = data.enemies[77].unlocked == 0
                             ? Calls{{3, 3, 0x20, 0x21, 0xE7}}
                             : Calls{{3, 3, 77, 0x40, 0xE7}};
  CHECK(host.calls == expected);
}

TEST_CASE("F02805 refuses only the description it would read",
          "[gazette-terminal][F02805]") {
  fixture::EnemyData data;
  fixture::EncyclopediaHost host;
  data.enemies[1].description = {};
  REQUIRE(score::ShowEnemyDescription(data.enemies, 1, data.locked_description,
                                      host));
  host.calls.clear();
  data.enemies[1].unlocked = 1;
  CHECK_FALSE(score::ShowEnemyDescription(data.enemies, 1,
                                          data.locked_description, host));
  CHECK_FALSE(score::ShowEnemyDescription(data.enemies, -1,
                                          data.locked_description, host));
  CHECK_FALSE(score::ShowEnemyDescription(data.enemies, 79,
                                          data.locked_description, host));
  CHECK(host.calls.empty());
}

TEST_CASE(
    "F02817 emits all three digits in units-first order with intact padding",
    "[gazette-terminal][F02817]") {
  fixture::EncyclopediaHost host;
  VS::Render::UiTexturedQuadPacket initial{};
  initial.fill(0xABCD9876);
  host.storage.assign(3, initial);
  host.chain = 0x13572468;
  REQUIRE(score::DrawEnemyNumber({70, 30, 78, false}, host));
  REQUIRE(host.packets.size() == 3);
  CHECK(host.packets[0][0] == 0x09572468);
  CHECK(host.packets[0][1] == 0x2E808080);
  CHECK(host.packets[0][2] == fw::PackXy(94, 26));
  CHECK(host.packets[1][3] == 0x37F4002A);
  CHECK(host.packets[2][3] == 0x37F40000);
  CHECK(host.packets[2][7] == 0xABCD0A00);
}

TEST_CASE(
    "F02817 dims its texture and wraps coordinates without losing hundreds",
    "[gazette-terminal][F02817]") {
  fixture::EncyclopediaHost host;
  REQUIRE(score::DrawEnemyNumber({65535, -32768, 999, true}, host));
  REQUIRE(host.packets.size() == 3);
  CHECK(host.packets[0][1] == 0x2E202020);
  CHECK(host.packets[0][2] == fw::PackXy(23, 32764));
  CHECK(host.packets[2][3] == 0x37F70036);
  CHECK(host.packets[0][5] == 0x002C003C);
}

TEST_CASE("F02817 refuses unsupported numbers and stops at failed emission",
          "[gazette-terminal][F02817]") {
  fixture::EncyclopediaHost host;
  CHECK_FALSE(score::DrawEnemyNumber({0, 0, -1, false}, host));
  CHECK_FALSE(score::DrawEnemyNumber({0, 0, 1000, false}, host));
  CHECK(host.packets.empty());
  host.quad_capacity = 1;
  CHECK_FALSE(score::DrawEnemyNumber({0, 0, 0, false}, host));
  CHECK(host.packets.size() == 1);
}

TEST_CASE("F02818 links first last and cross-word unlocked enemies circularly",
          "[gazette-terminal][F02818]") {
  fixture::EnemyData data;
  data.flags[0] = 1;
  data.flags[1] = 1;
  data.flags[25] = 0x80000000;
  data.enemies[31].defeat_bit_count = 2;
  data.enemies[77].first_defeat_bit = 831;
  data.enemies[3].previous = 99;
  REQUIRE(score::InitializeEnemyList(data.enemies, data.flags));
  CHECK(data.enemies[0].previous == 77);
  CHECK(data.enemies[0].next == 31);
  CHECK(data.enemies[31].next == 32);
  CHECK(data.enemies[32].next == 77);
  CHECK(data.enemies[77].next == 0);
  CHECK(data.enemies[3].previous == 99);
  CHECK(data.enemies[78].unlocked == 0);
}

TEST_CASE("F02818 preserves signed state and locked links with no defeats",
          "[gazette-terminal][F02818]") {
  fixture::EnemyData data;
  data.enemies[1].unlocked = 255;
  data.enemies[1].selected = -123;
  data.enemies[1].highlight_step = -32768;
  data.enemies[1].previous = 71;
  data.enemies[1].next = 72;
  REQUIRE(score::InitializeEnemyList(data.enemies, data.flags));
  CHECK(data.enemies[1].unlocked == 0);
  CHECK(data.enemies[1].selected == -123);
  CHECK(data.enemies[1].highlight_step == -32768);
  CHECK(data.enemies[1].previous == 71);
  CHECK(data.enemies[1].next == 72);
}

TEST_CASE("F02818 one unlocked entry links to itself on repeat initialization",
          "[gazette-terminal][F02818]") {
  fixture::EnemyData data;
  data.flags[2] = 1U << 13;
  REQUIRE(score::InitializeEnemyList(data.enemies, data.flags));
  CHECK(data.enemies[77].previous == 77);
  CHECK(data.enemies[77].next == 77);
  data.flags.fill(0);
  REQUIRE(score::InitializeEnemyList(data.enemies, data.flags));
  CHECK(data.enemies[77].unlocked == 0);
  CHECK(data.enemies[77].previous == 77);
}

TEST_CASE("F02818 validates every positive defeat range before writing",
          "[gazette-terminal][F02818]") {
  fixture::EnemyData data;
  data.enemies[0].unlocked = 7;
  data.enemies[77].first_defeat_bit =
      static_cast<std::int16_t>(GENERATE(-1, 832, 32767));
  CHECK_FALSE(score::InitializeEnemyList(data.enemies, data.flags));
  CHECK(data.enemies[0].unlocked == 7);
  data.enemies[77].defeat_bit_count = 0;
  REQUIRE(score::InitializeEnemyList(data.enemies, data.flags));
  CHECK_FALSE(
      score::InitializeEnemyList(std::span(data.enemies).first(0), data.flags));
}

TEST_CASE("F02818 nonpositive counts never require defeat flag storage",
          "[gazette-terminal][F02818]") {
  fixture::EnemyData data;
  for (auto& enemy : data.enemies) {
    enemy.first_defeat_bit = -32768;
    enemy.defeat_bit_count = -1;
    enemy.unlocked = 1;
    enemy.previous = 31;
  }
  REQUIRE(score::InitializeEnemyList(data.enemies, {}));
  CHECK(data.enemies[0].unlocked == 0);
  CHECK(data.enemies[77].unlocked == 0);
  CHECK(data.enemies[77].previous == 31);
  CHECK(data.enemies[78].unlocked == 0);
}

TEST_CASE("F02819 updates nonvisible rows and preserves negative highlights",
          "[gazette-terminal][F02819]") {
  fixture::EnemyData data;
  fixture::EncyclopediaHost host;
  score::GazetteRecordAnimation animation;
  data.enemies[9].highlight_step = -1;
  data.enemies[0].highlight_step = -32768;
  data.enemies[77].highlight_step = 300;
  data.enemies[78].selected = 42;
  REQUIRE(List(data, host, {2, 7}, 8, animation));
  CHECK(data.enemies[9].selected == 1);
  CHECK(data.enemies[9].highlight_step == -1);
  CHECK(data.enemies[0].highlight_step == -32768);
  CHECK(data.enemies[77].highlight_step == 299);
  CHECK(data.enemies[78].selected == 0);
  CHECK(animation.cursor_step == 1);
  CHECK(host.menu.state == fw::RowState::kInactive);
}

TEST_CASE(
    "F02819 locked rows retain framework fields except assigned colors and "
    "arrows",
    "[gazette-terminal][F02819]") {
  fixture::EnemyData data;
  fixture::EncyclopediaHost host;
  score::GazetteRecordAnimation animation;
  for (auto& reply : host.menu_replies) reply.fill(5);
  REQUIRE(List(data, host, {1, 0}, 8, animation));
  const auto& row = Find(host.calls, 7);
  REQUIRE(row.size() == 61);
  CHECK(row[1] == 5);
  CHECK(row[6] == 1);
  CHECK(row[11] == 3);
  CHECK(host.created == 8);
  CHECK(Find(host.calls, 5) == Call{5, 46, 56, 2, 1});
}

TEST_CASE("F02819 slides rows while suppressing the cursor until step eight",
          "[gazette-terminal][F02819]") {
  fixture::EnemyData data;
  fixture::EncyclopediaHost host;
  score::GazetteRecordAnimation animation{33};
  data.enemies[0].unlocked = 1;
  REQUIRE(List(data, host, {0, 0}, 2, animation));
  CHECK(Find(host.calls, 2) == Call{2, 0, 60, 30, 200, 0, 4, 0xFA, 8, 0, 0xE7});
  CHECK(Find(host.calls, 4).empty());
  CHECK(data.enemies[0].highlight_step == 8);
  CHECK(animation.cursor_step == 33);
  CHECK(Find(host.calls, 7).empty());
}

TEST_CASE(
    "F02819 refuses bad pages and chosen text without state or draw changes",
    "[gazette-terminal][F02819]") {
  fixture::EnemyData data;
  fixture::EncyclopediaHost host;
  score::GazetteRecordAnimation animation;
  data.enemies[0].selected = 17;
  CHECK_FALSE(List(data, host, {72, 0}, 8, animation));
  data.enemies[2].unlocked = 1;
  data.enemies[2].name = {};
  CHECK_FALSE(List(data, host, {0, 0}, 8, animation));
  CHECK(data.enemies[0].selected == 17);
  CHECK(host.calls.empty());
}

TEST_CASE(
    "F02820 restores projection before ordered viewer UI and complete "
    "identification",
    "[gazette-terminal][F02820]") {
  fixture::EnemyData data;
  fixture::EncyclopediaHost host;
  const score::EnemyViewerPresentation presentation{7,     4,        0,
                                                    false, 0x112233, 0x445566};
  REQUIRE(score::RenderEnemyViewer(data.enemies, presentation, host));
  const std::vector<std::int64_t> order{12, 8,  9,  10, 11, 9,  2,  6,  1, 13,
                                        14, 15, 13, 14, 15, 14, 16, 14, 16};
  CHECK(Kinds(host.calls) == order);
  CHECK(host.calls[2] == Call{9, 32, 104});
  CHECK(host.calls[5] == Call{9, 97, 103});
  CHECK(host.calls[6] == Call{2, 0, 248, 166, 256, 0, 2, 7, 0xE7});
  CHECK(host.calls[15] == Call{14, fw::PackXy(-56, 160), 0x808080, 7, 14, 'N',
                               'O', '.', ' ', ' ', ' ', '0', '0', '8', '/', '0',
                               '7', '9', 0});
  CHECK(host.menu.state == fw::RowState::kInactive);
}

TEST_CASE("F02820 right switch slides the model but settles all UI",
          "[gazette-terminal][F02820]") {
  fixture::EnemyData data;
  fixture::EncyclopediaHost host;
  REQUIRE(score::RenderEnemyViewer(data.enemies, {0, 0, 2}, host));
  CHECK(host.calls[2] == Call{9, 416, 104});
  CHECK(Find(host.calls, 2) == Call{2, 0, 184, 166, 256, 0, 2, 0, 0xE7});
  CHECK(Find(host.calls, 13) == Call{13, 6, 8, 15, 6});
}

TEST_CASE(
    "F02820 hidden loading draws the edge fade without requiring enemy data",
    "[gazette-terminal][F02820]") {
  fixture::EncyclopediaHost host;
  REQUIRE(score::RenderEnemyViewer({}, {-1, 0, 3, true}, host));
  CHECK(host.calls == Calls{{12}});
  host.calls.clear();
  REQUIRE(score::RenderEnemyViewer({}, {-1, 8, 3, true}, host));
  CHECK(host.calls.empty());
}

TEST_CASE(
    "F02820 refuses out-of-range class selection and unterminated sliced name",
    "[gazette-terminal][F02820]") {
  fixture::EnemyData data;
  fixture::EncyclopediaHost host;
  data.enemies[0].class_id = 6;
  CHECK_FALSE(score::RenderEnemyViewer(data.enemies, {}, host));
  data.enemies[0].class_id = 0;
  data.enemies[0].name = std::span(data.names[0]).first(2);
  CHECK_FALSE(score::RenderEnemyViewer(data.enemies, {}, host));
  CHECK_FALSE(score::RenderEnemyViewer(data.enemies, {-1}, host));
  CHECK(host.calls.empty());
}

TEST_CASE("F02834 keeps raw RGB padding and the original left arrow bounce",
          "[gazette-terminal][F02834]") {
  fixture::EncyclopediaHost host;
  VS::Render::UiTexturedQuadPacket initial{};
  initial.fill(0xAB123456);
  host.storage.push_back(initial);
  REQUIRE(score::DrawEnemyNavigation(0, 8, false, host));
  REQUIRE(host.packets.size() == 1);
  CHECK(host.packets[0][1] == 0x2D123456);
  CHECK(host.packets[0][2] == fw::PackXy(20, 96));
  CHECK(host.packets[0][3] == 0x37F83000);
  CHECK(host.packets[0][7] == 0xAB124000);
  CHECK(Find(host.calls, 14) ==
        Call{14, fw::PackXy(34, 98), 0x808080, 0, 3, 'L', '1', 0});
}

TEST_CASE(
    "F02834 distinguishes transitioning right arrows at the bounce threshold",
    "[gazette-terminal][F02834]") {
  fixture::EncyclopediaHost host;
  REQUIRE(score::DrawEnemyNavigation(3, 9, false, host));
  REQUIRE(host.packets.size() == 1);
  CHECK(host.packets[0][2] == fw::PackXy(284, 96));
  CHECK(host.packets[0][3] == 0x37F93010);
  CHECK(Find(host.calls, 14) ==
        Call{14, fw::PackXy(286, 98), 0x808080, 0, 4, '#', 'R', '1', 0});
}

TEST_CASE(
    "F02834 hidden controls need no target but visible invalid directions "
    "refuse",
    "[gazette-terminal][F02834]") {
  fixture::EncyclopediaHost host;
  REQUIRE(score::DrawEnemyNavigation(-1, 0, true, host));
  CHECK_FALSE(score::DrawEnemyNavigation(-1, 0, false, host));
  CHECK_FALSE(score::DrawEnemyNavigation(4, 0, false, host));
  CHECK(host.calls.empty());
  host.quad_capacity = 0;
  CHECK_FALSE(score::DrawEnemyNavigation(1, 0, false, host));
}

TEST_CASE(
    "F02803 initializes background then builds the rank and overview camera",
    "[gazette-terminal][F02803]") {
  fixture::TerminalHost host;
  REQUIRE(host.Tick() == 0);
  CHECK(host.state.phase == score::GazettePhase::kPrepareOverview);
  CHECK(host.calls == Calls{{20, 2047, 128}});
  host.calls.clear();
  REQUIRE(host.Tick() == 0);
  CHECK(host.state.camera.distance == 336);
  CHECK(host.state.camera.pitch == 3904);
  CHECK(host.state.camera.yaw == 3655);
  CHECK(Find(host.calls, 23) == Call{23, 0, 0, 11, 248, 16, 1, 11, 248});
  CHECK(host.state.phase == score::GazettePhase::kOverview);
}

TEST_CASE(
    "F02803 begins MON read before dismissing the overview and remembers "
    "cursor",
    "[gazette-terminal][F02803]") {
  fixture::TerminalHost host;
  host.state.phase = score::GazettePhase::kOverview;
  host.top_result = 3;
  host.overview_cursor = 3;
  REQUIRE(host.Tick() == 0);
  CHECK(Kinds(host.calls) ==
        std::vector<std::int64_t>{28, 25, 27, 38, 39, 40, 26});
  CHECK(host.state.animation_step == 1);
  CHECK(host.cursor.overview_row == 3);
  CHECK(host.state.phase == score::GazettePhase::kLeaveOverview);
  CHECK(host.data_started);
}

TEST_CASE(
    "F02803 overview leaves only after animation and preserves exit choice",
    "[gazette-terminal][F02803]") {
  fixture::TerminalHost host;
  host.state.phase = score::GazettePhase::kLeaveOverview;
  host.state.animation_step = 1;
  host.state.next_result = 2;
  host.box_present = false;
  REQUIRE(host.Tick() == 0);
  CHECK(host.state.animation_step == 0);
  host.calls.clear();
  REQUIRE(host.Tick() == 2);
  CHECK(Kinds(host.calls) == std::vector<std::int64_t>{28, 29, 30});
}

TEST_CASE("F02803 each remembered overview choice dispatches its distinct page",
          "[gazette-terminal][F02803]") {
  fixture::TerminalHost host;
  host.state.phase = score::GazettePhase::kLeaveOverview;
  host.cursor.overview_row = static_cast<std::uint8_t>(GENERATE(0, 1, 2, 3));
  REQUIRE(host.Tick() == 0);
  CHECK(static_cast<int>(host.state.phase) == 4 + host.cursor.overview_row * 3);
  CHECK(Find(host.calls, 30).empty());
}

TEST_CASE("F02803 title and statistic wraps use pressed buttons before repeat",
          "[gazette-terminal][F02803]") {
  fixture::TerminalHost host;
  const bool titles = GENERATE(true, false);
  host.state.phase =
      titles ? score::GazettePhase::kTitles : score::GazettePhase::kStatistics;
  auto& cursor = titles ? host.cursor.titles : host.cursor.statistics;
  host.input.pressed = 0x1000;
  host.input.repeated = 0x4000;
  REQUIRE(host.Tick() == 0);
  CHECK(cursor.first_row == (titles ? 24 : 15));
  CHECK(cursor.selected_row == 7);
  host.input.pressed = 0x4000;
  REQUIRE(host.Tick() == 0);
  CHECK(cursor.first_row == 0);
  CHECK(cursor.selected_row == 0);
}

TEST_CASE("F02803 moving within a page sounds before the row decrement",
          "[gazette-terminal][F02803]") {
  fixture::TerminalHost host;
  host.state.phase = score::GazettePhase::kTitles;
  host.cursor.titles = {2, 4};
  host.input.repeated = 0x1000;
  REQUIRE(host.Tick() == 0);
  CHECK(host.cursor.titles.selected_row == 3);
  REQUIRE(Find(host.calls, 37).size() == 11);
  CHECK(Find(host.calls, 37)[4] == 4);
  REQUIRE(Find(host.calls, 34).size() == 13);
  CHECK(Find(host.calls, 34)[6] == 3);
}

TEST_CASE("F02803 title cancel still renders the current frame and then leaves",
          "[gazette-terminal][F02803]") {
  fixture::TerminalHost host;
  host.state.phase = score::GazettePhase::kTitles;
  host.state.animation_step = 7;
  host.input.pressed = 0x50;
  REQUIRE(host.Tick() == 0);
  CHECK(Kinds(host.calls) == std::vector<std::int64_t>{37, 32, 34});
  CHECK(host.state.phase == score::GazettePhase::kLeaveTitles);
  CHECK(host.state.next_result == 1);
  CHECK(host.state.animation_step == 8);
  host.state.animation_step = 0;
  REQUIRE(host.Tick() == 2);
  CHECK(host.calls.back() == Call{30});
}

TEST_CASE(
    "F02803 time-trial creation preserves animation and cancellation orders "
    "flyout",
    "[gazette-terminal][F02803]") {
  fixture::TerminalHost host;
  host.state.phase = score::GazettePhase::kPrepareTimeTrials;
  host.state.animation_step = 3;
  REQUIRE(host.Tick() == 0);
  CHECK(host.state.animation_step == 3);
  host.calls.clear();
  host.input.repeated = 0x1000;
  host.input.pressed = 0x40;
  REQUIRE(host.Tick() == 0);
  CHECK(host.cursor.time_trial == 7);
  CHECK(Kinds(host.calls) == std::vector<std::int64_t>{37, 37, 32, 35, 34});
  CHECK(host.state.phase == score::GazettePhase::kLeaveTimeTrials);
}

TEST_CASE("F02803 time-trial cleanup deactivates only completed row delays",
          "[gazette-terminal][F02803]") {
  fixture::TerminalHost host;
  host.state.phase = score::GazettePhase::kLeaveTimeTrials;
  host.delays = {0, 1, 255, 0, 8, 0, 2, 0};
  REQUIRE(host.Tick() == 0);
  CHECK(Kinds(host.calls) == std::vector<std::int64_t>{34, 36, 36, 36, 36});
  CHECK(host.calls[1] == Call{36, 0, 0});
  CHECK(host.calls[4] == Call{36, 7, 7});
  CHECK(host.state.animation_step == 0);
  CHECK(host.state.phase == score::GazettePhase::kPrepareOverview);
}

TEST_CASE(
    "F02803 async enemy list waits then releases request before building rows",
    "[gazette-terminal][F02803]") {
  fixture::TerminalHost host;
  host.state.phase = score::GazettePhase::kWaitForEnemies;
  REQUIRE(host.Tick() == 0);
  CHECK(host.calls.empty());
  host.read_status = score::GazetteReadStatus::kLoaded;
  REQUIRE(host.Tick() == 0);
  CHECK(Kinds(host.calls) == std::vector<std::int64_t>{41, 31, 33});
  CHECK(host.calls.back() == Call{33, 3});
  CHECK(host.description_bound);
  CHECK(host.state.phase == score::GazettePhase::kEnemies);
}

TEST_CASE("F02803 enemy confirm has priority over simultaneous cancellation",
          "[gazette-terminal][F02803]") {
  fixture::TerminalHost host;
  host.state.phase = score::GazettePhase::kEnemies;
  host.input.pressed = 0x70;
  host.data.enemies[0].unlocked = static_cast<std::uint8_t>(GENERATE(0, 1));
  REQUIRE(host.Tick() == 0);
  CHECK(Find(host.calls, 32).empty());
  CHECK(host.state.phase == (host.data.enemies[0].unlocked != 0
                                 ? score::GazettePhase::kLeaveEnemies
                                 : score::GazettePhase::kEnemies));
  CHECK(Find(host.calls, 37)[2] ==
        (host.data.enemies[0].unlocked != 0 ? 5 : 7));
}

TEST_CASE("F02803 enemy-list exit distinguishes preview transfer from release",
          "[gazette-terminal][F02803]") {
  fixture::TerminalHost host;
  host.state.phase = score::GazettePhase::kLeaveEnemies;
  host.state.next_result = 2;
  REQUIRE(host.Tick() == 0);
  CHECK(host.state.phase == score::GazettePhase::kWaitForPreview);
  CHECK(Find(host.calls, 42).empty());
  host.state.phase = score::GazettePhase::kLeaveEnemies;
  host.state.next_result = 1;
  REQUIRE(host.Tick() == 2);
  CHECK(host.calls[host.calls.size() - 2] == Call{42});
  CHECK(host.calls.back() == Call{30});
}

TEST_CASE(
    "F02803 viewer camera uses the signed preset only after preview completion",
    "[gazette-terminal][F02803]") {
  fixture::TerminalHost host;
  host.state.phase = score::GazettePhase::kWaitForPreview;
  host.cursor.enemies = {30, 3};
  host.preview_result = 1;
  REQUIRE(host.Tick() == 0);
  CHECK(host.state.phase == score::GazettePhase::kWaitForPreview);
  host.preview_result = 0;
  host.preset = score::GazettePreviewCamera{65535, -200};
  REQUIRE(host.Tick() == 0);
  CHECK(host.state.camera.distance == 640);
  CHECK(host.state.camera.height_offset == -200);
  CHECK(host.state.camera.yaw == 256);
  CHECK(host.state.selected_enemy == 33);
  CHECK(host.calls.back() == Call{46, 1});
}

TEST_CASE(
    "F02803 left stick rotation wins over zoom and clamps after halfword "
    "wrapping",
    "[gazette-terminal][F02803]") {
  fixture::TerminalHost host;
  host.state.phase = score::GazettePhase::kViewer;
  host.state.animation_step = 8;
  host.state.camera = {32760, 32760, 7, 900, 12};
  host.input = {.held = 2, .left_x = 255, .left_y = 0};
  REQUIRE(host.Tick() == 0);
  CHECK(host.state.camera.pitch == 0);
  CHECK(host.state.camera.yaw == -32712);
  CHECK(host.state.camera.distance == 900);
  CHECK(host.state.camera.roll == 7);
  CHECK(host.state.zoom_color == 0x202020);
  CHECK(host.state.rotation_color == 0x808080);
}

TEST_CASE("F02803 rotation deadzone is strict and L2 has priority over R2",
          "[gazette-terminal][F02803]") {
  fixture::TerminalHost host;
  host.state.phase = score::GazettePhase::kViewer;
  host.state.animation_step = 8;
  host.state.camera = {1000, 0, 0, 900, 0};
  host.input = {.held = 0x3003, .left_x = 64, .left_y = 192};
  REQUIRE(host.Tick() == 0);
  CHECK(host.state.camera.pitch == 1024);
  CHECK(host.state.camera.yaw == 64);
  CHECK(host.state.camera.distance == 900);
  CHECK(host.state.zoom_color == 0x202020);
}

TEST_CASE(
    "F02803 zoom keeps analog four-unit and digital sixty-four-unit speeds",
    "[gazette-terminal][F02803]") {
  fixture::TerminalHost host;
  host.state.phase = score::GazettePhase::kViewer;
  host.state.animation_step = 8;
  host.state.camera.distance = 1000;
  host.input = {.held = 2, .right_y = 255};
  REQUIRE(host.Tick() == 0);
  CHECK(host.state.camera.distance == 1004);
  host.input = {.held = 0x1002};
  REQUIRE(host.Tick() == 0);
  CHECK(host.state.camera.distance == 940);
  host.state.camera.distance = 640;
  REQUIRE(host.Tick() == 0);
  CHECK(host.state.camera.distance == 640);
}

TEST_CASE("F02803 released bumper switches after draw even on the cancel frame",
          "[gazette-terminal][F02803]") {
  fixture::TerminalHost host;
  host.state.phase = score::GazettePhase::kViewer;
  host.state.animation_step = 8;
  host.data.enemies[0].previous = 77;
  host.data.enemies[0].next = 1;
  host.input = {.pressed = 0x48, .repeated = 8, .held = 8, .released = 8};
  REQUIRE(host.Tick() == 0);
  CHECK(host.state.selected_enemy == 1);
  CHECK(host.state.phase == score::GazettePhase::kSwitchLeave);
  CHECK(host.state.enemy_button_or_slide == 1);
  CHECK(host.state.enemy_skip_delay == 1);
  CHECK(Find(host.calls, 45) == Call{45, 1});
  REQUIRE(Find(host.calls, 48).size() == 12);
  CHECK(Find(host.calls, 48)[10] == 0x808080);
  CHECK(host.state.rotation_color == 0x202020);
}

TEST_CASE("F02803 held dual bumpers suppress selection but not UI toggle",
          "[gazette-terminal][F02803]") {
  fixture::TerminalHost host;
  host.state.phase = score::GazettePhase::kViewer;
  host.state.animation_step = 8;
  host.data.enemies[0].next = 1;
  host.input = {.pressed = 0x10C, .repeated = 12, .held = 12};
  REQUIRE(host.Tick() == 0);
  CHECK(host.state.hide_ui == 1);
  CHECK(host.state.selected_enemy == 0);
  CHECK(Find(host.calls, 46) == Call{46, 0});
  CHECK(Find(host.calls, 45).empty());
}

TEST_CASE("F02803 both repeat bits share the same four-step skip delay",
          "[gazette-terminal][F02803]") {
  fixture::TerminalHost host;
  host.state.phase = score::GazettePhase::kViewer;
  host.state.animation_step = 8;
  host.state.enemy_skip_delay = 3;
  host.data.enemies[0].previous = 77;
  host.data.enemies[0].next = 1;
  host.input.repeated = 12;
  REQUIRE(host.Tick() == 0);
  CHECK(host.state.selected_enemy == 77);
  CHECK(host.state.enemy_button_or_slide == 4);
  CHECK(host.state.enemy_skip_delay == 1);
}

TEST_CASE(
    "F02803 viewer return frees preview then restores the remembered list "
    "description",
    "[gazette-terminal][F02803]") {
  fixture::TerminalHost host;
  host.state.phase = score::GazettePhase::kLeaveViewer;
  host.state.selected_enemy = 77;
  host.cursor.enemies = {20, 4};
  REQUIRE(host.Tick() == 0);
  CHECK(Kinds(host.calls) == std::vector<std::int64_t>{48, 44, 49});
  CHECK(host.state.selected_enemy == 24);
  CHECK(host.state.phase == score::GazettePhase::kEnemies);
  CHECK(host.calls.back() == Call{49, 3, 24, 0x40, 0xE7});
  CHECK(Find(host.calls, 42).empty());
}

TEST_CASE(
    "F02803 model switch starts selected model but polls remembered model",
    "[gazette-terminal][F02803]") {
  fixture::TerminalHost host;
  host.state.phase = score::GazettePhase::kSwitchLeave;
  host.state.selected_enemy = 77;
  host.state.enemy_button_or_slide = 1;
  host.cursor.enemies = {10, 2};
  REQUIRE(host.Tick() == 0);
  CHECK(host.calls.back() == Call{43, 77});
  CHECK(host.state.phase == score::GazettePhase::kSwitchLoad);
  host.calls.clear();
  REQUIRE(host.Tick() == 0);
  CHECK(Find(host.calls, 43) == Call{43, 12});
  CHECK(host.state.phase == score::GazettePhase::kSwitchEnter);
  CHECK(host.state.selected_enemy == 77);
}

TEST_CASE(
    "F02803 replacement entrance draws its final settled frame before "
    "reactivating",
    "[gazette-terminal][F02803]") {
  fixture::TerminalHost host;
  host.state.phase = score::GazettePhase::kSwitchEnter;
  host.state.enemy_button_or_slide = 2;
  host.state.animation_step = 7;
  REQUIRE(host.Tick() == 0);
  CHECK(host.state.phase == score::GazettePhase::kSwitchEnter);
  CHECK(Find(host.calls, 48)[2] == 1);
  REQUIRE(host.Tick() == 0);
  CHECK(host.state.phase == score::GazettePhase::kViewer);
  CHECK(host.state.enemy_button_or_slide == 0);
}

TEST_CASE(
    "F02803 refuses missing preview rank box list and row-delay dependencies",
    "[gazette-terminal][F02803]") {
  fixture::TerminalHost host;
  host.state.phase = score::GazettePhase::kPrepareOverview;
  host.preset.reset();
  CHECK_FALSE(host.Tick().has_value());
  host.state.phase = score::GazettePhase::kOverview;
  host.box_present = false;
  CHECK_FALSE(host.Tick().has_value());
  host.state.phase = score::GazettePhase::kViewer;
  host.enemies_present = false;
  CHECK_FALSE(host.Tick().has_value());
  host.state.phase = score::GazettePhase::kLeaveTimeTrials;
  host.delay_count = 7;
  CHECK_FALSE(host.Tick().has_value());
}

TEST_CASE("F02803 refuses failed reads invalid links and failed owner release",
          "[gazette-terminal][F02803]") {
  fixture::TerminalHost host;
  host.state.phase = score::GazettePhase::kWaitForEnemies;
  host.read_status = score::GazetteReadStatus::kRefused;
  CHECK_FALSE(host.Tick().has_value());
  host.state.phase = score::GazettePhase::kViewer;
  host.data.enemies[0].next = 255;
  host.input.repeated = 8;
  CHECK_FALSE(host.Tick().has_value());
  host.state.phase = score::GazettePhase::kLeaveEnemies;
  host.free_accepted = false;
  CHECK_FALSE(host.Tick().has_value());
  CHECK(Find(host.calls, 30).empty());
}

TEST_CASE(
    "F02803 refused read acquisition and completion never advance the page",
    "[gazette-terminal][F02803]") {
  fixture::TerminalHost host;
  host.state.phase = score::GazettePhase::kOverview;
  host.top_result = 3;
  host.read_accepted = false;
  CHECK_FALSE(host.Tick().has_value());
  CHECK(host.state.phase == score::GazettePhase::kOverview);
  CHECK(Find(host.calls, 26).empty());
  host.calls.clear();
  host.state.phase = score::GazettePhase::kWaitForEnemies;
  host.read_status = score::GazetteReadStatus::kLoaded;
  host.finish_accepted = false;
  CHECK_FALSE(host.Tick().has_value());
  CHECK(host.state.phase == score::GazettePhase::kWaitForEnemies);
  CHECK(Find(host.calls, 33).empty());
}

TEST_CASE(
    "F02803 a refused preview poll cannot publish viewer selection or camera",
    "[gazette-terminal][F02803]") {
  fixture::TerminalHost host;
  host.state.phase = score::GazettePhase::kWaitForPreview;
  host.state.camera.distance = 1001;
  host.state.selected_enemy = 77;
  host.preview_result.reset();
  CHECK_FALSE(host.Tick().has_value());
  CHECK(host.state.phase == score::GazettePhase::kWaitForPreview);
  CHECK(host.state.camera.distance == 1001);
  CHECK(host.state.selected_enemy == 77);
  CHECK(host.calls == Calls{{43, 0}});
}

TEST_CASE("An enemy preview that cannot start keeps the list open",
          "[gazette-terminal][gazette-preview-failure]") {
  fixture::TerminalHost host;
  host.state.phase = score::GazettePhase::kEnemies;
  host.input.pressed = 0x20;
  host.data.enemies[0].unlocked = 1;
  host.data.enemies[0].model_id = 9;
  host.preview_result = score::kGazettePreviewFailed;
  REQUIRE(host.Tick() == 0);
  CHECK(host.refusals == 1);
  CHECK(host.state.phase == score::GazettePhase::kEnemies);
  CHECK(host.state.next_result == 0);
  CHECK(Find(host.calls, 43) == Call{43, 9});
  // The next confirm tries again.
  host.preview_result = 1;
  REQUIRE(host.Tick() == 0);
  CHECK(host.state.phase == score::GazettePhase::kLeaveEnemies);
  CHECK(host.state.next_result == 2);
}

TEST_CASE("An enemy preview that fails to load returns to the list",
          "[gazette-terminal][gazette-preview-failure]") {
  fixture::TerminalHost host;
  host.state.phase = score::GazettePhase::kWaitForPreview;
  host.state.camera.distance = 1001;
  host.cursor.enemies = {20, 4};
  host.preview_result = score::kGazettePreviewFailed;
  REQUIRE(host.Tick() == 0);
  CHECK(host.refusals == 1);
  CHECK(host.state.phase == score::GazettePhase::kEnemies);
  CHECK(host.state.selected_enemy == 24);
  CHECK(host.state.animation_step == 0);
  CHECK(host.state.next_result == 0);
  CHECK(host.state.camera.distance == 1001);
  CHECK(Kinds(host.calls) == std::vector<std::int64_t>{43, 49});
}

TEST_CASE("A replacement preview that fails restores the menu and rank box",
          "[gazette-terminal][gazette-preview-failure]") {
  fixture::TerminalHost host;
  const bool starting = GENERATE(true, false);
  const int hidden = GENERATE(0, 1);
  host.state.phase = starting ? score::GazettePhase::kSwitchLeave
                              : score::GazettePhase::kSwitchLoad;
  host.state.selected_enemy = 1;
  host.state.hide_ui = hidden;
  host.cursor.enemies = {10, 2};
  host.preview_result = score::kGazettePreviewFailed;
  REQUIRE(host.Tick() == 0);
  CHECK(host.refusals == 1);
  CHECK(host.state.phase == score::GazettePhase::kEnemies);
  CHECK(host.state.selected_enemy == 12);
  CHECK(Kinds(host.calls) == ReplacementFailureKinds(starting, hidden != 0));
  if (hidden != 0) CHECK(Find(host.calls, 46) == Call{46, 1});
  // LeaveViewer's eight 16-unit steps, in one move.
  CHECK(BoxX(host.box) == 128);
}

TEST_CASE(
    "F04156 initializes only its two startup words and preserves last "
    "selection",
    "[gazette-terminal][F04156]") {
  fixture::ScreenHost host;
  host.phase = 3;
  host.state = {99, 123, 2};
  REQUIRE(host.Tick() == 0);
  CHECK(host.phase == 4);
  CHECK(host.state.initialization_delay == 8);
  CHECK(host.state.progress_complete == 0);
  CHECK(host.state.selected_result == 2);
  CHECK(host.calls == Calls{{60, 1}});
}

TEST_CASE(
    "F04156 computes progress while counting down but delays the readiness "
    "poll",
    "[gazette-terminal][F04156]") {
  fixture::ScreenHost host;
  host.phase = 4;
  host.state.initialization_delay = 1;
  REQUIRE(host.Tick() == 0);
  CHECK(host.state.initialization_delay == 0);
  CHECK(host.state.progress_complete == 1);
  CHECK(host.calls == Calls{{61}});
  REQUIRE(host.Tick() == 0);
  CHECK(host.phase == 6);
  CHECK(host.environment.hud_slide_offset == 0x200000);
  CHECK(host.calls.back() == Call{64, 5, 0x200000});
}

TEST_CASE(
    "F04156 retries incomplete progress after delay and retains byte-ready "
    "arithmetic",
    "[gazette-terminal][F04156]") {
  fixture::ScreenHost host;
  host.phase = 4;
  host.progress_reply = 0;
  REQUIRE(host.Tick() == 0);
  CHECK(host.phase == 4);
  host.progress_reply = 7;
  host.idle_reply = 257;
  REQUIRE(host.Tick() == 0);
  CHECK(host.state.progress_complete == 7);
  CHECK(host.phase == 6);
}

TEST_CASE("F04156 teardown orders field HUD command and row endpoints",
          "[gazette-terminal][F04156]") {
  fixture::ScreenHost host;
  host.phase = 6;
  host.menu_reply = 2;
  host.environment.hud_slide_offset = 0x200000;
  REQUIRE(host.Tick() == 0);
  CHECK(host.phase == 7);
  const Calls expected{{65},       {20, -4, 128}, {63, 0, 0x200000},
                       {62, 1, 0}, {66, 0},       {67, 40}};
  CHECK(host.calls == expected);
  CHECK(host.state.selected_result == 2);
  REQUIRE(host.Tick() == 0);
  CHECK(host.phase == 8);
  CHECK(host.calls.back() == Call{60, 0});
}

TEST_CASE(
    "F04156 only fade completion exits and menu return depends on selected "
    "result",
    "[gazette-terminal][F04156]") {
  fixture::ScreenHost host;
  host.phase = 8;
  host.state.selected_result = GENERATE(1, 2, -1);
  host.environment = {0, 1, 4, 1};
  REQUIRE(host.Tick() == 0);
  CHECK(host.phase == 8);
  host.environment.background_fade_step = 0;
  REQUIRE(host.Tick() == 1);
  CHECK(host.phase == 0);
  CHECK(host.environment.hide_menu == 0);
  CHECK(host.environment.current_menu ==
        (host.state.selected_result == 2 ? 9 : 4));
}

TEST_CASE("F02803 and F04156 unhandled phases remain no-ops",
          "[gazette-terminal][F02803][F04156]") {
  fixture::TerminalHost terminal;
  terminal.state.phase =
      static_cast<score::GazettePhase>(GENERATE(-1, 16, 24, 28));
  REQUIRE(terminal.Tick() == 0);
  CHECK(terminal.calls.empty());
  fixture::ScreenHost screen;
  screen.phase = 255;
  REQUIRE(screen.Tick() == 0);
  CHECK(screen.phase == 255);
  CHECK(screen.calls.empty());
}
