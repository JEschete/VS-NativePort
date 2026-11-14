#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Menu/BreakArts.hpp"

namespace {
namespace break_arts = VS::Menu::break_arts;

struct CostText {
  int index = 0;
  std::string text;
  int x = 0;
  int disabled = 0;
};

struct CreatedRows {
  int row_info = 0;
  std::vector<break_arts::Row> rows;
  std::uint8_t cursor_memory = 0;
};

class FakeHost final : public break_arts::Host {
 public:
  std::vector<std::uint16_t> menu_strings =
      std::vector<std::uint16_t>(break_arts::kMenuStringHalfwords);
  std::vector<std::uint16_t> battle_strings = std::vector<std::uint16_t>(32);
  std::array<std::uint8_t, 256> costs{};
  std::array<std::uint32_t, 256> restrictions{};
  std::array<bool, 256> unlocked{};
  std::array<int, 12> learned{};
  std::array<int, 10> kills{};
  std::array<std::array<int, 4>, 15> requirements{};
  std::array<std::uint8_t, 11> animation_indices{};
  std::array<std::int8_t, 36> row_animation_steps{};
  std::optional<int> equipped = 1;
  std::uint16_t art_description_index = 40;
  std::uint16_t view_name_offset = 400;
  std::uint16_t view_description_offset = 401;
  std::uint16_t header_index = 5;
  std::int32_t dagger_art_one = 184;
  std::uint8_t cursor_memory = 3;
  std::uint32_t action_restrictions = 0;
  bool shortcut = false;
  bool rows_idle = true;
  bool menu_item_succeeds = true;
  bool rows_succeed = true;
  int selected_row = -1;
  int highlighted_row = 0;
  int display_action_cost = 1;
  std::uint8_t current_menu = 7;
  std::uint8_t return_state = 4;
  std::uint8_t execute_ability_type = 0;
  std::int16_t selection_kind = 0;
  std::uint16_t selected_action = 0;
  int information_initializations = 0;
  std::vector<std::string> refusals;
  std::vector<std::pair<break_arts::RowPlacement, break_arts::TextRef>>
      menu_items;
  std::vector<CreatedRows> created_rows;
  std::vector<CostText> cost_text;
  std::vector<std::pair<std::string, std::uint32_t>> drawings;
  std::vector<int> cleared_rows;
  std::vector<int> command_labels;
  int information_dismissals = 0;

  FakeHost() {
    for (std::size_t index = 0; index < menu_strings.size(); ++index) {
      menu_strings[index] = static_cast<std::uint16_t>(500 + index);
    }
    for (std::size_t category = 0; category < break_arts::kCategoryCount;
         ++category) {
      for (std::size_t part = 0; part < 3; ++part) {
        menu_strings[category * 3 + part] =
            static_cast<std::uint16_t>(100 + category * 3 + part);
      }
    }
    for (std::size_t index = 0; index < 40; ++index) {
      menu_strings[art_description_index + index] =
          static_cast<std::uint16_t>(200 + index);
    }
    for (std::size_t index = 0; index < animation_indices.size(); ++index) {
      animation_indices[index] = static_cast<std::uint8_t>(index);
    }
    battle_strings[header_index] = 15;
  }

