// Hand-maintained (tools/port/owned.csv).
// The dialogue text box drawers (BATTLE.PRG 5BF94.c:1706-2330) over spans:
// the text, its argument strings and the printed digits are read through a
// cursor whose span ends read as the terminator.
#include "Render/TextBoxText.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include "Render/DisplayControl.hpp"
#include "Render/EncodedText.hpp"
#include "Render/GameFont.hpp"
#include "Render/ScaledFont.hpp"
#include "Render/UiPackets.hpp"

namespace VS::Render {
namespace {
constexpr int kLineHeight = 13;
// Justification centres a line in three-pixel half-units of the line unit.
constexpr int kJustifyHalfUnit = 3;
constexpr std::uint8_t kSpacingBackLimit = 0xF0;
constexpr std::uint32_t kSpacingRange = 256;
constexpr std::uint32_t kSpacingCellUnits = 12;
constexpr std::uint8_t kFontMoveDownMask = 0xF8;
constexpr int kFontMoveDownShift = 3;
constexpr std::uint8_t kFontTableZero = 6;
constexpr std::uint8_t kFontTableOne = 5;
constexpr std::uint8_t kFontJustify = 4;
constexpr std::uint8_t kPaletteBankBit = 4;
constexpr std::uint8_t kTableZeroBoxKind = 4;
constexpr std::uint8_t kPlainBoxKind = 2;
constexpr std::uint32_t kScaledColorCode = 0x2C000000;
constexpr int kColorChannelBits = 8;
constexpr int kArrowFrameCount = 24;
constexpr int kArrowFrameShift = 3;
constexpr int kPostContinuationSteps = 2;
constexpr std::uint8_t kSilentSpeakerBit = 0x20;
constexpr int kSpeakerPauseDivisor = 4;
constexpr int kFixedPointShift = 16;
constexpr int kScreenWidth = 320;
constexpr int kScreenHeight = 224;
constexpr int kGlyphSize = 12;
constexpr int kBoxWidthMargin = 10;
constexpr int kBoxHeightMargin = 4;
constexpr std::uint32_t kGlyphSheetX = 832;
constexpr std::size_t kPixelsPerCellWord = 4;
constexpr std::uint32_t kCellNibbleBits = 4;
constexpr std::uint32_t kCellNibbleMask = 0xF;
constexpr std::array<std::uint32_t, 3> kColorChannelMasks{0x1F, 0x3E0, 0x7C00};
// A string reference past the ten strings.
constexpr int kMissingString = -2;
// The original's digit buffer: a nested print rewrites its start and leaves
// the rest.
constexpr std::size_t kDigitBufferBytes = 16;
static_assert(kMaxEncodedIntegerBytes < kDigitBufferBytes);

using Text = std::span<const std::uint8_t>;

[[nodiscard]] std::int32_t Wrap(std::int64_t value) noexcept {
  return static_cast<std::int32_t>(static_cast<std::uint32_t>(value));
}

[[nodiscard]] int SpacingOffset(std::uint8_t parameter) noexcept {
  return parameter >= kSpacingBackLimit
             ? int{parameter} - static_cast<int>(kSpacingRange)
             : int{parameter};
}

// The glyph table, colour and palette a box's text starts with.
void ApplyBoxStyle(const TextBoxText& box, FontStyle& style) noexcept {
  style.table_select = box.kind != kTableZeroBoxKind ? 1 : 0;
  style.color = box.brightness;
  style.palette_offset = (box.kind & 1U) == 0 ? kPaletteBankBit : 0;
  if (box.kind == kPlainBoxKind) {
    style.table_select = 0;
    style.palette_offset = 0;
  }
}

enum class FontCommand : std::uint8_t {
  kMoveDown,
  kSelectTable,
  kJustify,
  kPalette
};

// kTextFontControl: bits 3-7 move the pen down; otherwise 6 and 5 select a
// glyph table, 4 toggles justification and the rest select a palette.
FontCommand ApplyFontControl(FontStyle& style, std::uint8_t parameter,
                             bool selects_palette) noexcept {
  if ((parameter & kFontMoveDownMask) != 0) {
    return FontCommand::kMoveDown;
  }
  if (parameter == kFontTableZero || parameter == kFontTableOne) {
    style.table_select = parameter == kFontTableOne ? 1 : 0;
    return FontCommand::kSelectTable;
  }
  if (parameter == kFontJustify) {
    return FontCommand::kJustify;
  }
  if (selects_palette) {
    style.palette_offset = static_cast<std::uint8_t>(
        (style.palette_offset & kPaletteBankBit) | parameter);
  }
  return FontCommand::kPalette;
}

// Half-units left on the line from `rest` (the original's
// charsPerLine * 2 - vs_battle_getTextLineLength).
[[nodiscard]] int JustifyUnits(const TextBoxText& box, Text rest,
                               Text glyph_widths) noexcept {
  return (box.chars_per_line * 2) - MeasureEncodedTextLine(rest, glyph_widths);
}

// Reads the box's text and follows one reference at a time into an argument
// string or a printed integer's digits.
class TextCursor {
 public:
  TextCursor(Text text, const EncodedTextArguments& arguments) noexcept
      : text_(text), arguments_(arguments) {}

