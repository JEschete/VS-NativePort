#include <catch2/catch_test_macros.hpp>

#include "Menu/InventoryTerminalWorld.hpp"

namespace {
namespace Inventory = VS::Menu::Inventory;
using VS::tests::inventory_terminal::Category;
using VS::tests::inventory_terminal::World;
}  // namespace

TEST_CASE("F02591 inventory sort dispatches the original shield criteria",
          "[menu-inventory-terminal][F02591]") {
  World world;
  Inventory::InventorySortState state;
  REQUIRE(Inventory::UpdateSortMenu(state, world, Category::kShield) == 0);
  REQUIRE(Inventory::UpdateSortMenu(state, world) == 0);
  world.action_result = 0;
  REQUIRE(Inventory::UpdateSortMenu(state, world) == 1);
  REQUIRE(world.calls.back() == 1);
  world.action_result = -3;
  REQUIRE(Inventory::UpdateSortMenu(state, world) == -2);
}

TEST_CASE("F02591 inventory sort refuses an invalid category and selection",
          "[menu-inventory-terminal][F02591]") {
  World world;
  Inventory::InventorySortState state;
  REQUIRE_THROWS(Inventory::UpdateSortMenu(state, world, Category::kManage));
  state.step = 1;
  state.category = Category::kMisc;
  world.action_result = 2;
  REQUIRE_THROWS(Inventory::UpdateSortMenu(state, world));
}

TEST_CASE("F02592 inventory discard cancels and reopens without clearing data",
          "[menu-inventory-terminal][F02592]") {
  World world;
  Inventory::InventoryDiscardState state;
  Inventory::InventoryDiscardPresentation presentation;
  const Inventory::InventoryDiscardEntry entry{Category::kMisc, 1, 1};
  REQUIRE(Inventory::UpdateDiscardMenu(state, presentation, world, entry) == 0);
  state.step = 2;
  REQUIRE(Inventory::UpdateDiscardMenu(state, presentation, world) == 0);
  REQUIRE(state.maximum == 10);
  world.input.pressed = VS::Input::Triangle;
  REQUIRE(Inventory::UpdateDiscardMenu(state, presentation, world) == -2);
  REQUIRE(world.inventory.misc[0].count == 10);
  REQUIRE(Inventory::UpdateDiscardMenu(state, presentation, world, entry) == 0);
  REQUIRE(state.step == 0);
}

TEST_CASE("F02592 inventory quantity discard leaves the unselected remainder",
          "[menu-inventory-terminal][F02592]") {
  World world;
  Inventory::InventoryDiscardState state{
      .step = 7, .item = {Category::kMisc, 1, 1}, .quantity = 4};
  Inventory::InventoryDiscardPresentation presentation;
  world.action_result = 0;
  REQUIRE(Inventory::UpdateDiscardMenu(state, presentation, world) == 1);
  REQUIRE(world.inventory.misc[0].count == 6);
  state.place = 3;
  state.step = 5;
  REQUIRE_THROWS(Inventory::UpdateDiscardMenu(state, presentation, world));
}

TEST_CASE("F02597 inventory navigation distinguishes the two original merges",
          "[menu-inventory-terminal][F02597]") {
  World item;
  World loot;
  Inventory::InventoryNavigationState item_state{.step = 6, .selected_row = 1};
  auto loot_state = item_state;
  item.runtime.cursor.category = 7;
  loot.runtime.cursor.category = 7;
  item.selected_result = 1;
  loot.selected_result = 1;
  REQUIRE(Inventory::UpdateItemList(
              item_state, item, Inventory::ItemRowsContext::kItemMenu) == 0);
  REQUIRE(Inventory::UpdateItemList(loot_state, loot,
                                    Inventory::ItemRowsContext::kLoot) == 0);
  REQUIRE(item.inventory.misc[0].count == 20);
  REQUIRE(loot.inventory.misc[0].count == 20);
  REQUIRE(item.calls != loot.calls);
}

TEST_CASE("F02597 inventory row lease releases on refusal and on reentry",
          "[menu-inventory-terminal][F02597]") {
  World world;
  Inventory::InventoryNavigationState state{.step = 1};
  world.runtime.cursor.category = 1;
  world.fail_population = true;
  REQUIRE_THROWS(Inventory::UpdateItemList(
      state, world, Inventory::ItemRowsContext::kItemMenu));
  REQUIRE_FALSE(world.rows_active);
  world.fail_population = false;
  REQUIRE(Inventory::UpdateItemList(
              state, world, Inventory::ItemRowsContext::kItemMenu) == 0);
  REQUIRE_FALSE(world.rows_active);
  world.rows_active = true;
  state.step = 1;
  REQUIRE_THROWS(Inventory::UpdateItemList(
      state, world, Inventory::ItemRowsContext::kItemMenu));
  REQUIRE(world.rows_active);
}

TEST_CASE(
    "F02597 Manage restrictions differ by overlay without stale row flags",
    "[menu-inventory-terminal][F02597]") {
  World world;
  Inventory::InventoryNavigationState state{.step = 1};
  REQUIRE(Inventory::UpdateItemList(
              state, world, Inventory::ItemRowsContext::kItemMenu) == 0);
  const auto item_calls = world.calls;
  world.calls.clear();
  state.step = 1;
  REQUIRE(Inventory::UpdateItemList(state, world,
                                    Inventory::ItemRowsContext::kLoot) == 0);
  REQUIRE(item_calls != world.calls);
  REQUIRE_FALSE(world.rows_active);
}

