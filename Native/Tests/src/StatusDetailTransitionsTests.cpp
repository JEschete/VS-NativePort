#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Menu/Framework/EquipmentDetails.hpp"
#include "Menu/Framework/Framework.hpp"
#include "Menu/Status/DetailTransitions.hpp"
#include "Menu/StatusTransitionsWorld.hpp"

namespace {
namespace fixture = vs_test::status_transitions;
namespace status = VS::Menu::Status;
namespace Framework = VS::Menu::Framework;
using fixture::World;
}  // namespace

TEST_CASE(
    "F02655 navigation prioritizes up down left right and skips disabled links",
    "[status-transitions][F02655]") {
  status::DetailNavigation input{.selected = 3,
                                 .flags = 0x76,
                                 .repeat = 0xF000,
                                 .links = Framework::kStatusViewNavigation};
  CHECK(status::NavigateDetailElements(input) == 2);
  input.repeat = 0xE000;
  CHECK(status::NavigateDetailElements(input) == 4);
  input.repeat = 0xA000;
  CHECK(status::NavigateDetailElements(input) == 3);
  input.repeat = 0x2000;
  CHECK(status::NavigateDetailElements(input) == 12);
  input.selected = 5;
  input.flags = 0;
  CHECK(status::NavigateDetailElements(input) == 9);
}

TEST_CASE(
    "F02655 self links scan backward for middle elements and forward otherwise",
    "[status-transitions][F02655]") {
  status::DetailNavigation input{.links = Framework::kStatusViewNavigation};
  CHECK(status::NavigateDetailElements(input) == 2);
  input.selected = 15;
  CHECK(status::NavigateDetailElements(input) == 9);
  input.selected = 8;
  input.stat_page = Framework::stat_page::kType;
  CHECK(status::NavigateDetailElements(input) == 4);
  input.selected = 16;
  CHECK(status::NavigateDetailElements(input) == 18);
  input.selected = 8;
  input.stat_page = Framework::stat_page::kAffinity;
  CHECK(status::NavigateDetailElements(input) == 8);
}

TEST_CASE(
    "F02655 malformed elements links and nonterminating cycles are refused",
    "[status-transitions][F02655]") {
  auto links = Framework::kStatusViewNavigation;
  status::DetailNavigation input{.selected = 21, .links = links};
  CHECK_FALSE(status::NavigateDetailElements(input));
  input.selected = std::numeric_limits<std::uint32_t>::max();
  CHECK_FALSE(status::NavigateDetailElements(input));
  input.selected = 0;
  input.links = std::span(links).first(20);
  CHECK_FALSE(status::NavigateDetailElements(input));
  input.links = links;
  input.repeat = 0x1000;
  links[0][0] = 255;
  CHECK_FALSE(status::NavigateDetailElements(input));
  links[0][0] = 1;
  links[1][0] = 0;
  CHECK_FALSE(status::NavigateDetailElements(input));
}

TEST_CASE("F02669 hoisting keeps row contents while dropping borrowed subtext",
          "[status-transitions][F02669]") {
  World world(17);
  const auto before = world.rows[13];
  CHECK(status::HoistEquipmentRow(world, 3));
  const auto& row = world.rows[13];
  CHECK(row.state == Framework::RowState::kSlideX);
  CHECK(row.target0 == 155);
  CHECK(row.selected == 1);
  CHECK(row.sub_text == nullptr);
  CHECK(row.text == before.text);
  CHECK(row.x == before.x);
  CHECK(world.events == std::vector<std::uint32_t>{1, 4});
}

TEST_CASE("F02669 unavailable rows refuse before flyout",
          "[status-transitions][F02669]") {
  World world;
  const auto before = fixture::RowBytes(world.rows[10]);
  CHECK_FALSE(status::HoistEquipmentRow(world, -1));
  CHECK_FALSE(status::HoistEquipmentRow(world, 30));
  world.row_extent = 0;
  CHECK_FALSE(status::HoistEquipmentRow(world, 0));
  CHECK(world.events.empty());
  CHECK(fixture::RowBytes(world.rows[10]) == before);
  CHECK(world.text.refusals.size() == 3);
}

TEST_CASE(
    "F02670 category title uses table offsets and slides selected row "
    "vertically",
    "[status-transitions][F02670]") {
  const int selected = GENERATE(0, 1, 2, 8);
  World world;
  const auto before = world.rows[static_cast<std::size_t>(selected) + 10];
  REQUIRE(status::BeginDetailTransition(world, selected));
  CHECK(world.rows[32].state == Framework::RowState::kDismiss);
  CHECK(world.rows[32].target0 == 16);
  CHECK(world.rows[32].selected == 1);
  CHECK(world.rows[32].row_icon == (selected == 0   ? 24
                                    : selected == 1 ? 27
                                                    : 28));
  const auto& row = world.rows[static_cast<std::size_t>(selected) + 10];
  CHECK(row.state == Framework::RowState::kSlideY);
  CHECK(row.target0 == 18);
  CHECK(row.sub_text == before.sub_text);
  CHECK(world.events[6] ==
        fixture::kStatusAddress + 64 + (selected < 2 ? selected + 9 : 11) * 8);
}