  std::uint8_t Next() noexcept {
    const Text source = Source(current_.source);
    return current_.offset < source.size() ? source[current_.offset++]
                                           : kTextTerminator;
  }

  [[nodiscard]] Text Rest() const noexcept {
    const Text source = Source(current_.source);
    return current_.offset < source.size() ? source.subspan(current_.offset)
                                           : Text{};
  }

  [[nodiscard]] TextPosition Position() const noexcept { return current_; }
  [[nodiscard]] bool Inline() const noexcept { return inline_; }
  [[nodiscard]] int MissingStrings() const noexcept { return missing_; }

  void EnterString(std::uint8_t index) noexcept {
    const bool known = index < kEncodedTextArgumentCount;
    if (!known || arguments_.strings[index].empty()) {
      ++missing_;
    }
    Enter({.source = known ? int{index} : kMissingString, .offset = 0});
  }

  void EnterInteger(std::uint8_t code, std::uint8_t parameter) noexcept {
    const EncodedInteger encoded = EncodeTextInteger(
        code, parameter,
        arguments_.integers[parameter % kEncodedTextArgumentCount]);
    std::ranges::copy(encoded.View(), digits_.begin());
    digits_.at(encoded.size) = kTextTerminator;
    Enter({.source = kPrintedIntegerText, .offset = 0});
  }

  // The end of a referenced text resumes the text that referenced it.
  void Return() noexcept {
    current_ = saved_;
    inline_ = false;
  }

 private:
  void Enter(TextPosition position) noexcept {
    saved_ = current_;
    current_ = position;
    inline_ = true;
  }

  [[nodiscard]] Text Source(int source) const noexcept {
    if (source == kTextBoxString) {
      return text_;
    }
    if (source == kPrintedIntegerText) {
      return digits_;
    }
    if (source >= 0 && source < static_cast<int>(kEncodedTextArgumentCount)) {
      return arguments_.strings[static_cast<std::size_t>(source)];
    }
    return {};
  }

  Text text_;
  const EncodedTextArguments& arguments_;
  std::array<std::uint8_t, kDigitBufferBytes> digits_{};
  TextPosition current_;
  TextPosition saved_;
  bool inline_ = false;
  int missing_ = 0;
};

class ProgressiveTextDrawer {
 public:
  // NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the public entry's.
  ProgressiveTextDrawer(TextBoxText& box, const TextBoxInput& input,
                        FontStyle& style, Text glyph_widths,
                        const EncodedTextArguments& arguments,
                        PacketQueue& glyphs) noexcept
      : box_(box),
        input_(input),
        style_(style),
        widths_(glyph_widths),
        glyphs_(glyphs),
        cursor_(box.text, arguments),
        remaining_(box.speed),
        x_(box.x_indent),
        y_(box.y_indent) {}

  ProgressiveTextResult Draw() {
    ApplyBoxStyle(box_, style_);
    box_.done = false;
    for (int step = 0; remaining_ > 0 && !stopped_; ++step) {
      if (step >= kMaxProgressiveTextSteps) {
        result_.overran = true;
        break;
      }
      if (post_continuation_ != 0) {
        --post_continuation_;
      }
      const std::uint8_t code = cursor_.Next();
      if (code < kTextFirstControl) {
        DrawGlyph(code);
      } else if (code < kTextFirstParameterized) {
        Control(code);
      } else {
        Parameterized(code, cursor_.Next());
      }
    }
    Finish();
    result_.missing_strings = cursor_.MissingStrings();
    return result_;
  }

