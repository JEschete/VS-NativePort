// Hand-maintained (tools/port/owned.csv).
#include "Menu/Results/GameClearScore.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "Core/Math/ScalarMath.hpp"
#include "Menu/Results/ResultsDrawing.hpp"
#include "Render/TextureRegionDraw.hpp"

namespace VS::Menu::Results {
namespace {
namespace math = Core::Math;

// The readouts' layout below the label line: the colon, the digits, the
// commas and the unit drop by these many pixels.
constexpr std::int32_t kColonDrop = 7;
constexpr std::int32_t kDigitDrop = 3;
constexpr std::int32_t kCommaDrop = 14;
constexpr std::int32_t kUnitDrop = 8;
constexpr std::int32_t kColonGap = 2;
constexpr std::int32_t kDigitAdvance = 12;
constexpr std::int32_t kCommaGap = 3;
// The centring adds these to the label, colon and unit widths: nine digits and
// the gaps for the score, three digits and the gap for the map.
constexpr std::int32_t kScorePadding = 116;
constexpr std::int32_t kMapPadding = 38;
constexpr std::size_t kFirstComma = 2;
constexpr std::size_t kSecondComma = 5;
// The counting score climbs by score / 32 a step.
constexpr std::uint32_t kCountingShift = 5;
constexpr std::uint32_t kDecimal = 10;
constexpr std::size_t kMaxDecimalDigits = 10;

constexpr std::size_t kScoreGlyphs = kGameClearScoreDigits + 5;
constexpr std::size_t kMapGlyphs = kGameClearMapDigits + 3;
// The highest fixed atlas entry each draw reads.
constexpr std::size_t kReadoutAtlasEntries = kGameClearColon + 1;
constexpr std::size_t kBannerAtlasEntries = kGameClearCongratulations + 2;
constexpr std::size_t kHeadingAtlasEntries = kGameClearRankHeading + 1;

struct Rgb {
  std::uint8_t r = 0;
  std::uint8_t g = 0;
  std::uint8_t b = 0;
};

// The functions' static CVECTOR colors (3B8.c:1334, 1359, 1488, 1600, 1620).
constexpr Rgb kCongratulationsDark{.r = 128, .g = 96, .b = 64};
constexpr Rgb kCongratulationsLight{.r = 200, .g = 180, .b = 160};
constexpr Rgb kReadoutBlue{.r = 100, .g = 180, .b = 220};
constexpr Rgb kHeadingOrange{.r = 200, .g = 80, .b = 20};
constexpr Rgb kRankGrey{.r = 128, .g = 128, .b = 128};

[[nodiscard]] constexpr Render::RegionColor WithStep(Rgb color,
                                                     std::int32_t step) {
  return {.r = color.r,
          .g = color.g,
          .b = color.b,
          .step = static_cast<std::uint8_t>(step)};
}

// Each function stores the clamped timer in its first color's step; the
// counting readouts never do, so theirs stays at its initial zero.
[[nodiscard]] constexpr GameClearColors Revealed(Rgb color,
                                                 std::int32_t timer) {
  return {WithStep(color, timer), WithStep(color, 0)};
}

[[nodiscard]] constexpr std::int32_t ClampTimer(std::int32_t timer) {
  return std::clamp(timer, 0, kGameClearRevealSteps);
}

[[nodiscard]] std::int32_t Width(std::span<const Render::TextureRegion> atlas,
                                 std::size_t index) {
  return atlas[index].width;
}

struct Glyph {
  ResultsPoint origin;
  std::size_t texture_index = 0;
};

// The digits of the first N characters "%0Nd" prints for `value`, or nothing
// when it prints a minus sign (whose glyph lies outside the atlas).
template <std::size_t N>
[[nodiscard]] std::optional<std::array<std::size_t, N>> PrintedDigits(
    std::int32_t value) {
  if (value < 0) {
    return std::nullopt;
  }
  std::array<std::size_t, kMaxDecimalDigits> reversed{};
  std::size_t count = 0;
  auto remaining = static_cast<std::uint32_t>(value);
  do {
    reversed.at(count) = remaining % kDecimal;
    remaining /= kDecimal;
    ++count;
  } while (remaining != 0);
  std::array<std::size_t, N> digits{};
  const std::size_t padding = count < N ? N - count : 0;
  for (std::size_t i = padding; i < N; ++i) {
    digits.at(i) = reversed.at(count - 1 - (i - padding));
  }
  return digits;
}

// "SCORE", ":", the nine digits with a comma after the third and sixth, and
// "pts" (3B8.c:1378-1405, 1458-1483).
[[nodiscard]] std::array<Glyph, kScoreGlyphs> ScoreLayout(
    std::span<const Render::TextureRegion> atlas, std::int32_t x,
    std::int32_t y,
    const std::array<std::size_t, kGameClearScoreDigits>& digits) {
  const auto comma = Width(atlas, kGameClearComma);
  const auto label = Width(atlas, kGameClearScoreLabel);
  const auto colon = Width(atlas, kGameClearColon);
  x = math::WrappingSubtract(
      x, (comma * 2 + label + colon + Width(atlas, kGameClearPoints) +
          kScorePadding) >>
             1);
  std::array<Glyph, kScoreGlyphs> glyphs{};
  std::size_t next = 0;
  glyphs.at(next++) = {.origin = {.x = x, .y = y},
                       .texture_index = kGameClearScoreLabel};
  x = math::WrappingAdd(x, label);
  glyphs.at(next++) = {
      .origin = {.x = x, .y = math::WrappingAdd(y, kColonDrop)},
      .texture_index = kGameClearColon};
  x = math::WrappingAdd(math::WrappingAdd(x, kColonGap), colon);
  for (std::size_t i = 0; i < digits.size(); ++i) {
    glyphs.at(next++) = {
        .origin = {.x = x, .y = math::WrappingAdd(y, kDigitDrop)},
        .texture_index = digits.at(i)};
    x = math::WrappingAdd(x, kDigitAdvance);
    if (i == kFirstComma || i == kSecondComma) {
      glyphs.at(next++) = {
          .origin = {.x = x, .y = math::WrappingAdd(y, kCommaDrop)},
          .texture_index = kGameClearComma};
      x = math::WrappingAdd(math::WrappingAdd(x, kCommaGap), comma);
    }
  }
  glyphs.at(next) = {.origin = {.x = x, .y = math::WrappingAdd(y, kUnitDrop)},
                     .texture_index = kGameClearPoints};
  return glyphs;
}

// "MAP COMPLETED", ":", three digits and "%" (3B8.c:1505-1523, 1574-1595).
[[nodiscard]] std::array<Glyph, kMapGlyphs> MapLayout(
    std::span<const Render::TextureRegion> atlas, std::int32_t x,
    std::int32_t y,
    const std::array<std::size_t, kGameClearMapDigits>& digits) {
  const auto label = Width(atlas, kGameClearMapLabel);
  const auto colon = Width(atlas, kGameClearColon);
  x = math::WrappingSubtract(
      x, (label + colon + Width(atlas, kGameClearPercent) + kMapPadding) >> 1);
  std::array<Glyph, kMapGlyphs> glyphs{};
  std::size_t next = 0;
  glyphs.at(next++) = {.origin = {.x = x, .y = y},
                       .texture_index = kGameClearMapLabel};
  x = math::WrappingAdd(x, label);
  glyphs.at(next++) = {
      .origin = {.x = x, .y = math::WrappingAdd(y, kColonDrop)},
      .texture_index = kGameClearColon};
  x = math::WrappingAdd(math::WrappingAdd(x, kColonGap), colon);
  for (const auto digit : digits) {
    glyphs.at(next++) = {
        .origin = {.x = x, .y = math::WrappingAdd(y, kDigitDrop)},
        .texture_index = digit};
    x = math::WrappingAdd(x, kDigitAdvance);
  }
  glyphs.at(next) = {.origin = {.x = x, .y = math::WrappingAdd(y, kUnitDrop)},
                     .texture_index = kGameClearPercent};
  return glyphs;
}

// From step 32 of a nonzero target: the counting sound starts while the
// readout shows zero, and stops with the counted sound once it shows the
// target (3B8.c:1430-1446, 1548-1565).
void UpdateCountingSound(std::int32_t timer, std::uint32_t target,
                         CountingReadout& readout, GameClearDrawHost& host) {
  if (target == 0 || timer < kGameClearCountingStart) {
    return;
  }
  if (readout.shown == 0) {
    readout.counting = 1;
    host.PlaySfx(kGameClearSfxGroup, kGameClearCountingSfx);
  }
  if (static_cast<std::uint32_t>(readout.shown) == target &&
      readout.counting != 0) {
    readout.counting = 0;
    host.StopSfx(kGameClearSfxGroup, kGameClearCountingSfx);
    host.PlaySfx(kGameClearSfxGroup, kGameClearCountedSfx);
  }
}

[[nodiscard]] std::uint32_t CountingSteps(std::int32_t timer) {
  return static_cast<std::uint32_t>(timer - kGameClearCountingStart);
}

template <std::size_t N>
void Wipe(const std::array<Glyph, N>& glyphs, std::int32_t wipe_edge_x,
          GameClearDrawHost& host) {
  const auto colors = Revealed(kReadoutBlue, 0);
  for (const auto& glyph : glyphs) {
    host.DrawWipe(glyph.origin, glyph.texture_index, colors, wipe_edge_x);
  }
}

template <std::size_t N>
void TintFade(const std::array<Glyph, N>& glyphs, std::int32_t timer,
              GameClearDrawHost& host) {
  const auto colors = Revealed(kReadoutBlue, timer);
  for (const auto& glyph : glyphs) {
    host.DrawTintFade(glyph.origin, glyph.texture_index, colors);
  }
}
}  // namespace

// decomp: MENUF.PRG _renderCongratulations (3B8.c:1332)
ResultsDrawResult DrawCongratulations(
    std::span<const Render::TextureRegion> atlas, std::int32_t x,
    std::int32_t y, std::int32_t timer, GameClearDrawHost& host) {
  timer = ClampTimer(timer);
  if (timer <= 0) {
    return ResultsDrawResult::kHidden;
  }
  if (atlas.size() < kBannerAtlasEntries) {
    return ResultsDrawResult::kInvalidTexture;
  }
  // The second half draws with &colors[1]: the light color, then the dark
  // third color whose step is never set.
  const auto dark = WithStep(kCongratulationsDark, timer);
  const auto light = WithStep(kCongratulationsLight, timer);
  const auto left = Width(atlas, kGameClearCongratulations);
  x = math::WrappingSubtract(
      x, (left + Width(atlas, kGameClearCongratulations + 1)) >> 1);
  host.DrawFlashFade({.x = x, .y = y}, kGameClearCongratulations,
                     {dark, light});
  host.DrawFlashFade({.x = math::WrappingAdd(x, left), .y = y},
                     kGameClearCongratulations + 1,
                     {light, WithStep(kCongratulationsDark, 0)});
  return ResultsDrawResult::kDrawn;
}

// decomp: MENUF.PRG _renderScore (3B8.c:1357)
ResultsDrawResult DrawScore(std::span<const Render::TextureRegion> atlas,
                            std::int32_t x, std::int32_t y, std::int32_t timer,
                            std::uint32_t score, GameClearDrawHost& host) {
  timer = ClampTimer(timer);
  if (timer <= 0) {
    return ResultsDrawResult::kHidden;
  }
  const auto digits =
      PrintedDigits<kGameClearScoreDigits>(static_cast<std::int32_t>(score));
  if (atlas.size() < kReadoutAtlasEntries || !digits.has_value()) {
    return ResultsDrawResult::kInvalidTexture;
  }
  TintFade(ScoreLayout(atlas, x, y, *digits), timer, host);
  return ResultsDrawResult::kDrawn;
}

// decomp: MENUF.PRG _renderIncrementalScore (3B8.c:1409)
ResultsDrawResult DrawCountingScore(
    std::span<const Render::TextureRegion> atlas, std::int32_t x,
    std::int32_t y, std::int32_t timer, std::uint32_t score,
    CountingReadout& readout, GameClearDrawHost& host) {
  timer = ClampTimer(timer);
  if (timer <= 0) {
    return ResultsDrawResult::kHidden;
  }
  if (atlas.size() < kReadoutAtlasEntries) {
    return ResultsDrawResult::kInvalidTexture;
  }
  const auto wipe_edge_x =
      math::WrappingAdd(x, math::WrappingMultiply(timer, kGameClearWipeStride));
  UpdateCountingSound(timer, score, readout, host);
  if (timer >= kGameClearCountingStart) {
    readout.shown = static_cast<std::int32_t>((score >> kCountingShift) *
                                              CountingSteps(timer));
  }
  if (timer == kGameClearRevealSteps) {
    readout.shown = static_cast<std::int32_t>(score);
  }
  const auto digits = PrintedDigits<kGameClearScoreDigits>(readout.shown);
  if (!digits.has_value()) {
    return ResultsDrawResult::kInvalidTexture;
  }
  Wipe(ScoreLayout(atlas, x, y, *digits), wipe_edge_x, host);
  return ResultsDrawResult::kDrawn;
}

// decomp: MENUF.PRG _renderMapCompletion (3B8.c:1486)
ResultsDrawResult DrawMapCompletion(
    std::span<const Render::TextureRegion> atlas, std::int32_t x,
    std::int32_t y, std::int32_t timer, std::uint32_t completion,
    GameClearDrawHost& host) {
  timer = ClampTimer(timer);
  if (timer <= 0) {
    return ResultsDrawResult::kHidden;
  }
  const auto digits =
      PrintedDigits<kGameClearMapDigits>(static_cast<std::int32_t>(completion));
  if (atlas.size() < kReadoutAtlasEntries || !digits.has_value()) {
    return ResultsDrawResult::kInvalidTexture;
  }
  TintFade(MapLayout(atlas, x, y, *digits), timer, host);
  return ResultsDrawResult::kDrawn;
}

// decomp: MENUF.PRG _renderIncrementalMapCompletion (3B8.c:1527)
ResultsDrawResult DrawCountingMapCompletion(
    std::span<const Render::TextureRegion> atlas, std::int32_t x,
    std::int32_t y, std::int32_t timer, std::uint32_t completion,
    CountingReadout& readout, GameClearDrawHost& host) {
  timer = ClampTimer(timer);
  if (timer <= 0) {
    return ResultsDrawResult::kHidden;
  }
  if (atlas.size() < kReadoutAtlasEntries) {
    return ResultsDrawResult::kInvalidTexture;
  }
  const auto wipe_edge_x =
      math::WrappingAdd(x, math::WrappingMultiply(timer, kGameClearWipeStride));
  UpdateCountingSound(timer, completion, readout, host);
  if (timer >= kGameClearCountingStart) {
    if (completion >= static_cast<std::uint32_t>(kGameClearCountingStart)) {
      readout.shown = static_cast<std::int32_t>(
          (completion * CountingSteps(timer)) >> kCountingShift);
    } else if (static_cast<std::uint32_t>(readout.shown) < completion) {
      readout.shown = math::WrappingAdd(readout.shown, 1);
    }
  }
  const auto digits = PrintedDigits<kGameClearMapDigits>(readout.shown);
  if (!digits.has_value()) {
    return ResultsDrawResult::kInvalidTexture;
  }
  Wipe(MapLayout(atlas, x, y, *digits), wipe_edge_x, host);
  return ResultsDrawResult::kDrawn;
}

// decomp: MENUF.PRG _renderRiskbreakerRankHeader (3B8.c:1598)
ResultsDrawResult DrawRiskbreakerRankHeading(
    std::span<const Render::TextureRegion> atlas, std::int32_t x,
    std::int32_t y, std::int32_t timer, GameClearDrawHost& host) {
  timer = ClampTimer(timer);
  if (timer <= 0) {
    return ResultsDrawResult::kHidden;
  }
  if (atlas.size() < kHeadingAtlasEntries) {
    return ResultsDrawResult::kInvalidTexture;
  }
  host.DrawTintFade(
      {.x = math::WrappingSubtract(x, Width(atlas, kGameClearRankHeading) >> 1),
       .y = y},
      kGameClearRankHeading, Revealed(kHeadingOrange, timer));
  return ResultsDrawResult::kDrawn;
}

// decomp: MENUF.PRG _renderRiskbreakerRank (3B8.c:1618)
ResultsDrawResult DrawRiskbreakerRank(
    std::span<const Render::TextureRegion> atlas, std::int32_t x,
    // NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
    std::int32_t y, std::int32_t timer, std::int32_t rank,
    std::span<const RiskbreakerRankRow> ranks, GameClearDrawHost& host) {
  timer = ClampTimer(timer);
  if (timer <= 0) {
    return ResultsDrawResult::kHidden;
  }
  if (rank < 0 || static_cast<std::size_t>(rank) >= ranks.size()) {
    return ResultsDrawResult::kInvalidTexture;
  }
  const auto& row = ranks[static_cast<std::size_t>(rank)];
  const std::size_t count = row[0];
  if (count > kRiskbreakerRankWords) {
    return ResultsDrawResult::kInvalidTexture;
  }
  const auto words = std::span(row).subspan(1, count);
  std::int32_t width = 0;
  for (const auto word : words) {
    if (word >= atlas.size()) {
      return ResultsDrawResult::kInvalidTexture;
    }
    width += Width(atlas, word);
  }
  if (words.empty()) {
    return ResultsDrawResult::kHidden;
  }
  x = math::WrappingSubtract(x, width / 2);
  const auto colors = Revealed(kRankGrey, timer);
  for (const auto word : words) {
    host.DrawFlashFade({.x = x, .y = y}, word, colors);
    x = math::WrappingAdd(x, Width(atlas, word));
  }
  return ResultsDrawResult::kDrawn;
}
}  // namespace VS::Menu::Results
