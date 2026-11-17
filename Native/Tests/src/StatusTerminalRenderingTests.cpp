#include <cstdint>

#include <catch2/catch_test_macros.hpp>

#include "Menu/StatusTerminalWorld.hpp"

namespace {
namespace st = VS::Menu::Status;
using status_terminal_tests::Event;
using status_terminal_tests::World;
}  // namespace

TEST_CASE("F02677 disabled fade requires no queue space",
          "[status-terminal][F02677]") {
  World world;
  world.packets.available = false;
  status_limb_tests::PacketChannel queue(world.packets, 0);
  REQUIRE(st::DrawModelSlideFade(queue, false, false));
  CHECK(world.packets.words.empty());
  CHECK_FALSE(st::DrawModelSlideFade(queue, true, false));
}

TEST_CASE("F02677 complete gradient switches framebuffer and preserves links",
          "[status-terminal][F02677]") {
  World world;
  status_limb_tests::PacketChannel queue(world.packets, 0);
  REQUIRE(st::DrawModelSlideFade(queue, true, false));
  REQUIRE(world.packets.words.size() == 14);
  CHECK(world.packets.words[0] == 0x0D123456);
  CHECK(world.packets.words[7] == 0x01050060);
  CHECK(world.packets.words[13] == 0x0000F060);
  REQUIRE(st::DrawModelSlideFade(queue, true, true));
  CHECK(world.packets.words[14] == 0x0D700000);
  CHECK(world.packets.words[21] == 0x01000060);
}

TEST_CASE("F02678 emits three linked background packets",
          "[status-terminal][F02678]") {
  World world;
  status_limb_tests::PacketChannel queue(world.packets, 1);
  REQUIRE(st::DrawBackground(queue, true));
  REQUIRE(world.packets.words.size() == 37);
  CHECK(world.packets.words[0] == 0x0D234567);
  CHECK(world.packets.words[14] == 0x08700000);
  CHECK(world.packets.words[23] == 0x0D700038);
  CHECK(world.packets.words[30] == 0x010000A0);
  CHECK(world.packets.heads[0] == 0x123456);
  CHECK(world.packets.heads[1] == 0x70005C);
}

TEST_CASE("F02678 refuses a full packet owner", "[status-terminal][F02678]") {
  World world;
  world.packets.available = false;
  status_limb_tests::PacketChannel queue(world.packets, 0);
  CHECK_FALSE(st::DrawBackground(queue, false));
  CHECK(world.packets.words.empty());
}

TEST_CASE("F02679 camera uses unsigned model dimensions and leaves padding",
          "[status-terminal][F02679]") {
  World world;
  world.model = {65535, 32768};
  world.frame.yaw = 0;
  world.frame.pitch = 0;
  world.frame.animation_step = 128;
  world.view.camera.angles_padding = 0x12345678;
  REQUIRE(st::DrawFrame(world).has_value());
  CHECK(world.view.camera.position.x == 1073725440);
  CHECK(world.view.camera.position.y == -67108864);
  CHECK(world.view.camera.look_at.z == 0);
  CHECK(world.view.camera.angles_padding == 0x12345678);
  CHECK(world.Count(Event::kPreview) == 1);
  CHECK(world.events.back()[0] == static_cast<std::uint32_t>(Event::kVitals));
}

TEST_CASE("F02679 delay byte skips preview but still emits frame",
          "[status-terminal][F02679]") {
  World world;
  world.frame.model_delay = 255;
  world.frame.fade = 1;
  REQUIRE(st::DrawFrame(world).has_value());
  CHECK(world.frame.model_delay == 254);
  CHECK(world.Count(Event::kPreview) == 0);
  CHECK(world.packets.words.size() == 51);
  CHECK(world.Count(Event::kLimb) == 1);
  world.model_present = false;
  CHECK_FALSE(st::DrawFrame(world).has_value());
}