 private:
  void DrawGlyph(std::uint8_t glyph) {
    x_ = DrawVariableWidthGlyph(glyphs_, style_, widths_, glyph, x_, y_);
    remaining_ -= speed_;
    ++chunks_;
  }

  void Control(std::uint8_t code) {
    switch (code) {
      case kTextPageBreak:
        result_.next_page = cursor_.Position();
        box_.speed = 1;
        box_.previous_chunks = 0;
        stopped_ = true;
        break;
      case kTextConfirm:
        Confirm();
        break;
      case kTextTerminator:
        Terminate();
        break;
      case kTextNewline:
        x_ = box_.x_indent + JustifiedOffset();
        y_ += kLineHeight;
        break;
      default:
        break;
    }
  }

  // The arrow waits on the last glyph of the frame; elsewhere it costs one
  // glyph and lets a confirm in the next steps skip the page.
  void Confirm() {
    if (remaining_ != 1) {
      --remaining_;
      post_continuation_ = kPostContinuationSteps;
      return;
    }
    if (input_.confirm_pressed) {
      remaining_ = 0;
    } else {
      const int frame =
          (box_.arrow_frame + input_.tick_speed) % kArrowFrameCount;
      const int arrow =
          static_cast<int>(kTextArrowGlyph) - (frame >> kArrowFrameShift);
      x_ = DrawVariableWidthGlyph(glyphs_, style_, widths_,
                                  static_cast<std::uint32_t>(arrow), x_, y_);
      box_.arrow_frame = static_cast<std::uint8_t>(frame);
    }
    stopped_ = true;
  }

  void Terminate() {
    if (cursor_.Inline()) {
      cursor_.Return();
      return;
    }
    const int pause = box_.speaker_pause;
    if (pause > 0) {
      box_.speaker_pause =
          static_cast<std::int16_t>((pause / kSpeakerPauseDivisor) - pause);
    }
    box_.done = true;
    if (input_.confirm_pressed || post_continuation_ != 0) {
      box_.speed = kTextBoxSkipSpeed;
    }
    stopped_ = true;
  }

  void Parameterized(std::uint8_t code, std::uint8_t parameter) {
    switch (code) {
      case kTextChunkSize:
        speed_ = parameter;
        break;
      case kTextSoundEffect:
        sound_ = parameter;
        break;
      case kTextSpacing:
        x_ += SpacingOffset(parameter);
        break;
      case kTextFontControl:
        FontControl(parameter);
        break;
      case kTextSpeakerPause:
        if (box_.speaker_pause > 0) {
          box_.speaker_pause = static_cast<std::int16_t>(-int{parameter});
        }
        break;
      case kTextPrintHex:
      case kTextPrintDecimal:
        cursor_.EnterInteger(code, parameter);
        break;
      case kTextPrintString:
        cursor_.EnterString(parameter);
        break;
      default:
        break;
    }
  }

  void FontControl(std::uint8_t parameter) {
    const FontCommand command = ApplyFontControl(style_, parameter, true);
    if (command == FontCommand::kMoveDown) {
      y_ += parameter >> kFontMoveDownShift;
    } else if (command == FontCommand::kJustify) {
      justify_ = !justify_;
      if (justify_) {
        x_ = box_.x_indent + JustifiedOffset();
      }
    }
  }

  [[nodiscard]] int JustifiedOffset() const noexcept {
    return justify_
               ? JustifyUnits(box_, cursor_.Rest(), widths_) * kJustifyHalfUnit
               : 0;
  }

  void Finish() {
    if (remaining_ <= 0) {
      box_.speed =
          static_cast<std::uint16_t>(box_.speed + (input_.tick_speed >> 1));
    }
    if (box_.previous_chunks != chunks_ && sound_ != 0) {
      result_.sound = sound_;
    }
    if ((box_.speaker_texture & kSilentSpeakerBit) == 0) {
      if (box_.speaker_pause != 0) {
        box_.speaker_pause = static_cast<std::int16_t>(box_.speaker_pause + 1);
      } else {
        result_.reset_speaker = true;
      }
    }
    box_.previous_chunks = static_cast<std::uint16_t>(chunks_);
  }

