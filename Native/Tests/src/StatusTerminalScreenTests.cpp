#include <cstdint>

#include <catch2/catch_test_macros.hpp>

#include "Menu/StatusTerminalWorld.hpp"

namespace {
namespace st = VS::Menu::Status;
using status_terminal_tests::Event;
using status_terminal_tests::World;
}  // namespace

TEST_CASE("F02682 initializes owned state and saves the real camera",
          "[status-terminal][F02682]") {
  World world;
  world.projection = 333;
  world.view.camera.position = {9, 8, 7};
  world.view.camera.angles_padding = 0x76543210;
  const auto previous = world.view;
  std::uint8_t phase = 3;
  REQUIRE(st::UpdateStatusTerminal(world.screen, world, phase) == 0);
  CHECK(phase == 5);
  CHECK(world.screen.screen_enabled == 0);
  CHECK(world.screen.saved_projection == 333);
  CHECK(world.screen.saved_view == previous);
  CHECK(world.projection == 512);
  CHECK(world.frame.yaw == 2816);
  CHECK(world.Count(Event::kBuildCamera) == 0);
}

TEST_CASE("F02682 does not enable rendering before menu readiness",
          "[status-terminal][F02682]") {
  World world;
  world.ready = false;
  world.item_text_present = false;
  world.screen.screen_enabled = 1;
  std::uint8_t phase = 3;
  REQUIRE(st::UpdateStatusTerminal(world.screen, world, phase) == 0);
  CHECK(phase == 3);
  CHECK(world.screen.screen_enabled == 0);
  CHECK(world.Count(Event::kLoadText) == 1);
  CHECK(world.Count(Event::kBuildCamera) == 0);
}

TEST_CASE("F02682 exit restores view and preserves independent owners",
          "[status-terminal][F02682]") {
  World world;
  world.screen.saved_projection = 444;
  world.screen.saved_view.camera.position = {11, 12, 13};
  world.screen.screen_enabled = 1;
  world.frame.current_menu = 9;
  std::uint8_t phase = 13;
  REQUIRE(st::UpdateStatusTerminal(world.screen, world, phase) == 0);
  CHECK(phase == 14);
  CHECK(world.view == world.screen.saved_view);
  CHECK(world.projection == 444);
  CHECK(world.screen.animation_wait == 8);
  CHECK(world.screen.screen_enabled == 0);
  CHECK(world.frame.current_menu == 9);
  CHECK(world.Count(Event::kBuildCamera) == 0);
}

TEST_CASE("F02682 successful unload returns before frame and prompt",
          "[status-terminal][F02682]") {
  World world;
  world.screen.screen_enabled = 1;
  world.screen.animation_wait = -1;
  std::uint8_t phase = 14;
  REQUIRE(st::UpdateStatusTerminal(world.screen, world, phase) == 1);
  CHECK(phase == 0);
  CHECK(world.frame.current_menu == 5);
  CHECK(world.Count(Event::kUnloadText) == 1);
  CHECK(world.Count(Event::kBuildCamera) == 0);
  CHECK(world.Count(Event::kPromptBackground) == 0);
}

TEST_CASE("F02682 rotates through production dead-zone mapping",
          "[status-terminal][F02682]") {
  World world;
  world.frame.held = 1 | 0x6000;
  world.frame.pitch = 10;
  world.frame.yaw = 100;
  world.frame.selected_element = 0;
  world.screen.prompt_animation = 10;
  std::uint8_t phase = 5;
  REQUIRE(st::UpdateStatusTerminal(world.screen, world, phase) == 0);
  CHECK(world.frame.yaw == 164);
  CHECK(world.frame.pitch == 0);
  CHECK(phase == 5);
}

TEST_CASE("F02682 defense wraps valid rows and retains full cursor word",
          "[status-terminal][F02682]") {
  World world;
  world.frame.stat_page = 2;
  world.frame.selected_stat = 250;
  world.frame.selected_element = 5;
  world.frame.repeat = 0x4000;
  world.cursor_result = 0x12345678;
  std::uint8_t phase = 9;
  REQUIRE(st::UpdateStatusTerminal(world.screen, world, phase) == 0);
  CHECK(world.frame.selected_stat == 0);
  CHECK(world.screen.cursor_animation == 0x12345678);
  CHECK(world.Count(Event::kInformation) == 1);
}
