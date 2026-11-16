#pragma once
// Hand-maintained (tools/port/owned.csv).
// MENUF.PRG results screens: the fading atlas textures, the time readout and
// the "PRESS BUTTON" prompt every results submenu draws with (F03040-F03042,
// F03044). The draws turn atlas entries and screen positions into textured
// quads and text for a ResultsDrawTarget, so they reach no global state.

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

#include "Render/TextureRegionDraw.hpp"
#include "Render/UiTexturedQuads.hpp"

namespace VS::Menu::Results {
// Brightness steps below this fade in: the quad is scaled by step * 16 and
// uses the next palette with additive blending (3B8.c:1899).
inline constexpr std::int32_t kResultsFadeSteps = 8;
inline constexpr std::int32_t kResultsFadeBrightnessScale = 16;
inline constexpr std::uint8_t kResultsFullBrightness = 128;
inline constexpr std::uint16_t kResultsFadePaletteStep = 1;
// getTPage(0, semiTransparencyFull, 0, 0).
inline constexpr std::uint16_t kResultsAdditiveBlend = 0x20;

// Atlas entries of the time readout (disIndices, 3B8.c:491); entries 0-9 are
// the digits.
inline constexpr std::size_t kResultsTimePrime = 73;
inline constexpr std::size_t kResultsTimeDoublePrime = 74;
inline constexpr std::size_t kResultsTimeDash = 83;
// The time value that reads as dashes: no time recorded.
inline constexpr std::int32_t kResultsNoTime = 0x800000;
inline constexpr std::size_t kResultsTimeGlyphs = 6;
// getClut(800, 511) and, for a new record, getClut(896, 511).
inline constexpr std::uint16_t kResultsTimeClut = 0x7FF2;
inline constexpr std::uint16_t kResultsNewRecordClut = 0x7FF8;

// The prompt slides in six pixels a step for at most this many steps.
inline constexpr std::int32_t kPromptMaxSlide = 48;
// vs_getRGB888(255, 128, 128).
inline constexpr std::uint32_t kPromptTextColor = 0x8080FF;
inline constexpr std::string_view kPromptPressText = "PRESS";
inline constexpr std::string_view kPromptButtonText = "BUTTON";
// The button icon: 16x16 texels at (32, 128) of getTPage(0, 0, 768, 0) with
// getClut(944, 223).
inline constexpr Render::TextureRegion kPromptButtonTexture{
    .u = 32,
    .v = 128,
    .width = 16,
    .height = 16,
    .texture_page = 0x0C,
    .clut = 0x37FB,
};

enum class ResultsDrawResult : std::uint8_t {
  kDrawn,
  // Nothing to draw yet (no brightness or a negative slide).
  kHidden,
  // The texture index lies outside the atlas; nothing was drawn.
  kInvalidTexture,
  // The primitive buffer refused at least one quad.
  kFull,
};

// Where the results draws go.
class ResultsDrawTarget {
 public:
  ResultsDrawTarget() = default;
  ResultsDrawTarget(const ResultsDrawTarget&) = delete;
  ResultsDrawTarget& operator=(const ResultsDrawTarget&) = delete;
  ResultsDrawTarget(ResultsDrawTarget&&) = delete;
  ResultsDrawTarget& operator=(ResultsDrawTarget&&) = delete;
  virtual ~ResultsDrawTarget() = default;
  // Queues the quad seven entries before the frame's ordering table; false
  // when the primitive buffer has no room for it.
  virtual bool DrawQuad(const Render::UiTexturedQuad& quad) = 0;
  // Draws compact text at the packed x | y << 16 position in front of the
  // overlay ordering table.
  virtual void DrawText(std::string_view text, std::int32_t xy,
                        std::uint32_t color) = 0;
};

// A screen position kept in the original's 32-bit arithmetic.
struct ResultsPoint {
  std::int32_t x = 0;
  std::int32_t y = 0;
};

// The semi-transparent quad of `texture` at `origin` with `brightness`, or
// nothing at a brightness of zero or less. The palette and coordinates
// truncate to their 16-bit fields.
[[nodiscard]] std::optional<Render::UiTexturedQuad> FadingTextureQuad(
    const Render::TextureRegion& texture, ResultsPoint origin,
    std::int32_t brightness, std::int32_t clut);

// Draws atlas entry `texture_index` faded in with its own palette.
ResultsDrawResult DrawFadingTexture(
    std::span<const Render::TextureRegion> atlas, std::int32_t x,
    std::int32_t y, std::int32_t texture_index, std::int32_t brightness,
    ResultsDrawTarget& target);

// Draws atlas entry `texture_index` faded in with palette `clut`.
ResultsDrawResult DrawFadingTextureClut(
    std::span<const Render::TextureRegion> atlas, std::int32_t x,
    std::int32_t y, std::int32_t texture_index, std::int32_t brightness,
    std::int32_t clut, ResultsDrawTarget& target);

// The first six characters of "%02d%02d%02d" over the time's three low bytes
// (a byte of 100 or more prints three digits and pushes the rest out), or
// dashes for kResultsNoTime.
[[nodiscard]] std::array<char, kResultsTimeGlyphs> TimeGlyphs(
    std::int32_t time);

// Draws the time as six glyphs with a prime after the second and a double
// prime after the fourth, each faded in by `timer`, in the new-record palette
// when `is_new_record`. Refuses an atlas without the time glyphs before
// drawing anything.
ResultsDrawResult DrawTime(std::span<const Render::TextureRegion> atlas,
                           std::int32_t x, std::int32_t y, std::int32_t timer,
                           std::int32_t time, bool is_new_record,
                           ResultsDrawTarget& target);

// The prompt's two text positions and its bouncing button icon.
struct PressButtonPromptLayout {
  std::int32_t press_xy = 0;
  std::int32_t button_xy = 0;
  Render::UiTexturedQuad icon{};
};

// The prompt at `origin` slid in by `slide` (clamped to kPromptMaxSlide) and
// bounced by the low four bits of `bounce`, or nothing for a negative slide.
[[nodiscard]] std::optional<PressButtonPromptLayout> PressButtonPrompt(
    ResultsPoint origin, std::int32_t slide, std::int32_t bounce);

// Draws "PRESS", "BUTTON" and then the button icon.
ResultsDrawResult DrawPressButtonPrompt(std::int32_t x, std::int32_t y,
                                        std::int32_t slide, std::int32_t bounce,
                                        ResultsDrawTarget& target);
}  // namespace VS::Menu::Results
