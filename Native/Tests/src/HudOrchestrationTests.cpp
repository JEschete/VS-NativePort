#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string_view>

#include <catch2/catch_test_macros.hpp>

#include "Battle/HudOrchestration.hpp"
#include "Battle/HudOrchestrationFixture.hpp"

namespace {
namespace b = VS::Battle;
namespace f = hud_orchestration_fixture;

std::size_t Calls(const f::World& world, int kind) {
  return std::ranges::count_if(world.events, [kind](const auto& event) {
    return event.front() == kind;
  });
}
}  // namespace

TEST_CASE("F00729 retains the low signed halfword rather than a boolean",
          "[hud-orchestration][F00729]") {
  b::HudFeedbackState state{.ashley_label_suppression = -17};
  for (const int command : {0, 1, 2, -1, 0x12348001, 0x10000}) {
    b::SetActorMarkersHidden(state, command);
    CHECK(state.marker_visibility_command ==
          static_cast<std::int16_t>(static_cast<std::uint16_t>(command)));
    CHECK(state.ashley_label_suppression == -17);
  }
}

TEST_CASE("F00730 bulk commands fill exactly sixteen bytes",
          "[hud-orchestration][F00730]") {
  std::array<std::byte, 18> owner{};
  owner.fill(std::byte{0x5A});
  auto markers = std::span(owner).subspan<1, b::kActorMarkerCount>();
  REQUIRE(b::SetActorMarker(markers, 0, 0x1234ABC0));
  CHECK(std::ranges::all_of(markers,
                            [](auto byte) { return byte == std::byte{0xC0}; }));
  CHECK(owner.front() == std::byte{0x5A});
  CHECK(owner.back() == std::byte{0x5A});
}

TEST_CASE("F00730 pin wins over unpin and low kinds retain their pin",
          "[hud-orchestration][F00730]") {
  std::array<std::byte, b::kActorMarkerCount> markers{};
  markers[15] = std::byte{0x2B};
  REQUIRE(b::SetActorMarker(markers, 15, 0x1C0));
  CHECK(markers[15] == std::byte{0xAB});
  REQUIRE(b::SetActorMarker(markers, 15, 0x43));
  CHECK(markers[15] == std::byte{0x2B});
  REQUIRE(b::SetActorMarker(markers, 15, 0x80));
  REQUIRE(b::SetActorMarker(markers, 15, 0x12340007));
  CHECK(markers[15] == std::byte{0x87});
}

TEST_CASE("F00730 rejects nonzero slots without byte truncation or mutation",
          "[hud-orchestration][F00730]") {
  std::array<std::byte, b::kActorMarkerCount> markers{};
  markers[15] = std::byte{0x87};
  REQUIRE_FALSE(b::SetActorMarker(markers, -1, 0));
  REQUIRE_FALSE(b::SetActorMarker(markers, 16, 0));
  REQUIRE_FALSE(b::SetActorMarker(markers, 256, 0));
  CHECK(markers.front() == std::byte{0});
  CHECK(markers[15] == std::byte{0x87});
}

TEST_CASE("F00743 suppression is idempotent and keeps marker commands",
          "[hud-orchestration][F00743]") {
  b::HudFeedbackState state{.marker_visibility_command = -123,
                            .ashley_label_suppression = -1};
  for (int repeat = 0; repeat < 2; ++repeat) {
    b::SuppressAshleyStatusLabels(state);
    CHECK(state.ashley_label_suppression == 1);
    CHECK(state.marker_visibility_command == -123);
  }
}

TEST_CASE("F01361 hidden animation consumes noncanonical requests lazily",
          "[hud-orchestration][F01361]") {
  f::World world;
  world.state.commands.shortcut = 255;
  world.state.shortcut_step = 11;
  world.refuse = 2;
  REQUIRE(b::DrawShortcutHud(world));
  CHECK(world.state.shortcut_step == 10);
  CHECK(world.state.commands.shortcut == 0);
  CHECK(world.events.empty());
  world.refuse = 0;
  REQUIRE(b::DrawShortcutHud(world));
  CHECK(world.state.shortcut_step == 10);
  CHECK(world.events.empty());
}

