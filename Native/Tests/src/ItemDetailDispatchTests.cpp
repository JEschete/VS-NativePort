#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Input/Pad.hpp"
#include "Menu/ItemDetailDispatchWorld.hpp"

namespace {
namespace inv = VS::Menu::Inventory;
using Category = VS::Items::ItemCategory;
using Calls = std::vector<std::string>;
using State = inv::EquipmentDetailState;
using Stage = inv::DetailStage;
using item_detail_tests::DispatchWorld;
using item_detail_tests::IconWorld;
using item_detail_tests::ItemWorld;
constexpr inv::DetailSelection kEntry{inv::DetailRowId{20}, inv::DetailPage{1}};
}  // namespace

TEST_CASE("F02568 merges the selected icon exactly at the header inset",
          "[item-detail-dispatch][F02568]") {
  IconWorld world;
  world.header.x = 60;
  inv::DrawItemCategoryIcons(world, 2, 1);
  CHECK(world.icons.size() == 7);
  CHECK(world.header.row_icon == 24);
  CHECK(world.header_reads == 1);
  CHECK(world.tab_draws == 0);
  CHECK(world.icons.front().x == 64);
}

TEST_CASE("F02568 keeps the selected icon on the overlay before it can merge",
          "[item-detail-dispatch][F02568]") {
  IconWorld world;
  world.header.x = 61;
  inv::DrawItemCategoryIcons(world, 0, 1);
  REQUIRE(world.icons.size() == 8);
  CHECK(world.icons.front() == inv::CategoryIcon{0, 48, 16, true, true});
  CHECK(world.icons.back() == inv::CategoryIcon{7, 32, -16, false, false});
}

TEST_CASE(
    "F02568 caches category before tabs but obtains the current row after",
    "[item-detail-dispatch][F02568]") {
  IconWorld world;
  world.tabs_callback = [&] {
    world.category = 3;
    world.header.x = 60;
  };
  inv::DrawItemCategoryIcons(world, 4, 1);
  CHECK(world.tab_draws == 1);
  CHECK(world.header.row_icon == 24);
  CHECK(world.icons.size() == 7);
}

TEST_CASE("F02568 masks the category and preserves word wrapping without rows",
          "[item-detail-dispatch][F02568]") {
  IconWorld world;
  world.category = 0;
  inv::DrawItemCategoryIcons(world, std::numeric_limits<std::int32_t>::max(),
                             2);
  REQUIRE(world.icons.size() == 8);
  CHECK(world.header_reads == 0);
  CHECK(world.icons.back().selected_palette);
  CHECK(world.icons.back().y == -24);
  CHECK_FALSE(world.icons.back().overlay);
}

TEST_CASE(
    "F02579 entry stores byte selections before transition and resets itself",
    "[item-detail-dispatch][F02579]") {
  ItemWorld world;
  State state{Stage::kReturn, 255, 31, 0, 65};
  world.callback = [&](std::string_view call) {
    if (call == "begin") {
      CHECK(state.stage == Stage::kReturn);
      CHECK(state.selected_page == 1);
    }
  };
  CHECK(inv::UpdateArmorDetails(state, world, kEntry) == 0);
  CHECK(state.stage == Stage::kInitialize);
  CHECK(state.animation_step == 10);
  CHECK(state.selected_row == 20);
  CHECK(state.unused_entry_byte == 65);
}

TEST_CASE("F02579 zero readiness does not read unused records or pages",
          "[item-detail-dispatch][F02579]") {
  ItemWorld world;
  world.ready = 0;
  State state{Stage::kInitialize, 10, 255, 255};
  CHECK(inv::UpdateArmorDetails(state, world) == 0);
  CHECK(world.calls == Calls{"ready"});
  CHECK(world.selected_pages.empty());
  CHECK(state.stage == Stage::kInitialize);
}

TEST_CASE("F02579 reloads armor applicability after the statistics callback",
          "[item-detail-dispatch][F02579]") {
  ItemWorld world;
  State state{Stage::kInitialize, 10, 20, 0};
  world.armor_categories[1] = 7;
  world.callback = [&](std::string_view call) {
    if (call == "stats") {
      world.direct[0] = 2;
    }
  };
  CHECK(inv::UpdateArmorDetails(state, world) == 0);
  CHECK(world.calls == Calls{"ready", "layout", "stats", "armor-category"});
  CHECK(world.references == std::vector<int>{20, 7, 1, 2});
  CHECK(state.stage == Stage::kAnimate);
}

TEST_CASE("F02579 opens only DP and allows nonzero negative readiness",
          "[item-detail-dispatch][F02579]") {
  ItemWorld world;
  world.ready = -7;
  State state{Stage::kInitialize, 10, 31, 0};
  CHECK(inv::UpdateArmorDetails(state, world) == 0);
  CHECK(world.calls ==
        Calls{"ready", "layout", "stats", "armor-category", "dp-only"});
  CHECK(world.selected_pages == std::vector<int>{0, 0});
}

