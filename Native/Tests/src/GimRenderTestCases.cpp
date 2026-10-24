#include <algorithm>
#include <array>
#include <cstddef>
#include <string_view>
#include <type_traits>

#include <catch2/catch_test_macros.hpp>

#include "Legacy/Psx.h"

extern "C" {
#include "Audio/Seq/Channel.h"
#include "Battle/Hud.h"
#include "Render/Render.h"
#include "Render/Text/CompactFont.h"
#include "Render/UiPrimitives.h"
#include "State/Timers.h"
}

extern "C" int Tests_GimEventLayerLayout(void);
extern "C" int Tests_EncodedTextFormatting(void);

TEST_CASE(
    "Encoded text formatting retains zero glyphs and terminates nested strings",
    "[render][text]") {
  REQUIRE(Tests_EncodedTextFormatting() == 1);
}

TEST_CASE("Event splash controls share the renderer's three native GIM layers",
          "[gim][layout]") {
  REQUIRE(Tests_GimEventLayerLayout() == 1);
}

extern "C" int Tests_GimTilePackets(int columns, int rows, int scale,
                                    int offsetX, int offsetY, int routine,
                                    int brightness, int framebuffer);
extern "C" int Tests_ScaledDialogueGlyphs(void);
extern "C" int Tests_CompactTextAndGaugeLinks(void);
extern "C" int CheckFeedbackEffectDialogueLinks(int framebuffer,
                                                int feedback_layer);

TEST_CASE("Compact text and gauge chains retain native packet links",
          "[render][text]") {
  REQUIRE(Tests_CompactTextAndGaugeLinks() == 1);
}

TEST_CASE("Scaled dialogue preserves glyph layout and unsigned control bytes",
          "[render][text]") {
  REQUIRE(Tests_ScaledDialogueGlyphs() == 1);
}

TEST_CASE("GIM tiles preserve fixed-point coordinates and native packet links",
          "[render][gim]") {
  const int cases[][5] = {
      {5, 16, 4096, 0, 0},  {5, 16, 2048, 0, 0},          {5, 16, 4095, 0, 0},
      {5, 16, 4097, 0, 0},  {5, 16, 16383, 0, 0},         {2, 3, 4096, 17, -9},
      {2, 3, 2048, -17, 9}, {5, 16, 4096 | 0x4000, 0, 0},
  };
  for (const auto& c : cases) {
    CAPTURE(c[0], c[1], c[2], c[3], c[4]);
    REQUIRE(Tests_GimTilePackets(c[0], c[1], c[2], c[3], c[4], 0, 128, 0) == 0);
  }
}

TEST_CASE("GIM scaled tiles clip offscreen rows and columns", "[render][gim]") {
  const int offsets[][2] = {
      {0, 0}, {-200, -100}, {200, 100}, {5000, 5000}, {-5000, -5000}};
  for (const auto& offset : offsets) {
    CAPTURE(offset[0], offset[1]);
    REQUIRE(Tests_GimTilePackets(5, 16, 8192, offset[0], offset[1], 0, 128,
                                 0) == 0);
  }
}

TEST_CASE("GIM byte-sized coordinate bounds do not overlap shared scratch",
          "[render][gim][bounds]") {
  const int dimensions[][2] = {{0, 0}, {255, 0}, {0, 255}};
  for (const auto& d : dimensions) {
    CAPTURE(d[0], d[1]);
    REQUIRE(Tests_GimTilePackets(d[0], d[1], 16383, 0, 0, 0, 128, 0) == 0);
  }
}

TEST_CASE("GIM grids respect the embedded tile-table capacity",
          "[render][gim][bounds]") {
  const int dimensions[][2] = {
      {250, 1}, {1, 250}, {25, 10}, {251, 1}, {1, 251}, {255, 255},
  };
  for (const auto& d : dimensions) {
    CAPTURE(d[0], d[1]);
    // Zero scale keeps every valid tile visible, exercising the entire table
    // and packet-buffer bounds.
    REQUIRE(Tests_GimTilePackets(d[0], d[1], 0, 0, 0, 0, 128, 0) == 0);
  }
}

TEST_CASE("GIM framebuffer fades retain tile and framebuffer packet chains",
          "[render][gim][fade]") {
  for (int framebuffer = 0; framebuffer < 2; ++framebuffer) {
    for (const int scale : {2048, 4096}) {
      for (const int brightness : {0, 64, 128}) {
        CAPTURE(framebuffer, scale, brightness);
        REQUIRE(Tests_GimTilePackets(5, 16, scale, 0, 0, 1, brightness,
                                     framebuffer) == 0);
      }
    }
  }
}