TEST_CASE("F02670 missing or unterminated titles refuse without row writes",
          "[status-transitions][F02670]") {
  World world;
  const auto before = fixture::RowBytes(world.rows[10]);
  world.status_extent = 2;
  CHECK_FALSE(status::BeginDetailTransition(world, 0));
  world.status_extent = world.status_text.size();
  world.status_text.fill(0);
  CHECK_FALSE(status::BeginDetailTransition(world, 0));
  CHECK(fixture::RowBytes(world.rows[10]) == before);
  CHECK(world.events.empty());
  CHECK(world.text.refusals.size() == 2);
}

TEST_CASE(
    "F02670 unavailable workspace or failed creation preserves equipment row",
    "[status-transitions][F02670]") {
  World world;
  const auto before = fixture::RowBytes(world.rows[11]);
  world.row_extent = 32;
  CHECK_FALSE(status::BeginDetailTransition(world, 1));
  CHECK(world.events.empty());
  world.row_extent = world.rows.size();
  world.refuse_creation = true;
  CHECK_FALSE(status::BeginDetailTransition(world, 1));
  CHECK(fixture::RowBytes(world.rows[11]) == before);
  CHECK(world.text.refusals.size() == 2);
}

TEST_CASE("F02671 initializes the title before stat pages and primary panel",
          "[status-transitions][F02671]") {
  const int pages = GENERATE(-1, 0, 7, 255);
  World world;
  REQUIRE(status::InitializeDetailPanel(world, 1, pages));
  REQUIRE(world.events.size() >= 4);
  const auto tail = std::span(world.events).last(4);
  CHECK(std::vector(tail.begin(), tail.end()) ==
        std::vector<std::uint32_t>{4, static_cast<std::uint32_t>(pages), 5, 1});
  CHECK(world.rows[32].row_icon == 27);
  CHECK(world.rows[11].target0 == 18);
  CHECK(world.text.refusals.empty());
}

TEST_CASE("F02671 refuses initialization without proceeding to panel callbacks",
          "[status-transitions][F02671]") {
  World world;
  world.status_extent = 0;
  CHECK_FALSE(status::InitializeDetailPanel(world, 0, 7));
  CHECK(world.events.empty());
  CHECK(world.text.refusals.size() == 1);
}

TEST_CASE(
    "F02672 blade row uses converted inventory fields and status row base",
    "[status-transitions][F02672]") {
  World world;
  auto weapon = fixture::Weapon();
  weapon.blade.id = 0x111;
  const auto before = weapon;
  const auto* row = status::SetWeaponComponentRow(world, 1, weapon, true);
  REQUIRE(row != nullptr);
  if (row == nullptr) return;
  CHECK(row == &world.rows[21]);
  CHECK(row->row_icon == 9);
  CHECK(row->material == 6);
  CHECK(row->x == 320);
  CHECK(row->target0 == 162);
  CHECK(row->unselectable == 0);
  CHECK(weapon == before);
  CHECK(world.text.refusals.empty());
}

TEST_CASE(
    "F02672 grip row formats its category and does not slide when disabled",
    "[status-transitions][F02672]") {
  World world;
  const auto before = world.rows[22];
  const auto* row =
      status::SetWeaponComponentRow(world, 2, fixture::Weapon(), false);
  REQUIRE(row != nullptr);
  if (row == nullptr) return;
  CHECK(row->row_icon == 14);
  CHECK(row->material == 0);
  CHECK(row->state == before.state);
  CHECK(row->x == before.x);
  CHECK(row->target0 == before.target0);
  CHECK(world.events[0] == 3);
  CHECK(world.events[1] == 22);
}

TEST_CASE("F02672 populated and empty gems retain socket selection semantics",
          "[status-transitions][F02672]") {
  const int component = GENERATE(3, 4, 5);
  World world;
  const auto* row =
      status::SetWeaponComponentRow(world, component, fixture::Weapon(), true);
  REQUIRE(row != nullptr);
  if (row == nullptr) return;
  CHECK(row->row_icon == 22);
  CHECK(row->material == 0);
  CHECK(row->unselectable == (component == 4 ? 1 : 0));
  CHECK(row->target0 == 169);
  CHECK(row->state == Framework::RowState::kSlideX);
  CHECK(world.text.refusals.empty());
}