  TextBoxText& box_;
  const TextBoxInput& input_;
  FontStyle& style_;
  Text widths_;
  PacketQueue& glyphs_;
  TextCursor cursor_;
  ProgressiveTextResult result_;
  int remaining_;
  int x_;
  int y_;
  int speed_ = 1;
  int chunks_ = 0;
  int post_continuation_ = 0;
  std::uint8_t sound_ = 0;
  bool justify_ = false;
  bool stopped_ = false;
};

// The 16.16 pen position of the box's indent, scaled about its frame.
[[nodiscard]] std::int32_t ScaledOrigin(std::int16_t frame, std::int16_t indent,
                                        std::int16_t position,
                                        int scale) noexcept {
  return Wrap((std::int64_t{frame} << kFixedPointShift) +
              (std::int64_t{indent - position} * scale));
}

class ScaledTextDrawer {
 public:
  ScaledTextDrawer(const TextBoxText& box, int scale, FontStyle& style,
                   Text glyph_widths, ScaledGlyphTarget& target) noexcept
      : box_(box),
        style_(style),
        widths_(glyph_widths),
        target_(target),
        scale_(scale),
        cell_(Wrap(std::int64_t{scale} * kGlyphSize)),
        origin_x_(ScaledOrigin(box.frame_x, box.x_indent, box.x, scale)),
        x_(origin_x_),
        y_(ScaledOrigin(box.frame_y, box.y_indent, box.y, scale)) {}

  void Draw() {
    ApplyBoxStyle(box_, style_);
    const std::uint32_t level = box_.brightness;
    style_.color = level | (level << kColorChannelBits) |
                   (level << (2 * kColorChannelBits)) | kScaledColorCode;
    for (;;) {
      const std::uint8_t code = Next();
      if (code < kTextFirstControl) {
        x_ = DrawScaledFontChar(code, x_, y_, cell_, cell_, scale_,
                                GlyphStyle(), target_);
      } else if (code == kTextTerminator) {
        return;
      } else if (code == kTextNewline) {
        x_ = Wrap(std::int64_t{origin_x_} + JustifiedOffset());
        y_ = Wrap(std::int64_t{y_} + (std::int64_t{scale_} * kLineHeight));
      } else if (code >= kTextFirstParameterized) {
        Parameterized(code, Next());
      }
    }
  }

 private:
  std::uint8_t Next() noexcept {
    return at_ < box_.text.size() ? box_.text[at_++] : kTextTerminator;
  }

  [[nodiscard]] ScaledFontStyle GlyphStyle() const noexcept {
    return {.glyph_widths = std::as_bytes(widths_),
            .table_select = style_.table_select,
            .palette_offset = style_.palette_offset,
            .color = style_.color};
  }

  [[nodiscard]] std::int64_t JustifiedOffset() const noexcept {
    if (!justify_) {
      return 0;
    }
    const Text rest = at_ < box_.text.size() ? box_.text.subspan(at_) : Text{};
    return std::int64_t{JustifyUnits(box_, rest, widths_)} * (cell_ >> 2);
  }

  // The original's spacing and move-down arithmetic is unsigned.
  void Parameterized(std::uint8_t code, std::uint8_t parameter) {
    const auto cell = static_cast<std::uint32_t>(cell_);
    const auto x = static_cast<std::uint32_t>(x_);
    if (code == kTextSpacing) {
      x_ = static_cast<std::int32_t>(
          parameter >= kSpacingBackLimit
              ? x - (((kSpacingRange - parameter) * cell) / kSpacingCellUnits)
              : x + ((parameter * cell) / kSpacingCellUnits));
      return;
    }
    if (code != kTextFontControl) {
      return;
    }
    const FontCommand command = ApplyFontControl(style_, parameter, true);
    if (command == FontCommand::kMoveDown) {
      y_ = static_cast<std::int32_t>(
          static_cast<std::uint32_t>(y_) +
          ((std::uint32_t{parameter} >> kFontMoveDownShift) *
           static_cast<std::uint32_t>(scale_)));
    } else if (command == FontCommand::kJustify) {
      justify_ = !justify_;
      if (justify_) {
        x_ = Wrap(std::int64_t{origin_x_} + JustifiedOffset());
      }
    }
  }

