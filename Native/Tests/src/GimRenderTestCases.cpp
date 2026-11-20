#include <algorithm>
#include <array>
#include <cstddef>
#include <string_view>
#include <type_traits>

#include <catch2/catch_test_macros.hpp>

#include "Battle/Hud.hpp"
#include "Battle/HudOrchestrationLive.hpp"
#include "Battle/HudStateLive.hpp"
#include "Legacy/Psx.h"
#include "Render/Display.hpp"
#include "Render/Frame.hpp"
#include "Render/Render.hpp"
#include "Render/Text/CompactFont.hpp"
#include "Render/UiPrimitives.hpp"
#include "State/SceneTimerLive.hpp"
#include "State/StateFlags.hpp"
#include "State/Timers.hpp"

int Tests_EncodedTextFormatting(void);

TEST_CASE(
    "Encoded text formatting retains zero glyphs and terminates nested strings",
    "[render][text]") {
  REQUIRE(Tests_EncodedTextFormatting() == 1);
}

int Tests_ScaledDialogueGlyphs(void);
int Tests_CompactTextAndGaugeLinks(void);
int CheckFeedbackEffectDialogueLinks(int framebuffer, int feedback_layer);

TEST_CASE("Compact text and gauge chains retain native packet links",
          "[render][text]") {
  REQUIRE(Tests_CompactTextAndGaugeLinks() == 1);
}

TEST_CASE("Scaled dialogue preserves glyph layout and unsigned control bytes",
          "[render][text]") {
  REQUIRE(Tests_ScaledDialogueGlyphs() == 1);
}

TEST_CASE("Screen feedback preserves the dialogue ordering chain",
          "[render][text][feedback]") {
  for (const int framebuffer : {0, 1}) {
    for (const int feedback_layer : {1, 2, 128}) {
      CAPTURE(framebuffer, feedback_layer);
      REQUIRE(CheckFeedbackEffectDialogueLinks(framebuffer, feedback_layer) ==
              1);
    }
  }
}

int CheckMenuRowNativeLinks(int scrolling);

TEST_CASE("Menu rows use native overlay links without changing scene ordering",
          "[render][menu][links]") {
  REQUIRE(CheckMenuRowNativeLinks(0) == 1);
  REQUIRE(CheckMenuRowNativeLinks(1) == 1);
}

int CheckMenuWorkspaceReset(void);

TEST_CASE("Menu initialization clears every native row and scratch byte",
          "[render][menu][workspace]") {
  REQUIRE(CheckMenuWorkspaceReset() == 1);
}

int CheckMenuReloadClosingDelay(void);

TEST_CASE("Menu reloads restore the original five-tick closing delay",
          "[render][menu][lifecycle]") {
  REQUIRE(CheckMenuReloadClosingDelay() == 1);
}

int CheckStatusPacketLinks(int background_first);

TEST_CASE("Status background and limb packets preserve the complete UI chain",
          "[render][menu][status]") {
  REQUIRE(CheckStatusPacketLinks(0) == 1);
  REQUIRE(CheckStatusPacketLinks(1) == 1);
}

int CheckMenuPromptPanelLinks(void);

TEST_CASE("Prompt and action-cost panels retain the lower menu chain",
          "[render][menu][panels]") {
  REQUIRE(CheckMenuPromptPanelLinks() == 1);
}

int CheckDefaultLoadoutNames(void);

TEST_CASE("Default actor and weapon names retain encoded glyphs and controls",
          "[render][menu][names]") {
  REQUIRE(CheckDefaultLoadoutNames() == 1);
}

namespace {
struct PuzzleTimerFixture {
  VS::Render::FrameScratch saved_frame = VS::Render::FrameScratchpad();
  decltype(std::to_array(VS::Render::compactGlyphWidths)) saved_widths =
      std::to_array(VS::Render::compactGlyphWidths);
  decltype(std::to_array(VS::Render::compactGlyphIndices)) saved_indices =
      std::to_array(VS::Render::compactGlyphIndices);
  decltype(VS::Battle::puzzleTimerText) saved_text =
      VS::Battle::puzzleTimerText;
  char* saved_pointer = VS::Battle::puzzleTimerTextPointer;
  char saved_visible = VS::Battle::lowerScreenUiState;
  std::uint8_t saved_hundredths = VS::State::stateFlags.puzzle_timer_hundredths;
  std::uint8_t saved_seconds = VS::State::stateFlags.puzzle_timer_seconds;
  std::uint8_t saved_minutes = VS::State::stateFlags.puzzle_timer_minutes;
  std::uint8_t saved_hours = VS::State::stateFlags.puzzle_set_max_timer;
  VS::State::SceneTimer saved_timer = VS::State::sceneTimer;
  void* saved_buffer = VS::Render::legacyPrimitiveBuffers[0];
  void* saved_ordering_table = VS::Render::legacyOrderingTables[0];
  int saved_framebuffer = VS::Render::frameBuf;
  std::array<u_long, 256> storage{};
  std::array<u_long, 16> ordering{};

