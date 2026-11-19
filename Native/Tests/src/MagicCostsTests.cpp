#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Battle/AbilityState.h"
#include "Battle/Actor.h"
#include "Core/Memory/HeapLive.hpp"
#include "Input/ControllerLiveState.hpp"
#include "Legacy/Psx.h"
#include "Menu/Framework/FrameworkLive.hpp"
#include "Menu/Framework/FrameworkLiveState.hpp"
#include "Menu/Magic.hpp"
#include "Menu/MagicCosts.hpp"
#include "Menu/MagicCostsLive.hpp"
#include "State/StateFlags.hpp"

namespace {
namespace magic = VS::Menu::Magic;
namespace framework = VS::Menu::Framework;

constexpr int kRoomOfSavePointFive = 0xCD;
constexpr int kUnknownRoom = 0x7777;

struct CostText {
  int index = 0;
  std::string text;
  int x = 0;
  int disabled = 0;
  friend bool operator==(const CostText&, const CostText&) = default;
};

using CostTexts = std::vector<CostText>;

struct CreatedRows {
  int row_info = 0;
  std::vector<magic::TeleportRow> rows;
  std::uint8_t cursor_memory_setting = 0;
};

class FakeHost final : public magic::Host {
 public:
  std::array<std::optional<std::uint8_t>, 256> costs{};
  std::array<std::optional<std::uint32_t>, 256> flags{};
  magic::SavePointTable save_points = magic::OriginalSavePoints();
  std::vector<std::uint8_t> states = std::vector<std::uint8_t>(48);
  std::vector<std::uint16_t> strings = std::vector<std::uint16_t>(200);
  std::vector<std::uint16_t> battle_strings = std::vector<std::uint16_t>(16);
  std::optional<std::int16_t> mp = 30;
  int room = kRoomOfSavePointFive;
  bool rows_idle = true;
  std::uint32_t action_restrictions = 0;
  std::uint8_t cursor_setting = 3;
  int selected_row = -1;
  int highlighted_row = 0;
  int display_action_cost = 1;

  std::vector<std::string> refusals;
  std::vector<CostText> cost_texts;
  std::vector<std::pair<int, int>> fly_outs;
  std::optional<std::pair<std::uint8_t, std::uint8_t>> cursor_memory;
  std::optional<CreatedRows> created;
  std::vector<int> cleared;
  int dismissed = 0;
  std::vector<framework::MenuCommand> commands;
  std::optional<magic::MenuItem> menu_item;
  int information_boxes = 0;

  FakeHost() {
    for (std::size_t index = 0; index < strings.size(); ++index) {
      strings[index] = static_cast<std::uint16_t>(100 + index % 90);
    }
    battle_strings[2] = 9;
  }

