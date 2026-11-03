#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "Render/DisplayControl.hpp"
#include "Render/EncodedText.hpp"
#include "Render/GameFont.hpp"
#include "Render/ScaledFont.hpp"
#include "Render/UiPackets.hpp"

// The dialogue text boxes' text (BATTLE.PRG 5BF94.c:1706-2330): the
// progressive drawer that reveals a page a few glyphs a frame, the scaled
// drawer used while a box opens or closes, and the framebuffer blend that
// fades text into the screen. The drawers read a TextBoxText and the font
// tables, draw through a PacketQueue, ScaledGlyphTarget or VramPixelTarget
// and change the shared FontStyle as the text's font codes do, so they reach
// no global state (GameFontLive.hpp binds them).
namespace VS::Render {
// The text a drawer reads: the box's own string, an argument string or the
// digits of a printed integer.
inline constexpr int kTextBoxString = -1;
inline constexpr int kPrintedIntegerText{kEncodedTextArgumentCount};

struct TextPosition {
  // kTextBoxString, an argument string's index or kPrintedIntegerText.
  int source = kTextBoxString;
  std::size_t offset = 0;
};

// The fields of a dialogue text box (vs_battle_textBox, 5BF94.h:121) the
// drawers read and update.
struct TextBoxText {
  std::span<const std::uint8_t> text;
  std::uint8_t kind = 0;
  std::uint8_t arrow_frame = 0;
  // Bit 5 set: the box has no speaker to animate.
  std::uint8_t speaker_texture = 0;
  bool done = false;
  // Glyphs revealed a frame, 0 for a box that draws nothing.
  std::uint16_t speed = 0;
  std::uint16_t previous_chunks = 0;
  std::int16_t x_indent = 0;
  std::int16_t y_indent = 0;
  std::int16_t x = 0;
  std::int16_t y = 0;
  std::int16_t frame_x = 0;
  std::int16_t frame_y = 0;
  std::uint8_t brightness = 0;
  std::int8_t chars_per_line = 0;
  std::int8_t line_count = 0;
  // The speaker's mouth animation counter (opaque2C): positive while the
  // speaker talks, negative for a pause.
  std::int16_t speaker_pause = 0;
};

// What the progressive drawer reads from the frame.
struct TextBoxInput {
  // A button newly pressed on the first controller's low byte.
  bool confirm_pressed = false;
  int tick_speed = 0;
};

inline constexpr std::uint16_t kTextBoxSkipSpeed = 0xFFFF;
// An argument string that references itself without drawing a glyph looped
// for ever in the original; the drawer stops after this many codes.
inline constexpr int kMaxProgressiveTextSteps = 0x10000;

struct ProgressiveTextResult {
  // Where the next page starts when the text reached a page break.
  std::optional<TextPosition> next_page;
  // Nonzero: the text sound to play for the glyphs revealed this frame.
  std::uint8_t sound = 0;
  // The speaker stopped talking: its texture animation is reset.
  bool reset_speaker = false;
  // String references past the ten strings or never set, printed empty (the
  // original read through them).
  int missing_strings = 0;
  // Drawing stopped at kMaxProgressiveTextSteps.
  bool overran = false;
};

// BATTLE.PRG _printVariableWidthFont (5BF94.c:1706): reveals up to
// box.speed glyphs of the page through DrawVariableWidthGlyph, prints the
// integers and strings the text references from the arguments, animates the
// continuation arrow and finishes the page on confirm. As in the original,
// one reference is followed at a time: a reference inside a referenced text
// returns to that text, whose end then ends the box.
ProgressiveTextResult DrawProgressiveTextBoxText(
    TextBoxText& box, const TextBoxInput& input, FontStyle& style,
    std::span<const std::uint8_t> glyph_widths,
    const EncodedTextArguments& arguments, PacketQueue& glyphs);

// BATTLE.PRG _printFixedWidthFont (5BF94.c:2096): the whole text, scaled by
// the 16.16 `scale` about the box's frame, through DrawScaledFontChar.
void DrawScaledTextBoxText(const TextBoxText& box, int scale, FontStyle& style,
                           std::span<const std::uint8_t> glyph_widths,
                           ScaledGlyphTarget& target);

inline constexpr std::size_t kGlyphPixelExpansionCount = 16;
inline constexpr int kGlyphCellWidth = 3;
inline constexpr int kGlyphCellHeight = 12;
inline constexpr int kGlyphCellPixels = kGlyphCellWidth * kGlyphCellHeight;
inline constexpr std::uint32_t kFullGlyphAlpha = 0x10000;

// BATTLE.PRG func_800C7EBC (5BF94.c:2192): blends the 12x12 glyph whose
// packed 4-bit cell is `cell` into the 15-bit pixels, `stride` pixels a row,
// with 16.16 `alpha`, at most kFullGlyphAlpha. A pixel of colour 0 is left;
// the others take their colour from `pixel_expansion` (D_800EB98C). Pixels
// past the span are left.
void BlendGlyphIntoPixels(std::span<std::uint16_t> pixels, std::size_t stride,
                          std::span<const std::uint16_t> cell,
                          std::span<const std::uint16_t> pixel_expansion,
                          std::uint32_t alpha) noexcept;

// VRAM areas read into, and written from, 15-bit pixels a row at a time.
class VramPixelTarget {
 public:
  virtual ~VramPixelTarget() = default;
  // `pixels` holds area.width * area.height pixels.
  virtual void Read(const ScreenRect& area,
                    std::span<std::uint16_t> pixels) = 0;
  virtual void Upload(const ScreenRect& area,
                      std::span<const std::uint16_t> pixels) = 0;
};

struct FramebufferTextBlend {
  // 16.16; anything from 1.0 or below 0 is 1.0.
  int alpha = 0;
  // The drawn framebuffer's x in VRAM.
  int framebuffer_x = 0;
  // Circle is newly pressed: the box is skipped to its end.
  bool skip_pressed = false;
};

// The variable-width font sheet's widths and pixel colours.
struct VariableFontTables {
  std::span<const std::uint8_t> glyph_widths;
  std::span<const std::uint16_t> pixel_expansion;
};

// BATTLE.PRG _renderTextImmediate (5BF94.c:2236): blends the box's text into
// its framebuffer area with BlendGlyphIntoPixels, using table 1 unless the
// text selects another. Returns false, drawing nothing, when the box is off
// the 320x224 screen or has no area. A glyph whose cell would cross the
// box's edge is skipped (the original wrote past its buffer for boxes under
// 12 pixels).
bool BlendTextBoxIntoFramebuffer(TextBoxText& box,
                                 const FramebufferTextBlend& blend,
                                 FontStyle& style,
                                 const VariableFontTables& font,
                                 VramPixelTarget& vram);
}  // namespace VS::Render
