#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Menu/LootTerminalWorld.hpp"

namespace VS::tests::loot_terminal {
namespace {
void WeaponLoot(World& world) {
  world.runtime.cursor = {2, 0, 1};
  world.workspace.rows[0] = {6, 0, 0, 1};
  world.workspace.rows[1] = {0, 0, 0, 0};
  auto& inventory = world.workspace.inventory;
  inventory.weapons[0] = {
      .index = 1, .blade = 1, .grip = 2, .gems = {1, 0, 2, 0}};
  inventory.blades[0].assembled_weapon_index = 1;
  inventory.grips[1].assembled_weapon_index = 1;
  inventory.grips[1].gem_slots = 3;
  inventory.gems[0].set_item_index = 1;
  inventory.gems[1].set_item_index = 1;
}
void MiscLoot(World& world, std::uint8_t first, std::uint8_t second) {
  world.runtime.cursor = {3, 0, 2};
  world.workspace.rows[0] = {6, 2, 0, 0};
  world.workspace.rows[1] = {1, 1, 0, 1};
  world.workspace.rows[2] = {6, 4, 0, 1};
  world.workspace.inventory.misc[2] = {323, first, 3};
  world.workspace.inventory.misc[4] = {323, second, 5};
  world.workspace.inventory.blades[1].id = 10;
}
void Preview(loot::StackConsolidationState& state, World& world) {
  static_cast<void>(loot::UpdateStackConsolidation(state, world, 0));
  world.runtime.loot_step = 6;
  static_cast<void>(loot::UpdateStackConsolidation(state, world));
}
std::uint32_t Total(const Items::InventoryContents& inventory,
                    std::uint16_t id) {
  std::uint32_t result = 0;
  for (const auto& item : inventory.misc) {
    if (item.id == id) result += item.count;
  }
  return result;
}
}  // namespace

TEST_CASE("F02843 loot transition preserves the already hidden HUD",
          "[F02843][menu-loot-terminal]") {
  World world;
  loot::InventoryTransitionState state;
  world.runtime.hud_hidden = 1;
  world.runtime.hud_slide_offset = 41;
  CHECK(loot::UpdateInventoryTransition(state, world, Category::kGem) == 0);
  CHECK(world.runtime.inventory_category == 6);
  CHECK(loot::UpdateInventoryTransition(state, world) == 0);
  CHECK(loot::UpdateInventoryTransition(state, world) == 0);
  world.inventory_result = -1;
  CHECK(loot::UpdateInventoryTransition(state, world) == 0);
  CHECK(world.runtime.hud_hidden == 1);
  CHECK(world.runtime.hud_slide_offset == 41);
  CHECK(loot::UpdateInventoryTransition(state, world) == 1);
  CHECK(world.runtime.show_loot == 1);
}

TEST_CASE(
    "F02843 loot inventory uses callee panel changes and restores its HUD",
    "[F02843][menu-loot-terminal]") {
  World world;
  loot::InventoryTransitionState state{1, 0};
  world.change_inventory_ui = true;
  world.nested_ui.secondary_title = 1;
  world.nested_category = 3;
  CHECK(loot::UpdateInventoryTransition(state, world) == 0);
  CHECK(world.runtime.hud_slide_offset == 0x200000);
  CHECK(world.runtime.inventory_ui.secondary_step == 1);
  CHECK(world.menu_rows[31].max == Items::kGripSlots);
  world.inventory_result = 1;
  CHECK(loot::UpdateInventoryTransition(state, world) == 0);
  CHECK(world.runtime.hud_hidden == 0);
  CHECK(world.runtime.hud_slide_offset == 0);
}

TEST_CASE("F02843 loot transition refuses invalid category and curve indices",
          "[F02843][menu-loot-terminal]") {
  World world;
  loot::InventoryTransitionState state;
  CHECK_THROWS_AS(
      loot::UpdateInventoryTransition(state, world, Category::kManage),
      std::runtime_error);
  world.runtime.inventory_ui.secondary_step = 11;
  CHECK_THROWS_AS(loot::UpdateInventoryTransition(state, world),
                  std::runtime_error);
}

TEST_CASE(
    "F02844 disassembly preserves component order and detaches all owners",
    "[F02844][menu-loot-terminal]") {
  World world;
  WeaponLoot(world);
  loot::DisassemblyState state;
  CHECK(loot::UpdateDisassembly(state, world, 1) == 0);
  world.runtime.loot_step = 6;
  CHECK(loot::UpdateDisassembly(state, world) == 0);
  CHECK(world.runtime.cursor.count == 5);
  CHECK(world.workspace.rows[2] == loot::LootRow{2, 1, 0, 4});
  state.step = 5;
  world.runtime.loot_step = 6;
  CHECK(loot::UpdateDisassembly(state, world) == 0);
  CHECK(world.runtime.cursor.count == 5);
  CHECK(world.workspace.inventory.blades[0].assembled_weapon_index == 0);
  CHECK(world.workspace.inventory.grips[1].assembled_weapon_index == 0);
  CHECK(world.workspace.inventory.gems[1].set_item_index == 0);
}

TEST_CASE(
    "F02844 shield disassembly retains shield and cancels without changes",
    "[F02844][menu-loot-terminal]") {
  World world;
  world.runtime.cursor.count = 1;
  world.workspace.rows[0] = {3, 7, 0, 0};
  world.workspace.inventory.shields[7].base.gem_slots = 4;
  world.workspace.inventory.shields[7].gems = {1, 2, 0, 48};
  const auto inventory = world.workspace.inventory;
  const auto rows = world.workspace.rows;
  loot::DisassemblyState state;
  CHECK(loot::UpdateDisassembly(state, world, 0) == 0);
  world.runtime.loot_step = 6;
  CHECK(loot::UpdateDisassembly(state, world) == 0);
  CHECK(world.workspace.rows[1] == loot::LootRow{3, 7, 0, 3});
  state.step = 4;
  world.input.pressed = 0x40;
  CHECK(loot::UpdateDisassembly(state, world) == 0);
  world.runtime.loot_step = 6;
  CHECK(loot::UpdateDisassembly(state, world) == 0);
  CHECK(world.workspace.inventory == inventory);
  CHECK(world.workspace.rows == rows);
  world.runtime.loot_step = 0;
  CHECK(loot::UpdateDisassembly(state, world) == -1);
}

TEST_CASE("F02844 disassembly validates links before taking its backup",
          "[F02844][menu-loot-terminal]") {
  World world;
  WeaponLoot(world);
  world.workspace.working_rows[0].state = 9;
  world.workspace.inventory.weapons[0].grip = 0;
  loot::DisassemblyState state;
  CHECK_THROWS_AS(loot::UpdateDisassembly(state, world, 1), std::runtime_error);
  CHECK(world.workspace.working_rows[0].state == 9);
  world.workspace.inventory.weapons[0].grip = 2;
  world.workspace.inventory.grips[1].gem_slots = 5;
  CHECK_THROWS_AS(loot::UpdateDisassembly(state, world, 1), std::runtime_error);
  world.workspace.inventory.grips[1].gem_slots = 3;
  world.workspace.inventory.weapons[0].gems[0] = 49;
  CHECK_THROWS_AS(loot::UpdateDisassembly(state, world, 1), std::runtime_error);
}

TEST_CASE(
    "F02844 confirmed shield disassembly keeps the shield but unsets gems",
    "[F02844][menu-loot-terminal]") {
  World world;
  world.runtime.cursor.count = 1;
  world.workspace.rows[0] = {3, 7, 0, 0};
  auto& inventory = world.workspace.inventory;
  inventory.shields[7].base.gem_slots = 4;
  inventory.shields[7].gems = {1, 2, 0, 48};
  inventory.gems[47].set_item_index = 8;
  loot::DisassemblyState state;
  CHECK(loot::UpdateDisassembly(state, world, 0) == 0);
  world.runtime.loot_step = 6;
  CHECK(loot::UpdateDisassembly(state, world) == 0);
  state.step = 5;
  world.runtime.loot_step = 6;
  CHECK(loot::UpdateDisassembly(state, world) == 0);
  CHECK(world.runtime.cursor.count == 4);
  CHECK(world.workspace.rows[0] == loot::LootRow{3, 7, 0, 3});
  CHECK(inventory.shields[7].gems == std::array<std::uint8_t, 4>{});
  CHECK(inventory.gems[47].set_item_index == 0);
}

TEST_CASE("F02844 disassembly backs up all 176 native physical rows",
          "[F02844][menu-loot-terminal]") {
  World world;
  WeaponLoot(world);
  world.runtime.cursor.count = 176;
  world.workspace.rows[175] = {3, 0, 0, 0};
  loot::DisassemblyState state;
  CHECK(loot::UpdateDisassembly(state, world, 175) == 0);
  CHECK(world.workspace.working_rows[175] == world.workspace.rows[175]);
  state.step = 6;
  world.runtime.loot_step = 6;
  world.workspace.rows[175] = {};
  CHECK(loot::UpdateDisassembly(state, world) == 0);
  CHECK(world.workspace.rows[175] == loot::LootRow{3, 0, 0, 0});
}

TEST_CASE(
    "F02845 small loot does not underflow when matching bag room is larger",
    "[F02845][menu-loot-terminal][issue-412]") {
  World world;
  MiscLoot(world, 2, 3);
  world.ashley.misc[0] = {323, 80, 1};
  loot::StackConsolidationState state;
  Preview(state, world);
  CHECK(world.runtime.cursor.count == 1);
  CHECK(world.workspace.inventory.misc[0].count == 5);
  CHECK(state.remaining == 0);
  CHECK(state.stack_room == 15);
  state.step = 5;
  world.runtime.loot_step = 6;
  CHECK(loot::UpdateStackConsolidation(state, world) == 0);
  CHECK(Total(world.workspace.inventory, 323) == 5);
  CHECK(world.runtime.cursor.count == 2);
  CHECK(world.workspace.rows[0].category == 1);
}

TEST_CASE("F02845 consolidation partitions matching room and 100-item stacks",
          "[F02845][menu-loot-terminal]") {
  World world;
  MiscLoot(world, 100, 50);
  world.ashley.misc[0] = {323, 60, 1};
  loot::StackConsolidationState state;
  Preview(state, world);
  CHECK(world.runtime.cursor.count == 3);
  CHECK(world.workspace.inventory.misc[0].count == 40);
  CHECK(world.workspace.inventory.misc[1].count == 100);
  CHECK(world.workspace.inventory.misc[2].count == 10);
  CHECK(world.workspace.rows[0].state == 1);
  CHECK(world.workspace.rows[1].state == 0);
  CHECK(Total(world.workspace.inventory, 323) == 150);
}

TEST_CASE(
    "F02845 consolidation cancellation restores inventory and tail metadata",
    "[F02845][menu-loot-terminal]") {
  World world;
  MiscLoot(world, 55, 45);
  world.workspace.rows[175] = {5, 47, 9, 5};
  const auto rows = world.workspace.rows;
  const auto inventory = world.workspace.inventory;
  loot::StackConsolidationState state;
  Preview(state, world);
  CHECK(world.runtime.cursor.count == 1);
  state.step = 4;
  state.choice = 1;
  world.input.pressed = 0x20;
  CHECK(loot::UpdateStackConsolidation(state, world) == 0);
  world.runtime.loot_step = 6;
  CHECK(loot::UpdateStackConsolidation(state, world) == 0);
  CHECK(world.workspace.inventory == inventory);
  CHECK(world.workspace.rows == rows);
  world.runtime.loot_step = 0;
  CHECK(loot::UpdateStackConsolidation(state, world) == -1);
}

TEST_CASE(
    "F02845 consolidation refuses invalid records without overwriting backup",
    "[F02845][menu-loot-terminal]") {
  World world;
  MiscLoot(world, 10, 20);
  loot::StackConsolidationState state;
  world.workspace.working_rows[0].state = 9;
  CHECK_THROWS_AS(loot::UpdateStackConsolidation(state, world, 1),
                  std::runtime_error);
  CHECK(world.workspace.working_rows[0].state == 9);
  world.workspace.inventory.misc[2].id = 458;
  CHECK_THROWS_AS(loot::UpdateStackConsolidation(state, world, 0),
                  std::runtime_error);
  world.backups_available = false;
  CHECK_THROWS_AS(loot::UpdateStackConsolidation(state, world, 0),
                  std::runtime_error);
}

TEST_CASE(
    "F02845 an unsplittable full preview is refused without partial changes",
    "[F02845][menu-loot-terminal]") {
  World world;
  world.runtime.cursor.count = 64;
  for (std::size_t slot = 0; slot < Items::kMiscSlots; ++slot) {
    world.workspace.inventory.misc[slot] = {
        324, 1, static_cast<std::uint8_t>(slot + 1)};
    world.workspace.rows[slot] = {6, static_cast<std::uint8_t>(slot), 0, 0};
  }
  world.workspace.inventory.misc[0] = {323, 100, 1};
  world.ashley.misc[0] = {323, 50, 1};
  const auto inventory = world.workspace.inventory;
  const auto rows = world.workspace.rows;
  loot::StackConsolidationState state;
  CHECK(loot::UpdateStackConsolidation(state, world, 0) == 0);
  world.runtime.loot_step = 6;
  CHECK_THROWS_AS(loot::UpdateStackConsolidation(state, world),
                  std::runtime_error);
  CHECK(world.workspace.inventory == inventory);
  CHECK(world.workspace.rows == rows);
  CHECK(world.runtime.cursor.count == 64);
}

TEST_CASE("F02864 take-all marks only successful claims and keys",
          "[F02864][menu-loot-terminal]") {
  World world;
  MiscLoot(world, 10, 20);
  world.workspace.inventory.misc[2].id = 458;
  world.claim_results[Items::kMiscSlots + 1] = 0;
  world.input.pressed = 0x80;
  loot::LootSelectionState state{1, 0, 0};
  CHECK(loot::UpdateLootSelection(state, world) == 0);
  CHECK(world.runtime.claimed == 2);
  CHECK(world.workspace.rows[0].slide_step == 1);
  CHECK(world.workspace.rows[1].slide_step == 0);
  CHECK(state.step == 9);
  CHECK(loot::UpdateLootSelection(state, world) == 0);
  CHECK(world.runtime.cursor.count == 1);
  CHECK(world.workspace.rows[0] == loot::LootRow{1, 1, 0, 1});
}

TEST_CASE("F02864 a single claim retains the three-frame removal delay",
          "[F02864][menu-loot-terminal]") {
  World world;
  MiscLoot(world, 10, 20);
  world.input.pressed = 0x20;
  loot::LootSelectionState state{1, 0, 0};
  CHECK(loot::UpdateLootSelection(state, world) == 0);
  CHECK(state.removal_delay == 3);
  for (int frame = 0; frame < 3; ++frame) {
    CHECK(loot::UpdateLootSelection(state, world) == 0);
  }
  CHECK(world.runtime.cursor.count == 3);
  CHECK(loot::UpdateLootSelection(state, world) == 0);
  CHECK(world.runtime.cursor.count == 2);
  CHECK(world.workspace.rows[0].item_index == 2);
}

TEST_CASE("F02864 nine-row wrapping needs a fresh direction press",
          "[F02864][menu-loot-terminal]") {
  World world;
  world.runtime.cursor = {9, 0, 0};
  loot::LootSelectionState state{1, 0, 0};
  world.input.repeat = 0x1000;
  CHECK(loot::UpdateLootSelection(state, world) == 0);
  CHECK(world.runtime.cursor == loot::LootCursor{9, 0, 0});
  world.input.pressed = 0x1000;
  CHECK(loot::UpdateLootSelection(state, world) == 0);
  CHECK(world.runtime.cursor == loot::LootCursor{9, 1, 7});
  world.input.repeat = 0x4000;
  world.input.pressed = 0;
  CHECK(loot::UpdateLootSelection(state, world) == 0);
  CHECK(world.runtime.cursor == loot::LootCursor{9, 1, 7});
  world.input.pressed = 0x4000;
  CHECK(loot::UpdateLootSelection(state, world) == 0);
  CHECK(world.runtime.cursor == loot::LootCursor{9, 0, 0});
}

TEST_CASE(
    "F02864 organizing forwards the absolute row and waits for panel exit",
    "[F02864][menu-loot-terminal]") {
  World world;
  world.runtime.cursor = {10, 2, 1};
  world.organize_result = 2;
  loot::LootSelectionState state{3, 0, 0};
  CHECK(loot::UpdateLootSelection(state, world) == 0);
  CHECK(state.step == 5);
  CHECK(world.calls ==
        std::vector<std::int64_t>{static_cast<int>(Call::kOrganize), 1, 0,
                                  static_cast<int>(Call::kDisassemble), 1, 4});
  world.disassembly_result = -1;
  CHECK(loot::UpdateLootSelection(state, world) == 0);
  CHECK(state.step == 8);
  world.runtime.loot_step = 1;
  CHECK(loot::UpdateLootSelection(state, world) == 0);
  CHECK(state.step == 8);
}

TEST_CASE(
    "F02864 positive leave advances pending batches but cancellation does not",
    "[F02864][menu-loot-terminal]") {
  World world;
  world.batch_available = true;
  world.staged_count = 8;
  world.leave_result = -1;
  loot::LootSelectionState state{2, 0, 0};
  CHECK(loot::UpdateLootSelection(state, world) == 0);
  CHECK(world.advances == 0);
  state.step = 2;
  world.leave_result = 2;
  CHECK(loot::UpdateLootSelection(state, world) == 0);
  CHECK(world.runtime.cursor == loot::LootCursor{8, 0, 0});
  CHECK(world.runtime.controls_enabled == 0);
  CHECK(world.runtime.show_loot == 1);
  state.step = 2;
  CHECK(loot::UpdateLootSelection(state, world) == 2);
}

TEST_CASE("F02864 refuses a selected row beyond its list",
          "[F02864][menu-loot-terminal]") {
  World world;
  world.runtime.cursor = {2, 1, 1};
  loot::LootSelectionState state{1, 0, 0};
  CHECK_THROWS_AS(loot::UpdateLootSelection(state, world), std::runtime_error);
  CHECK(world.runtime.claimed == 0);
  world.runtime.cursor = {1, 0, 0};
  world.workspace.rows[0] = {6, 64, 0, 1};
  CHECK_THROWS_AS(loot::UpdateLootSelection(state, world), std::runtime_error);
}

TEST_CASE("F02865 loot teardown waits for both text layers before releasing",
          "[F02865][menu-loot-terminal]") {
  World world;
  std::uint8_t step = 5;
  world.dismiss_pending = true;
  world.text_unloaded = false;
  CHECK(loot::UpdateLootScreen(step, world) == 0);
  CHECK(world.releases == 0);
  world.dismiss_pending = false;
  CHECK(loot::UpdateLootScreen(step, world) == 0);
  CHECK(world.releases == 0);
  world.text_unloaded = true;
  CHECK(loot::UpdateLootScreen(step, world) == 1);
  CHECK(world.releases == 1);
  CHECK(world.paging_resets == 1);
  CHECK(step == 0);
  CHECK_FALSE(world.active_workspace);
}

TEST_CASE(
    "F02865 reopen initializes storage and gating without stale selection",
    "[F02865][menu-loot-terminal]") {
  World world;
  std::uint8_t step = 0;
  world.active_workspace = false;
  world.runtime.claimed = 21;
  world.runtime.cursor.offset = 14;
  CHECK(loot::UpdateLootScreen(step, world) == 0);
  CHECK(world.allocations == 1);
  CHECK(world.runtime.claimed == 0);
  CHECK(world.runtime.cursor.offset == 0);
  CHECK(world.runtime.inventory_step == 9);
  CHECK(loot::UpdateLootScreen(step, world) == 0);
  CHECK(step == 2);
  world.runtime.loot_step = 0;
  CHECK(loot::UpdateLootScreen(step, world) == 0);
  CHECK(step == 3);
}

TEST_CASE(
    "F02865 empty loot waits for completed information and face-button input",
    "[F02865][menu-loot-terminal]") {
  World world;
  std::uint8_t step = 1;
  CHECK(loot::UpdateLootScreen(step, world) == 0);
  CHECK(step == 4);
  world.information_done = false;
  world.input.pressed = 0x20;
  CHECK(loot::UpdateLootScreen(step, world) == 0);
  CHECK(step == 4);
  world.information_done = true;
  world.input.pressed = 0x1000;
  CHECK(loot::UpdateLootScreen(step, world) == 0);
  CHECK(step == 4);
  world.input.pressed = 0x20;
  CHECK(loot::UpdateLootScreen(step, world) == 0);
  CHECK(step == 5);
}

TEST_CASE(
    "F02865 selection records only an accepted result and defers teardown",
    "[F02865][menu-loot-terminal]") {
  for (int result : std::array{1, 2, -1}) {
    World world;
    std::uint8_t step = 2;
    world.runtime.cursor.count = 8;
    world.runtime.show_loot = 1;
    world.selection_preview = 8;
    world.selection_result = result;
    CHECK(loot::UpdateLootScreen(step, world) == 0);
    CHECK(world.runtime.result_kind == (result == 1 ? 1 : 0));
    CHECK(step == 5);
    CHECK(world.releases == 0);
    CHECK(world.menu_rows[39].selected == 1);
  }
}

TEST_CASE("F02865 rejects invalid caller state and animation indices",
          "[F02865][menu-loot-terminal]") {
  World world;
  std::uint8_t step = 6;
  CHECK_THROWS_AS(loot::UpdateLootScreen(step, world), std::runtime_error);
  step = 3;
  world.runtime.inventory_step = 11;
  CHECK_THROWS_AS(loot::UpdateLootScreen(step, world), std::runtime_error);
  CHECK(world.releases == 0);
}
}  // namespace VS::tests::loot_terminal