  void Refuse(std::string_view reason) override {
    refusals.emplace_back(reason);
  }
  std::optional<std::uint8_t> ActionCost(int action_id) override {
    return costs.at(static_cast<std::size_t>(action_id));
  }
  std::optional<std::uint32_t> ActionRestrictionFlags(int action_id) override {
    return flags.at(static_cast<std::size_t>(action_id));
  }
  void SetActionCostText(int index, std::string_view text, int x,
                         int disabled) override {
    cost_texts.push_back({index, std::string(text), x, disabled});
  }
  int CurrentRoomId() override { return room; }
  const magic::SavePointTable& SavePoints() override { return save_points; }
  std::span<const std::uint8_t> SavePointStates() override { return states; }
  std::optional<std::int16_t> CurrentMp() override { return mp; }
  std::span<const std::uint16_t> TeleportationStrings() override {
    return strings;
  }
  std::uint16_t LocationsIndex() const override { return 3; }
  std::uint16_t DisabledDescriptionOffset() const override { return 144; }
  void FlyOutRowsAndHoistSelection(int row, int offset) override {
    fly_outs.emplace_back(row, offset);
  }
  bool RowsIdle() override { return rows_idle; }
  std::uint32_t ActionRestrictions() override { return action_restrictions; }
  void SetTeleportCursorMemory(std::uint8_t row, std::uint8_t scroll) override {
    cursor_memory = std::pair{row, scroll};
  }
  std::uint8_t CursorMemorySetting() override { return cursor_setting; }
  void SetCursorMemorySetting(std::uint8_t value) override {
    cursor_setting = value;
  }
  void CreateRows(int row_info,
                  std::span<const magic::TeleportRow> rows) override {
    created = CreatedRows{row_info, {rows.begin(), rows.end()}, cursor_setting};
  }
  int SelectedRow() override { return selected_row; }
  int HighlightedRow() override { return highlighted_row; }
  void SetDisplayActionCost(int enabled) override {
    display_action_cost = enabled;
  }
  void ClearRowsExcept(int row) override { cleared.push_back(row); }
  void DismissInformationBox() override { ++dismissed; }
  void SetCommandLabel(framework::MenuCommand command) override {
    commands.push_back(command);
  }
  std::span<const std::uint16_t> BattleMenuStrings() override {
    return battle_strings;
  }
  std::uint16_t MagicHeaderIndex() const override { return 2; }
  void SetMenuItem(const magic::MenuItem& item) override { menu_item = item; }
  void InitializeInformationBox() override { ++information_boxes; }
};

// The original's interval costs: every save point but the last adds 4 MP.
int ExpectedCost(int target, int current) {
  const int first = std::min(target, current);
  const int last = std::max(target, current);
  int cost = magic::kTeleportBaseMpCost;
  for (int index = first; index < last; ++index) {
    cost += index < 47 ? 4 : 0;
  }
  return cost;
}

magic::TeleportMenuState OpenedState(FakeHost& host) {
  magic::TeleportMenuState state;
  (void)magic::UpdateTeleportMenu(1, state, host);
  (void)magic::UpdateTeleportMenu(0, state, host);
  return state;
}

// The created row types; empty when no rows were created.
std::vector<std::int32_t> RowTypes(const FakeHost& host) {
  std::vector<std::int32_t> types;
  if (host.created) {
    for (const magic::TeleportRow& row : host.created->rows) {
      types.push_back(row.type);
    }
  }
  return types;
}
}  // namespace

TEST_CASE("Magic SetSpellMpCost formats fixed costs and affordability",
          "[magic-costs]") {
  FakeHost host;
  host.costs[12] = 0;
  host.flags[12] = 2;
  magic::SetSpellMpCost(12, host);
  CHECK(host.cost_texts == CostTexts{{1, "MP", 8, 1}, {0, "#0", 72, 1}});

  host.cost_texts.clear();
  host.costs[255] = 255;
  host.flags[255] = 0xFFFFFFFDU;
  magic::SetSpellMpCost(255, host);
  CHECK(host.cost_texts == CostTexts{{1, "MP", 8, 0}, {0, "#255", 72, 0}});
  CHECK(host.refusals.empty());
}

TEST_CASE("Magic SetSpellMpCost marks variable costs", "[magic-costs]") {
  FakeHost host;
  host.costs[5] = 5;
  host.flags[5] = 0;
  magic::SetSpellMpCost(-5, host);
  REQUIRE(host.cost_texts.size() == 2);
  CHECK(host.cost_texts[1].text == "#[5[-");
  CHECK(host.cost_texts[1].x == 76);
  CHECK(host.cost_texts[0].x == 8);

  host.cost_texts.clear();
  host.costs[200] = 123;
  host.flags[200] = 3;
  magic::SetSpellMpCost(-200, host);
  REQUIRE(host.cost_texts.size() == 2);
  CHECK(host.cost_texts[1].text == "#[1[2[3[-");
  CHECK(host.cost_texts[1].disabled == 1);
  CHECK(host.refusals.empty());
}

