#pragma once
// Hand-maintained (tools/port/owned.csv).
// MENUF.PRG game-clear presentation: the congratulations banner, the final and
// counting score, the final and counting map completion and the Riskbreaker
// rank (F03025-F03031). The draws turn atlas widths, a reveal timer and the
// results values into tint-fade, flash-fade and wipe draws for a
// GameClearDrawHost, which also plays the counting sound effects, so the cores
// reach no global state.

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include "Menu/Results/ResultsDrawing.hpp"
#include "Render/TextureRegionDraw.hpp"

namespace VS::Menu::Results {
// Every reveal clamps its timer to [0, 64] and draws nothing at zero.
inline constexpr std::int32_t kGameClearRevealSteps = 64;
// The counting values start rising, and the counting sound starts, halfway
// through the reveal.
inline constexpr std::int32_t kGameClearCountingStart = 32;
// The wipe edge moves eight pixels a step from the unadjusted x.
inline constexpr std::int32_t kGameClearWipeStride = 8;

// Atlas entries (disIndexRank0*, 3B8.c); entries 0-9 are the digits.
inline constexpr std::size_t kGameClearComma = 10;
inline constexpr std::size_t kGameClearRankHeading = 12;
inline constexpr std::size_t kGameClearScoreLabel = 18;
inline constexpr std::size_t kGameClearPercent = 19;
inline constexpr std::size_t kGameClearPoints = 20;
inline constexpr std::size_t kGameClearMapLabel = 21;
inline constexpr std::size_t kGameClearCongratulations = 22;
inline constexpr std::size_t kGameClearColon = 26;

// "%09d" and "%03d": the drawn digit counts.
inline constexpr std::size_t kGameClearScoreDigits = 9;
inline constexpr std::size_t kGameClearMapDigits = 3;

// vs_main_playSfxDefault(0x7E, 0x72) while counting and 0x73 when done.
inline constexpr std::int32_t kGameClearSfxGroup = 0x7E;
inline constexpr std::int32_t kGameClearCountingSfx = 0x72;
inline constexpr std::int32_t kGameClearCountedSfx = 0x73;

// A rank row: the entry count, then up to three atlas entries.
inline constexpr std::size_t kRiskbreakerRankWords = 3;
inline constexpr std::size_t kRiskbreakerRowBytes = kRiskbreakerRankWords + 1;
using RiskbreakerRankRow = std::array<std::uint8_t, kRiskbreakerRowBytes>;

// The left and right colors of one draw; the left color's step drives it.
using GameClearColors = std::array<Render::RegionColor, 2>;

// A counting readout: the value on screen and whether its counting sound is
// playing (_onScreenScore/_incrementingScore and their map counterparts).
struct CountingReadout {
  std::int32_t shown = 0;
  std::int32_t counting = 0;
};

// Where the game-clear draws and sound effects go. The texture index is
// always inside the atlas the draw was given.
class GameClearDrawHost {
 public:
  GameClearDrawHost() = default;
  GameClearDrawHost(const GameClearDrawHost&) = delete;
  GameClearDrawHost& operator=(const GameClearDrawHost&) = delete;
  GameClearDrawHost(GameClearDrawHost&&) = delete;
  GameClearDrawHost& operator=(GameClearDrawHost&&) = delete;
  virtual ~GameClearDrawHost() = default;
  // _renderTextureFadeInTint.
  virtual void DrawTintFade(ResultsPoint origin, std::size_t texture_index,
                            const GameClearColors& colors) = 0;
  // _renderTexturePopIn.
  virtual void DrawFlashFade(ResultsPoint origin, std::size_t texture_index,
                             const GameClearColors& colors) = 0;
  // _renderTextureWipe.
  virtual void DrawWipe(ResultsPoint origin, std::size_t texture_index,
                        const GameClearColors& colors,
                        std::int32_t wipe_edge_x) = 0;
  virtual void PlaySfx(std::int32_t group, std::int32_t effect) = 0;
  virtual void StopSfx(std::int32_t group, std::int32_t effect) = 0;
};

// The two congratulations halves flash in, centred on `x`. kHidden at a timer
// of zero or less; kInvalidTexture for an atlas without the banner.
ResultsDrawResult DrawCongratulations(
    std::span<const Render::TextureRegion> atlas, std::int32_t x,
    std::int32_t y, std::int32_t timer, GameClearDrawHost& host);

// "SCORE:" and the nine digits of `score` grouped by commas, then "pts",
// tint-faded and centred on `x`. A score that prints negative ("%09d" of a
// value of 2^31 or more) refuses with kInvalidTexture before drawing.
ResultsDrawResult DrawScore(std::span<const Render::TextureRegion> atlas,
                            std::int32_t x, std::int32_t y, std::int32_t timer,
                            std::uint32_t score, GameClearDrawHost& host);

// The score wiped in while `readout` counts up to `score` over the reveal's
// second half, with the counting and counted sound effects. The readout
// changes before the draws, so a readout that prints negative still counts
// but draws nothing (kInvalidTexture).
ResultsDrawResult DrawCountingScore(
    std::span<const Render::TextureRegion> atlas, std::int32_t x,
    std::int32_t y, std::int32_t timer, std::uint32_t score,
    CountingReadout& readout, GameClearDrawHost& host);

// "MAP COMPLETED:", the three digits of `completion` and "%", tint-faded and
// centred on `x`. A completion of 1000 or more draws its first three digits.
ResultsDrawResult DrawMapCompletion(
    std::span<const Render::TextureRegion> atlas, std::int32_t x,
    std::int32_t y, std::int32_t timer, std::uint32_t completion,
    GameClearDrawHost& host);

// The map completion wiped in while `readout` counts up to `completion`:
// scaled by the elapsed steps from 32 up, or one a frame below 32.
ResultsDrawResult DrawCountingMapCompletion(
    std::span<const Render::TextureRegion> atlas, std::int32_t x,
    std::int32_t y, std::int32_t timer, std::uint32_t completion,
    CountingReadout& readout, GameClearDrawHost& host);

// "RISKBREAKER RANK" tint-faded and centred on `x`.
ResultsDrawResult DrawRiskbreakerRankHeading(
    std::span<const Render::TextureRegion> atlas, std::int32_t x,
    std::int32_t y, std::int32_t timer, GameClearDrawHost& host);

// The words of rank `rank` from `ranks` flashed in side by side, centred on
// `x`. A rank outside `ranks`, a row of more than three words or a word
// outside the atlas refuses with kInvalidTexture before drawing; an empty row
// is kHidden.
ResultsDrawResult DrawRiskbreakerRank(
    std::span<const Render::TextureRegion> atlas, std::int32_t x,
    std::int32_t y, std::int32_t timer, std::int32_t rank,
    std::span<const RiskbreakerRankRow> ranks, GameClearDrawHost& host);
}  // namespace VS::Menu::Results