TEST_CASE("F01361 draws both rows and reverse digits with packed carries",
          "[hud-orchestration][F01361]") {
  f::World world;
  world.state.commands.shortcut = 1;
  world.displacement[0] = 255;
  world.markers[0][0] = {.x = 0, .y = 255};
  world.icons[1][0] = {.position = 0xFF00FF, .size = -1};
  world.digits[3] = std::numeric_limits<std::int32_t>::max();
  REQUIRE(b::DrawShortcutHud(world));
  CHECK(Calls(world, 1) == 8);
  CHECK(Calls(world, 2) == 8);
  REQUIRE(world.sprite_count == 13);
  CHECK(world.events[1][1] == 0xFFFF01);
  CHECK(world.sprites[5].request.packed_position == 0xFF029C);
  CHECK(world.sprites[6].request.packed_position ==
        std::numeric_limits<std::int32_t>::min() + 414);
  CHECK(world.sprites[6].texture == 0x37FB8030);
}

TEST_CASE("F01361 reads icon layout after marker and texture after sprite",
          "[hud-orchestration][F01361]") {
  f::World world;
  world.state.commands.shortcut = 1;
  world.tones.fill(0);
  world.tones[4] = 0x180;
  world.callback = [](auto& owner, const auto& event) {
    if (event[0] == 2) {
      owner.icons[0][0] = {.position = 0x120034, .size = 0x560078};
      owner.state.commands.shortcut = 77;
    }
    if (event[0] == 3) owner.textures[0][0] = 0xABCD;
  };
  REQUIRE(b::DrawShortcutHud(world));
  CHECK(world.sprites[0].request.packed_position == 0x120034);
  CHECK(world.sprites[0].request.packed_size == 0x560078);
  CHECK(world.sprites[0].texture == 0x373DABCD);
  CHECK(world.state.commands.shortcut == 0);
}

TEST_CASE("F01361 refuses missing used services without consuming its request",
          "[hud-orchestration][F01361]") {
  for (const int refusal : {1, 2, 3, 4, 5, 6, 7, 8, 9}) {
    f::World world;
    world.state.commands.shortcut = refusal == 1 ? 0 : 7;
    world.refuse = refusal;
    REQUIRE_FALSE(b::DrawShortcutHud(world));
    CHECK(world.state.commands.shortcut == (refusal == 1 ? 0 : 7));
  }
}

TEST_CASE("F01362 shortcut and indicator precede the fully hidden return",
          "[hud-orchestration][F01362]") {
  f::World world;
  world.state.commands.visibility = b::HudVisibilityCommand{255};
  world.state.commands.battle_mode = b::HudBattleModeCommand{1};
  world.state.slide = 0x200000;
  world.state.indicator_phase = 123;
  world.ticks = -1;
  world.sine = -33;
  REQUIRE(b::DrawBattleHud(world));
  REQUIRE(world.events.size() == 3);
  CHECK(world.events[0][0] == 4);
  CHECK(world.events[2][0] == 5);
  CHECK(world.state.indicator_step == 1);
  CHECK(world.state.indicator_phase == 65472);
  CHECK(world.sprites[0].request.packed_position == -0x210000 + 0xEC);
  CHECK(world.sprites[0].color == 0x6640403E);
}

TEST_CASE("F01362 only exact mode one grows its indicator",
          "[hud-orchestration][F01362]") {
  for (const std::uint8_t mode : {0, 2, 3, 129, 255}) {
    f::World world;
    world.state.commands.battle_mode = b::HudBattleModeCommand{mode};
    world.state.commands.visibility = b::HudVisibilityCommand{1};
    world.state.slide = 0x200000;
    world.state.indicator_step = 255;
    REQUIRE(b::DrawBattleHud(world));
    CHECK(world.state.indicator_step == 254);
  }
}

TEST_CASE("F01362 keeps late consumer color and phase mutations",
          "[hud-orchestration][F01362]") {
  f::World world;
  world.state.indicator_step = 3;
  world.state.commands.visibility = b::HudVisibilityCommand{1};
  world.state.slide = 0x200000;
  world.sine = 96;
  world.callback = [](auto& owner, const auto& event) {
    if (event[0] == 5) {
      owner.sprites[0].color = 0xFFFFFFFF;
      owner.state.indicator_phase = 65530;
      owner.ticks = 4;
    }
  };
  REQUIRE(b::DrawBattleHud(world));
  CHECK(world.sprites[0].color == 2);
  CHECK(world.state.indicator_phase == 250);
}