  PuzzleTimerFixture() {
    VS::Battle::puzzleTimerTextPointer = nullptr;
    std::fill_n(VS::Render::compactGlyphWidths + 8, 6, 0x66);
    for (int glyph = 0; glyph < 11; ++glyph) {
      VS::Render::compactGlyphIndices[16 + glyph] =
          static_cast<u_char>(glyph * 2);
    }
  }

  ~PuzzleTimerFixture() {
    VS::Render::FrameScratchpad() = saved_frame;
    std::copy(saved_widths.begin(), saved_widths.end(),
              VS::Render::compactGlyphWidths);
    std::copy(saved_indices.begin(), saved_indices.end(),
              VS::Render::compactGlyphIndices);
    std::copy(saved_text.begin(), saved_text.end(),
              VS::Battle::puzzleTimerText.begin());
    VS::Battle::puzzleTimerTextPointer = saved_pointer;
    VS::Battle::lowerScreenUiState = saved_visible;
    VS::State::stateFlags.puzzle_timer_hundredths = saved_hundredths;
    VS::State::stateFlags.puzzle_timer_seconds = saved_seconds;
    VS::State::stateFlags.puzzle_timer_minutes = saved_minutes;
    VS::State::stateFlags.puzzle_set_max_timer = saved_hours;
    VS::State::sceneTimer = saved_timer;
    VS::Render::legacyPrimitiveBuffers[0] = saved_buffer;
    VS::Render::legacyOrderingTables[0] = saved_ordering_table;
    VS::Render::frameBuf = saved_framebuffer;
  }

  void DrawCounter(const VS::State::SceneTimer& timer, int hundredths) {
    storage.fill(0);
    storage.front() = storage.back() = 0xC0FFEE00;
    ordering.fill(0xFFFFFF);
    // The Live queue reserves its packets inside the frame's buffer.
    VS::Render::frameBuf = 0;
    VS::Render::legacyPrimitiveBuffers[0] = storage.data() + 1;
    VS::Render::legacyOrderingTables[0] = ordering.data();
    VS::Render::FrameScratchpad().primitive_cursor = storage.data() + 1;
    VS::Render::FrameScratchpad().ordering_table = ordering.data() + 1;
    VS::State::stateFlags.puzzle_timer_hundredths =
        static_cast<char>(hundredths);
    VS::Battle::DrawLivePuzzleTimer(&timer);
  }

  void Draw(int packed_timer, int hundredths) {
    VS::State::SceneTimer timer{};
    timer.time.frames = static_cast<signed char>(packed_timer & 0xFF);
    timer.time.seconds = static_cast<signed char>((packed_timer >> 8) & 0xFF);
    timer.time.minutes = static_cast<signed char>((packed_timer >> 16) & 0xFF);
    timer.time.hours = static_cast<signed char>((packed_timer >> 24) & 0xFF);
    DrawCounter(timer, hundredths);
  }

  void DrawCurrent(int hundredths) {
    DrawCounter(VS::State::sceneTimer, hundredths);
  }

  static void CheckGlyph(const u_long* sprite, std::size_t glyph,
                         char character) {
    CHECK(sprite[1] == 0x64808080);
    CHECK(sprite[2] == static_cast<u_long>((128 + glyph * 6) | (200 << 16)));
    CHECK(sprite[4] == 0x000A0006);
    CHECK((sprite[3] & 0xFF) == static_cast<u_long>((character - '0') * 2));
  }

  void CheckGuards() const {
    CHECK(storage.front() == 0xC0FFEE00);
    CHECK(storage.back() == 0xC0FFEE00);
    CHECK(VS::Battle::puzzleTimerTextPointer == nullptr);
    CHECK(VS::Battle::puzzleTimerText[8] == '\0');
  }