TEST_CASE("F02672 sockets beyond grip capacity deactivate without formatting",
          "[status-transitions][F02672]") {
  World world;
  auto weapon = fixture::Weapon();
  weapon.grip.gem_slots = 0;
  const auto before = world.text.buffer;
  CHECK(status::SetWeaponComponentRow(world, 3, weapon, true) == nullptr);
  CHECK(world.events == std::vector<std::uint32_t>{3, 23});
  CHECK(world.text.buffer == before);
  CHECK(world.text.refusals.empty());
}

TEST_CASE(
    "F02672 invalid rows and out of loadout socket are refused before "
    "callbacks",
    "[status-transitions][F02672]") {
  World world;
  auto weapon = fixture::Weapon();
  CHECK(status::SetWeaponComponentRow(world, 0, weapon, true) == nullptr);
  CHECK(status::SetWeaponComponentRow(world, 20, weapon, true) == nullptr);
  weapon.grip.gem_slots = 255;
  CHECK(status::SetWeaponComponentRow(world, 6, weapon, true) == nullptr);
  CHECK(world.events.empty());
  CHECK(world.text.refusals.size() == 3);
}

TEST_CASE(
    "F02672 missing text and row allocation refusal never mutate menu fields",
    "[status-transitions][F02672]") {
  World world;
  const auto weapon = fixture::Weapon();
  const auto before = fixture::RowBytes(world.rows[21]);
  world.text.names_extent = 0;
  CHECK(status::SetWeaponComponentRow(world, 1, weapon, true) == nullptr);
  CHECK(world.events.empty());
  world.text.names_extent = world.text.names.size();
  world.refuse_creation = true;
  CHECK(status::SetWeaponComponentRow(world, 1, weapon, true) == nullptr);
  CHECK(fixture::RowBytes(world.rows[21]) == before);
}

TEST_CASE("F02673 shield gems preserve material while assigning the gem icon",
          "[status-transitions][F02673]") {
  const int component = GENERATE(1, 2, 3);
  World world(19);
  const auto material =
      world.rows[static_cast<std::size_t>(component) + 20].material;
  const auto shield = fixture::Shield();
  const auto before = shield;
  const auto* row = status::SetShieldGemRow(world, component, shield, true);
  REQUIRE(row != nullptr);
  if (row == nullptr) return;
  CHECK(row->row_icon == 22);
  CHECK(row->material == material);
  CHECK(row->unselectable == (component == 2 ? 1 : 0));
  CHECK(row->state == Framework::RowState::kSlideX);
  CHECK(row->target0 == 169);
  CHECK(shield == before);
  CHECK(world.text.refusals.empty());
}

TEST_CASE(
    "F02673 missing socket only deactivates and nonanimated rows retain "
    "placement",
    "[status-transitions][F02673]") {
  World world;
  const auto before = world.rows[21];
  const auto* row = status::SetShieldGemRow(world, 1, fixture::Shield(), false);
  REQUIRE(row != nullptr);
  if (row == nullptr) return;
  CHECK(row->state == before.state);
  CHECK(row->x == before.x);
  world.events.clear();
  CHECK(status::SetShieldGemRow(world, 4, fixture::Shield(), true) == nullptr);
  CHECK(world.events == std::vector<std::uint32_t>{3, 24});
  CHECK(world.text.refusals.empty());
}

TEST_CASE(
    "F02673 invalid shield slot and missing empty socket text are refused",
    "[status-transitions][F02673]") {
  World world;
  auto shield = fixture::Shield();
  CHECK(status::SetShieldGemRow(world, 0, shield, false) == nullptr);
  CHECK(status::SetShieldGemRow(world, 20, shield, false) == nullptr);
  shield.base.gem_slots = 255;
  CHECK(status::SetShieldGemRow(world, 4, shield, false) == nullptr);
  world.text.help_extent = 0;
  CHECK(status::SetShieldGemRow(world, 2, shield, false) == nullptr);
  CHECK(world.events.empty());
  CHECK(world.text.refusals.size() == 4);
}

TEST_CASE("F02674 exit preserves callback order and optional bar dismissal",
          "[status-transitions][F02674]") {
  World world;
  status::ExitEquipmentDetails(world, false);
  const std::vector<std::uint32_t> expected{6, 7,          40, 8, 0,
                                            4, 0xFFFFFFFF, 5,  2};
  CHECK(world.events == expected);
  world.events.clear();
  status::ExitEquipmentDetails(world, true);
  auto with_bars = expected;
  with_bars.insert(with_bars.end(), {9, 4});
  CHECK(world.events == with_bars);
  CHECK(world.text.refusals.empty());
}