TEST_CASE("F01362 refreshes each limb mode and current condition after queuing",
          "[hud-orchestration][F01362]") {
  f::World world;
  world.limbs[6] = {.position = 0x100, .size = 0x200};
  world.limb_colors[0] = 0x123456;
  world.callback = [](auto& owner, const auto& event) {
    if (event[0] == 3) {
      owner.state.commands.battle_mode = b::HudBattleModeCommand{1};
      owner.conditions.fill(b::HudLimbCondition::kCritical);
    }
    if (event[0] == 9) owner.limb_textures[0] = 0xABCDEF;
  };
  REQUIRE(b::DrawBattleHud(world));
  CHECK(world.sprites[0].color == 0x64123456);
  CHECK(world.sprites[0].texture == 0xABCDEF);
  CHECK(world.sprites[1].request.packed_position == 0xB80108);
  CHECK(world.sprites[1].request.packed_size == 0x200);
  CHECK(Calls(world, 9) == 5);
}

TEST_CASE("F01362 carousel sentinel shifts mask and callbacks refresh icons",
          "[hud-orchestration][F01362]") {
  f::World world;
  world.state.commands.lower_screen = 2;
  world.state.carousel = {255, 31, 255, 12};
  world.status_flags = 0x80000001;
  world.ticks = 4;
  world.callback = [](auto& owner, const auto& event) {
    if (event[0] == 10) {
      owner.state.carousel[1] = 9;
      owner.state.carousel[0] = 8;
    }
  };
  REQUIRE(b::DrawBattleHud(world));
  CHECK(world.state.carousel[3] == 14);
  CHECK(world.state.carousel[2] == 0);
  CHECK(Calls(world, 10) == 4);
  const auto second = std::ranges::find_if(
      world.events, [](const auto& event) { return event[0] == 10; });
  REQUIRE(second != world.events.end());
  CHECK((*second)[1] == 0);
  CHECK((*(second + 1))[1] == 9);
  CHECK((*(second + 3))[1] == 8);
}

TEST_CASE("F01362 status counts keep both rows and the pre-rotation selection",
          "[hud-orchestration][F01362]") {
  for (const auto flags : {0U, 1U, 3U, 7U, 255U, 511U, 0xFFFFFFFFU}) {
    f::World world;
    world.state.commands.lower_screen = 2;
    world.state.carousel = {0, 8, 1, 12};
    world.status_flags = flags;
    REQUIRE(b::DrawBattleHud(world));
    CHECK(Calls(world, 10) == (flags == 0 ? 0 : 4));
    CHECK(world.sprite_count ==
          (flags == 0
               ? 0
               : (flags == 1 ? 1 : (flags == 3 ? 2 : (flags < 256 ? 1 : 3)))));
  }
}

TEST_CASE("F01362 missing used data refuses but suppressed data stays lazy",
          "[hud-orchestration][F01362]") {
  for (const int refusal : {9, 10, 11, 12, 13, 14, 15, 16, 17, 18}) {
    f::World world;
    world.status_flags = 1;
    world.refuse = refusal;
    if (refusal == 16) world.risk = 0;
    REQUIRE_FALSE(b::DrawBattleHud(world));
  }
  f::World world;
  world.state.commands.lower_screen = 2;
  world.state.status_icons_hidden = 128;
  world.refuse = 17;
  REQUIRE(b::DrawBattleHud(world));
  CHECK(Calls(world, 8) == 0);
}

TEST_CASE("F01384 formats bytes without clamping and preserves punctuation",
          "[hud-orchestration][F01384]") {
  std::array<char, 9> text{'0', '0', '-', '0', '0', '/', '0', '0', '!'};
  b::FormatPuzzleTimer({.frames = -1, .seconds = 34, .minutes = 12}, 56, text);
  CHECK(std::string_view(text.data(), text.size()) == "12-34/56!");
  b::FormatPuzzleTimer({.seconds = -1, .minutes = -128}, 255, text);
  CHECK(std::string_view(text.data(), text.size()) == "<8-I5/I5!");
}

TEST_CASE("F01384 any nonzero hours saturate and repeat entry restores digits",
          "[hud-orchestration][F01384]") {
  std::array<char, 9> text{'0', '0', ':', '0', '0', ':', '0', '0', '\0'};
  b::FormatPuzzleTimer({.seconds = -1, .minutes = -1, .hours = -128}, 255,
                       text);
  CHECK(std::string_view(text.data()) == "59:59:99");
  b::FormatPuzzleTimer({}, 0, text);
  CHECK(std::string_view(text.data()) == "00:00:00");
}