TEST_CASE("GIM wipe and crossfade fast paths use the shared tile renderer",
          "[render][gim][fade]") {
  REQUIRE(Tests_GimTilePackets(5, 16, 4096, 0, 0, 2, 32, 0) == 0);
  REQUIRE(Tests_GimTilePackets(5, 16, 4096, 0, 0, 3, 128, 0) == 0);
  REQUIRE(Tests_GimTilePackets(5, 16, 2048, 0, 0, 3, 128, 0) == 0);
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

extern "C" int CheckMenuRowNativeLinks(int scrolling);

TEST_CASE("Menu rows use native overlay links without changing scene ordering",
          "[render][menu][links]") {
  REQUIRE(CheckMenuRowNativeLinks(0) == 1);
  REQUIRE(CheckMenuRowNativeLinks(1) == 1);
}

extern "C" int CheckMenuWorkspaceReset(void);

TEST_CASE("Menu initialization clears every native row and scratch byte",
          "[render][menu][workspace]") {
  REQUIRE(CheckMenuWorkspaceReset() == 1);
}

extern "C" int CheckMenuReloadClosingDelay(void);

TEST_CASE("Menu reloads restore the original five-tick closing delay",
          "[render][menu][lifecycle]") {
  REQUIRE(CheckMenuReloadClosingDelay() == 1);
}

extern "C" int CheckStatusPacketLinks(int background_first);

TEST_CASE("Status background and limb packets preserve the complete UI chain",
          "[render][menu][status]") {
  REQUIRE(CheckStatusPacketLinks(0) == 1);
  REQUIRE(CheckStatusPacketLinks(1) == 1);
}

extern "C" int CheckMenuPromptPanelLinks(void);

TEST_CASE("Prompt and action-cost panels retain the lower menu chain",
          "[render][menu][panels]") {
  REQUIRE(CheckMenuPromptPanelLinks() == 1);
}

extern "C" int CheckDefaultLoadoutNames(void);

TEST_CASE("Default actor and weapon names retain encoded glyphs and controls",
          "[render][menu][names]") {
  REQUIRE(CheckDefaultLoadoutNames() == 1);
}

namespace {
struct PuzzleTimerFixture {
  Render_FrameScratch saved_frame = Render_frameScratch;
  decltype(std::to_array(Render_compactGlyphWidths)) saved_widths =
      std::to_array(Render_compactGlyphWidths);
  decltype(std::to_array(Render_compactGlyphIndices)) saved_indices =
      std::to_array(Render_compactGlyphIndices);
  decltype(std::to_array(Battle_puzzleTimerText)) saved_text =
      std::to_array(Battle_puzzleTimerText);
  char* saved_pointer = Battle_puzzleTimerTextPointer;
  char saved_visible = Battle_lowerScreenUiState;
  char saved_hundredths = Audio_Seq_unusedStateFlags.puzzleTimerHundredths;
  char saved_seconds = Audio_Seq_unusedStateFlags.puzzleTimerSeconds;
  char saved_minutes = Audio_Seq_unusedStateFlags.puzzleTimerMins;
  char saved_hours = Audio_Seq_unusedStateFlags.puzzleSetMaxTimer;
  State_SceneTimer saved_timer = State_sceneTimer;
  int saved_packed_timer = Battle_puzzleTimer;
  std::array<u_long, 256> storage{};
  std::array<u_long, 16> ordering{};

  PuzzleTimerFixture() {
    Battle_puzzleTimerTextPointer = nullptr;
    std::fill_n(Render_compactGlyphWidths + 8, 6, 0x66);
    for (int glyph = 0; glyph < 11; ++glyph) {
      Render_compactGlyphIndices[16 + glyph] = static_cast<u_char>(glyph * 2);
    }
  }

  ~PuzzleTimerFixture() {
    Render_frameScratch = saved_frame;
    std::copy(saved_widths.begin(), saved_widths.end(),
              Render_compactGlyphWidths);
    std::copy(saved_indices.begin(), saved_indices.end(),
              Render_compactGlyphIndices);
    std::copy(saved_text.begin(), saved_text.end(), Battle_puzzleTimerText);
    Battle_puzzleTimerTextPointer = saved_pointer;
    Battle_lowerScreenUiState = saved_visible;
    Audio_Seq_unusedStateFlags.puzzleTimerHundredths = saved_hundredths;
    Audio_Seq_unusedStateFlags.puzzleTimerSeconds = saved_seconds;
    Audio_Seq_unusedStateFlags.puzzleTimerMins = saved_minutes;
    Audio_Seq_unusedStateFlags.puzzleSetMaxTimer = saved_hours;
    State_sceneTimer = saved_timer;
    Battle_puzzleTimer = saved_packed_timer;
  }

  void DrawCounter(const State_SceneTimer& timer, int hundredths) {
    storage.fill(0);
    storage.front() = storage.back() = 0xC0FFEE00;
    ordering.fill(0xFFFFFF);
    Render_frameScratch.primitiveCursor = storage.data() + 1;
    Render_frameScratch.orderingTable = ordering.data() + 1;
    Audio_Seq_unusedStateFlags.puzzleTimerHundredths =
        static_cast<char>(hundredths);
    Battle_DrawPuzzleTimer(&timer);
  }

  void Draw(int packed_timer, int hundredths) {
    State_SceneTimer timer{};
    timer.frames = static_cast<signed char>(packed_timer & 0xFF);
    timer.seconds = static_cast<signed char>((packed_timer >> 8) & 0xFF);
    timer.minutes = static_cast<signed char>((packed_timer >> 16) & 0xFF);
    timer.hours = static_cast<signed char>((packed_timer >> 24) & 0xFF);
    DrawCounter(timer, hundredths);
  }

  void DrawCurrent(int hundredths) {
    DrawCounter(State_sceneTimer, hundredths);
  }

  void CheckGlyph(std::size_t glyph, char character) const {
    const auto* sprite = storage.data() + 3 + glyph * 5;
    CHECK(sprite[1] == 0x64808080);
    CHECK(sprite[2] == static_cast<u_long>((128 + glyph * 6) | (200 << 16)));
    CHECK(sprite[4] == 0x000A0006);
    CHECK((sprite[3] & 0xFF) == static_cast<u_long>((character - '0') * 2));
  }

  void CheckGuards() const {
    CHECK(storage.front() == 0xC0FFEE00);
    CHECK(storage.back() == 0xC0FFEE00);
    CHECK(Battle_puzzleTimerTextPointer == nullptr);
    CHECK(Battle_puzzleTimerText[8] == '\0');
  }

  void CheckRendering(std::string_view expected) const {
    CHECK(std::string_view(Battle_puzzleTimerText, 8) == expected);
    CHECK(Render_frameScratch.primitiveCursor == storage.data() + 44);
    CHECK(LegacyGpu_WalkList(ordering.data()) == 11);
    for (std::size_t glyph = 0; glyph < 8; ++glyph) {
      CheckGlyph(glyph, expected[glyph]);
    }
    CheckGuards();
  }

  void CheckClockState(int mode, int visible, int minutes, int seconds) const {
    CHECK(State_sceneTimer.mode == mode);
    CHECK(Battle_lowerScreenUiState == visible);
    CHECK(State_sceneTimer.minutes == minutes);
    CHECK(State_sceneTimer.seconds == seconds);
  }
};
}  // namespace

TEST_CASE("Puzzle timer owns a complete writable native text buffer",
          "[render][puzzle-timer]") {
  CHECK_FALSE(
      std::is_const_v<std::remove_extent_t<decltype(Battle_puzzleTimerText)>>);
  CHECK(std::extent_v<decltype(Battle_puzzleTimerText)> == 9);
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
    Battle_ShowPuzzleTimer();
    CHECK(Battle_lowerScreenUiState == 1);
    fixture.Draw(0, 12);
    fixture.CheckRendering("00:00:12");
    Battle_HidePuzzleTimer();
    CHECK(Battle_lowerScreenUiState == 0);
  }
}