TEST_CASE("Magic SetSpellMpCost refuses ids outside the action table",
          "[magic-costs]") {
  FakeHost host;
  magic::SetSpellMpCost(std::numeric_limits<int>::min(), host);
  magic::SetSpellMpCost(256, host);
  magic::SetSpellMpCost(-256, host);
  magic::SetSpellMpCost(7, host);
  CHECK(host.refusals.size() == 4);
  CHECK(host.cost_texts.empty());
}

TEST_CASE("Magic SetTeleportMpCost formats the cost and disabled value",
          "[magic-costs]") {
  FakeHost host;
  magic::SetTeleportMpCost(15, 7, host);
  CHECK(host.cost_texts == CostTexts{{1, "MP", 8, 7}, {0, "#15", 72, 7}});

  host.cost_texts.clear();
  magic::SetTeleportMpCost(0, 0, host);
  magic::SetTeleportMpCost(magic::kMaxFormattedMpCost, 1, host);
  CHECK(host.cost_texts == CostTexts{{1, "MP", 8, 0},
                                     {0, "#0", 72, 0},
                                     {1, "MP", 8, 1},
                                     {0, "#65535", 72, 1}});
  CHECK(host.refusals.empty());
}

TEST_CASE("Magic SetTeleportMpCost refuses costs it cannot format",
          "[magic-costs]") {
  FakeHost host;
  magic::SetTeleportMpCost(-1, 0, host);
  magic::SetTeleportMpCost(magic::kMaxFormattedMpCost + 1, 0, host);
  CHECK(host.refusals.size() == 2);
  CHECK(host.cost_texts.empty());
}

TEST_CASE("Magic TeleportMpCost sums intervals from the current room",
          "[magic-costs]") {
  const magic::SavePointTable& table = magic::OriginalSavePoints();
  const auto cost = [&](int target, int room) {
    const std::optional<int> value = magic::TeleportMpCost(target, room, table);
    REQUIRE(value.has_value());
    return value.value_or(-1);
  };
  CHECK(cost(5, kRoomOfSavePointFive) == 15);
  CHECK(cost(0, kRoomOfSavePointFive) == 35);
  CHECK(cost(10, kRoomOfSavePointFive) == 35);
  CHECK(cost(47, kRoomOfSavePointFive) == ExpectedCost(47, 5));
  CHECK(cost(48, kRoomOfSavePointFive) == 183);
  // The first matching entry wins: the unused entries all have room 0.
  CHECK(cost(35, 0) == 15);
  CHECK(cost(47, 0) == ExpectedCost(47, 35));
}

TEST_CASE("Magic TeleportMpCost handles unknown rooms and refuses targets",
          "[magic-costs]") {
  const magic::SavePointTable& table = magic::OriginalSavePoints();
  CHECK(magic::TeleportMpCost(47, kUnknownRoom, table) == std::optional{15});
  CHECK(magic::TeleportMpCost(0, kUnknownRoom, table) == std::optional{203});
  CHECK(magic::TeleportMpCost(48, kUnknownRoom, table) == std::optional{15});
  CHECK(magic::TeleportMpCost(0, -1, table) == std::optional{203});
  CHECK(magic::TeleportMpCost(0, 0x100CD, table) == std::optional{203});
  CHECK_FALSE(magic::TeleportMpCost(-1, kRoomOfSavePointFive, table));
  CHECK_FALSE(magic::TeleportMpCost(49, kRoomOfSavePointFive, table));
}