TEST_CASE("F02579 countdown consumes its zero frame before processing input",
          "[item-detail-dispatch][F02579]") {
  ItemWorld world;
  world.pressed = VS::Input::Triangle;
  State state{Stage::kAnimate, 1, 20, 0};
  CHECK(inv::UpdateArmorDetails(state, world) == 0);
  CHECK(state.animation_step == 0);
  CHECK(state.stage == Stage::kAnimate);
  CHECK(inv::UpdateArmorDetails(state, world) == 0);
  CHECK(state.stage == Stage::kInput);
  CHECK(world.calls.empty());
}

TEST_CASE("F02579 exit wins over paging and return preserves readiness",
          "[item-detail-dispatch][F02579]") {
  ItemWorld world;
  world.pressed = VS::Input::Cross | VS::Input::R1;
  State state{Stage::kInput, 0, 20, 0};
  CHECK(inv::UpdateArmorDetails(state, world) == 0);
  CHECK(world.calls == Calls{"exit"});
  CHECK(world.references == std::vector<int>{1});
  CHECK(state.stage == Stage::kReturn);
  world.ready = -7;
  CHECK(inv::UpdateArmorDetails(state, world) == -7);
}

TEST_CASE(
    "F02579 paging uses bound order and retains borrowed texts across stats",
    "[item-detail-dispatch][F02579]") {
  ItemWorld world;
  State state{Stage::kInput, 0, 20, 0};
  world.next_page = {1};
  world.bound[1] = 3;
  world.direct[1] = 9;
  world.callback = [&](std::string_view call) {
    if (call == "stats") {
      state.selected_row = 31;
      world.name[0] = 'a';
      world.description[0] = 'b';
    }
  };
  CHECK(inv::UpdateArmorDetails(state, world) == 0);
  CHECK(world.calls == Calls{"paging", "select", "format", "stats", "change"});
  CHECK(world.references == std::vector<int>{3, 3, 31, 1, 'a', 'b'});
  CHECK(state.selected_page == 1);
}

TEST_CASE(
    "F02580 initialization has affinity pages and no DP applicability read",
    "[item-detail-dispatch][F02580]") {
  ItemWorld world;
  State state{Stage::kInitialize, 10, 20, 1};
  world.direct[1] = 48;
  CHECK(inv::UpdateGemDetails(state, world) == 0);
  CHECK(world.calls == Calls{"ready", "layout", "stats"});
  CHECK(world.references == std::vector<int>{20, 3, 48});
  CHECK(world.selected_pages == std::vector<int>{1});
  CHECK(state.stage == Stage::kAnimate);
}

TEST_CASE("F02580 entry truncates both stored bytes and leaves unrelated state",
          "[item-detail-dispatch][F02580]") {
  ItemWorld world;
  State state{Stage::kReturn, 22, 1, 2, 73};
  CHECK(inv::UpdateGemDetails(state, world,
                              inv::DetailSelection{inv::DetailRowId{276},
                                                   inv::DetailPage{257}}) == 0);
  CHECK(state.selected_row == 20);
  CHECK(state.selected_page == 1);
  CHECK(state.animation_step == 10);
  CHECK(state.stage == Stage::kInitialize);
  CHECK(state.unused_entry_byte == 73);
}

TEST_CASE("F02580 wait is lazy and does not repoll input at its zero boundary",
          "[item-detail-dispatch][F02580]") {
  ItemWorld world;
  State state{Stage::kAnimate, 0, 255, 255};
  CHECK(inv::UpdateGemDetails(state, world) == 0);
  CHECK(state.stage == Stage::kInput);
  CHECK(world.calls.empty());
  CHECK(world.selected_pages.empty());
}

TEST_CASE(
    "F02580 samples battle intent again after exit callbacks and stage store",
    "[item-detail-dispatch][F02580]") {
  ItemWorld world;
  State state{Stage::kInput, 0, 20, 0};
  world.pressed = VS::Input::Cross;
  world.callback = [&](std::string_view call) {
    if (call == "exit") {
      world.pressed = VS::Input::Triangle;
    }
    if (call == "return-intent") {
      CHECK(state.stage == Stage::kReturn);
    }
  };
  CHECK(inv::UpdateGemDetails(state, world) == 0);
  CHECK(world.references == std::vector<int>{0});
  CHECK(world.return_intent == inv::BattleReturnIntent::kBattle);
  CHECK(world.calls == Calls{"exit", "return-intent"});
}

TEST_CASE("F02580 circle does not exit and a stable page does not format",
          "[item-detail-dispatch][F02580]") {
  ItemWorld world;
  State state{Stage::kInput, 0, 20, 0};
  world.pressed = VS::Input::Circle;
  CHECK(inv::UpdateGemDetails(state, world) == 0);
  CHECK(world.calls == Calls{"paging"});
  CHECK(state.stage == Stage::kInput);
}

TEST_CASE(
    "F02580 paging retains the selected slot through formatting and stats",
    "[item-detail-dispatch][F02580]") {
  ItemWorld world;
  State state{Stage::kInput, 0, 20, 0};
  world.next_page = {47};
  world.bound[47] = 48;
  world.callback = [&](std::string_view call) {
    if (call == "format") {
      world.bound[47] = 2;
    }
  };
  CHECK(inv::UpdateGemDetails(state, world) == 0);
  CHECK(world.references == std::vector<int>{48, 48, 20, 47, 'n', 'd'});
  CHECK(state.selected_page == 47);
}

