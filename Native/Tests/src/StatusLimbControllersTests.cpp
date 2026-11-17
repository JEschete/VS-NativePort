#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <span>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Input/Pad.hpp"
#include "Menu/Status/LimbControllers.hpp"
#include "Menu/StatusLimbControllersWorld.hpp"

namespace {
namespace status = VS::Menu::Status;
namespace render = VS::Render;
using status_limb_tests::Event;
using status_limb_tests::EventKind;
using status_limb_tests::PacketChannel;
using status_limb_tests::World;
constexpr std::array<std::uint8_t, 3> kName{0x10, 0x11, 0xE7};
constexpr std::array<std::uint8_t, 2> kCondition{0x30, 0xE7};
constexpr std::array<std::uint8_t, 6> kFormat{0xFF, 0, 0x8F, 0xFF, 1, 0xE7};
}  // namespace

TEST_CASE("F02662 draws nested tiles with packed carry and masked shifts",
          "[status-limbs][F02662]") {
  World world;
  world.palette[4] = 0xF1234567;
  PacketChannel queue(world.packets, 0);
  const auto result = status::DrawLimbConditionIndicator(
      queue, world.palette,
      {.condition = 4, .position = 0x1234FFFF, .dimming = 33});
  CHECK(result.has_value());
  CHECK(world.packets.words ==
        std::vector<std::uint32_t>{0x06123456, 0x7891A2B3, 0x1234FFFF,
                                   0x00070007, 0xF1234566, 0x12360000,
                                   0x00050005});
  CHECK(world.packets.heads[0] == 0x700000);
}

TEST_CASE("F02662 refuses out of range palette and unavailable packets",
          "[status-limbs][F02662]") {
  World world;
  PacketChannel queue(world.packets, 0);
  CHECK_FALSE(status::DrawLimbConditionIndicator(queue, world.palette,
                                                 {.condition = -1})
                  .has_value());
  CHECK_FALSE(status::DrawLimbConditionIndicator(queue, world.palette,
                                                 {.condition = 10})
                  .has_value());
  world.packets.available = false;
  CHECK_FALSE(
      status::DrawLimbConditionIndicator(queue, world.palette, {}).has_value());
  CHECK(world.packets.words.empty());
  CHECK(world.packets.heads[0] == 0x123456);
}

TEST_CASE("F02663 uses signed anchors and both original chain tags",
          "[status-limbs][F02663]") {
  World world;
  PacketChannel queue(world.packets, 1);
  CHECK(status::DrawLimbLeaderLine(queue,
                                   {.limb = 1,
                                    .position = render::PackScreenWord(-12, 55),
                                    .opacity = 1})
            .has_value());
  CHECK(world.packets.words ==
        std::vector<std::uint32_t>{0x08234567, 0xE1000000, 0x40D0E030,
                                   0x0037FFF4, 0x003AFFF6, 0x03234567,
                                   0x40D0E030, 0x003AFFF6, 0x003A00D8});
  CHECK_FALSE(status::DrawLimbLeaderLine(queue, {.limb = 6}).has_value());
  CHECK(world.packets.words.size() == 9);
}

TEST_CASE("F02664 places elemental markers before icons and wraps x",
          "[status-limbs][F02664]") {
  World world;
  world.texture_offsets[0] = 0x8123;
  world.texture_offsets[16] = 0x4567;
  const auto result = status::DrawStatusEffectIcons(world, 0x10001, 0x10010,
                                                    world.texture_offsets);
  CHECK(result.has_value());
  REQUIRE(world.packets.words.size() == 18);
  CHECK(world.packets.words[3] == 0x00900000);
  CHECK(world.packets.words[4] == 0x37F88123);
  CHECK(world.packets.words[9] == 0x00900018);
  CHECK(world.packets.words[10] == 0x37FF3068);
  CHECK(world.packets.words[15] == 0x00900010);
  CHECK(world.packets.words[16] == 0x37F94567);
}

TEST_CASE("F02664 covers all rows and empty or unavailable render targets",
          "[status-limbs][F02664]") {
  World world;
  CHECK(status::DrawStatusEffectIcons(world, 0, 0, world.texture_offsets)
            .has_value());
  CHECK(world.packets.words.empty());
  CHECK(
      status::DrawStatusEffectIcons(world, 0xFFFFFFFF, 0, world.texture_offsets)
          .has_value());
  CHECK(world.packets.words.size() == 48 * render::kSpritePacketWords);
  CHECK(world.packets.words[world.packets.words.size() - 3] == 0x00C00080);
  world.packets.available = false;
  CHECK_FALSE(status::DrawStatusEffectIcons(world, 1, 0, world.texture_offsets)
                  .has_value());
}

