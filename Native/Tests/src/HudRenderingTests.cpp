#include <array>
#include <bit>
#include <cstdint>
#include <limits>

#include <catch2/catch_test_macros.hpp>

#include "Battle/HudRenderingFixture.hpp"
#include "Render/Sprites.hpp"

namespace {
namespace f = hud_rendering_fixture;
namespace b = VS::Battle;
namespace r = VS::Render;

f::World World() {
  f::World world;
  world.input = f::Seed();
  return world;
}
}  // namespace

TEST_CASE("F01354 consumes the maximum pen and keeps its gauge owner",
          "[hud-rendering][F01354]") {
  auto world = World();
  world.input[f::kCallbackChanges] = f::kNumberCallback;
  CHECK(b::DrawHudStatGauge(world, {.current = 99, .maximum = 100}));
  REQUIRE(world.events.size() == 5);
  if (world.events.size() != 5) return;
  CHECK(world.events[0][2] == 64);
  CHECK(world.events[2][2] == world.input[f::kFirstNumberPen]);
  CHECK(world.events[3][2] == 0x00080000U - 5U);
  CHECK(world.events[4][4] == 0x805000F4);
  CHECK(world.events[4][2] == 0x7FFF0001);
}

TEST_CASE("F01354 reads label UV after its sprite callback",
          "[hud-rendering][F01354]") {
  auto world = World();
  world.input[f::kCallbackChanges] = f::kSpriteCallback;
  CHECK(b::DrawHudStatGauge(
      world, {.kind = b::HudGaugeKind::kRisk, .current = 1, .maximum = 100}));
  REQUIRE(world.events.size() == 3);
  if (world.events.size() != 3) return;
  CHECK(world.events[1][2] == 0x00150040);
  CHECK(world.events[2][2] == 0x000A000A);
  CHECK(world.events[2][5] == 0x37F600F9);
}

TEST_CASE("F01354 fixed point rounding and division wrap as signed words",
          "[hud-rendering][F01354]") {
  auto world = World();
  CHECK(b::DrawHudStatGauge(
      world, {.kind = b::HudGaugeKind::kFixedPointRisk,
              .current = std::numeric_limits<std::int32_t>::max(),
              .maximum = -1}));
  REQUIRE(world.events.size() == 3);
  if (world.events.size() != 3) return;
  CHECK(world.events[0][2] == 66);
  CHECK(world.events[1][3] == 0xFF800000);
}

TEST_CASE("F01354 absent maximum leaves unused label and render services alone",
          "[hud-rendering][F01354]") {
  auto world = World();
  world.label_available = false;
  world.rendering_available = false;
  CHECK(b::DrawHudStatGauge(world, {.current = 99, .maximum = 0}));
  CHECK(world.events.empty());
  CHECK(world.label_reads == 0);
  CHECK(world.slide_reads == 1);
  world.layout_available = false;
  CHECK_FALSE(b::DrawHudStatGauge(world, {.maximum = 0}));
}

TEST_CASE("F01354 refuses only used missing presentation data",
          "[hud-rendering][F01354]") {
  auto world = World();
  world.label_available = false;
  CHECK_FALSE(b::DrawHudStatGauge(
      world, {.kind = b::HudGaugeKind::kRisk, .maximum = 100}));
  CHECK(world.events.size() == 2);
  auto failed = World();
  failed.rendering_available = false;
  CHECK_FALSE(b::DrawHudStatGauge(failed, {.maximum = 1}));
  CHECK(failed.events.empty());
  CHECK_FALSE(b::DrawHudStatGauge(
      failed, {.kind = b::HudGaugeKind::kCount, .maximum = 0}));
}

TEST_CASE("F01355 invisible icons do not read unused tables",
          "[hud-rendering][F01355]") {
  auto world = World();
  world.texture_available = false;
  CHECK(b::DrawSlidingStatusIcon(world, {}));
  CHECK(world.slide_reads == 0);
  world.input[f::kSlide] = 1;
  CHECK(b::DrawSlidingStatusIcon(world, {.icon = -1}));
  CHECK(world.texture_reads == 0);
  world.input[f::kSlide] = 0;
  CHECK_FALSE(b::DrawSlidingStatusIcon(world, {.icon = 32}));
  CHECK_FALSE(b::DrawSlidingStatusIcon(world, {.icon = 0}));
}

