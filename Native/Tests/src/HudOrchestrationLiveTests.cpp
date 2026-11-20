#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

#include <catch2/catch_test_macros.hpp>

#include "Battle/HudOrchestrationLive.hpp"
#include "Battle/HudOrchestrationLiveFixture.hpp"
#include "Render/Sprites.hpp"
#include "State/StateFlags.hpp"

namespace {
namespace b = VS::Battle;
namespace f = hud_orchestration_live_fixture;

const u_long* NextPacket(const u_long* packet) {
  return static_cast<const u_long*>(LegacyGpu_Pointer(packet[0]));
}

int CheckTimerGlyph(const u_long* packet, char character, int x) {
  const auto glyph = static_cast<std::uint8_t>(character - ' ');
  const auto widths = std::span(VS::Render::compactGlyphWidths);
  const auto width = (widths[glyph / 2] >> ((glyph & 1U) * 4)) & 15;
  const auto cell = std::span(VS::Render::compactGlyphIndices)[glyph];
  CHECK(packet[1] == 0x64808080);
  CHECK(packet[2] == static_cast<u_long>(x | (200 << 16)));
  CHECK(packet[3] ==
        (0x37F60000U | (cell & 0xFEU) | ((76U + (cell & 1U) * 10U) << 8)));
  CHECK(packet[4] == static_cast<u_long>(width | (10 << 16)));
  return width;
}
}  // namespace

TEST_CASE("F00729 Live retains the signed command without changing cue frames",
          "[hud-orchestration-live][F00729]") {
  f::LiveState fixture;
  const auto cue = VS::Battle::abilityTimingCueFrames;
  b::SetLiveActorMarkersHidden(0x12348001);
  CHECK(VS::Battle::actorMarkersHidden == -32767);
  CHECK(VS::Battle::abilityTimingCueFrames == cue);
  b::SetLiveActorMarkersHidden(0x10000);
  CHECK(VS::Battle::actorMarkersHidden == 0);
}

TEST_CASE("F00730 Live bulk fill does not consume neighboring native records",
          "[hud-orchestration-live][F00730]") {
  f::LiveState fixture;
  VS::Battle::actorMarkersHidden = -123;
  VS::Battle::ashleyStatusLabelsSuppressed = -234;
  VS::Battle::actorMarkers[16] = 77;
  b::SetLiveActorMarker(0, 0x1234ABC0);
  CHECK(std::ranges::all_of(
      std::span(VS::Battle::actorMarkers).first<16>(),
      [](char value) { return static_cast<unsigned char>(value) == 0xC0; }));
  CHECK(VS::Battle::actorMarkers[16] == 77);
  CHECK(VS::Battle::actorMarkersHidden == -123);
  CHECK(VS::Battle::ashleyStatusLabelsSuppressed == -234);
  b::SetLiveActorMarker(15, 0x40);
  b::SetLiveActorMarker(15, 0x80);
  b::SetLiveActorMarker(15, 7);
  CHECK(static_cast<unsigned char>(VS::Battle::actorMarkers[15]) == 0x87);
}

TEST_CASE("F00743 Live re-entry never changes the marker visibility command",
          "[hud-orchestration-live][F00743]") {
  f::LiveState fixture;
  VS::Battle::actorMarkersHidden = -456;
  VS::Battle::ashleyStatusLabelsSuppressed = -1;
  b::SuppressLiveAshleyStatusLabels();
  b::SuppressLiveAshleyStatusLabels();
  CHECK(VS::Battle::ashleyStatusLabelsSuppressed == 1);
  CHECK(VS::Battle::actorMarkersHidden == -456);
}

TEST_CASE("F01361 Live hidden panel needs no actor or render target",
          "[hud-orchestration-live][F01361]") {
  f::LiveState fixture;
  VS::Battle::characterState = nullptr;
  fixture.frame.Disconnect();
  VS::Battle::shortcutHudAnimationStep = 11;
  VS::Battle::shortcutHudRequested = static_cast<char>(255);
  b::DrawLiveShortcutHud();
  CHECK(VS::Battle::shortcutHudAnimationStep == 10);
  CHECK(VS::Battle::shortcutHudRequested == 0);
  CHECK(fixture.frame.UsedWords() == 0);
}

TEST_CASE("F01361 Live emits both actual sprite rows and reverse digit owners",
          "[hud-orchestration-live][F01361]") {
  f::LiveState fixture;
  VS::Battle::shortcutHudRequested = 1;
  VS::Battle::shortcutHudAnimationStep = 0;
  b::DrawLiveShortcutHud();
  CHECK(fixture.frame.UsedWords() == 42);
  CHECK(VS::Battle::shortcutHudRequested == 0);
  const auto* digit = NextPacket(static_cast<const u_long*>(
      VS::Render::FrameScratchpad().overlay_ordering_table));
  REQUIRE(digit != nullptr);
  CHECK(digit[VS::Render::kSpriteTextureWord] == 0x37FB8000);
  CHECK(digit[VS::Render::kSpriteSizeWord] == 0x100010);
}