TEST_CASE("F02553 misc shortcut handles keys and retains its reentry latch",
          "[menu-inventory-terminal][F02553]") {
  World world;
  Inventory::MiscShortcutState state;
  Inventory::UpdateMiscItemShortcut(state, world, 1);
  Inventory::UpdateMiscItemShortcut(state, world, 2);
  REQUIRE(state.no_force_cursor_memory == 1);
  state.step = 2;
  world.order[6][1] = 0;
  world.inventory.misc[0].id = 458;
  state.actions[0] = 17;
  Inventory::UpdateMiscItemShortcut(state, world);
  REQUIRE(state.actions[0] == 17);
  REQUIRE(state.step == 3);
  REQUIRE(state.no_force_cursor_memory == 0);
  REQUIRE_FALSE(world.rows_active);
}

TEST_CASE("F02597 parent search replaces the whole remembered cursor",
          "[menu-inventory-terminal][F02597]") {
  for (const auto context : {Inventory::ItemRowsContext::kItemMenu,
                             Inventory::ItemRowsContext::kLoot}) {
    World world;
    Inventory::InventoryNavigationState state{.step = 10, .selected_row = 1};
    world.runtime.cursor.category = 2;
    world.runtime.cursor.positions[0] = {9, 31};
    world.parent_weapon = 1;
    world.find_result = 4;
    REQUIRE(Inventory::UpdateItemList(state, world, context) == 0);
    REQUIRE(world.runtime.cursor.category == 1);
    REQUIRE(world.runtime.cursor.positions[0].row == 3);
    REQUIRE(world.runtime.cursor.positions[0].page == 0);
  }
}

TEST_CASE("F02597 MENUB text owns valid input and retains it after refusal",
          "[menu-inventory-terminal][F02597]") {
  Inventory::LootInventoryText text;
  std::array<std::uint8_t, Inventory::kLootInventoryTextBytes> source{};
  source.fill(0xE7);
  for (std::size_t entry = 0; entry < Inventory::kLootInventoryTextEntries;
       ++entry) {
    source[entry * 2] = Inventory::kLootInventoryTextEntries;
    source[entry * 2 + 1] = 0;
  }
  REQUIRE(Inventory::BindLootInventoryText(text, source));
  const auto saved = text.bytes;
  source[0] = 0;
  REQUIRE_FALSE(Inventory::BindLootInventoryText(text, source));
  REQUIRE(text.bytes == saved);
  REQUIRE_FALSE(
      Inventory::BindLootInventoryText(text, std::span(source).first(63)));
  REQUIRE(text.bytes == saved);
  source = saved;
  std::fill(source.begin() + Inventory::kLootInventoryTextEntries * 2,
            source.end(), std::uint8_t{0});
  REQUIRE_FALSE(Inventory::BindLootInventoryText(text, source));
  REQUIRE(text.bytes == saved);
}

TEST_CASE("F02553 refuses an unmapped misc action and releases its row storage",
          "[menu-inventory-terminal][F02553]") {
  World world;
  Inventory::MiscShortcutState state{.step = 2};
  world.order[6][1] = 0;
  world.inventory.misc[0].id = 457;
  REQUIRE_THROWS(Inventory::UpdateMiscItemShortcut(state, world));
  REQUIRE_FALSE(world.rows_active);
  world.inventory.misc[0].id = 323;
  Inventory::UpdateMiscItemShortcut(state, world);
  REQUIRE(state.step == 3);
  REQUIRE_FALSE(world.rows_active);
}

TEST_CASE("F02553 misc shortcut waits for unloading before queuing an item",
          "[menu-inventory-terminal][F02553]") {
  World world;
  Inventory::MiscShortcutState state{.selected_row = 1, .step = 4};
  world.text_unloaded = false;
  Inventory::UpdateMiscItemShortcut(state, world);
  REQUIRE(world.runtime.use.kind == 0);
  world.text_unloaded = true;
  Inventory::UpdateMiscItemShortcut(state, world);
  REQUIRE(world.runtime.use.kind == 8);
  REQUIRE(world.runtime.control.current_menu == 63);
  REQUIRE(world.runtime.use.command == 2);
}

TEST_CASE("F02636 inventory screen waits through its battle-return countdown",
          "[menu-inventory-terminal][F02636]") {
  World world;
  Inventory::InventoryScreenState state;
  std::uint8_t step = 10;
  REQUIRE(Inventory::UpdateInventoryScreen(state, step, world) == 0);
  REQUIRE(state.quit_countdown == 10);
  for (int frame = 0; frame < 10; ++frame) {
    REQUIRE(Inventory::UpdateInventoryScreen(state, step, world) == 0);
  }
  REQUIRE(world.runtime.control.current_menu == 0);
  REQUIRE(Inventory::UpdateInventoryScreen(state, step, world) == 1);
  REQUIRE(step == 0);
  REQUIRE(world.runtime.control.current_menu == 4);
}

TEST_CASE("F02636 inventory screen resumes with the original cursor policy",
          "[menu-inventory-terminal][F02636]") {
  World world;
  Inventory::InventoryScreenState state;
  std::uint8_t step = 13;
  REQUIRE(Inventory::UpdateInventoryScreen(state, step, world) == 0);
  REQUIRE(state.resume_without_cursor_reset == 1);
  step = 3;
  world.nested_navigation_changes_panels = true;
  REQUIRE(Inventory::UpdateInventoryScreen(state, step, world) == 0);
  REQUIRE(state.resume_without_cursor_reset == 0);
  REQUIRE(step == 5);
  REQUIRE(world.runtime.presentation.status_command == 2);
}

TEST_CASE("F02636 refuses a panel step outside its owned animation curve",
          "[menu-inventory-terminal][F02636]") {
  World world;
  Inventory::InventoryScreenState state;
  std::uint8_t step = 0;
  world.runtime.presentation.secondary_title = 1;
  world.runtime.presentation.secondary_step = 11;
  REQUIRE_THROWS(Inventory::UpdateInventoryScreen(state, step, world));
}