TEST_CASE("Magic save point words decode and pack the legacy destination",
          "[magic-costs]") {
  const magic::SavePoint first{
      .room_id = 0x1B, .location = 0x1C, .tile_x = 5, .tile_z = 3};
  CHECK(magic::DecodeSavePoint(0x0305001BU) == first);
  CHECK(magic::OriginalSavePoints()[0] == first);
  // The destination fields come from the word plus one, so a full room id
  // carries into the tile x field.
  CHECK(magic::DecodeSavePoint(0x1F1FFFFFU) ==
        magic::SavePoint{
            .room_id = 0xFFFF, .location = 0, .tile_x = 0, .tile_z = 0x1F});

  CHECK(magic::PackTeleportDestination({.save_point = 0,
                                        .location = 0x1C,
                                        .tile_x = 5,
                                        .tile_z = 3,
                                        .mp_cost = 15}) ==
        (0x1C | (5 << 9) | (3 << 14) | (15 << 19)));
  CHECK(magic::PackTeleportDestination({.save_point = 0,
                                        .location = 0x1FF,
                                        .tile_x = 0x1F,
                                        .tile_z = 0x1F,
                                        .mp_cost = 0xFFFF}) == -1);

  // The battle flow reads the word back unsigned: a cost reaching bit 31
  // must not come back negative.
  CHECK(magic::UnpackTeleportDestination(
            magic::PackTeleportDestination({.save_point = 4,
                                            .location = 0x1A5,
                                            .tile_x = 0x11,
                                            .tile_z = 0x1D,
                                            .mp_cost = 0x1ABC})) ==
        magic::TeleportDestination{.save_point = 0,
                                   .location = 0x1A5,
                                   .tile_x = 0x11,
                                   .tile_z = 0x1D,
                                   .mp_cost = 0x1ABC});
  CHECK(magic::UnpackTeleportDestination(-1) ==
        magic::TeleportDestination{.save_point = 0,
                                   .location = 0x1FF,
                                   .tile_x = 0x1F,
                                   .tile_z = 0x1F,
                                   .mp_cost = 0x1FFF});
}

TEST_CASE("Magic UpdateTeleportMenu init flies rows out and captures MP",
          "[magic-costs]") {
  FakeHost host;
  magic::TeleportMenuState state;
  state.step = magic::TeleportStep::kReturnIfReady;
  const magic::TeleportMenuResult result =
      magic::UpdateTeleportMenu(1, state, host);
  CHECK(result.selection == 0);
  CHECK_FALSE(result.destination);
  REQUIRE(host.fly_outs.size() == 1);
  CHECK(host.fly_outs[0] == std::pair{4, 1});
  CHECK(state.step == magic::TeleportStep::kInit);
  CHECK(state.current_mp == 30);

  host.mp = std::nullopt;
  (void)magic::UpdateTeleportMenu(-3, state, host);
  CHECK(state.current_mp == 0);
  CHECK(host.refusals.size() == 1);
}

namespace {
// Discovers save points 3, 5 (the current one) and 10.
void DiscoverThreeSavePoints(FakeHost& host) {
  host.states[3] = 1;
  host.states[5] = 2;
  host.states[10] = 1;
  host.strings[3 + 3 * 2] = 11;
  host.strings[3 + 3 * 2 + 1] = 12;
  host.strings[3 + 5 * 2] = 13;
}
}  // namespace

TEST_CASE("Magic UpdateTeleportMenu lists discovered save points",
          "[magic-costs]") {
  FakeHost host;
  DiscoverThreeSavePoints(host);
  (void)OpenedState(host);
  REQUIRE(host.created);
  const CreatedRows created = host.created.value_or(CreatedRows{});
  CHECK(created.row_info == 0x20B);
  CHECK(created.cursor_memory_setting == 1);
  CHECK(host.cursor_setting == 3);
  // Row 1 is the current save point and disabled: the disabled description,
  // type 1 | 4. Row 2 costs more than the 30 MP available.
  CHECK(created.rows == std::vector<magic::TeleportRow>{
                            {11, 12, 0}, {13, 144, 5}, {123, 124, 1}});
}