TEST_CASE("F02665 rebuilds sparse effects preserving the unused tail",
          "[status-limbs][F02665]") {
  World world;
  world.navigation.active.fill(0xAA);
  world.navigation.selected = 31;
  world.navigation_frame.selected_element = 11;
  world.cursor_return = 0x1FF;
  const auto result = status::NavigateStatusEffects(
      world.navigation, world.navigation_frame,
      status::StatusEffectTarget{.actor_slot = 4, .flags = 0x80010101}, true,
      world);
  CHECK(result == 0);
  CHECK(world.navigation.count == 4);
  CHECK(world.navigation.selected == 0);
  CHECK(world.navigation.active[3] == 31);
  CHECK(world.navigation.active[4] == 0xAA);
  CHECK(world.navigation.target_slot == 4);
  CHECK(world.navigation.cursor_animation == 255);
  CHECK(world.navigation_frame.cursor_color == 1);
}

TEST_CASE("F02665 clamps entry selection or preserves it without cursor",
          "[status-limbs][F02665]") {
  World world;
  world.navigation.selected = 31;
  CHECK(status::NavigateStatusEffects(
            world.navigation, world.navigation_frame,
            status::StatusEffectTarget{.flags = 0x105}, false, world) == 0);
  CHECK(world.navigation.selected == 2);
  CHECK(world.events.size() == 1);
  CHECK(status::NavigateStatusEffects(
            world.navigation, world.navigation_frame,
            status::StatusEffectTarget{.flags = 0xFFFF}, true, world) == 0);
  CHECK(world.navigation.selected == 7);
  CHECK(status::NavigateStatusEffects(world.navigation, world.navigation_frame,
                                      status::StatusEffectTarget{.flags = 0},
                                      true, world) == 1);
  CHECK(world.navigation.count == 16);
}

TEST_CASE("F02665 directional priority exits without committing selection",
          "[status-limbs][F02665]") {
  World world;
  world.navigation.count = 16;
  world.navigation.selected = 7;
  world.navigation_frame.repeat = VS::Input::Right;
  CHECK(status::NavigateStatusEffects(world.navigation, world.navigation_frame,
                                      {}, true, world) == 1);
  CHECK(world.navigation_frame.selected_element == 6);
  CHECK(world.navigation.selected == 7);
  world.navigation_frame.repeat = VS::Input::Up | VS::Input::Down;
  CHECK(status::NavigateStatusEffects(world.navigation, world.navigation_frame,
                                      {}, true, world) == 1);
  CHECK(world.navigation_frame.selected_element == 11);
  CHECK(world.events.size() == 2);
}

TEST_CASE("F02665 down clamps the partial row before left and right",
          "[status-limbs][F02665]") {
  World world;
  world.navigation.count = 10;
  world.navigation.selected = 5;
  world.navigation.active[8] = 28;
  world.navigation_frame.repeat = VS::Input::Down | VS::Input::Left;
  CHECK(status::NavigateStatusEffects(world.navigation, world.navigation_frame,
                                      {}, true, world) == 0);
  CHECK(world.navigation.selected == 8);
  CHECK(world.description == 28);
  CHECK(world.events.front() ==
        Event{static_cast<std::int64_t>(EventKind::kSound)});
  CHECK(world.events.back() ==
        Event{static_cast<std::int64_t>(EventKind::kCursor), 0, 0x00980002});
}

TEST_CASE("F02665 buttons preempt rotation and rotation wraps before clamping",
          "[status-limbs][F02665]") {
  World world;
  auto& frame = world.navigation_frame;
  frame.pressed = VS::Input::Triangle | VS::Input::Cross;
  frame.held = VS::Input::Square | VS::Input::Down | VS::Input::Right;
  frame.stick_x = frame.stick_y = 128;
  frame.yaw = std::numeric_limits<std::int32_t>::max();
  frame.pitch = 10;
  CHECK(status::NavigateStatusEffects(world.navigation, frame, {}, true,
                                      world) == 2);
  CHECK(frame.pitch == 10);
  frame.pressed = 0;
  CHECK(status::NavigateStatusEffects(world.navigation, frame, {}, true,
                                      world) == 0);
  CHECK(frame.yaw == std::numeric_limits<std::int32_t>::min() + 63);
  CHECK(frame.pitch == 0);
  CHECK(world.events.empty());
}