  const TextBoxText& box_;
  FontStyle& style_;
  Text widths_;
  ScaledGlyphTarget& target_;
  int scale_;
  std::int32_t cell_;
  std::int32_t origin_x_;
  std::int32_t x_;
  std::int32_t y_;
  std::size_t at_ = 0;
  bool justify_ = false;
};

[[nodiscard]] std::uint16_t BlendPixel(std::uint32_t destination,
                                       std::uint32_t source,
                                       std::uint32_t alpha,
                                       std::uint32_t inverse) noexcept {
  std::uint32_t blended = 0;
  for (const std::uint32_t mask : kColorChannelMasks) {
    blended |=
        ((((destination & mask) * inverse) + ((source & mask) * alpha)) >>
         kFixedPointShift) &
        mask;
  }
  return static_cast<std::uint16_t>(blended);
}

class FramebufferTextBlender {
 public:
  // NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the public entry's.
  FramebufferTextBlender(TextBoxText& box, const FramebufferTextBlend& blend,
                         FontStyle& style, const VariableFontTables& font,
                         VramPixelTarget& vram) noexcept
      : box_(box),
        blend_(blend),
        style_(style),
        font_(font),
        vram_(vram),
        width_((box.chars_per_line * kGlyphSize) + kBoxWidthMargin),
        height_((box.line_count * kLineHeight) + kBoxHeightMargin),
        alpha_((static_cast<std::uint32_t>(blend.alpha) >> kFixedPointShift) !=
                       0
                   ? kFullGlyphAlpha
                   : static_cast<std::uint32_t>(blend.alpha)),
        x_(LineStart()),
        y_(static_cast<std::uint32_t>(box.y_indent - box.y)) {}

  bool Blend() {
    if (box_.x < 0 || box_.y < 0 || width_ <= 0 || height_ <= 0 ||
        box_.x + width_ >= kScreenWidth || box_.y + height_ >= kScreenHeight) {
      return false;
    }
    const ScreenRect area{
        .x = static_cast<std::int16_t>(box_.x + blend_.framebuffer_x),
        .y = box_.y,
        .width = static_cast<std::int16_t>(width_),
        .height = static_cast<std::int16_t>(height_)};
    pixels_.assign(
        static_cast<std::size_t>(width_) * static_cast<std::size_t>(height_),
        0);
    vram_.Read(area, pixels_);
    style_.table_select = 1;
    for (;;) {
      const std::uint8_t code = Next();
      if (code < kTextFirstControl) {
        Glyph(code);
      } else if (code == kTextTerminator) {
        if (blend_.skip_pressed) {
          box_.speed = kTextBoxSkipSpeed;
        }
        break;
      } else if (code == kTextNewline) {
        x_ = LineStart() + JustifiedOffset();
        y_ += kLineHeight;
      } else if (code >= kTextFirstParameterized) {
        Parameterized(code, Next());
      }
    }
    vram_.Upload(area, pixels_);
    return true;
  }

 private:
  std::uint8_t Next() noexcept {
    return at_ < box_.text.size() ? box_.text[at_++] : kTextTerminator;
  }

  [[nodiscard]] std::uint32_t LineStart() const noexcept {
    return static_cast<std::uint32_t>(box_.x_indent - box_.x);
  }

  [[nodiscard]] std::uint32_t JustifiedOffset() const noexcept {
    if (!justify_) {
      return 0;
    }
    const Text rest = at_ < box_.text.size() ? box_.text.subspan(at_) : Text{};
    return static_cast<std::uint32_t>(
        JustifyUnits(box_, rest, font_.glyph_widths) * kJustifyHalfUnit);
  }

  // Space advances by its table-0 width; the other glyphs are drawn from the
  // selected table.
  void Glyph(std::uint8_t code) {
    std::uint32_t glyph = code;
    if (code != kTextSpaceGlyph) {
      glyph += std::uint32_t{style_.table_select} * kVariableFontGlyphsPerTable;
      if (glyph < font_.glyph_widths.size()) {
        BlendGlyph(glyph);
      }
    }
    if (glyph < font_.glyph_widths.size()) {
      x_ += font_.glyph_widths[glyph];
    }
  }