TEST_CASE("Magic UpdateTeleportMenu records listed costs and save points",
          "[magic-costs]") {
  FakeHost host;
  DiscoverThreeSavePoints(host);
  const magic::TeleportMenuState state = OpenedState(host);
  CHECK(std::vector<std::uint16_t>(state.mp_costs.begin(),
                                   state.mp_costs.begin() + 3) ==
        std::vector<std::uint16_t>{23, 15, 35});
  CHECK(std::vector<std::uint8_t>(state.save_points.begin(),
                                  state.save_points.begin() + 3) ==
        std::vector<std::uint8_t>{3, 5, 10});
  CHECK(host.cursor_memory == std::pair<std::uint8_t, std::uint8_t>{1, 0});
  CHECK(state.step == magic::TeleportStep::kHandleInput);
}

TEST_CASE("Magic UpdateTeleportMenu defaults the cursor when no row costs 15",
          "[magic-costs]") {
  FakeHost host;
  host.room = kUnknownRoom;
  DiscoverThreeSavePoints(host);
  const magic::TeleportMenuState state = OpenedState(host);
  REQUIRE(host.created);
  CHECK(RowTypes(host).size() == 3);
  CHECK(std::ranges::none_of(
      RowTypes(host), [](std::int32_t type) { return (type & 4) != 0; }));
  CHECK(std::ranges::none_of(state.mp_costs.begin(), state.mp_costs.begin() + 3,
                             [](std::uint16_t cost) { return cost == 15; }));
  CHECK(host.cursor_memory == std::pair<std::uint8_t, std::uint8_t>{0, 0});
  CHECK(host.refusals.empty());
}

TEST_CASE("Magic UpdateTeleportMenu disables rows under action restrictions",
          "[magic-costs]") {
  FakeHost host;
  host.states[5] = 1;
  host.states[6] = 1;
  host.action_restrictions = 0x48;
  OpenedState(host);
  REQUIRE(host.created);
  CHECK(RowTypes(host) == std::vector<std::int32_t>{4, 0});

  FakeHost restricted;
  restricted.states[5] = 1;
  restricted.states[6] = 1;
  restricted.action_restrictions = 0x80;
  OpenedState(restricted);
  REQUIRE(restricted.created);
  CHECK(RowTypes(restricted) == std::vector<std::int32_t>{5, 1});
}

TEST_CASE("Magic UpdateTeleportMenu waits for idle rows and opens empty lists",
          "[magic-costs]") {
  FakeHost host;
  host.rows_idle = false;
  magic::TeleportMenuState state;
  (void)magic::UpdateTeleportMenu(1, state, host);
  (void)magic::UpdateTeleportMenu(0, state, host);
  CHECK_FALSE(host.created);
  CHECK(state.step == magic::TeleportStep::kInit);

  host.rows_idle = true;
  (void)magic::UpdateTeleportMenu(0, state, host);
  REQUIRE(host.created);
  CHECK(RowTypes(host).empty());
  CHECK(host.cursor_memory == std::pair<std::uint8_t, std::uint8_t>{0, 0});
  CHECK(state.step == magic::TeleportStep::kHandleInput);
}

TEST_CASE("Magic UpdateTeleportMenu scrolls long lists to the current point",
          "[magic-costs]") {
  struct Layout {
    int row_count;
    int current;
    std::uint8_t row;
    std::uint8_t scroll;
  };
  const std::array<Layout, 7> layouts{{
      {8, 7, 7, 0},
      {9, 7, 7, 0},
      {9, 8, 7, 1},
      {20, 12, 3, 9},
      {20, 13, 1, 12},
      {20, 19, 7, 12},
      {20, 10, 3, 7},
  }};
  for (const Layout& layout : layouts) {
    FakeHost host;
    host.room = static_cast<int>(
        magic::OriginalSavePoints()[static_cast<std::size_t>(layout.current)]
            .room_id);
    std::fill_n(host.states.begin(), layout.row_count, std::uint8_t{1});
    OpenedState(host);
    INFO("rows " << layout.row_count << " current " << layout.current);
    CHECK(host.cursor_memory == std::pair{layout.row, layout.scroll});
  }
}

