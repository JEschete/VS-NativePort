#pragma once
// Hand-maintained (tools/port/owned.csv).
// MENUF.PRG cube-puzzle quit, start and results presentation: the "giving up"
// and "chicken" quit messages, the "evolve or die" and "food chain" banners,
// the average-time heading, the par time, the rank title and the counting
// clear time (F03052-F03059). The draws turn atlas widths, a reveal timer and
// the puzzle times into fade-in, flash-fade and time draws for a
// CubePuzzleTextHost, so the cores reach no global state.

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include "Menu/Results/ResultsDrawing.hpp"
#include "Render/TextureRegionDraw.hpp"

namespace VS::Menu::Results {
// Every reveal clamps its timer to at most 64 and draws nothing at zero.
inline constexpr std::int32_t kCubeTextRevealSteps = 64;
// The counting clear time stays at zero for the first steps of its reveal and
// then rises over the remaining 48 (kCubeTextRevealSteps - 16).
inline constexpr std::int32_t kCubeTimeCountingStart = 16;
inline constexpr std::int32_t kCubeTimeCountingSteps = 48;

// Atlas entries (disIndexIq0*, disIndexIq1*, disIndexEsc0*, 3B8.c).
inline constexpr std::size_t kCubeTextTimeLabel = 72;
inline constexpr std::size_t kCubeTextEvolveOrDie = 102;
inline constexpr std::size_t kCubeTextAverageTime = 104;
inline constexpr std::size_t kCubeTextFoodChain = 105;
inline constexpr std::size_t kCubeTextFirstRank = 107;
inline constexpr std::size_t kCubeTextGivingUp = 123;
inline constexpr std::size_t kCubeTextChicken = 125;
// The quit messages draw their halves at x 0 and 160.
inline constexpr std::int32_t kCubeTextQuitSecondHalfX = 160;
// The counting time sits two pixels below its label.
inline constexpr std::int32_t kCubeTimeDrop = 2;

// The left and right colors of one flash-fade draw; the left color's step
// drives it.
using CubeTextColors = std::array<Render::RegionColor, 2>;

// The puzzle clock of vs_main_stateFlags (puzzleTimerMins, puzzleTimerSeconds,
// puzzleTimerHundredths).
struct CubePuzzleClock {
  std::uint8_t minutes = 0;
  std::uint8_t seconds = 0;
  std::uint8_t hundredths = 0;
};

// Where the cube-puzzle draws go. The texture index is always inside the atlas
// the draw was given.
class CubePuzzleTextHost {
 public:
  CubePuzzleTextHost() = default;
  CubePuzzleTextHost(const CubePuzzleTextHost&) = delete;
  CubePuzzleTextHost& operator=(const CubePuzzleTextHost&) = delete;
  CubePuzzleTextHost(CubePuzzleTextHost&&) = delete;
  CubePuzzleTextHost& operator=(CubePuzzleTextHost&&) = delete;
  virtual ~CubePuzzleTextHost() = default;
  // _renderTextureFadeIn.
  virtual void DrawFadingTexture(ResultsPoint origin, std::size_t texture_index,
                                 std::int32_t brightness) = 0;
  // _renderTexturePopIn.
  virtual void DrawFlashFade(ResultsPoint origin, std::size_t texture_index,
                             const CubeTextColors& colors) = 0;
  // _renderTime with no new record.
  virtual void DrawTime(ResultsPoint origin, std::int32_t timer,
                        std::int32_t time) = 0;
};

// The par time in seconds packed as minutes << 16 | seconds << 8. A negative
// par time keeps the original's sign-extended seconds, which overwrite the
// minutes.
[[nodiscard]] std::int32_t PackedParTime(std::int16_t par_seconds);

// The clear time shown at reveal step `timer` (already clamped): zero below
// step 16, then the clock scaled by (timer - 16) / 48 and packed as
// minutes << 16 | seconds << 8 | hundredths.
[[nodiscard]] std::int32_t CountedCubePuzzleTime(std::int32_t timer,
                                                 CubePuzzleClock clock);

// The two "giving up" halves fade in at x 0 and 160. kHidden at a timer of
// zero or less; kInvalidTexture for an atlas without the message.
ResultsDrawResult DrawGivingUp(std::span<const Render::TextureRegion> atlas,
                               std::int32_t y, std::int32_t timer,
                               CubePuzzleTextHost& host);

// The two "chicken" halves fade in at x 0 and 160.
ResultsDrawResult DrawChicken(std::span<const Render::TextureRegion> atlas,
                              std::int32_t y, std::int32_t timer,
                              CubePuzzleTextHost& host);

// The two "evolve or die" halves flash in, centred on `x`.
ResultsDrawResult DrawEvolveOrDie(std::span<const Render::TextureRegion> atlas,
                                  std::int32_t x, std::int32_t y,
                                  std::int32_t timer, CubePuzzleTextHost& host);

// The two "food chain" halves flash in, centred on `x`.
ResultsDrawResult DrawFoodChain(std::span<const Render::TextureRegion> atlas,
                                std::int32_t x, std::int32_t y,
                                std::int32_t timer, CubePuzzleTextHost& host);

// "AVERAGE TIME" fades in, centred on `x`.
ResultsDrawResult DrawAverageTimeHeading(
    std::span<const Render::TextureRegion> atlas, std::int32_t x,
    std::int32_t y, std::int32_t timer, CubePuzzleTextHost& host);

// The par time drawn as a time readout at `x`, `y`.
ResultsDrawResult DrawCubePuzzleParTime(std::int32_t x, std::int32_t y,
                                        std::int32_t timer,
                                        std::int16_t par_seconds,
                                        CubePuzzleTextHost& host);

// The title of rank `rank` (atlas entry 107 + rank) flashed in, centred on
// `x`. A title entry outside the atlas refuses with kInvalidTexture.
ResultsDrawResult DrawCubePuzzleRank(
    std::span<const Render::TextureRegion> atlas, std::int32_t x,
    std::int32_t y, std::int32_t timer, std::int32_t rank,
    CubePuzzleTextHost& host);

// "TIME" fades in at `x` and the clear time counts up beside it.
ResultsDrawResult DrawCountingCubePuzzleTime(
    std::span<const Render::TextureRegion> atlas, std::int32_t x,
    std::int32_t y, std::int32_t timer, CubePuzzleClock clock,
    CubePuzzleTextHost& host);
}  // namespace VS::Menu::Results