TEST_CASE("F02665 refuses uninitialized or corrupt navigation explicitly",
          "[status-limbs][F02665]") {
  World world;
  CHECK_FALSE(status::NavigateStatusEffects(
                  world.navigation, world.navigation_frame, {}, true, world)
                  .has_value());
  world.navigation.count = 1;
  world.navigation.active[0] = 32;
  CHECK_FALSE(status::NavigateStatusEffects(
                  world.navigation, world.navigation_frame, {}, true, world)
                  .has_value());
  CHECK(world.events.empty());
}

TEST_CASE("F02668 commands preserve byte truncation and six staggered steps",
          "[status-limbs][F02668]") {
  World world;
  world.panel_frame.selected_actor = 0x103;
  CHECK(status::UpdateLimbPanel(world.panel, world.panel_frame, -9, world) ==
        0);
  CHECK(world.panel.actor_slot == 2);
  CHECK(world.panel.animation_steps ==
        std::array<std::int32_t, 6>{12, 13, 14, 15, 16, 17});
  CHECK(status::UpdateLimbPanel(world.panel, world.panel_frame, -3, world) ==
        1);
  CHECK(status::UpdateLimbPanel(world.panel, world.panel_frame, 0x182, world) ==
        0);
  CHECK(world.panel_frame.selection_and_dimming == 0x81);
  CHECK(status::UpdateLimbPanel(world.panel, world.panel_frame, -2, world) ==
        0);
  CHECK(world.panel.phase == 3);
}

TEST_CASE("F02668 entering completion follows the last named limb only",
          "[status-limbs][F02668]") {
  World world;
  world.names = {1, 2, 0, 0, 0, 0};
  world.panel.phase = 1;
  world.panel.animation_steps = {4, 0, 19, 20, 21, 22};
  CHECK(status::UpdateLimbPanel(world.panel, world.panel_frame, 0, world) == 0);
  CHECK(world.panel.phase == 2);
  CHECK(world.panel.animation_steps[0] == 3);
  CHECK(world.panel.animation_steps[2] == 19);
  CHECK(world.packets.words.size() == 32);
  CHECK(world.events.size() == 6);
}

TEST_CASE("F02668 refuses zero limbs and invalid animation before drawing",
          "[status-limbs][F02668]") {
  World world;
  world.panel.phase = 1;
  world.names.fill(0);
  const auto empty =
      status::UpdateLimbPanel(world.panel, world.panel_frame, 0, world);
  REQUIRE_FALSE(empty.has_value());
  CHECK(empty.error() == status::LimbControllerError::kEmptyEnteringPanel);
  world.names[0] = 1;
  world.panel.animation_steps[0] = -1;
  CHECK_FALSE(status::UpdateLimbPanel(world.panel, world.panel_frame, 0, world)
                  .has_value());
  CHECK(world.panel.phase == 1);
  CHECK(world.events.empty());
  CHECK(world.packets.words.empty());
}

TEST_CASE("F02668 dimming suppresses lines but not projection",
          "[status-limbs][F02668]") {
  World world;
  world.names = {1, 2, 0, 0, 0, 0};
  world.panel.phase = 2;
  world.panel_frame.selection_and_dimming = 0x81;
  CHECK(status::UpdateLimbPanel(world.panel, world.panel_frame, 0, world) == 0);
  CHECK(world.packets.words.size() == 14);
  REQUIRE(world.events.size() == 6);
  CHECK(world.events[0][3] == 0x404040);
  CHECK(world.events[3][3] == 0x808080);
  CHECK(world.events[2][0] ==
        static_cast<std::int64_t>(EventKind::kProjection));
  CHECK(world.events[5][0] ==
        static_cast<std::int64_t>(EventKind::kProjection));
}