TEST_CASE("Magic UpdateTeleportMenu previews the highlighted cost",
          "[magic-costs]") {
  FakeHost host;
  host.states[3] = 1;
  host.states[10] = 1;
  host.mp = 30;
  magic::TeleportMenuState state = OpenedState(host);
  host.highlighted_row = 1;
  CHECK(magic::UpdateTeleportMenu(0, state, host).selection == 0);
  REQUIRE(host.cost_texts.size() == 2);
  CHECK(host.cost_texts[1].text == "#35");
  CHECK(host.cost_texts[1].disabled == 1);
  CHECK(state.step == magic::TeleportStep::kHandleInput);

  host.cost_texts.clear();
  host.highlighted_row = 48;
  (void)magic::UpdateTeleportMenu(0, state, host);
  CHECK(host.cost_texts.empty());
  CHECK(host.refusals.size() == 1);
}

TEST_CASE("Magic UpdateTeleportMenu closes the list on a selection",
          "[magic-costs]") {
  FakeHost host;
  host.states[3] = 1;
  host.states[10] = 1;
  magic::TeleportMenuState state = OpenedState(host);
  host.selected_row = 1;
  CHECK(magic::UpdateTeleportMenu(0, state, host).selection == 0);
  CHECK(host.display_action_cost == 0);
  CHECK(host.cleared == std::vector<int>{40});
  CHECK(host.dismissed == 1);
  CHECK(host.commands == std::vector{framework::MenuCommand::kNone});
  CHECK(state.step == magic::TeleportStep::kReturnIfReady);
}

TEST_CASE("Magic UpdateTeleportMenu confirms a destination", "[magic-costs]") {
  FakeHost host;
  host.states[3] = 1;
  host.states[10] = 1;
  magic::TeleportMenuState state = OpenedState(host);
  host.selected_row = 1;
  (void)magic::UpdateTeleportMenu(0, state, host);

  host.rows_idle = false;
  CHECK(magic::UpdateTeleportMenu(0, state, host).selection == 0);
  host.rows_idle = true;
  const magic::TeleportMenuResult result =
      magic::UpdateTeleportMenu(0, state, host);
  CHECK(result.selection == 2);
  const magic::SavePoint& target = magic::OriginalSavePoints()[10];
  CHECK(result.destination ==
        std::optional{magic::TeleportDestination{.save_point = 10,
                                                 .location = target.location,
                                                 .tile_x = target.tile_x,
                                                 .tile_z = target.tile_z,
                                                 .mp_cost = 35}});
}

TEST_CASE("Magic UpdateTeleportMenu returns the back sentinel",
          "[magic-costs]") {
  FakeHost host;
  host.states[3] = 1;
  magic::TeleportMenuState state = OpenedState(host);
  host.selected_row = -2;
  (void)magic::UpdateTeleportMenu(0, state, host);
  CHECK(host.cleared == std::vector<int>{0});
  CHECK(host.dismissed == 0);
  CHECK(host.commands.empty());
  const magic::TeleportMenuResult back =
      magic::UpdateTeleportMenu(0, state, host);
  CHECK(back.selection == -1);
  CHECK_FALSE(back.destination);
}

TEST_CASE("Magic UpdateTeleportMenu returns quit and overflowed selections",
          "[magic-costs]") {
  FakeHost host;
  host.states[3] = 1;
  magic::TeleportMenuState state = OpenedState(host);
  host.selected_row = -3;
  (void)magic::UpdateTeleportMenu(0, state, host);
  CHECK(host.dismissed == 1);
  CHECK(magic::UpdateTeleportMenu(0, state, host).selection == -2);

  state.step = magic::TeleportStep::kHandleInput;
  host.selected_row = std::numeric_limits<int>::max();
  (void)magic::UpdateTeleportMenu(0, state, host);
  CHECK(magic::UpdateTeleportMenu(0, state, host).selection ==
        std::numeric_limits<int>::min());

  state.step = magic::TeleportStep::kHandleInput;
  host.selected_row = 48;
  (void)magic::UpdateTeleportMenu(0, state, host);
  CHECK(magic::UpdateTeleportMenu(0, state, host).selection == -1);
  CHECK(host.refusals.size() == 1);
}

