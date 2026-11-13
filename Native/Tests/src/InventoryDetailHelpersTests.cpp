#include <array>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Input/Pad.hpp"
#include "Menu/Inventory/DetailsPolicies.hpp"
#include "Menu/InventoryDetailHelpersWorld.hpp"

namespace {
namespace inventory = VS::Menu::Inventory;
namespace fw = VS::Menu::Framework;
using inventory_detail_tests::Call;
using inventory_detail_tests::Event;
using inventory_detail_tests::World;
constexpr int kSelectedRow = 20;
}  // namespace

TEST_CASE("F02566 switches all three panels together",
          "[inventory-detail-helpers][F02566]") {
  inventory::PanelVisibility panels{3, 4, 5};
  inventory::SetPanelVisibility(panels, 0);
  CHECK(panels == inventory::PanelVisibility{0, 0, 0});
  inventory::SetPanelVisibility(panels, 2);
  CHECK(panels == inventory::PanelVisibility{2, 2, 2});
}

TEST_CASE("F02566 preserves nonboolean command visibility bytes",
          "[inventory-detail-helpers][F02566]") {
  inventory::PanelVisibility panels;
  inventory::SetPanelVisibility(panels, 0x80);
  CHECK(panels == inventory::PanelVisibility{0x80, 0x80, 0x80});
  inventory::SetPanelVisibility(panels, 0xFF);
  CHECK(panels == inventory::PanelVisibility{0xFF, 0xFF, 0xFF});
}

TEST_CASE("F02567 simultaneous shoulders do not read unused paging inputs",
          "[inventory-detail-helpers][F02567]") {
  World world;
  world.held = VS::Input::L1 | VS::Input::R1;
  world.on_count = [] { throw std::runtime_error("unused category"); };
  CHECK(inventory::PollDetailsPaging(world, {7}) == inventory::DetailPage{7});
  CHECK(world.held_reads == 1);
  CHECK(world.count_reads == 0);
  CHECK(world.repeat_reads == 0);
  CHECK(world.pressed_reads == 0);
  CHECK(world.calls.empty());
}

TEST_CASE("F02567 reads repeats after count and wraps both directions",
          "[inventory-detail-helpers][F02567]") {
  World world;
  world.on_count = [&] { world.repeat = VS::Input::L1; };
  CHECK(inventory::PollDetailsPaging(world, {0}) == inventory::DetailPage{7});
  world.on_count = [&] { world.repeat = VS::Input::R1; };
  CHECK(inventory::PollDetailsPaging(world, {7}) == inventory::DetailPage{0});
  CHECK(world.count_reads == 2);
  CHECK(world.repeat_reads == 2);
  CHECK(world.calls.empty());
}

TEST_CASE("F02567 rejects unchanged edge presses but not repeats",
          "[inventory-detail-helpers][F02567]") {
  World world;
  world.count = 1;
  world.repeat = VS::Input::L1;
  world.pressed = VS::Input::L1;
  CHECK(inventory::PollDetailsPaging(world, {0}) == inventory::DetailPage{0});
  CHECK(world.calls == std::vector<Call>{{Event::kInvalid}});
  world.pressed = 0;
  CHECK(inventory::PollDetailsPaging(world, {0}) == inventory::DetailPage{0});
  CHECK(world.calls.size() == 1);
}

TEST_CASE("F02567 retains one-pass signed wrap and empty-inventory behavior",
          "[inventory-detail-helpers][F02567]") {
  World world;
  world.repeat = VS::Input::R1;
  CHECK(inventory::PollDetailsPaging(world, {16}) == inventory::DetailPage{9});
  CHECK(inventory::PollDetailsPaging(
            world, {std::numeric_limits<std::int32_t>::max()}) ==
        inventory::DetailPage{std::numeric_limits<std::int32_t>::min()});
  world.count = 0;
  world.repeat = VS::Input::L1;
  CHECK(inventory::PollDetailsPaging(world, {0}) == inventory::DetailPage{-1});
}

TEST_CASE("F02569 clears first and hoists the category and selected row",
          "[inventory-detail-helpers][F02569]") {
  World world;
  world.rows[kSelectedRow].sub_text = "detail";
  world.rows[kSelectedRow].w = 73;
  inventory::BeginDetailsTransition(world, {kSelectedRow});
  CHECK(world.calls ==
        std::vector<Call>{{Event::kClear},
                          {Event::kRow, inventory::kCategoryHeaderRow.index},
                          {Event::kRow, kSelectedRow}});
  CHECK(world.rows[31].state == fw::RowState::kSlideY);
  CHECK(world.rows[31].target0 == 18);
  CHECK(world.rows[kSelectedRow].state == fw::RowState::kSlideX);
  CHECK(world.rows[kSelectedRow].target0 == 155);
  CHECK(world.rows[kSelectedRow].selected == 1);
  CHECK(world.rows[kSelectedRow].sub_text == nullptr);
  CHECK(world.rows[kSelectedRow].w == 73);
}