  void CheckRendering(std::string_view expected) const {
    CHECK(std::string_view(VS::Battle::puzzleTimerText.data(), 8) == expected);
    // The draw mode and eight glyphs; the chain, not the buffer, orders them.
    CHECK(VS::Render::FrameScratchpad().primitive_cursor ==
          storage.data() + 43);
    CHECK(LegacyGpu_WalkList(ordering.data()) == 10);
    const auto* packet = static_cast<const u_long*>(nextPrim(ordering.data()));
    CHECK(packet[1] == 0xE100000C);
    for (std::size_t glyph = 0; glyph < 8; ++glyph) {
      packet = static_cast<const u_long*>(nextPrim(packet));
      CheckGlyph(packet, glyph, expected[glyph]);
    }
    CheckGuards();
  }

  struct ClockState {
    int mode;
    int visible;
    int minutes;
    int seconds;
  };

  void CheckClockState(const ClockState& expected) const {
    CHECK(VS::State::sceneTimer.mode == expected.mode);
    CHECK(VS::Battle::lowerScreenUiState == expected.visible);
    CHECK(VS::State::sceneTimer.time.minutes == expected.minutes);
    CHECK(VS::State::sceneTimer.time.seconds == expected.seconds);
  }
};
}  // namespace

TEST_CASE("Puzzle timer owns a complete writable native text buffer",
          "[render][puzzle-timer]") {
  CHECK_FALSE(
      std::is_const_v<decltype(VS::Battle::puzzleTimerText)::value_type>);
  CHECK(VS::Battle::puzzleTimerText.size() == 9);
}

TEST_CASE(
    "Puzzle timer formats and renders every digit without a pointer bridge",
    "[render][puzzle-timer]") {
  PuzzleTimerFixture fixture;
  fixture.Draw((12 << 16) | (34 << 8) | 99, 56);
  fixture.CheckRendering("12:34:56");
  fixture.Draw((59 << 16) | (59 << 8), 99);
  fixture.CheckRendering("59:59:99");
  fixture.Draw(1 << 24, 17);
  fixture.CheckRendering("59:59:99");
  fixture.Draw(0, 0);
  fixture.CheckRendering("00:00:00");
}

TEST_CASE("Puzzle timer hide and show preserve repeatable native rendering",
          "[render][puzzle-timer]") {
  PuzzleTimerFixture fixture;
  for (int repeat = 0; repeat < 2; ++repeat) {
    VS::Battle::ShowLivePuzzleTimer();
    CHECK(VS::Battle::lowerScreenUiState == 1);
    fixture.Draw(0, 12);
    fixture.CheckRendering("00:00:12");
    VS::Battle::HideLivePuzzleTimer();
    CHECK(VS::Battle::lowerScreenUiState == 0);
  }
}

TEST_CASE("Puzzle HUD reads the authoritative clock without copying its alias",
          "[render][puzzle-timer][timer-binding]") {
  PuzzleTimerFixture fixture;
  VS::State::sceneTimer = {};
  VS::State::sceneTimer.time.minutes = 12;
  VS::State::sceneTimer.time.seconds = 34;
  fixture.DrawCurrent(56);
  fixture.CheckRendering("12:34:56");
}

TEST_CASE("Scene timer pause and reset preserve HUD visibility and clock state",
          "[render][puzzle-timer][timer-binding]") {
  PuzzleTimerFixture fixture;
  VS::State::sceneTimer = {};
  VS::State::sceneTimer.mode = 1;
  VS::State::sceneTimer.time.minutes = 1;
  VS::State::sceneTimer.time.seconds = 2;
  VS::State::SetLiveSceneTimerPaused(1);
  fixture.CheckClockState(
      {.mode = 0, .visible = 0, .minutes = 1, .seconds = 2});
  fixture.DrawCurrent(56);
  fixture.CheckRendering("01:02:56");
  VS::State::SetLiveSceneTimerPaused(0);
  fixture.CheckClockState(
      {.mode = 1, .visible = 1, .minutes = 1, .seconds = 2});
  VS::State::ResetLiveSceneTimer();
  fixture.CheckClockState(
      {.mode = 0, .visible = 0, .minutes = 0, .seconds = 0});
  fixture.DrawCurrent(0);
  fixture.CheckRendering("00:00:00");
}