TEST_CASE("Magic CreateHeader creates the title row and information box",
          "[magic-costs]") {
  FakeHost host;
  magic::CreateHeader(host);
  CHECK(host.menu_item == std::optional{magic::MenuItem{
                              .row = 0,
                              .x = 320,
                              .y = 18,
                              .width = 0x8C,
                              .background_width = 8,
                              .text_offset = 9,
                              .state = 2,
                              .target_position = 180,
                              .selected = 1,
                          }});
  CHECK(host.commands == std::vector{framework::MenuCommand::kMenu});
  CHECK(host.information_boxes == 1);
  CHECK(host.refusals.empty());
}

TEST_CASE("Magic CreateHeader refuses a label outside the menu strings",
          "[magic-costs]") {
  FakeHost broken;
  broken.battle_strings[2] = 16;
  magic::CreateHeader(broken);
  CHECK_FALSE(broken.menu_item);
  CHECK(broken.commands.empty());
  CHECK(broken.information_boxes == 0);
  CHECK(broken.refusals.size() == 1);
}

namespace {
struct TeleportLiveFixture {
  VS::Input::Settings saved_settings = VS::Input::settings;
  VS::Menu::Framework::MenuMemory saved_memory =
      VS::Menu::Framework::menuMemory;
  VS::Menu::Framework::MenuRow* saved_items = VS::Menu::Framework::menuItems;
  u_short* saved_text = VS::Menu::Framework::itemsText;
  int saved_selected = VS::Menu::Framework::selectedRow;
  int saved_restrictions = VS::Menu::Framework::actionRestrictions;
  char saved_display_cost = VS::Menu::Framework::displayActionCost;
  Battle_Actor* saved_character = Battle_characterState;
  std::array<char, 48> saved_states{};
  std::array<std::uint16_t, magic::kTeleportationStringWords> saved_strings{};
  alignas(VS::Core::Memory::kHeapUnitBytes)
      std::array<std::byte, VS::Core::Memory::kHeapUnitBytes * 768> arena{};
  VS::Core::Memory::ScopedLiveGameHeap heap{arena};
  std::array<VS::Menu::Framework::MenuRow, 40> rows{};
  Battle_Actor actor{};
  Battle_ActorStats stats{};

  TeleportLiveFixture() {
    std::copy_n(VS::State::stateFlags.save_point_states, 48,
                saved_states.begin());
    std::ranges::copy(magic::teleportationStrings, saved_strings.begin());
    std::fill_n(VS::State::stateFlags.save_point_states, 48, char{0});
    std::ranges::fill(magic::teleportationStrings, std::uint16_t{0});
    VS::Menu::Framework::menuItems = rows.data();
    VS::Menu::Framework::itemsText = nullptr;
    VS::Menu::Framework::menuMemory = {};
    VS::Menu::Framework::actionRestrictions = 0;
    VS::Input::settings.menu_flags = 0x3FF;
    VS::Input::settings.cursor_memory = 0;
    Battle_characterState = &actor;
    actor.stats = &stats;
    stats.currentMp = 500;
    VS::Menu::Magic::ResetTeleportMenuLiveState();
  }

  ~TeleportLiveFixture() {
    VS::Menu::Magic::ResetTeleportMenuLiveState();
    if (VS::Menu::Framework::itemsText != nullptr) {
      VS::Core::Memory::FreeLiveFromHighEnd(VS::Menu::Framework::itemsText);
    }
    std::copy(saved_states.begin(), saved_states.end(),
              VS::State::stateFlags.save_point_states);
    std::copy(saved_strings.begin(), saved_strings.end(),
              magic::teleportationStrings);
    VS::Input::settings = saved_settings;
    VS::Menu::Framework::menuMemory = saved_memory;
    VS::Menu::Framework::menuItems = saved_items;
    VS::Menu::Framework::itemsText = saved_text;
    VS::Menu::Framework::selectedRow = saved_selected;
    VS::Menu::Framework::actionRestrictions = saved_restrictions;
    VS::Menu::Framework::displayActionCost = saved_display_cost;
    Battle_characterState = saved_character;
  }

