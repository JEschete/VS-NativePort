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

namespace {
namespace b = VS::Battle;
namespace f = hud_orchestration_live_fixture;

const u_long* NextPacket(const u_long* packet) {
  return static_cast<const u_long*>(LegacyGpu_Pointer(packet[0]));
}

int CheckTimerGlyph(const u_long* packet, char character, int x) {
  const auto glyph = static_cast<std::uint8_t>(character - ' ');
  const auto widths = std::span(Render_compactGlyphWidths);
  const auto width = (widths[glyph / 2] >> ((glyph & 1U) * 4)) & 15;
  const auto cell = std::span(Render_compactGlyphIndices)[glyph];
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
  const auto cue = Battle_abilityTimingCueFrames;
  b::SetLiveActorMarkersHidden(0x12348001);
  CHECK(Battle_actorMarkersHidden == -32767);
  CHECK(Battle_abilityTimingCueFrames == cue);
  b::SetLiveActorMarkersHidden(0x10000);
  CHECK(Battle_actorMarkersHidden == 0);
}

TEST_CASE("F00730 Live bulk fill does not consume neighboring native records",
          "[hud-orchestration-live][F00730]") {
  f::LiveState fixture;
  Battle_actorMarkersHidden = -123;
  Battle_ashleyStatusLabelsSuppressed = -234;
  Battle_actorMarkers[16] = 77;
  b::SetLiveActorMarker(0, 0x1234ABC0);
  CHECK(std::ranges::all_of(
      std::span(Battle_actorMarkers).first<16>(),
      [](char value) { return static_cast<unsigned char>(value) == 0xC0; }));
  CHECK(Battle_actorMarkers[16] == 77);
  CHECK(Battle_actorMarkersHidden == -123);
  CHECK(Battle_ashleyStatusLabelsSuppressed == -234);
  b::SetLiveActorMarker(15, 0x40);
  b::SetLiveActorMarker(15, 0x80);
  b::SetLiveActorMarker(15, 7);
  CHECK(static_cast<unsigned char>(Battle_actorMarkers[15]) == 0x87);
}

TEST_CASE("F00743 Live re-entry never changes the marker visibility command",
          "[hud-orchestration-live][F00743]") {
  f::LiveState fixture;
  Battle_actorMarkersHidden = -456;
  Battle_ashleyStatusLabelsSuppressed = -1;
  b::SuppressLiveAshleyStatusLabels();
  b::SuppressLiveAshleyStatusLabels();
  CHECK(Battle_ashleyStatusLabelsSuppressed == 1);
  CHECK(Battle_actorMarkersHidden == -456);
}

TEST_CASE("F01361 Live hidden panel needs no actor or render target",
          "[hud-orchestration-live][F01361]") {
  f::LiveState fixture;
  Battle_characterState = nullptr;
  fixture.frame.Disconnect();
  Battle_shortcutHudAnimationStep = 11;
  Battle_shortcutHudRequested = static_cast<char>(255);
  b::DrawLiveShortcutHud();
  CHECK(Battle_shortcutHudAnimationStep == 10);
  CHECK(Battle_shortcutHudRequested == 0);
  CHECK(fixture.frame.UsedWords() == 0);
}

TEST_CASE("F01361 Live emits both actual sprite rows and reverse digit owners",
          "[hud-orchestration-live][F01361]") {
  f::LiveState fixture;
  Battle_shortcutHudRequested = 1;
  Battle_shortcutHudAnimationStep = 0;
  b::DrawLiveShortcutHud();
  CHECK(fixture.frame.UsedWords() == 42);
  CHECK(Battle_shortcutHudRequested == 0);
  const auto* digit = NextPacket(
      static_cast<const u_long*>(Render_frameScratch.overlayOrderingTable));
  REQUIRE(digit != nullptr);
  CHECK(digit[VS::Render::kSpriteTextureWord] == 0x37FB8000);
  CHECK(digit[VS::Render::kSpriteSizeWord] == 0x100010);
}

TEST_CASE("F01362 Live invisible state does not require unused render state",
          "[hud-orchestration-live][F01362]") {
  f::LiveState fixture;
  fixture.frame.Disconnect();
  Battle_hudHidden = static_cast<char>(255);
  Battle_hudSlideOffset = 0x200000;
  b::DrawLiveBattleHud();
  CHECK(fixture.frame.UsedWords() == 0);
  CHECK(Battle_hudSlideOffset == 0x200000);
}

TEST_CASE("F01362 Live uses real limb colors and compact zero-risk font",
          "[hud-orchestration-live][F01362]") {
  f::LiveState fixture;
  fixture.actor.stats.limbs[0].hp = 0;
  b::DrawLiveBattleHud();
  CHECK(fixture.frame.primitives[2] ==
        (Battle_limbStatusColors[0] | 0x64000000U));
  CHECK(fixture.frame.primitives[4] == Battle_limbDisplaySpriteTexCoords[0]);
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
  Battle_hudHidden = 1;
  Battle_hudSlideOffset = 0x200000;
  Battle_hudBattleMode = 2;
  Battle_battleModeIndicatorStep = 4;
  Battle_battleModeIndicatorWobble = 1024;
  b::DrawLiveBattleHud();
  CHECK(fixture.frame.UsedWords() == 6);
  CHECK(fixture.frame.primitives[2] == 0x664040C0);
  CHECK(fixture.frame.primitives[3] == 0xFFE500EC);
  CHECK(Battle_battleModeIndicatorWobble == 1152);
}

TEST_CASE(
    "F01384 Live renders all eight original glyphs with their full widths",
    "[hud-orchestration-live][F01384]") {
  f::LiveState fixture;
  State_SceneTimer timer{};
  timer.minutes = 12;
  timer.seconds = 34;
  Audio_Seq_unusedStateFlags.puzzleTimerHundredths = 56;
  Battle_puzzleTimerTextPointer = nullptr;
  b::DrawLivePuzzleTimer(&timer);
  CHECK(std::string_view(Battle_puzzleTimerText) == "12:34:56");
  CHECK(Battle_puzzleTimerTextPointer == nullptr);
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
  State_SceneTimer timer{};
  timer.hours = -128;
  Audio_Seq_unusedStateFlags.puzzleTimerHundredths = -1;
  b::DrawLivePuzzleTimer(&timer);
  CHECK(std::string_view(Battle_puzzleTimerText) == "59:59:99");
  timer = {};
  Audio_Seq_unusedStateFlags.puzzleTimerHundredths = 0;
  b::DrawLivePuzzleTimer(&timer);
  CHECK(std::string_view(Battle_puzzleTimerText) == "00:00:00");
  CHECK(fixture.frame.UsedWords() == 84);
}