TEST_CASE("Puzzle HUD reads the authoritative clock without copying its alias",
          "[render][puzzle-timer][timer-binding]") {
  PuzzleTimerFixture fixture;
  State_sceneTimer = {};
  State_sceneTimer.minutes = 12;
  State_sceneTimer.seconds = 34;
  Battle_puzzleTimer = 0;
  fixture.DrawCurrent(56);
  fixture.CheckRendering("12:34:56");
  CHECK(Battle_puzzleTimer == 0);
}

TEST_CASE("Scene timer pause and reset preserve HUD visibility and clock state",
          "[render][puzzle-timer][timer-binding]") {
  PuzzleTimerFixture fixture;
  State_sceneTimer = {};
  State_sceneTimer.mode = 1;
  State_sceneTimer.minutes = 1;
  State_sceneTimer.seconds = 2;
  State_SetSceneTimerPaused(1);
  fixture.CheckClockState(0, 0, 1, 2);
  fixture.DrawCurrent(56);
  fixture.CheckRendering("01:02:56");
  State_SetSceneTimerPaused(0);
  fixture.CheckClockState(1, 1, 1, 2);
  State_ResetSceneTimer();
  fixture.CheckClockState(0, 0, 0, 0);
  fixture.DrawCurrent(0);
  fixture.CheckRendering("00:00:00");
}