  TeleportLiveFixture(const TeleportLiveFixture&) = delete;
  TeleportLiveFixture& operator=(const TeleportLiveFixture&) = delete;

  void FinishRowAnimations() {
    for (auto& row : rows) row.state = VS::Menu::Framework::RowState::kIdle;
  }
};
}  // namespace

TEST_CASE("Magic SetSpellMpCostLive refuses invalid ids without cost text",
          "[magic-costs-live]") {
  const char saved = VS::Menu::Framework::displayActionCost;
  Battle_Actor* const saved_character = Battle_characterState;
  VS::Menu::Framework::displayActionCost = 0;
  magic::SetSpellMpCostLive(std::numeric_limits<int>::min());
  magic::SetSpellMpCostLive(256);
  magic::SetSpellMpCostLive(-256);
  Battle_characterState = nullptr;
  magic::SetSpellMpCostLive(1);
  CHECK(VS::Menu::Framework::displayActionCost == 0);
  Battle_characterState = saved_character;
  VS::Menu::Framework::displayActionCost = saved;
}

TEST_CASE("Magic SetTeleportMpCostLive publishes only formattable costs",
          "[magic-costs-live]") {
  const char saved = VS::Menu::Framework::displayActionCost;
  VS::Menu::Framework::displayActionCost = 0;
  magic::SetTeleportMpCostLive(-1, 0);
  magic::SetTeleportMpCostLive(0x10000, 0);
  CHECK(VS::Menu::Framework::displayActionCost == 0);
  magic::SetTeleportMpCostLive(15, 1);
  CHECK(VS::Menu::Framework::displayActionCost == 1);
  VS::Menu::Framework::displayActionCost = saved;
}

TEST_CASE("Magic TeleportMpCostLive refuses targets outside the table",
          "[magic-costs-live]") {
  CHECK(magic::TeleportMpCostLive(-1) == 0);
  CHECK(magic::TeleportMpCostLive(49) == 0);
  const int cost = magic::TeleportMpCostLive(48);
  CHECK(cost >= magic::kTeleportBaseMpCost);
  CHECK(cost <= magic::kTeleportBaseMpCost + 4 * 47);
}

TEST_CASE("Magic UpdateTeleportMenuLive packs the confirmed destination",
          "[magic-costs-live]") {
  TeleportLiveFixture fixture;
  VS::State::stateFlags.save_point_states[3] = 1;
  VS::State::stateFlags.save_point_states[10] = 2;
  VS::Input::settings.cursor_memory = 0;
  CHECK(magic::UpdateTeleportMenuLive(1) == 0);
  fixture.FinishRowAnimations();
  CHECK(magic::UpdateTeleportMenuLive(0) == 0);
  CHECK(VS::Input::settings.cursor_memory == 0);

  VS::Menu::Framework::selectedRow = 1;
  VS::Menu::Framework::CloseLiveRowsAndRememberCursor();
  CHECK(magic::UpdateTeleportMenuLive(0) == 0);
  CHECK(VS::Menu::Framework::displayActionCost == 0);
  fixture.FinishRowAnimations();
  const int packed = magic::UpdateTeleportMenuLive(0);
  const magic::SavePoint& target = magic::OriginalSavePoints()[10];
  const int cost = magic::TeleportMpCostLive(10);
  CHECK(packed == magic::PackTeleportDestination(
                      {.save_point = 10,
                       .location = target.location,
                       .tile_x = target.tile_x,
                       .tile_z = target.tile_z,
                       .mp_cost = static_cast<std::uint16_t>(cost)}));
}