  void Refuse(std::string_view reason) override {
    refusals.emplace_back(reason);
  }
  std::span<const std::uint16_t> MenuStrings() override { return menu_strings; }
  std::span<const std::uint16_t> BattleMenuStrings() override {
    return battle_strings;
  }
  std::uint16_t DaggerArtDescriptionIndex() const override {
    return art_description_index;
  }
  std::uint16_t ViewArtsNameOffset() const override { return view_name_offset; }
  std::uint16_t ViewArtsDescriptionOffset() const override {
    return view_description_offset;
  }
  std::uint16_t BreakArtsHeaderIndex() const override { return header_index; }
  std::int32_t DaggerArtOneId() const override { return dagger_art_one; }
  std::optional<std::uint8_t> ActionCost(int action_id) override {
    if (action_id < 0 || action_id >= static_cast<int>(costs.size())) {
      return std::nullopt;
    }
    return costs[static_cast<std::size_t>(action_id)];
  }
  std::optional<std::uint32_t> ActionRestrictionFlags(int action_id) override {
    if (action_id < 0 || action_id >= static_cast<int>(restrictions.size())) {
      return std::nullopt;
    }
    return restrictions[static_cast<std::size_t>(action_id)];
  }
  std::optional<bool> ActionUnlocked(int action_id) override {
    if (action_id < 0 || action_id >= static_cast<int>(unlocked.size())) {
      return std::nullopt;
    }
    return unlocked[static_cast<std::size_t>(action_id)];
  }
  bool ActionNameAvailable(int action_id) override {
    return action_id >= 0 && action_id < static_cast<int>(unlocked.size());
  }
  std::optional<int> EquippedWeaponCategory() override { return equipped; }
  std::optional<int> ArtsLearned(int category) override {
    if (category < 0 || category >= static_cast<int>(learned.size())) {
      return std::nullopt;
    }
    return learned[static_cast<std::size_t>(category)];
  }
  std::optional<int> CategoryKills(int category) override {
    if (category < 0 || category >= static_cast<int>(kills.size())) {
      return std::nullopt;
    }
    return kills[static_cast<std::size_t>(category)];
  }
  std::optional<int> PointRequirement(int category, int art_index) override {
    if (category < 0 || category >= static_cast<int>(requirements.size()) ||
        art_index < 0 ||
        art_index >= static_cast<int>(requirements[0].size())) {
      return std::nullopt;
    }
    return requirements[static_cast<std::size_t>(category)]
                       [static_cast<std::size_t>(art_index)];
  }
  std::uint8_t CursorMemory() override { return cursor_memory; }
  void SetCursorMemory(std::uint8_t value) override { cursor_memory = value; }
  std::uint32_t ActionRestrictions() override { return action_restrictions; }
  bool ShortcutInvoked() override { return shortcut; }
  bool RowsIdle() override { return rows_idle; }
  int SelectedRow() override { return selected_row; }
  int HighlightedRow() override { return highlighted_row; }
  std::span<const std::uint8_t> AnimationIndices() override {
    return animation_indices;
  }
  std::span<const std::int8_t> RowAnimationSteps() override {
    return row_animation_steps;
  }
  bool SetMenuItem(const break_arts::RowPlacement& placement,
                   break_arts::TextRef text) override {
    menu_items.emplace_back(placement, text);
    return menu_item_succeeds;
  }
  bool CreateRows(int row_info,
                  std::span<const break_arts::Row> rows) override {
    created_rows.push_back({.row_info = row_info,
                            .rows = {rows.begin(), rows.end()},
                            .cursor_memory = cursor_memory});
    return rows_succeed;
  }
  void SetActionCostText(int index, std::string_view text, int x,
                         int disabled) override {
    cost_text.push_back({.index = index,
                         .text = std::string(text),
                         .x = x,
                         .disabled = disabled});
  }
  void DrawCompactText(std::string_view text, std::uint32_t position) override {
    drawings.emplace_back(text, position);
  }
  void SetDisplayActionCost(int enabled) override {
    display_action_cost = enabled;
  }
  void ClearRowsExcept(int row) override { cleared_rows.push_back(row); }
  void DismissInformationBox() override { ++information_dismissals; }
  void InitializeInformationBox() override { ++information_initializations; }
  void SetCommandLabel(int command) override {
    command_labels.push_back(command);
  }
  void SetActionSelection(std::uint16_t action_id) override {
    selected_action = action_id;
  }
  void SetExecuteAbilityType(std::uint8_t type) override {
    execute_ability_type = type;
  }
  std::uint8_t CurrentMenu() override { return current_menu; }
  void SetCurrentMenu(std::uint8_t menu) override { current_menu = menu; }
  void SetReturnState(std::uint8_t state) override { return_state = state; }
  void SetSelectionKind(std::int16_t kind) override { selection_kind = kind; }
};

}  // namespace

TEST_CASE("Break Arts SetHpCost formats costs and restrictions",
          "[menu-break-arts]") {
  FakeHost host;
  host.costs[184] = 10;
  host.restrictions[184] = 2;
  break_arts::SetHpCost(184, host);
  REQUIRE(host.cost_text.size() == 2);
  CHECK(host.cost_text[0].text == "HP");
  CHECK(host.cost_text[0].x == 8);
  CHECK(host.cost_text[0].disabled == 1);
  CHECK(host.cost_text[1].text == "#10");
  CHECK(host.cost_text[1].x == 72);
  CHECK(host.cost_text[1].disabled == 1);
  break_arts::SetHpCost(-1, host);
  CHECK(host.refusals.size() == 1);
}

TEST_CASE("Break Arts DrawNextUnlockPoints uses category progress",
          "[menu-break-arts]") {
  FakeHost host;
  host.learned[0] = 2;
  host.kills[0] = 17;
  host.requirements[0][2] = 25;
  break_arts::DrawNextUnlockPoints(10, 10, 2, host);
  REQUIRE(host.drawings.size() == 2);
  CHECK((host.drawings[0] ==
         std::pair<std::string, std::uint32_t>{"NEXT", 0x005200D8}));
  CHECK((host.drawings[1] ==
         std::pair<std::string, std::uint32_t>{"#8PT", 0x00520138}));
  host.drawings.clear();
  host.learned[0] = 4;
  break_arts::DrawNextUnlockPoints(10, 10, 2, host);
  CHECK(host.drawings.empty());
  break_arts::DrawNextUnlockPoints(10, 11, 2, host);
  CHECK(host.refusals.size() == 1);
}

TEST_CASE("Break Arts UpdateArtList initializes its title",
          "[menu-break-arts]") {
  FakeHost host;
  break_arts::State state{};
  break_arts::Context context{.state = state, .host = host};
  CHECK(break_arts::UpdateArtList(0x11, context) == 0);
  REQUIRE(host.menu_items.size() == 1);
  CHECK(host.menu_items[0].first.row == 10);
  CHECK(host.menu_items[0].second.value == 100);
  CHECK(state.art_list.force_cursor_memory == 1);
}