TEST_CASE("F01355 clips the base and aligns the overlay at its right edge",
          "[hud-rendering][F01355]") {
  auto world = World();
  CHECK(b::DrawSlidingStatusIcon(world, {.icon = 16, .slide = -1}));
  REQUIRE(world.events.size() == 2);
  if (world.events.size() != 2) return;
  CHECK(world.events[0][2] == 0x000800A0);
  CHECK(world.events[0][3] == 0x0010000F);
  CHECK(world.events[0][5] == 0x37F91081);
  CHECK(world.events[1][2] == 0x000800A7);
  CHECK(world.events[1][5] == 0x37FF3068);
}

TEST_CASE("F01355 misaligned and narrow overlays retain packed clipping",
          "[hud-rendering][F01355]") {
  auto world = World();
  CHECK(b::DrawSlidingStatusIcon(world, {.icon = 19, .slide = 1}));
  REQUIRE(world.events.size() == 2);
  if (world.events.size() != 2) return;
  CHECK(world.events[1][2] == 0x000800A9);
  CHECK(world.events[1][3] == 0x00080007);
  auto narrow = World();
  CHECK(b::DrawSlidingStatusIcon(narrow, {.icon = 18, .slide = -8}));
  REQUIRE(narrow.events.size() == 2);
  if (narrow.events.size() != 2) return;
  CHECK(narrow.events[1][5] == 0x37FF3078);
}

TEST_CASE("F01355 resamples the status owner after the first sprite",
          "[hud-rendering][F01355]") {
  auto world = World();
  world.input[f::kCallbackChanges] = f::kSpriteCallback;
  CHECK(b::DrawSlidingStatusIcon(world, {.icon = 16}));
  REQUIRE(world.events.size() == 2);
  if (world.events.size() != 2) return;
  CHECK(world.events[0][4] == 0x805000FC);
  CHECK(world.events[1][4] == 0x805100FC);
  CHECK(world.texture_reads == 1);
  CHECK(world.events[1][3] == 0x00080008);
}

TEST_CASE("F01355 does not clamp negative packed widths or overflowing slots",
          "[hud-rendering][F01355]") {
  auto world = World();
  CHECK(b::DrawSlidingStatusIcon(world, {.icon = 0, .slide = 17, .slot = -11}));
  REQUIRE(world.events.size() == 1);
  if (world.events.size() != 1) return;
  CHECK(world.events[0][2] == 0x00080001);
  CHECK(world.events[0][3] == 0xFFFFFFFF);
  CHECK(world.events[0][5] == 0x37F81080);
  world.rendering_available = false;
  CHECK_FALSE(b::DrawSlidingStatusIcon(world, {.icon = 16}));
}

TEST_CASE("F01358 maps spell shortcuts and retains packed transparency",
          "[hud-rendering][F01358]") {
  auto world = World();
  world.input[f::kSpellUnlock] = 0xFFFFFFFF;
  world.input[f::kRestrictions] = 0x20;
  const auto tone = b::ShortcutIconBrightness(world, 0);
  REQUIRE(tone.has_value());
  if (!tone.has_value()) return;
  CHECK(tone.value() == b::HudShortcutTone::kRestricted);
  REQUIRE(world.events.size() == 2);
  if (world.events.size() != 2) return;
  CHECK(world.events[0][0] == 5);
  CHECK(world.events[1][1] == 1);
  const r::SpriteRequest request{
      .draw_mode = r::kDefaultSpriteDrawMode,
      .brightness_transparency = static_cast<int>(tone.value())};
  CHECK(r::BuildSpritePacket(request, 0)[r::kSpriteColorWord] == 0x66404040);
}