TEST_CASE("F02668 leaving becomes idle when any limb already reached eight",
          "[status-limbs][F02668]") {
  World world;
  world.names = {1, 2, 3, 0, 0, 0};
  world.panel.phase = 3;
  world.panel.animation_steps = {7, 8, 0, 12, 13, 14};
  CHECK(status::UpdateLimbPanel(world.panel, world.panel_frame, 0, world) == 0);
  CHECK(world.panel.phase == 0);
  CHECK(world.panel.animation_steps ==
        std::array<std::int32_t, 6>{8, 8, 1, 12, 13, 14});
  CHECK(world.packets.words.size() == 14);
  CHECK(world.events.size() == 4);
}

TEST_CASE("F02680 formats borrowed arguments before selecting the limb",
          "[status-limbs][F02680]") {
  World world;
  render::EncodedTextArguments arguments;
  arguments.integers[7] = 0x1234;
  std::array<std::uint8_t, 16> output;
  output.fill(0xAA);
  CHECK(status::ShowSelectedLimbCondition(
            4, {.name = kName, .condition = kCondition, .format = kFormat},
            arguments, output, world)
            .has_value());
  CHECK(std::ranges::equal(
      std::span(output).first<5>(),
      std::array<std::uint8_t, 5>{0x10, 0x11, 0x8F, 0x30, 0xE7}));
  CHECK(output[5] == 0xAA);
  CHECK(arguments.strings[0].data() == kName.data());
  CHECK(arguments.integers[7] == 0x1234);
  CHECK(world.panel_frame.selection_and_dimming == 4);
}

TEST_CASE(
    "F02680 short output refuses without publishing arguments or selection",
    "[status-limbs][F02680]") {
  World world;
  render::EncodedTextArguments arguments;
  std::array<std::uint8_t, 2> output{0xAB, 0xCD};
  CHECK_FALSE(status::ShowSelectedLimbCondition(
                  1,
                  {.name = kName, .condition = kCondition, .format = kFormat},
                  arguments, output, world)
                  .has_value());
  CHECK(output == std::array<std::uint8_t, 2>{0xAB, 0xCD});
  CHECK(arguments.strings[0].empty());
  CHECK(world.events.empty());
  CHECK(world.selection_request == 0);
}

TEST_CASE("F02681 preserves signed defense bits and unrelated stat entries",
          "[status-limbs][F02681]") {
  World world;
  const std::array<std::int16_t, 8> affinities{-32768, -1, 0, 1,
                                               2,      3,  4, 32767};
  const std::array<std::int16_t, 4> types{-10, 20, -30, 40};
  std::array<std::int16_t, 64> stats;
  stats.fill(1234);
  CHECK(status::PopulateSelectedLimbDefense(
            5, {.affinities = affinities, .damage_types = types}, stats, world)
            .has_value());
  CHECK(stats[15] == 1234);
  CHECK(stats[16] == -32768);
  CHECK(stats[24] == -32768);
  CHECK(stats[31] == 32767);
  CHECK(stats[32] == -10);
  CHECK(stats[36] == 1234);
  CHECK(world.panel_frame.selection_and_dimming == 0x85);
}

TEST_CASE("F02681 sequential copying preserves source destination aliases",
          "[status-limbs][F02681]") {
  World world;
  std::array<std::int16_t, 64> stats{};
  for (std::size_t index = 0; index < stats.size(); ++index) {
    stats[index] = static_cast<std::int16_t>(index);
  }
  const auto source = std::span(stats).subspan<15, 8>();
  const auto types = std::span(stats).subspan<12, 4>();
  CHECK(status::PopulateSelectedLimbDefense(
            0, {.affinities = source, .damage_types = types}, stats, world)
            .has_value());
  CHECK(stats[16] == 15);
  CHECK(stats[31] == 15);
  CHECK(stats[32] == 12);
  CHECK(stats[35] == 15);
  CHECK(stats[36] == 36);
}

TEST_CASE("F02681 refuses a short destination and invalid limb before stores",
          "[status-limbs][F02681]") {
  World world;
  const std::array<std::int16_t, 8> affinities{};
  const std::array<std::int16_t, 4> types{};
  std::array<std::int16_t, 35> stats{};
  stats[16] = 123;
  CHECK_FALSE(
      status::PopulateSelectedLimbDefense(
          0, {.affinities = affinities, .damage_types = types}, stats, world)
          .has_value());
  CHECK_FALSE(
      status::PopulateSelectedLimbDefense(
          6, {.affinities = affinities, .damage_types = types}, stats, world)
          .has_value());
  CHECK(stats[16] == 123);
  CHECK(world.selection_request == 0);
}