TEST_CASE("F02580 return remains until a nonzero new selection reinitializes",
          "[item-detail-dispatch][F02580]") {
  ItemWorld world;
  State state{Stage::kReturn, 0, 20, 47};
  world.ready = -19;
  CHECK(inv::UpdateGemDetails(state, world) == -19);
  CHECK(state.stage == Stage::kReturn);
  CHECK(inv::UpdateGemDetails(state, world, kEntry) == 0);
  CHECK(state.stage == Stage::kInitialize);
  CHECK(state.animation_step == 10);
}

TEST_CASE(
    "F02581 entry resets presentation without resetting the child controller",
    "[item-detail-dispatch][F02581]") {
  DispatchWorld world;
  world.ready = 0;
  inv::ItemDetailDispatcherState state{Category::kGem};
  CHECK(inv::UpdateItemDetails(
            state, world, inv::DetailDispatchEntry{Category::kArmor, kEntry}) ==
        0);
  CHECK(state.category == Category::kArmor);
  CHECK(world.header.item_page == 2);
  CHECK(world.selected_element == 9);
  CHECK(world.command == inv::DetailCommand::kNone);
  if (!world.selection) {
    FAIL("dispatcher did not retain its entry selection");
    return;
  }
  CHECK(world.selection.value().row.index == 20);
  CHECK(world.selection.value().page.index == 1);
}

TEST_CASE("F02581 category-only entry dispatches continuation and a zero page",
          "[item-detail-dispatch][F02581]") {
  DispatchWorld world;
  world.ready = 0;
  inv::ItemDetailDispatcherState state;
  CHECK(inv::UpdateItemDetails(
            state, world,
            inv::DetailDispatchEntry{Category::kGem, std::nullopt}) == 0);
  CHECK(world.dispatched == Category::kGem);
  CHECK_FALSE(world.selection.has_value());
  CHECK(world.header.item_page == 0);
}

TEST_CASE("F02581 restores exactly list commands and preview on a child return",
          "[item-detail-dispatch][F02581]") {
  DispatchWorld world;
  inv::ItemDetailDispatcherState state{Category::kArmor};
  world.child_result = -7;
  CHECK(inv::UpdateItemDetails(state, world) == -7);
  CHECK(world.calls == Calls{"controller", "battle-check", "categories",
                             "header-placement", "commands", "preview",
                             "command"});
  CHECK(world.categories == 1);
  CHECK(world.commands == 1);
  CHECK(world.placement == inv::DetailHeaderPlacement::kList);
  CHECK(world.preview == inv::PreviewLayout::kItems);
  CHECK(world.command == inv::DetailCommand::kMenu);
}

TEST_CASE(
    "F02581 reads battle return after the child without probing readiness",
    "[item-detail-dispatch][F02581]") {
  DispatchWorld world;
  inv::ItemDetailDispatcherState state{Category::kGem};
  world.child_result = 1;
  world.callback = [&](std::string_view call) {
    if (call == "controller") {
      world.intent = inv::BattleReturnIntent::kBattle;
    }
  };
  CHECK(inv::UpdateItemDetails(state, world) == -2);
  CHECK(world.calls == Calls{"controller", "battle-check"});
  CHECK(world.categories == 0);
}

TEST_CASE("F02581 waits without touching the detail presentation",
          "[item-detail-dispatch][F02581]") {
  DispatchWorld world;
  inv::ItemDetailDispatcherState state{Category::kGem};
  world.ready = 0;
  CHECK(inv::UpdateItemDetails(state, world) == 0);
  CHECK(world.calls == Calls{"controller", "ready"});
}

TEST_CASE("F02581 caches cursor brightness before the tab callback",
          "[item-detail-dispatch][F02581]") {
  DispatchWorld world;
  inv::ItemDetailDispatcherState state{Category::kGem};
  world.selected_element = 9;
  world.callback = [&](std::string_view call) {
    if (call == "tabs") {
      world.selected_element = 20;
    }
  };
  CHECK(inv::UpdateItemDetails(state, world) == 0);
  CHECK_FALSE(world.bright);
  CHECK(world.calls ==
        Calls{"controller", "ready", "cursor", "tabs", "details"});
}

TEST_CASE(
    "F02581 keeps its category on continuation and diagnoses an unsupported "
    "native category",
    "[item-detail-dispatch][F02581]") {
  DispatchWorld world;
  inv::ItemDetailDispatcherState state{Category::kArmor};
  world.ready = 0;
  CHECK(inv::UpdateItemDetails(state, world) == 0);
  CHECK(world.dispatched == Category::kArmor);
  state.category = Category::kMisc;
  world.calls.clear();
  CHECK_THROWS_AS(inv::UpdateItemDetails(state, world), std::runtime_error);
  CHECK(world.calls.empty());
}
