#include <array>
#include <cstdint>
#include <limits>

#include <catch2/catch_test_macros.hpp>

#include "Menu/StatusTerminalWorld.hpp"

namespace {
namespace st = VS::Menu::Status;
namespace fixture = status_terminal_tests;
using fixture::Event;
using fixture::World;
}  // namespace

TEST_CASE("F02675 narrows entry row but keeps word request and return",
          "[status-terminal][F02675]") {
  World world;
  world.frame.vitals_phase = 7;
  REQUIRE(st::UpdateEquipmentDetails(world.detail, world, 257) == 0);
  CHECK(world.detail.selected_row == 0);
  CHECK(world.detail.selected_element == 9);
  CHECK(world.frame.vitals_phase == 0);
  CHECK(world.Count(Event::kHoist) == 1);
  CHECK(world.Count(Event::kCommand) == 1);
}

TEST_CASE("F02675 enters rows before accepting input",
          "[status-terminal][F02675]") {
  World world;
  world.frame.vitals_slide = 5;
  world.ready = false;
  REQUIRE(st::UpdateEquipmentDetails(world.detail, world, 0) == 0);
  CHECK(world.detail.phase == 0);
  CHECK(world.frame.vitals_slide == 4);
  world.ready = true;
  REQUIRE(st::UpdateEquipmentDetails(world.detail, world, 0) == 0);
  CHECK(world.detail.phase == 1);
  REQUIRE(st::UpdateEquipmentDetails(world.detail, world, 0) == 0);
  CHECK(world.detail.row_to_render == 1);
  CHECK(world.frame.animation_step == 65);
  CHECK(world.Count(Event::kPart) == 1);
}

TEST_CASE("F02675 equipment cycling skips empty rows and favors R1",
          "[status-terminal][F02675]") {
  World world;
  world.detail.phase = 2;
  world.detail.selected_element = 9;
  world.actors[0].equipment_ids[1] = 0;
  world.frame.repeat = 12;
  world.row_flags[2] = 0x74050000;
  REQUIRE(st::UpdateEquipmentDetails(world.detail, world, 0) == 0);
  CHECK(world.detail.selected_row == 2);
  CHECK(world.Count(Event::kDeactivate) == 16);
  CHECK(world.rows[12].row_icon == 29);
  CHECK(world.rows[12].material == 5);
  CHECK(world.rows[32].row_icon == 28);
  CHECK(world.Count(Event::kTabs) == 1);
}

TEST_CASE("F02675 shield help retains signed limb comparison and byte cursor",
          "[status-terminal][F02675]") {
  World world;
  world.detail = {255, 2, 1, 0, 18};
  world.detail_selection = 18;
  world.cursor_result = 0x1234;
  world.frame.pressed = 80;
  REQUIRE(st::UpdateEquipmentDetails(world.detail, world, 0) == -1);
  CHECK(world.detail.cursor_animation == 0x34);
  CHECK(world.Count(Event::kVitalsBegin) == 0);
  const auto& information = world.events[3];
  CHECK(information[0] == static_cast<std::uint32_t>(Event::kInformation));
  CHECK(information[2] == world.help_text[661][0]);
}

TEST_CASE("F02675 refuses missing owners without indexing invalid actor",
          "[status-terminal][F02675]") {
  World world;
  world.frame.selected_actor = std::numeric_limits<std::int32_t>::min();
  CHECK_FALSE(st::UpdateEquipmentDetails(world.detail, world, 0).has_value());
  CHECK(world.events.empty());
  world.frame.selected_actor = 1;
  world.detail.phase = 2;
  world.rows_present = false;
  CHECK_FALSE(st::UpdateEquipmentDetails(world.detail, world, 0).has_value());
}

TEST_CASE("F02676 list style and temporary cursor memory preserve owners",
          "[status-terminal][F02676]") {
  World world;
  world.frame.selected_actor = 2;
  world.frame.cursor_memory = 0xA7;
  world.frame.animation_step = 128;
  world.actors[1].analyzed = false;
  world.actors[1].equipment_ids[0] = 0;
  REQUIRE(st::UpdateEquipmentList(world.list, world.detail, world, 0) == 0);
  CHECK(world.frame.cursor_memory == 0xA7);
  CHECK(world.list.phase == 1);
  CHECK(world.list.timer == 10);
  const auto& list = world.events.back();
  CHECK(list[0] == static_cast<std::uint32_t>(Event::kList));
  CHECK(list[3] == 1);
  CHECK(list[4] == 0xF001);
  CHECK(world.Count(Event::kFormat) == 8);
}

TEST_CASE("F02676 waiting and row return perform bounded animation",
          "[status-terminal][F02676]") {
  World world;
  world.list = {1, 1};
  REQUIRE(st::UpdateEquipmentList(world.list, world.detail, world, 0) == 0);
  CHECK(world.frame.animation_step == 0);
  REQUIRE(st::UpdateEquipmentList(world.list, world.detail, world, 0) == 0);
  CHECK(world.list.phase == 2);
  world.list.phase = 4;
  world.rows[4].x = 220;
  REQUIRE(st::UpdateEquipmentList(world.list, world.detail, world, 0) == 0);
  CHECK(world.rows[4].state == fixture::fw::RowState::kSlideX);
  CHECK(world.rows[4].target0 == 180);
  CHECK(world.rows[4].selected == 1);
}

TEST_CASE("F02676 negative selection preserves word result and zero-limb byte",
          "[status-terminal][F02676]") {
  World world;
  world.list.phase = 2;
  world.selected_row = -3;
  world.actors[0].limb_names.fill(0);
  REQUIRE(st::UpdateEquipmentList(world.list, world.detail, world, 0) == -2);
  CHECK(world.frame.selected_element == 255);
  CHECK(world.Count(Event::kComparison) == 1);
  CHECK(world.Count(Event::kClear) == 1);
}

TEST_CASE("F02676 detail cancellation remains a signed controller result",
          "[status-terminal][F02676]") {
  World world;
  world.list.phase = 5;
  world.detail = {0, 2, 0, 0, 9};
  world.frame.pressed = 16;
  REQUIRE(st::UpdateEquipmentList(world.list, world.detail, world, 0) == -2);
  CHECK(world.list.phase == 5);
  CHECK(world.Count(Event::kExitDetail) == 1);
}