TEST_CASE("F01362 Live invisible state does not require unused render state",
          "[hud-orchestration-live][F01362]") {
  f::LiveState fixture;
  fixture.frame.Disconnect();
  VS::Battle::hudHidden = static_cast<char>(255);
  VS::Battle::hudSlideOffset = 0x200000;
  b::DrawLiveBattleHud();
  CHECK(fixture.frame.UsedWords() == 0);
  CHECK(VS::Battle::hudSlideOffset == 0x200000);
}

TEST_CASE("F01362 Live uses real limb colors and compact zero-risk font",
          "[hud-orchestration-live][F01362]") {
  f::LiveState fixture;
  fixture.actor.stats.limbs[0].hp = 0;
  b::DrawLiveBattleHud();
  CHECK(fixture.frame.primitives[2] ==
        (VS::Battle::limbStatusColors[0] | 0x64000000U));
  CHECK(fixture.frame.primitives[4] ==
        VS::Battle::limbDisplaySpriteTexCoords[0]);
  // Five six-word limb sprites, the real compact text and its draw mode.
  CHECK(fixture.frame.UsedWords() == 82);
  const auto* head = NextPacket(fixture.frame.ordering.data() + 1);
  REQUIRE(head != nullptr);
  CHECK(head[1] == 0xE100000C);
  std::size_t glyphs = 0;
  for (auto* glyph = NextPacket(head); glyph != nullptr;
       glyph = NextPacket(glyph)) {
    ++glyphs;
    CHECK((glyph[0] >> 24) == 4);
  }
  CHECK(glyphs == 10);
}

TEST_CASE("F01362 Live indicator modulates color rather than screen position",
          "[hud-orchestration-live][F01362]") {
  f::LiveState fixture;
  VS::Battle::hudHidden = 1;
  VS::Battle::hudSlideOffset = 0x200000;
  VS::Battle::hudBattleMode = 2;
  VS::Battle::battleModeIndicatorStep = 4;
  VS::Battle::battleModeIndicatorWobble = 1024;
  b::DrawLiveBattleHud();
  CHECK(fixture.frame.UsedWords() == 6);
  CHECK(fixture.frame.primitives[2] == 0x664040C0);
  CHECK(fixture.frame.primitives[3] == 0xFFE500EC);
  CHECK(VS::Battle::battleModeIndicatorWobble == 1152);
}

TEST_CASE(
    "F01384 Live renders all eight original glyphs with their full widths",
    "[hud-orchestration-live][F01384]") {
  f::LiveState fixture;
  VS::State::SceneTimer timer{};
  timer.time.minutes = 12;
  timer.time.seconds = 34;
  VS::State::stateFlags.puzzle_timer_hundredths = 56;
  VS::Battle::puzzleTimerTextPointer = nullptr;
  b::DrawLivePuzzleTimer(&timer);
  CHECK(std::string_view(VS::Battle::puzzleTimerText.data()) == "12:34:56");
  CHECK(VS::Battle::puzzleTimerTextPointer == nullptr);
  CHECK(fixture.frame.UsedWords() == 42);
  auto* packet = NextPacket(fixture.frame.ordering.data() + 3);
  REQUIRE(packet != nullptr);
  CHECK(packet[1] == 0xE100000C);
  const std::string_view expected = "12:34:56";
  int x = 128;
  for (const char character : expected) {
    packet = NextPacket(packet);
    REQUIRE(packet != nullptr);
    x += CheckTimerGlyph(packet, character, x);
  }
  CHECK(NextPacket(packet) == nullptr);
}

TEST_CASE("F01384 Live saturation and repeat entry consume only timer values",
          "[hud-orchestration-live][F01384]") {
  f::LiveState fixture;
  VS::State::SceneTimer timer{};
  timer.time.hours = -128;
  VS::State::stateFlags.puzzle_timer_hundredths = -1;
  b::DrawLivePuzzleTimer(&timer);
  CHECK(std::string_view(VS::Battle::puzzleTimerText.data()) == "59:59:99");
  timer = {};
  VS::State::stateFlags.puzzle_timer_hundredths = 0;
  b::DrawLivePuzzleTimer(&timer);
  CHECK(std::string_view(VS::Battle::puzzleTimerText.data()) == "00:00:00");
  CHECK(fixture.frame.UsedWords() == 84);
}