  void BlendGlyph(std::uint32_t glyph) {
    const auto room_x = static_cast<std::uint32_t>(width_ - kGlyphSize);
    const auto room_y = static_cast<std::uint32_t>(height_ - kGlyphSize);
    if (width_ < kGlyphSize || height_ < kGlyphSize || x_ > room_x ||
        y_ > room_y) {
      return;
    }
    std::array<std::uint16_t, kGlyphCellPixels> cell{};
    vram_.Read(
        ScreenRect{.x = static_cast<std::int16_t>(
                       ((glyph % kVariableFontGlyphsPerRow) * kGlyphCellWidth) +
                       kGlyphSheetX),
                   .y = static_cast<std::int16_t>(
                       (glyph / kVariableFontGlyphsPerRow) * kGlyphCellHeight),
                   .width = kGlyphCellWidth,
                   .height = kGlyphCellHeight},
        cell);
    const std::size_t stride = static_cast<std::size_t>(width_);
    BlendGlyphIntoPixels(std::span(pixels_).subspan(x_ + (y_ * stride)), stride,
                         cell, font_.pixel_expansion, alpha_);
  }

  void Parameterized(std::uint8_t code, std::uint8_t parameter) {
    if (code == kTextSpacing) {
      x_ += static_cast<std::uint32_t>(SpacingOffset(parameter));
      return;
    }
    if (code != kTextFontControl) {
      return;
    }
    const FontCommand command = ApplyFontControl(style_, parameter, false);
    if (command == FontCommand::kMoveDown) {
      y_ += std::uint32_t{parameter} >> kFontMoveDownShift;
    } else if (command == FontCommand::kJustify) {
      justify_ = !justify_;
      if (justify_) {
        x_ = LineStart() + JustifiedOffset();
      }
    }
  }

  TextBoxText& box_;
  const FramebufferTextBlend& blend_;
  FontStyle& style_;
  const VariableFontTables& font_;
  VramPixelTarget& vram_;
  int width_;
  int height_;
  std::uint32_t alpha_;
  std::uint32_t x_;
  std::uint32_t y_;
  std::vector<std::uint16_t> pixels_;
  std::size_t at_ = 0;
  bool justify_ = false;
};
}  // namespace

ProgressiveTextResult DrawProgressiveTextBoxText(
    TextBoxText& box, const TextBoxInput& input, FontStyle& style,
    std::span<const std::uint8_t> glyph_widths,
    const EncodedTextArguments& arguments, PacketQueue& glyphs) {
  if (box.speed == 0) {
    return {};
  }
  return ProgressiveTextDrawer(box, input, style, glyph_widths, arguments,
                               glyphs)
      .Draw();
}

void DrawScaledTextBoxText(const TextBoxText& box, int scale, FontStyle& style,
                           std::span<const std::uint8_t> glyph_widths,
                           ScaledGlyphTarget& target) {
  if (box.speed == 0) {
    return;
  }
  ScaledTextDrawer(box, scale, style, glyph_widths, target).Draw();
}

void BlendGlyphIntoPixels(std::span<std::uint16_t> pixels, std::size_t stride,
                          std::span<const std::uint16_t> cell,
                          std::span<const std::uint16_t> pixel_expansion,
                          std::uint32_t alpha) noexcept {
  const std::uint32_t weight = std::min(alpha, kFullGlyphAlpha);
  const std::uint32_t inverse = kFullGlyphAlpha - weight;
  constexpr auto kSize = static_cast<std::size_t>(kGlyphSize);
  constexpr auto kCellWords = static_cast<std::size_t>(kGlyphCellWidth);
  for (std::size_t row = 0; row < kSize; ++row) {
    for (std::size_t column = 0; column < kSize; ++column) {
      const std::size_t word =
          (column / kPixelsPerCellWord) + (row * kCellWords);
      const std::uint32_t packed = word < cell.size() ? cell[word] : 0;
      const std::uint32_t colour =
          (packed >> ((column % kPixelsPerCellWord) * kCellNibbleBits)) &
          kCellNibbleMask;
      const std::size_t at = (row * stride) + column;
      if (colour == 0 || colour >= pixel_expansion.size() ||
          at >= pixels.size()) {
        continue;
      }
      pixels[at] =
          BlendPixel(pixels[at], pixel_expansion[colour], weight, inverse);
    }
  }
}

bool BlendTextBoxIntoFramebuffer(TextBoxText& box,
                                 const FramebufferTextBlend& blend,
                                 FontStyle& style,
                                 const VariableFontTables& font,
                                 VramPixelTarget& vram) {
  return FramebufferTextBlender(box, blend, style, font, vram).Blend();
}
}  // namespace VS::Render
