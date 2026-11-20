// Hand-maintained (tools/port/owned.csv).
// Binds the typed cube-puzzle text, rank and time draws (CubePuzzleText.cpp)
// to kDisMap, the par time, the puzzle clock and the live atlas
// and time draws.
#include "Menu/Results/CubePuzzleTextLive.hpp"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>

#include "Core/Log.hpp"
#include "Legacy/Psx.hpp"
#include "Menu/Results/CubePuzzle.hpp"
#include "Menu/Results/CubePuzzleText.hpp"
#include "Menu/Results/Results.hpp"
#include "Menu/Results/ResultsDrawing.hpp"
#include "Menu/Results/ResultsDrawingLive.hpp"
#include "Render/Images.hpp"
#include "Render/TextureRegionDrawLive.hpp"
#include "State/StateFlags.hpp"

using VS::Core::Gte::CVECTOR;

namespace VS::Menu::Results {
namespace {
// Refused draws so far; the warning repeats at each power of two.
std::uint64_t titleRefusals = 0;

void Report(ResultsDrawResult result) {
  if (result != ResultsDrawResult::kInvalidTexture) {
    return;
  }
  ++titleRefusals;
  if (std::has_single_bit(titleRefusals)) {
    VS::Core::LogWarn(
        "menu.results",
        "Cube-puzzle drawing refused a title outside the atlas ({} "
        "refusals)",
        titleRefusals);
  }
}

constexpr CVECTOR ToCvector(const Render::RegionColor& color) {
  return {.r = color.r, .g = color.g, .b = color.b, .cd = color.step};
}

class LiveCubePuzzleTextHost final : public CubePuzzleTextHost {
 public:
  void DrawFadingTexture(ResultsPoint origin, std::size_t texture_index,
                         std::int32_t brightness) override {
    LiveDrawFadingTexture(origin.x, origin.y, static_cast<int>(texture_index),
                          brightness);
  }
  void DrawFlashFade(ResultsPoint origin, std::size_t texture_index,
                     const CubeTextColors& colors) override {
    const std::array vectors{ToCvector(colors[0]), ToCvector(colors[1])};
    Render::DrawLiveTextureFlashFade(kDisMap, origin.x, origin.y,
                                     static_cast<int>(texture_index), vectors);
  }
  void DrawTime(ResultsPoint origin, std::int32_t timer,
                std::int32_t time) override {
    LiveDrawTime(origin.x, origin.y, timer, time, 0);
  }
};

// The flags' fields are plain char; the original loads them unsigned.
CubePuzzleClock LivePuzzleClock() {
  return {
      .minutes =
          static_cast<std::uint8_t>(VS::State::stateFlags.puzzle_timer_minutes),
      .seconds =
          static_cast<std::uint8_t>(VS::State::stateFlags.puzzle_timer_seconds),
      .hundredths = static_cast<std::uint8_t>(
          VS::State::stateFlags.puzzle_timer_hundredths)};
}
}  // namespace

// decomp: MENUF.PRG _renderGivingUp (src/MENU/MENUF.PRG/3B8.c:2610), 0x801084F4
void LiveDrawGivingUp(int draw_y, int fade_timer) {
  LiveCubePuzzleTextHost host;
  Report(DrawGivingUp(LiveResultsAtlas(), draw_y, fade_timer, host));
}

// decomp: MENUF.PRG _renderChicken (src/MENU/MENUF.PRG/3B8.c:2624), 0x80108564
void LiveDrawChicken(int draw_y, int fade_timer) {
  LiveCubePuzzleTextHost host;
  Report(DrawChicken(LiveResultsAtlas(), draw_y, fade_timer, host));
}

// decomp: MENUF.PRG _renderEvolveOrDie (src/MENU/MENUF.PRG/3B8.c:2640),
// 0x801085D4
void LiveDrawEvolveOrDie(int x, int y, int step) {
  LiveCubePuzzleTextHost host;
  Report(DrawEvolveOrDie(LiveResultsAtlas(), x, y, step, host));
}

// decomp: MENUF.PRG _renderFoodchain (src/MENU/MENUF.PRG/3B8.c:2664),
// 0x80108688
void LiveDrawFoodChain(int x, int y, int timer) {
  LiveCubePuzzleTextHost host;
  Report(DrawFoodChain(LiveResultsAtlas(), x, y, timer, host));
}

// decomp: MENUF.PRG _renderAverageTimeHeader (src/MENU/MENUF.PRG/3B8.c:2687),
// 0x8010873C
void LiveDrawAverageTimeHeading(int x, int y, int timer) {
  LiveCubePuzzleTextHost host;
  Report(DrawAverageTimeHeading(LiveResultsAtlas(), x, y, timer, host));
}

// decomp: MENUF.PRG _renderCubePuzzleTime (src/MENU/MENUF.PRG/3B8.c:2699),
// 0x80108784
void LiveDrawCubePuzzleParTime(int x, int y, int timer) {
  LiveCubePuzzleTextHost host;
  Report(DrawCubePuzzleParTime(x, y, timer, puzzleParTime, host));
}

// decomp: MENUF.PRG _renderCubePuzzleRank (src/MENU/MENUF.PRG/3B8.c:2712),
// 0x8010880C
void LiveDrawCubePuzzleRank(int x, int y, int timer, int rank) {
  LiveCubePuzzleTextHost host;
  Report(DrawCubePuzzleRank(LiveResultsAtlas(), x, y, timer, rank, host));
}

// decomp: MENUF.PRG _renderCubePuzzleIncrementalTime
// (src/MENU/MENUF.PRG/3B8.c:2732), 0x8010887C
void LiveDrawCountingCubePuzzleTime(int x, int y, int timer) {
  LiveCubePuzzleTextHost host;
  Report(DrawCountingCubePuzzleTime(LiveResultsAtlas(), x, y, timer,
                                    LivePuzzleClock(), host));
}
}  // namespace VS::Menu::Results