TEST_CASE("F01358 disabled abilities remain hidden before restriction dimming",
          "[hud-rendering][F01358]") {
  auto world = World();
  world.input[f::kRestrictions] = 0xFFFFFFFF;
  const auto tone = b::ShortcutIconBrightness(world, 7);
  REQUIRE(tone.has_value());
  if (!tone.has_value()) return;
  CHECK(tone.value() == b::HudShortcutTone::kHidden);
  REQUIRE(world.events.size() == 2);
  if (world.events.size() != 2) return;
  CHECK(world.events[1][0] == 8);
  world.unlock_available = false;
  CHECK_FALSE(b::ShortcutIconBrightness(world, 0).has_value());
}

TEST_CASE("F01358 chain and defense use different selectors and ignore masks",
          "[hud-rendering][F01358]") {
  auto defense = World();
  defense.input[f::kAbilityUnlock] = 1;
  defense.input[f::kRestrictions] = 0xFFFFFFFF;
  const auto tone = b::ShortcutIconBrightness(defense, 5);
  REQUIRE(tone.has_value());
  if (!tone.has_value()) return;
  CHECK(tone.value() == b::HudShortcutTone::kEnabled);
  REQUIRE(defense.events.size() == 2);
  if (defense.events.size() != 2) return;
  CHECK(defense.events[1][1] == 1);
  auto chain = World();
  chain.input[f::kAbilityUnlock] = 1;
  CHECK(b::ShortcutIconBrightness(chain, 6).has_value());
  REQUIRE(chain.events.size() == 2);
  if (chain.events.size() != 2) return;
  CHECK(chain.events[1][1] == 0);
}

TEST_CASE("F01358 item and default shortcuts do not inspect unused unlocks",
          "[hud-rendering][F01358]") {
  auto world = World();
  world.unlock_available = false;
  world.input[f::kRestrictions] = 8;
  const auto item = b::ShortcutIconBrightness(world, 4);
  REQUIRE(item.has_value());
  if (!item.has_value()) return;
  CHECK(item.value() == b::HudShortcutTone::kEnabled);
  const auto other = b::ShortcutIconBrightness(world, -1);
  REQUIRE(other.has_value());
  if (!other.has_value()) return;
  CHECK(other.value() == b::HudShortcutTone::kEnabled);
  CHECK(world.events.size() == 2);
  world.restrictions_available = false;
  CHECK_FALSE(b::ShortcutIconBrightness(world, -1).has_value());
}

TEST_CASE("F01360 marker keeps the caller's full input words and overlay",
          "[hud-rendering][F01360]") {
  auto world = World();
  CHECK(b::DrawShortcutButtonMarker(world, -1, 0x180));
  REQUIRE(world.events.size() == 1);
  if (world.events.size() != 1) return;
  CHECK(world.events[0][2] == 0xFFFFFFFF);
  CHECK(world.events[0][4] == 0x80502084);
  CHECK(world.events[0][5] == 0x373D80C0);
  CHECK(world.events[0][6] == 0xE1000017);
}

TEST_CASE("F01360 marker packet uses the sprite brightness and flag policy",
          "[hud-rendering][F01360]") {
  auto world = World();
  CHECK(b::DrawShortcutButtonMarker(world, f::Signed(0x8000FFFF), -1));
  REQUIRE(world.events.size() == 1);
  if (world.events.size() != 1) return;
  const auto& event = world.events[0];
  const auto packet =
      r::BuildSpritePacket({.draw_mode = event[6],
                            .brightness_transparency = f::Signed(event[1]),
                            .packed_position = f::Signed(event[2]),
                            .packed_size = f::Signed(event[3])},
                           0xAB123456);
  CHECK(packet[r::kSpriteTagWord] == 0x05123456);
  CHECK(packet[r::kSpriteColorWord] == 0x66FFFFFF);
  CHECK(packet[r::kSpriteSizeWord] == 0x00060006);
  world.rendering_available = false;
  CHECK_FALSE(b::DrawShortcutButtonMarker(world, 0, 0));
}