TEST_CASE("Break Arts UpdateArtList creates action and view rows",
          "[menu-break-arts]") {
  FakeHost host;
  host.unlocked[184] = true;
  host.unlocked[185] = true;
  break_arts::State state{};
  state.art_list.step = break_arts::ArtListStep::kInit;
  state.art_list.weapon_type = 1;
  state.art_list.force_cursor_memory = 1;
  break_arts::Context context{.state = state, .host = host};
  host.cursor_memory = 3;
  CHECK(break_arts::UpdateArtList(0, context) == 0);
  REQUIRE(host.created_rows.size() == 1);
  CHECK(host.created_rows[0].row_info == 0x20C);
  CHECK(host.created_rows[0].rows.size() == 3);
  CHECK((host.created_rows[0].rows[0].name ==
         break_arts::TextRef{break_arts::TextKind::kActionName, 184}));
  CHECK(host.created_rows[0].rows[2].name.value == 400);
  CHECK(host.created_rows[0].cursor_memory == 1);
  CHECK(host.cursor_memory == 3);
}

TEST_CASE("Break Arts UpdateArtList confirms and returns a row",
          "[menu-break-arts]") {
  FakeHost host;
  host.costs[184] = 7;
  break_arts::State state{};
  state.art_list.step = break_arts::ArtListStep::kWaitForSelection;
  state.art_list.weapon_type = 1;
  state.art_list.row_count = 2;
  state.art_list.remaining_rows = 1;
  state.art_list.action_ids[0] = 184;
  state.art_list.action_ids[1] = 0xFFFF;
  break_arts::Context context{.state = state, .host = host};
  host.selected_row = -1;
  CHECK(break_arts::UpdateArtList(0, context) == 0);
  CHECK(host.cost_text[1].text == "#7");
  host.selected_row = 1;
  CHECK(break_arts::UpdateArtList(0, context) == 0);
  CHECK(host.cleared_rows.back() == 1);
  CHECK(break_arts::UpdateArtList(0, context) == 0xFFFF);
}

TEST_CASE("Break Arts UpdateWeaponCategories initializes its title",
          "[menu-break-arts]") {
  FakeHost host;
  break_arts::State state{};
  break_arts::Context context{.state = state, .host = host};
  CHECK(break_arts::UpdateWeaponCategories(1, context) == 0);
  REQUIRE(host.menu_items.size() == 1);
  CHECK(host.menu_items[0].second.value == host.view_name_offset);
  CHECK(state.categories.step == break_arts::CategoryStep::kInit);
}

TEST_CASE("Break Arts UpdateWeaponCategories builds locked and equipped rows",
          "[menu-break-arts]") {
  FakeHost host;
  host.unlocked[184] = true;
  break_arts::State state{};
  break_arts::Context context{.state = state, .host = host};
  CHECK(break_arts::UpdateWeaponCategories(0, context) == 0);
  REQUIRE(host.created_rows.size() == 1);
  CHECK(host.created_rows[0].rows.size() == 10);
  CHECK(host.created_rows[0].row_info == 0x216);
  CHECK(host.created_rows[0].rows[0].type == 0x04000004);
  CHECK(host.created_rows[0].rows[1].type == 0x08000001);
  CHECK(host.created_rows[0].rows[1].description.value == 105);
}

TEST_CASE("Break Arts CreateHeader creates the title and information box",
          "[menu-break-arts]") {
  FakeHost host;
  break_arts::CreateHeader(host);
  REQUIRE(host.menu_items.size() == 1);
  CHECK(host.menu_items[0].first.row == 1);
  CHECK(host.menu_items[0].first.x == 320);
  CHECK(host.menu_items[0].first.y == 18);
  CHECK((host.menu_items[0].second ==
         break_arts::TextRef{break_arts::TextKind::kBattleMenuIndex, 5}));
  CHECK(host.command_labels == std::vector<int>{2});
  CHECK(host.information_initializations == 1);
}

TEST_CASE("Break Arts Update transitions into art execution",
          "[menu-break-arts]") {
  FakeHost host;
  break_arts::State state{};
  break_arts::Context context{.state = state, .host = host};
  std::uint8_t step = static_cast<std::uint8_t>(break_arts::ScreenStep::kInit);
  CHECK(break_arts::Update(context, step) == 0);
  CHECK(step == static_cast<std::uint8_t>(break_arts::ScreenStep::kArtsMenu));
  CHECK(state.update_weapon_type == 1);
  CHECK(host.information_initializations == 1);
  step = static_cast<std::uint8_t>(break_arts::ScreenStep::kExecuteArt);
  CHECK(break_arts::Update(context, step) == 1);
  CHECK(step == static_cast<std::uint8_t>(break_arts::ScreenStep::kNone));
  CHECK(host.selection_kind == 6);
  CHECK(host.return_state == 7);
  CHECK(host.current_menu == 2);
}