TEST_CASE("F02569 respects the row alias and reacquires callback owners",
          "[inventory-detail-helpers][F02569]") {
  World world;
  inventory::BeginDetailsTransition(world, inventory::kCategoryHeaderRow);
  CHECK(world.rows[31].state == fw::RowState::kSlideX);
  CHECK(world.rows[31].target0 == 155);
  world.on_row = [&](int row) {
    if (row == kSelectedRow) world.replacement_owner = true;
  };
  inventory::BeginDetailsTransition(world, {kSelectedRow});
  CHECK(world.rows[31].state == fw::RowState::kSlideY);
  CHECK(world.replacement_rows[kSelectedRow].selected == 1);
  CHECK(world.rows[kSelectedRow].selected == 0);
}

TEST_CASE("F02571 coordinates details layout before opening stat panels",
          "[inventory-detail-helpers][F02571]") {
  World world;
  inventory::InitializeDetailsLayout(world, {kSelectedRow}, 3);
  CHECK(world.preview == inventory::PreviewLayout::kDetails);
  CHECK(world.panels == inventory::PanelVisibility{5, 0, 0});
  CHECK(world.calls == std::vector<Call>{{Event::kPreview, 1},
                                         {Event::kCategories, 0},
                                         {Event::kCommand, 0},
                                         {Event::kContinue, kSelectedRow},
                                         {Event::kPages, 3},
                                         {Event::kPrimary, 1}});
}

TEST_CASE("F02571 passes the page selection and preserves callback writes",
          "[inventory-detail-helpers][F02571]") {
  World world;
  world.on_primary = [&] { world.panels.category_icons = 9; };
  inventory::InitializeDetailsLayout(world, {0}, 4);
  CHECK(world.calls[4] == Call{Event::kPages, 4});
  CHECK(world.panels.category_icons == 9);
  inventory::InitializeDetailsLayout(world, {39}, 7);
  CHECK(world.calls[10] == Call{Event::kPages, 7});
  CHECK(world.panels.secondary_title == 5);
}

TEST_CASE("F02573 stores the cursor pair and maps the current display owner",
          "[inventory-detail-helpers][F02573]") {
  World world;
  world.category = 5;
  world.cursors.fill({0xA5, 0x5A});
  const std::array<std::uint8_t, 8> replacement{8, 7, 6, 5, 4, 3, 2, 1};
  world.on_remember = [&] { world.displayed = replacement; };
  CHECK(inventory::SelectDetailsItem(world, {7}) ==
        inventory::InventorySlot{1});
  CHECK(world.cursors[5] == inventory::CursorMemory{0, 7});
  CHECK(world.cursors[4] == inventory::CursorMemory{0xA5, 0x5A});
  CHECK(world.cursors[6] == inventory::CursorMemory{0xA5, 0x5A});
}

TEST_CASE("F02573 truncates only the page store and refuses missing entries",
          "[inventory-detail-helpers][F02573]") {
  World world;
  std::array<std::uint8_t, 300> order{};
  order[1] = 7;
  order[257] = 8;
  world.displayed = order;
  CHECK(inventory::SelectDetailsItem(world, {257}) ==
        inventory::InventorySlot{8});
  CHECK(world.cursors[0] == inventory::CursorMemory{0, 1});
  world.displayed = {};
  CHECK_THROWS_AS(inventory::SelectDetailsItem(world, {-1}), std::out_of_range);
  CHECK(world.cursors[0] == inventory::CursorMemory{0, 0xFF});
}

TEST_CASE("F02574 closes details in order without unused gauge cleanup",
          "[inventory-detail-helpers][F02574]") {
  World world;
  world.pressed = VS::Input::Circle;
  inventory::ExitDetailsView(world, false);
  CHECK(world.calls == std::vector<Call>{{Event::kLeave},
                                         {Event::kClear},
                                         {Event::kDismiss},
                                         {Event::kPages, -1},
                                         {Event::kPrimary, 2},
                                         {Event::kHeader, 2},
                                         {Event::kReturn, 0}});
  CHECK(world.return_intent == inventory::BattleReturnIntent::kMenu);
  CHECK(world.pressed_reads == 1);
}

TEST_CASE("F02574 records current exit input before optional DP PP cleanup",
          "[inventory-detail-helpers][F02574]") {
  World world;
  world.on_primary = [&] { world.pressed = VS::Input::Triangle; };
  inventory::ExitDetailsView(world, true);
  CHECK(world.return_intent == inventory::BattleReturnIntent::kBattle);
  CHECK(world.header == inventory::CategoryHeaderPlacement::kReturning);
  CHECK(world.calls[6] == Call{Event::kReturn, 1});
  CHECK(world.calls[7] == Call{Event::kBars, 0});
}
