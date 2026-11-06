// Hand-maintained (tools/port/owned.csv).
#include "Ending/Credits/StaffRollCore.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <optional>
#include <span>

#include "Core/Log.hpp"
#include "Core/Math/Interpolation.hpp"
#include "Core/Math/ScalarMath.hpp"

namespace VS::Ending::Credits {
namespace {
using BlendMode::kAdd;
using BlendMode::kSubtract;
using TextureDepth::k4Bit;
using TextureDepth::k8Bit;

// Element phases: kPhaseDone frees the slot on the next DrawElements pass.
constexpr std::int16_t kPhaseDone = -1;
constexpr std::int16_t kPhaseFree = 0;
constexpr std::int16_t kPhaseStart = 1;
constexpr std::int16_t kPhaseFadeIn = 2;
constexpr std::int16_t kPhaseHold = 3;
constexpr std::int16_t kPhaseFadeOut = 4;
constexpr std::int16_t kPhaseLastHold = 5;

// Scene ordering slots.
constexpr int kOverlaySlot = 0;
constexpr int kFinSlot = 1;
constexpr int kBackdropSlot = 2;
constexpr int kTextSlot = 3;
constexpr int kIllustrationSlot = 4;

constexpr int kFullBrightness = 128;
constexpr int kBrightnessShift = 7;
constexpr int kFixedShift = 12;
constexpr int kPositionShift = 8;
constexpr int kScreenWidth = 320;
constexpr int kScreenHeight = 240;
constexpr int kScrollStartY = 224;
constexpr int kTextHeight = 12;
constexpr int kEaseFrames = 128;
// An ease result (0..4096) over 32 is a brightness (0..128).
constexpr std::uint32_t kEaseToBrightness = 32;

constexpr int kRippleFadeFrames = 90;
constexpr int kRippleHoldFrames = 120;
constexpr int kRippleAngleStep = 200;
constexpr int kRippleDistanceScale = 400;
constexpr int kRippleScaleNumerator = 65;
constexpr int kRippleScaleShift = 10;
constexpr int kRippleDepth = 512;
constexpr int kRippleCellWidth = 9;
constexpr int kRippleCellHeight = 4;
constexpr int kRippleLeft = 72;
constexpr int kRippleTop = 8;
constexpr int kRippleTexelTop = 180;
constexpr int kRippleCenterRow = 2;
constexpr int kRippleCenterColumn = 8;
constexpr int kRippleColumnSpacing = 18;
constexpr int kRippleRowShift = 2;
constexpr std::uint16_t kRipplePalette = PaletteBits(896, 83);
constexpr std::uint16_t kRippleTexturePage = 46;

constexpr int kFinFadeFrames = 60;
constexpr int kFinHoldFrames = 240;
constexpr SpriteDraw kFinSprite{
    .ordering_slot = kFinSlot,
    .position = {248, 184},
    .texel = {0, 160},
    .width = 36,
    .height = 17,
    .palette = PaletteBits(896, 82),
};
constexpr auto kFinTexturePage = TexturePageBits(k4Bit, kAdd, 896, 0);

constexpr int kLogoFadeFrames = 60;
constexpr int kLogoStartX = 61;
constexpr int kLogoStartY = 80;
constexpr int kLogoHeight = 60;
constexpr SpriteDraw kLogoSprite{
    .ordering_slot = kTextSlot,
    .texel = {0, 96},
    .width = 208,
    .height = 48,
    .palette = PaletteBits(896, 80),
    .semi_transparent = true,
};
constexpr std::uint16_t kLogoShadowPalette = PaletteBits(896, 81);
constexpr std::uint16_t kLogoTexturePage = 0x2E;
constexpr std::uint16_t kLogoShadowTexturePage = 0x4E;

constexpr int kScreenCenterX = 160;
constexpr int kScreenCenterY = 112;
constexpr int kRadialFadeFrames = 80;
constexpr int kRadialFadeRadius = 200;
constexpr int kRadialFadeSpread = 400;
constexpr int kRadialFadeCeiling = 0x81;

constexpr int kIllustrationFadeFrames = 80;
constexpr std::uint16_t kLastIllustration = 15;
constexpr std::uint16_t kFirstDecodedIllustration = 4;
constexpr std::uint16_t kFirstUploadedIllustration = 5;
constexpr int kIllustrationPages = 5;
constexpr int kIllustrationPageStride = 192;
constexpr int kIllustrationPaletteRow = 480;
constexpr int kIllustrationPageRow = 256;
constexpr int kTexturePageWidth = 64;
constexpr SpriteDraw kIllustrationSprite{
    .ordering_slot = kIllustrationSlot,
    .width = 0xA0,
    .height = 0xE0,
    .semi_transparent = true,
};
constexpr std::int16_t kIllustrationRightX = 0xA0;
constexpr std::uint8_t kIllustrationRightTexel = 0x20;

constexpr std::uint8_t kFirstPrintable = 32;
constexpr std::uint8_t kSelectFirstTable = 1;
constexpr std::uint8_t kSelectSecondTable = 2;
constexpr std::uint8_t kUnderlineCode = 3;
constexpr std::uint8_t kBackspaceCode = 31;
constexpr std::uint8_t kSecondTableOffset = 112;
constexpr std::int16_t kSecondTableUnderline = 18;
constexpr std::int16_t kFirstTableUnderline = 17;
constexpr int kGlyphCellSize = 16;
constexpr int kGlyphColumnMask = 0xF;
constexpr int kGlyphRowShift = 4;
constexpr std::uint16_t kTextPalette = PaletteBits(960, 480);
constexpr std::uint16_t kTextTexturePage = 31;
constexpr int kUnderlineCapWidth = 16;
constexpr std::int16_t kUnderlineHeight = 3;
constexpr std::uint8_t kUnderlineTexelTop = 208;
constexpr std::uint8_t kUnderlineTexelBottom = 211;
constexpr std::uint8_t kUnderlineBarLeft = 8;
constexpr std::uint8_t kUnderlineBarRight = 16;
constexpr std::uint8_t kUnderlineRightCapU = 168;

constexpr int kCopyrightStartX = 100;
constexpr SpriteDraw kCopyrightSprite{
    .ordering_slot = kTextSlot,
    .texel = {0, 0x30},
    .width = 0x78,
    .height = 0x10,
    .palette = PaletteBits(960, 64),
};
constexpr auto kCopyrightTexturePage = TexturePageBits(k4Bit, kAdd, 960, 0);

constexpr int kBackgroundSlot = 0x7FF;
constexpr std::uint16_t kBackgroundPalette = PaletteBits(780, 255);
constexpr SpriteDraw kBackgroundLeft{
    .ordering_slot = kBackgroundSlot,
    .position = {0, 10},
    .texel = {0, 10},
    .width = 256,
    .height = 204,
    .palette = kBackgroundPalette,
};
constexpr SpriteDraw kBackgroundRight{
    .ordering_slot = kBackgroundSlot,
    .position = {256, 10},
    .texel = {0, 10},
    .width = 64,
    .height = 204,
    .palette = kBackgroundPalette,
};
constexpr auto kBackgroundLeftPage = TexturePageBits(k4Bit, kAdd, 768, 0);
constexpr auto kBackgroundRightPage = TexturePageBits(k4Bit, kAdd, 832, 0);
constexpr GradientQuadDraw kTopShade{
    .ordering_slot = kBackdropSlot,
    .corners = {{{0, 10}, {320, 10}, {0, 30}, {320, 30}}},
    .brightness = {255, 255, 0, 0},
    .semi_transparent = true,
};
constexpr GradientQuadDraw kBottomShade{
    .ordering_slot = kBackdropSlot,
    .corners = {{{0, 194}, {320, 194}, {0, 214}, {320, 214}}},
    .brightness = {0, 0, 255, 255},
    .semi_transparent = true,
};
constexpr TileDraw kTopBorder{
    .ordering_slot = kBackdropSlot,
    .width = 320,
    .height = 10,
};
constexpr TileDraw kBottomBorder{
    .ordering_slot = kBackdropSlot,
    .position = {0, 214},
    .width = 320,
    .height = 10,
};
constexpr auto kBackdropTexturePage = TexturePageBits(k8Bit, kSubtract, 0, 0);

constexpr int kSkipShadeMaximum = 255;
constexpr auto kSkipTexturePage = TexturePageBits(k4Bit, kSubtract, 0, 0);

constexpr int kGeometryCenterX = 0xA0;
constexpr int kGeometryCenterY = 0x70;
constexpr int kProjectionDistance = 0x200;
constexpr std::int16_t kScrollSpeedFixed8 = -128;
// The XA stream's byte-count argument (vs_main_startXaStream).
constexpr int kEndingMusicByteCount = 0x7FFF;
constexpr std::size_t kPrimitiveBufferBytes = 0x20000;
constexpr std::size_t kOrderingTableBytes = 0x2088;
constexpr OrderingTableLayout kOrderingTables{
    .entries = 0x822,
    .overlay_offset = 2,
    .scene_offset = 0x12,
};
constexpr std::int16_t kViewportBorder = 16;
constexpr Render::ScreenRect kScreenWindow{0, 8, 256, 224};
constexpr Render::ScreenRect kWholeVram{0, 0, 1024, 512};
constexpr DisplaySettings kStaffRollDisplay{
    .width = kScreenWidth,
    .height = kScreenHeight,
    .projection_distance = kProjectionDistance,
};
constexpr std::uint16_t kDrawTexturePage = 0x20;

constexpr std::uint8_t kOpWait = 1;
constexpr std::uint8_t kOpText = 2;
constexpr std::uint8_t kOpIllustration = 3;
constexpr std::uint8_t kOpSpecial = 4;
constexpr std::uint8_t kOpFadeIn = 5;
constexpr std::uint8_t kOpFadeOut = 6;
constexpr std::uint8_t kOpFreezeScroll = 7;
constexpr std::uint8_t kOpHoldFrames = 8;
constexpr std::uint8_t kOpEnd = 0xFF;
constexpr std::uint8_t kSpecialCopyright = 0;
constexpr std::uint8_t kSpecialTitleLogo = 1;
constexpr std::uint8_t kSpecialFin = 2;
constexpr std::uint8_t kSpecialRipple = 3;
constexpr std::size_t kHoldOperandBytes = 4;
constexpr int kHoldFramesBias = 160;
constexpr int kByteShift = 8;

constexpr int kDecodeBudgetTicks = 200;
constexpr std::uint8_t kBackreferenceFlag = 0x80;
constexpr std::uint8_t kEndOfImage = 0x7F;
constexpr std::uint8_t kFillRun = 0x7D;
constexpr std::uint8_t kFillPairRun = 0x7E;
constexpr std::uint8_t kRepeatBackreference = 0x7C;
constexpr int kBackreferenceLengthShift = 4;
constexpr int kBackreferenceLengthMask = 7;
constexpr int kBackreferenceMinimum = 3;
constexpr int kBackreferenceHighMask = 0xF;

constexpr std::uint8_t kSmallTextGlyphs = 96;
constexpr std::uint8_t kSmallTextAlias = 60;
constexpr std::uint8_t kSmallTextAliasGlyph = 15;
constexpr std::uint8_t kSmallTextLowercase = 64;
constexpr std::uint8_t kSmallTextCaseShift = 32;
constexpr int kSmallGlyphSize = 8;
constexpr int kSmallGlyphRowMask = 0xF0;
constexpr int kSmallTextAdvance = 7;
constexpr int kSmallTextPaletteX = 960;
constexpr int kSmallTextPaletteRow = 32;
constexpr std::uint16_t kSmallTextTexturePage = 0xF;

constexpr std::size_t kTimModeOffset = 4;
constexpr std::size_t kTimFirstBlock = 8;
constexpr std::size_t kTimRectOffset = 4;
constexpr std::size_t kTimPixelOffset = 12;
constexpr std::uint32_t kTimClutFlag = 8;

std::uint8_t LowByte(int value) { return static_cast<std::uint8_t>(value); }

std::int16_t PixelOf(std::int32_t fixed8) {
  return static_cast<std::int16_t>(fixed8 >> kPositionShift);
}

std::int32_t Ease(Core::Math::EaseCurve curve, int current, int total) {
  return Core::Math::EvaluateEase(curve, {static_cast<std::int16_t>(current),
                                          static_cast<std::int16_t>(total)});
}

void WarnUndefinedPhase(const char* element, std::int16_t phase) {
  VS_LOG_WARN("ending",
              "Credits {} phase {} sets no brightness; drawing it at 0",
              element, phase);
}

template <typename Value>
std::optional<Value> ReadValue(std::span<const std::byte> bytes,
                               std::size_t offset) {
  if (offset > bytes.size() || bytes.size() - offset < sizeof(Value)) {
    return std::nullopt;
  }
  Value value{};
  std::memcpy(&value, bytes.subspan(offset).data(), sizeof value);
  return value;
}

std::int16_t RippleDepth(const RippleState& ripple, std::size_t row,
                         std::size_t column, std::uint16_t scale) {
  const int distance = ripple.distance_grid[(row * kRippleColumns) + column];
  return static_cast<std::int16_t>(
      (Core::Math::Sine(ripple.phase_angle +
                        (distance * kRippleDistanceScale)) *
       scale) >>
      kFixedShift);
}

void DrawRippleCell(const RippleState& ripple, std::size_t row,
                    std::size_t column, std::uint16_t scale,
                    std::uint8_t brightness, StaffRollHost& host) {
  const int cell_x = static_cast<int>(column) * kRippleCellWidth;
  const int cell_y = static_cast<int>(row) * kRippleCellHeight;
  const auto left = static_cast<std::int16_t>(cell_x - kRippleLeft);
  const auto right =
      static_cast<std::int16_t>(cell_x + kRippleCellWidth - kRippleLeft);
  const auto top = static_cast<std::int16_t>(cell_y - kRippleTop);
  const auto bottom =
      static_cast<std::int16_t>(cell_y + kRippleCellHeight - kRippleTop);
  const std::array<ModelVertex, kQuadCorners> vertices{{
      {left, top, RippleDepth(ripple, row, column, scale)},
      {right, top, RippleDepth(ripple, row, column + 1, scale)},
      {left, bottom, RippleDepth(ripple, row + 1, column, scale)},
      {right, bottom, RippleDepth(ripple, row + 1, column + 1, scale)},
  }};
  const auto texel_left = LowByte(cell_x);
  const auto texel_right = LowByte(cell_x + kRippleCellWidth);
  const auto texel_top = LowByte(cell_y + kRippleTexelTop);
  const auto texel_bottom =
      LowByte(cell_y + kRippleCellHeight + kRippleTexelTop);
  host.DrawTexturedQuad({.ordering_slot = kTextSlot,
                         .corners = host.ProjectQuad(vertices),
                         .texels = {{{texel_left, texel_top},
                                     {texel_right, texel_top},
                                     {texel_left, texel_bottom},
                                     {texel_right, texel_bottom}}},
                         .palette = kRipplePalette,
                         .texture_page = kRippleTexturePage,
                         .brightness = brightness,
                         .semi_transparent = true});
}

// The characters after the text's length byte, bounded by the bytes lent.
std::span<const std::uint8_t> TextCharacters(
    std::span<const std::uint8_t> text) {
  if (text.empty()) {
    VS_LOG_WARN("ending", "Credits text refused: it starts past the script");
    return {};
  }
  const std::size_t length = text.front();
  const std::size_t available = text.size() - 1;
  if (length > available) {
    VS_LOG_WARN("ending",
                "Credits text of {} characters runs past the script; using {}",
                length, available);
    return text.subspan(1, available);
  }
  return text.subspan(1, length);
}

std::optional<int> GlyphWidth(std::span<const std::uint8_t> glyph_widths,
                              std::size_t glyph) {
  if (glyph >= glyph_widths.size()) {
    VS_LOG_WARN("ending",
                "Credits glyph {} refused: the width table has {} entries",
                glyph, glyph_widths.size());
    return std::nullopt;
  }
  return glyph_widths[glyph];
}

std::span<const std::uint8_t> ElementText(
    const Element& element, std::span<const std::uint8_t> script) {
  if (element.text_offset >= script.size()) return {};
  return script.subspan(element.text_offset);
}

void DrawUnderline(std::uint16_t width, std::int16_t y, std::uint8_t brightness,
                   DisplayHost& host) {
  auto x = static_cast<std::int16_t>(kScreenCenterX -
                                     ((width + (2 * kUnderlineCapWidth)) >> 1));
  host.DrawSprite({.ordering_slot = kTextSlot,
                   .position = {x, y},
                   .texel = {0, kUnderlineTexelTop},
                   .width = kUnderlineCapWidth,
                   .height = kUnderlineHeight,
                   .palette = kTextPalette,
                   .brightness = brightness});
  x = static_cast<std::int16_t>(x + kUnderlineCapWidth);
  const auto right = static_cast<std::int16_t>(x + width);
  const auto bottom = static_cast<std::int16_t>(y + kUnderlineHeight);
  host.DrawTexturedQuad(
      {.ordering_slot = kTextSlot,
       .corners = {{{x, y}, {right, y}, {x, bottom}, {right, bottom}}},
       .texels = {{{kUnderlineBarLeft, kUnderlineTexelTop},
                   {kUnderlineBarRight, kUnderlineTexelTop},
                   {kUnderlineBarLeft, kUnderlineTexelBottom},
                   {kUnderlineBarRight, kUnderlineTexelBottom}}},
       .palette = kTextPalette,
       .texture_page = kTextTexturePage,
       .brightness = brightness});
  host.DrawSprite({.ordering_slot = kTextSlot,
                   .position = {right, y},
                   .texel = {kUnderlineRightCapU, kUnderlineTexelTop},
                   .width = kUnderlineCapWidth,
                   .height = kUnderlineHeight,
                   .palette = kTextPalette,
                   .brightness = brightness});
}

void DrawTextLine(const Element& element, std::uint8_t brightness,
                  std::span<const std::uint8_t> characters,
                  std::span<const std::uint8_t> glyph_widths,
                  DisplayHost& host) {
  auto x = PixelOf(element.x_fixed8);
  const auto y = PixelOf(element.y_fixed8);
  std::uint8_t table_offset = 0;
  std::int16_t underline = 0;
  for (const std::uint8_t character : characters) {
    if (character < kFirstPrintable) {
      if (character == kSelectFirstTable) table_offset = 0;
      if (character == kSelectSecondTable) table_offset = kSecondTableOffset;
      if (character == kUnderlineCode) {
        underline =
            table_offset != 0 ? kSecondTableUnderline : kFirstTableUnderline;
      }
      if (character == kBackspaceCode) --x;
      continue;
    }
    const auto glyph =
        static_cast<std::uint8_t>(table_offset + character - kFirstPrintable);
    const int width = GlyphWidth(glyph_widths, glyph).value_or(0);
    host.DrawSprite(
        {.ordering_slot = kTextSlot,
         .position = {x, y},
         .texel = {LowByte((glyph & kGlyphColumnMask) * kGlyphCellSize),
                   LowByte((glyph >> kGlyphRowShift) * kGlyphCellSize)},
         .width = static_cast<std::int16_t>(width),
         .height = kGlyphCellSize,
         .palette = kTextPalette,
         .brightness = brightness});
    x = static_cast<std::int16_t>(x + width - 1);
  }
  if (underline != 0) {
    DrawUnderline(element.width, static_cast<std::int16_t>(y + underline),
                  brightness, host);
  }
}

bool UploadTimBlock(std::span<const std::byte> tim, std::size_t block,
                    DisplayHost& host) {
  const auto rect = ReadValue<Render::ScreenRect>(tim, block + kTimRectOffset);
  if (!rect || rect->width < 0 || rect->height < 0) {
    VS_LOG_WARN("ending",
                "Texture upload refused: the TIM block at {} lies outside its "
                "{} bytes",
                block, tim.size());
    return false;
  }
  const std::size_t pixels = block + kTimPixelOffset;
  const auto bytes = static_cast<std::size_t>(rect->width) *
                     static_cast<std::size_t>(rect->height) * 2;
  if (pixels > tim.size() || tim.size() - pixels < bytes) {
    VS_LOG_WARN("ending",
                "Texture upload refused: the TIM block at {} needs {} pixel "
                "bytes past its {} bytes",
                block, bytes, tim.size());
    return false;
  }
  host.LoadImage(*rect, tim.subspan(pixels, bytes));
  return true;
}

Element* StartElement(Element& element, ElementKind kind) {
  element.kind = kind;
  element.phase = kPhaseStart;
  element.animation_frame = 0;
  return &element;
}

void DrawElement(Element& element, State& state, StaffRollHost& host) {
  switch (element.kind) {
    case ElementKind::kPhantomPainRipple:
      DrawPhantomPainRipple(element, state.ripple, host);
      break;
    case ElementKind::kFinCaption:
      DrawFinCaption(element, host);
      break;
    case ElementKind::kScrollingTitleLogo:
      DrawScrollingTitleLogo(element, state, host);
      break;
    case ElementKind::kIllustration:
      DrawIllustration(element, state, host);
      break;
    case ElementKind::kScrollingText:
      DrawScrollingText(element, state, host);
      break;
    case ElementKind::kScrollingCopyright:
      DrawScrollingCopyright(element, state, host);
      break;
    case ElementKind::kBackground:
      DrawBackground(state, host);
      break;
    case ElementKind::kFadeInBackground:
      FadeInBackground(element, state);
      break;
    case ElementKind::kFadeOutBackground:
      FadeOutBackground(element, state);
      break;
    case ElementKind::kFadeOutForSkip:
      FadeOutForSkip(element, state, host);
      break;
    case ElementKind::kNone:
      VS_LOG_WARN(
          "ending",
          "Credits element refused: phase {} has no renderer; freeing it",
          element.phase);
      element.phase = kPhaseFree;
      break;
  }
}

bool ScriptHolds(State& state, std::span<const std::uint8_t> script,
                 std::size_t at, std::size_t count) {
  if (at <= script.size() && script.size() - at >= count) return true;
  VS_LOG_WARN("ending",
              "Credits script read refused: {} bytes at {} pass its {} bytes; "
              "ending the staff roll",
              count, at, script.size());
  state.has_finished = true;
  return false;
}

void StartSpecialElement(State& state, std::uint8_t special) {
  if (special == kSpecialCopyright) {
    AddElement(state, ElementKind::kScrollingCopyright);
  } else if (special == kSpecialTitleLogo) {
    AddElement(state, ElementKind::kScrollingTitleLogo);
  } else if (special == kSpecialFin) {
    AddElement(state, ElementKind::kFinCaption);
  } else if (special == kSpecialRipple) {
    AddElement(state, ElementKind::kPhantomPainRipple);
  }
}

void SetHoldFrames(State& state, std::span<const std::uint8_t> operands) {
  const std::size_t index = operands[1];
  const int frames =
      operands[2] + (operands[3] << kByteShift) - kHoldFramesBias;
  if (index >= state.illustration.hold_frames.size()) {
    VS_LOG_WARN("ending",
                "Credits hold time refused: illustration {} is past the {} "
                "timers",
                index, state.illustration.hold_frames.size());
    return;
  }
  state.illustration.hold_frames[index] = static_cast<std::uint16_t>(frames);
}

// One script opcode at the cursor; false when the script yields the frame.
bool ExecuteOpcode(State& state, std::span<const std::uint8_t> script) {
  const std::size_t at = state.script_cursor;
  if (!ScriptHolds(state, script, at, 1)) return false;
  const std::uint8_t opcode = script[at];
  switch (opcode) {
    case kOpWait:
      if (!ScriptHolds(state, script, at, 2)) return false;
      state.script_cursor = at + 2;
      state.script_delay_frames = script[at + 1] * 2;
      return false;
    case kOpText: {
      state.script_cursor = at + 1;
      if (!ScriptHolds(state, script, at, 2)) return false;
      if (Element* element = AddElement(state, ElementKind::kScrollingText)) {
        element->text_offset = at + 1;
      }
      state.script_cursor += static_cast<std::size_t>(script[at + 1]) + 1;
      return true;
    }
    case kOpIllustration:
      state.script_cursor = at + 1;
      if (Element* element = AddElement(state, ElementKind::kIllustration)) {
        element->illustration_index = 0;
      }
      return true;
    case kOpSpecial:
      if (!ScriptHolds(state, script, at, 2)) return false;
      state.script_cursor = at + 2;
      StartSpecialElement(state, script[at + 1]);
      return true;
    case kOpFadeIn:
      state.script_cursor = at + 1;
      ReserveMainElement(state, ElementKind::kFadeInBackground);
      return true;
    case kOpFadeOut:
      state.script_cursor = at + 1;
      if (Element* element =
              ReserveMainElement(state, ElementKind::kFadeOutBackground)) {
        element->width = 0;
      } else {
        VS_LOG_WARN("ending",
                    "Credits fade-out refused: the main element is in use");
      }
      return true;
    case kOpFreezeScroll:
      state.script_cursor = at + 1;
      state.scroll_speed_fixed8 = 0;
      return true;
    case kOpHoldFrames:
      if (!ScriptHolds(state, script, at, kHoldOperandBytes)) return false;
      SetHoldFrames(state, script.subspan(at, kHoldOperandBytes));
      state.script_cursor = at + kHoldOperandBytes;
      return true;
    case kOpEnd:
      state.has_finished = true;
      return false;
    default:
      return false;
  }
}

enum class DecodeResult : std::uint8_t { kContinue, kComplete, kRefused };

// The illustration decoder's cursors over one compressed resource and the
// workspace; every read and write is bounded.
class ImageDecoder {
 public:
  ImageDecoder(std::span<const std::byte> input, std::span<std::byte> output,
               DecoderState& state)
      : input_(input), output_(output), state_(state) {}

  DecodeResult Step() {
    const auto opcode = Peek();
    if (!opcode) return Refuse("its input ends without an end code");
    if ((*opcode & kBackreferenceFlag) != 0) return CopyBackreferences(*opcode);
    if (*opcode == kEndOfImage) return DecodeResult::kComplete;
    if (*opcode == kFillRun || *opcode == kFillPairRun) {
      ++state_.input;
      const auto count = Read();
      const auto first = Read();
      const auto second = *opcode == kFillPairRun ? Read() : first;
      if (!count || !first || !second) return Refuse("a fill run ends early");
      for (int index = 0; index <= *count; ++index) {
        if (!Write(*first)) return Refuse("a fill run passes the workspace");
        if (*opcode == kFillPairRun && !Write(*second)) {
          return Refuse("a fill run passes the workspace");
        }
      }
      return DecodeResult::kContinue;
    }
    ++state_.input;
    for (int index = 0; index <= *opcode; ++index) {
      const auto literal = Read();
      if (!literal) return Refuse("a literal run ends early");
      if (!Write(*literal)) return Refuse("a literal run passes the workspace");
    }
    return DecodeResult::kContinue;
  }

 private:
  [[nodiscard]] std::optional<std::uint8_t> Peek() const {
    if (state_.input >= input_.size()) return std::nullopt;
    return static_cast<std::uint8_t>(input_[state_.input]);
  }

  std::optional<std::uint8_t> Read() {
    const auto value = Peek();
    if (value) ++state_.input;
    return value;
  }

  bool Write(std::uint8_t value) {
    if (state_.output >= output_.size()) return false;
    output_[state_.output++] = static_cast<std::byte>(value);
    return true;
  }

  bool CopyBack(std::size_t distance, int length) {
    if (distance > state_.output) return false;
    for (int index = 0; index < length; ++index) {
      if (state_.output >= output_.size()) return false;
      output_[state_.output] = output_[state_.output - distance];
      ++state_.output;
    }
    return true;
  }

  DecodeResult CopyBackreferences(std::uint8_t opcode) {
    const int length =
        ((opcode >> kBackreferenceLengthShift) & kBackreferenceLengthMask) +
        kBackreferenceMinimum;
    ++state_.input;
    const auto low = Read();
    if (!low) return Refuse("a back-reference ends early");
    const std::size_t distance =
        (static_cast<std::size_t>(opcode & kBackreferenceHighMask)
         << kByteShift) +
        *low + 1;
    if (!CopyBack(distance, length)) {
      return Refuse("a back-reference leaves the workspace");
    }
    while (Peek() == kRepeatBackreference) {
      ++state_.input;
      if (!CopyBack(distance, length)) {
        return Refuse("a back-reference leaves the workspace");
      }
    }
    return DecodeResult::kContinue;
  }

  DecodeResult Refuse(const char* reason) {
    VS_LOG_WARN("ending",
                "Illustration {} decode refused: {} (input {} of {}, output "
                "{} of {})",
                state_.resource, reason, state_.input, input_.size(),
                state_.output, output_.size());
    return DecodeResult::kRefused;
  }

  std::span<const std::byte> input_;
  std::span<std::byte> output_;
  DecoderState& state_;
};
}  // namespace

void FillRippleGrid(RippleState& ripple) {
  for (std::size_t entry = 0; entry < ripple.distance_grid.size(); ++entry) {
    const int row = static_cast<int>(entry / kRippleColumns);
    const int column = static_cast<int>(entry % kRippleColumns);
    const int dy = std::abs(kRippleCenterRow - row) << kRippleRowShift;
    const int dx =
        std::abs(kRippleCenterColumn - column) * kRippleColumnSpacing;
    ripple.distance_grid[entry] = static_cast<std::uint8_t>(
        Core::Math::SquareRootInteger((dx * dx) + (dy * dy)));
  }
}

// decomp: ENDING.PRG _renderPhantomPain (src/ENDING/ENDING.PRG/D4.c:218)
void DrawPhantomPainRipple(Element& element, RippleState& ripple,
                           StaffRollHost& host) {
  int brightness = 0;
  switch (element.phase) {
    case kPhaseStart:
      ripple.phase_angle = 0;
      element.animation_frame = 0;
      element.phase = kPhaseFadeIn;
      ripple.scale_frame = 0;
      FillRippleGrid(ripple);
      VS_LOG_WARN("ending",
                  "Phantom pain ripple: the last quad row reads {} distance "
                  "entries past the 4x16 grid; using the extended grid",
                  kRippleGridEntries - (kRippleRows * kRippleColumns));
      [[fallthrough]];
    case kPhaseFadeIn:
      brightness =
          (element.animation_frame << kBrightnessShift) / kRippleFadeFrames;
      ++ripple.scale_frame;
      if (++element.animation_frame == kRippleFadeFrames) {
        element.animation_frame = 0;
        element.phase = kPhaseHold;
      }
      break;
    case kPhaseHold:
      brightness = kFullBrightness;
      if (++element.animation_frame == kRippleHoldFrames) {
        element.animation_frame = 0;
        element.phase = kPhaseFadeOut;
      }
      break;
    case kPhaseFadeOut:
      brightness =
          ((kRippleFadeFrames - element.animation_frame) << kBrightnessShift) /
          kRippleFadeFrames;
      if (++element.animation_frame == kRippleFadeFrames) {
        element.phase = kPhaseDone;
      }
      break;
    default:
      WarnUndefinedPhase("phantom pain ripple", element.phase);
      break;
  }
  ripple.phase_angle += kRippleAngleStep;
  const auto scale = static_cast<std::uint16_t>(
      (Ease(Core::Math::EaseCurve::kEaseIn,
            kRippleFadeFrames - ripple.scale_frame, kRippleFadeFrames) *
       kRippleScaleNumerator) >>
      kRippleScaleShift);
  host.BeginRippleProjection(kRippleDepth);
  for (std::size_t row = 0; row < kRippleRows; ++row) {
    for (std::size_t column = 0; column < kRippleColumns; ++column) {
      DrawRippleCell(ripple, row, column, scale, LowByte(brightness), host);
    }
  }
}

// decomp: ENDING.PRG _renderFin (src/ENDING/ENDING.PRG/D4.c:325)
void DrawFinCaption(Element& element, DisplayHost& host) {
  int brightness = 0;
  switch (element.phase) {
    case kPhaseStart:
      element.animation_frame = 0;
      element.phase = kPhaseFadeIn;
      [[fallthrough]];
    case kPhaseFadeIn: {
      const int frame = element.animation_frame++;
      brightness = (frame << kBrightnessShift) / kFinFadeFrames;
      if (element.animation_frame == kFinFadeFrames) {
        element.animation_frame = 0;
        element.phase = kPhaseHold;
      }
      break;
    }
    case kPhaseHold:
      brightness = kFullBrightness;
      if (++element.animation_frame == kFinHoldFrames) {
        element.animation_frame = 0;
        element.phase = kPhaseFadeOut;
      }
      break;
    case kPhaseFadeOut: {
      const int frame = element.animation_frame++;
      brightness =
          ((kFinFadeFrames - frame) << kBrightnessShift) / kFinFadeFrames;
      if (element.animation_frame == kFinFadeFrames) {
        element.phase = kPhaseDone;
      }
      break;
    }
    default:
      WarnUndefinedPhase("fin caption", element.phase);
      break;
  }
  SpriteDraw sprite = kFinSprite;
  sprite.brightness = LowByte(brightness);
  host.DrawSprite(sprite);
  host.ChangeTexturePage({kFinSlot, kFinTexturePage});
}

// decomp: ENDING.PRG _renderVagrantStory (src/ENDING/ENDING.PRG/D4.c:390)
void DrawScrollingTitleLogo(Element& element, const State& state,
                            DisplayHost& host) {
  int brightness = 0;
  switch (element.phase) {
    case kPhaseStart:
      element.x_fixed8 = kLogoStartX << kPositionShift;
      element.y_fixed8 = kLogoStartY << kPositionShift;
      element.animation_frame = 0;
      element.phase = kPhaseFadeIn;
      [[fallthrough]];
    case kPhaseFadeIn:
      ++element.animation_frame;
      brightness = (state.background_brightness * element.animation_frame) /
                   kLogoFadeFrames;
      if (element.animation_frame == kLogoFadeFrames) {
        element.phase = kPhaseHold;
        element.animation_frame = 0;
      }
      break;
    case kPhaseHold:
      brightness = state.background_brightness;
      if (++element.animation_frame == kLogoFadeFrames) {
        element.phase = kPhaseFadeOut;
      }
      break;
    case kPhaseFadeOut:
      brightness = state.background_brightness;
      element.y_fixed8 += state.scroll_speed_fixed8;
      if (element.y_fixed8 < -(kLogoHeight << kPositionShift)) {
        element.phase = kPhaseDone;
        return;
      }
      break;
    default:
      WarnUndefinedPhase("title logo", element.phase);
      break;
  }
  SpriteDraw sprite = kLogoSprite;
  sprite.position = {PixelOf(element.x_fixed8), PixelOf(element.y_fixed8)};
  sprite.brightness = LowByte(brightness);
  host.DrawSprite(sprite);
  host.ChangeTexturePage({kTextSlot, kLogoTexturePage});
  sprite.palette = kLogoShadowPalette;
  host.DrawSprite(sprite);
  host.ChangeTexturePage({kTextSlot, kLogoShadowTexturePage});
}

// decomp: ENDING.PRG _inverseDistanceFromScreencenter
// (src/ENDING/ENDING.PRG/D4.c:468)
int DistanceFromScreenCenter(int x, int y) {
  const std::int64_t dx = static_cast<std::int64_t>(x) - kScreenCenterX;
  const std::int64_t dy = static_cast<std::int64_t>(y) - kScreenCenterY;
  // The R3000 multiply and add keep the low 32 bits.
  return Core::Math::SquareRootInteger(
      static_cast<std::int32_t>((dx * dx) + (dy * dy)));
}

// decomp: ENDING.PRG func_80069388 (src/ENDING/ENDING.PRG/D4.c:475)
void ApplyRadialVertexFade(std::span<ShadedVertex, kQuadCorners> corners,
                           std::int16_t fade_step, bool fade_out) {
  const int base = fade_out
                       ? ((kRadialFadeFrames - fade_step) << kBrightnessShift) /
                             kRadialFadeFrames
                       : (fade_step * kFullBrightness) / kRadialFadeFrames;
  for (ShadedVertex& vertex : corners) {
    const int falloff =
        ((kRadialFadeRadius -
          DistanceFromScreenCenter(vertex.position.x, vertex.position.y))
         << kBrightnessShift) /
        kRadialFadeSpread;
    const auto value =
        static_cast<std::int16_t>(fade_out ? base - falloff : base + falloff);
    if (fade_out) {
      vertex.brightness = value >= 0 ? LowByte(value) : 0;
    } else {
      vertex.brightness =
          value < kRadialFadeCeiling ? LowByte(value) : kFullBrightness;
    }
  }
}

// decomp: ENDING.PRG _renderllustration (src/ENDING/ENDING.PRG/D4.c:583)
void DrawIllustration(Element& element, State& state, StaffRollHost& host) {
  IllustrationState& illustration = state.illustration;
  const int background = state.background_brightness;
  switch (element.phase) {
    case kPhaseStart: {
      const std::uint16_t index = element.illustration_index;
      if (index >= kFirstDecodedIllustration && index != kLastIllustration) {
        QueueIllustrationDecode(state.decoder, index + 1U);
      }
      element.animation_frame = 0;
      element.phase = kPhaseFadeIn;
      illustration.texture_page_x = static_cast<std::int16_t>(
          (index % kIllustrationPages) * kIllustrationPageStride);
      illustration.palette_y = static_cast<std::uint16_t>(
          (index % kIllustrationPages) + kIllustrationPaletteRow);
      [[fallthrough]];
    }
    case kPhaseFadeIn:
      illustration.brightness = static_cast<std::int16_t>(
          (background * element.animation_frame) / kIllustrationFadeFrames);
      if (++element.animation_frame == kIllustrationFadeFrames) {
        element.animation_frame = 0;
        element.phase = element.illustration_index == kLastIllustration
                            ? kPhaseLastHold
                            : kPhaseHold;
      }
      break;
    case kPhaseHold: {
      illustration.brightness = kFullBrightness;
      const std::size_t index = element.illustration_index;
      const int hold = index < illustration.hold_frames.size()
                           ? illustration.hold_frames[index]
                           : 0;
      if (index >= illustration.hold_frames.size()) {
        VS_LOG_WARN("ending",
                    "Illustration {} hold refused: past the {} timers; holding",
                    index, illustration.hold_frames.size());
      }
      if (++element.animation_frame == hold) {
        element.animation_frame = 0;
        element.phase = kPhaseFadeOut;
      }
      break;
    }
    case kPhaseFadeOut:
      illustration.brightness = static_cast<std::int16_t>(
          (background * (kIllustrationFadeFrames - element.animation_frame)) /
          kIllustrationFadeFrames);
      if (++element.animation_frame == kIllustrationFadeFrames) {
        if (++element.illustration_index >= kFirstUploadedIllustration) {
          UploadTexture(host.IllustrationWorkspace(), host);
        }
        element.phase = kPhaseStart;
      }
      break;
    case kPhaseLastHold:
      illustration.brightness = kFullBrightness;
      break;
    default:
      break;
  }
  SpriteDraw sprite = kIllustrationSprite;
  sprite.palette = PaletteBits(0, illustration.palette_y);
  sprite.brightness = LowByte(illustration.brightness);
  host.DrawSprite(sprite);
  host.ChangeTexturePage(
      {kIllustrationSlot,
       TexturePageBits(TextureDepth::k8Bit, BlendMode::kSubtract,
                       illustration.texture_page_x, kIllustrationPageRow)});
  sprite.position.x = kIllustrationRightX;
  sprite.texel.u = kIllustrationRightTexel;
  host.DrawSprite(sprite);
  host.ChangeTexturePage(
      {kIllustrationSlot,
       TexturePageBits(TextureDepth::k8Bit, BlendMode::kSubtract,
                       illustration.texture_page_x + kTexturePageWidth,
                       kIllustrationPageRow)});
}

// decomp: ENDING.PRG _creditsStrWidth (src/ENDING/ENDING.PRG/D4.c:672)
std::int16_t MeasureTextWidth(std::span<const std::uint8_t> text,
                              std::span<const std::uint8_t> glyph_widths) {
  std::int16_t width = 0;
  std::size_t table_offset = 0;
  for (const std::uint8_t character : TextCharacters(text)) {
    if (character >= kFirstPrintable) {
      const std::size_t index =
          table_offset + static_cast<std::size_t>(character - kFirstPrintable);
      if (const auto glyph = GlyphWidth(glyph_widths, index)) {
        width = static_cast<std::int16_t>(width + *glyph - 1);
      }
    } else if (character == kSelectFirstTable) {
      table_offset = 0;
    } else if (character == kSelectSecondTable) {
      table_offset = kSecondTableOffset;
    } else if (character == kBackspaceCode) {
      --width;
    }
  }
  return width;
}

// decomp: ENDING.PRG _renderText (src/ENDING/ENDING.PRG/D4.c:711)
void DrawScrollingText(Element& element, const State& state,
                       StaffRollHost& host) {
  const auto text = ElementText(element, host.Script());
  const auto glyph_widths = host.GlyphWidths();
  if (element.phase == kPhaseStart) {
    const auto width =
        static_cast<std::uint16_t>(MeasureTextWidth(text, glyph_widths));
    element.width = width;
    element.x_fixed8 = ((kScreenWidth - width) >> 1) << kPositionShift;
    element.y_fixed8 = kScrollStartY << kPositionShift;
    element.phase = kPhaseFadeIn;
  }
  element.y_fixed8 += state.scroll_speed_fixed8;
  if (element.y_fixed8 < -(kTextHeight << kPositionShift)) {
    element.phase = kPhaseDone;
    return;
  }
  DrawTextLine(element, LowByte(state.background_brightness),
               TextCharacters(text), glyph_widths, host);
  host.ChangeTexturePage({kTextSlot, kTextTexturePage});
}

// decomp: ENDING.PRG _renderCopyright (src/ENDING/ENDING.PRG/D4.c:831)
void DrawScrollingCopyright(Element& element, const State& state,
                            DisplayHost& host) {
  if (element.phase == kPhaseStart) {
    element.x_fixed8 = kCopyrightStartX << kPositionShift;
    element.y_fixed8 = kScrollStartY << kPositionShift;
    element.phase = kPhaseFadeIn;
  }
  element.y_fixed8 += state.scroll_speed_fixed8;
  if (element.y_fixed8 < -(kTextHeight << kPositionShift)) {
    element.phase = kPhaseDone;
    return;
  }
  SpriteDraw sprite = kCopyrightSprite;
  sprite.position = {PixelOf(element.x_fixed8), PixelOf(element.y_fixed8)};
  sprite.brightness = LowByte(state.background_brightness);
  host.DrawSprite(sprite);
  host.ChangeTexturePage({kTextSlot, kCopyrightTexturePage});
}

// decomp: ENDING.PRG _renderBackground (src/ENDING/ENDING.PRG/D4.c:865)
void DrawBackground(const State& state, DisplayHost& host) {
  const auto brightness = LowByte(state.background_brightness);
  SpriteDraw left = kBackgroundLeft;
  left.brightness = brightness;
  host.DrawSprite(left);
  host.ChangeTexturePage({kBackgroundSlot, kBackgroundLeftPage});
  SpriteDraw right = kBackgroundRight;
  right.brightness = brightness;
  host.DrawSprite(right);
  host.ChangeTexturePage({kBackgroundSlot, kBackgroundRightPage});
  host.DrawGradientQuad(kTopShade);
  host.DrawGradientQuad(kBottomShade);
  host.DrawTile(kTopBorder);
  host.DrawTile(kTopBorder);
  host.DrawTile(kBottomBorder);
  host.ChangeTexturePage({kBackdropSlot, kBackdropTexturePage});
}

// decomp: ENDING.PRG _backgroundFadeIn (src/ENDING/ENDING.PRG/D4.c:939)
void FadeInBackground(Element& element, State& state) {
  if (element.phase == kPhaseStart) {
    element.animation_frame = 0;
    element.phase = kPhaseFadeIn;
  }
  if (++element.animation_frame == kEaseFrames) {
    state.is_background_fading = false;
    element.phase = kPhaseDone;
  }
  const auto eased = static_cast<std::uint32_t>(Ease(
      Core::Math::EaseCurve::kLinear, element.animation_frame, kEaseFrames));
  state.background_brightness =
      static_cast<std::uint16_t>(eased / kEaseToBrightness);
}

// decomp: ENDING.PRG _backgroundFadeOut (src/ENDING/ENDING.PRG/D4.c:954)
void FadeOutBackground(Element& element, State& state) {
  if (element.phase == kPhaseStart) {
    element.phase = kPhaseFadeIn;
    element.animation_frame = 0;
    state.is_background_fading = true;
  }
  if (++element.animation_frame == kEaseFrames) {
    if (element.width != 0) state.has_finished = true;
    element.phase = kPhaseDone;
  }
  const int eased = Ease(Core::Math::EaseCurve::kLinear,
                         element.animation_frame, kEaseFrames);
  state.background_brightness = static_cast<std::uint16_t>(
      kFullBrightness - ((eased << kBrightnessShift) >> kFixedShift));
}

// decomp: ENDING.PRG func_8006A4D8 (src/ENDING/ENDING.PRG/D4.c:972)
void FadeOutForSkip(Element& element, State& state, DisplayHost& host) {
  if (element.phase == kPhaseStart) {
    element.animation_frame = 0;
    element.phase = kPhaseFadeIn;
  }
  if (++element.animation_frame == kEaseFrames) state.has_finished = true;
  const int eased = Ease(Core::Math::EaseCurve::kEaseOut,
                         element.animation_frame, kEaseFrames);
  host.DrawTile(
      {.ordering_slot = kOverlaySlot,
       .width = kScreenWidth,
       .height = kScreenHeight,
       .brightness = LowByte((eased * kSkipShadeMaximum) >> kFixedShift),
       .semi_transparent = true});
  host.ChangeTexturePage({kOverlaySlot, kSkipTexturePage});
}

// decomp: ENDING.PRG vs_ending_exec (src/ENDING/ENDING.PRG/D4.c:999)
void RunStaffRoll(State& state, StaffRollHost& host) {
  AllocateRenderBuffers(host);
  if (!host.PrepareEmbeddedArt()) {
    VS_LOG_WARN("ending",
                "Staff roll refused: the embedded art could not be prepared");
    return;
  }
  host.SetGeometryOffset(kGeometryCenterX, kGeometryCenterY);
  host.SetProjectionDistance(kProjectionDistance);
  host.ApplyDrawEnvironment(0, ConfigureDrawEnvironment());
  host.ApplyDrawEnvironment(1, ConfigureDrawEnvironment());
  host.SetInterlaced(1, false);
  host.SetInterlaced(0, false);
  for (std::size_t index = 0; index < kEmbeddedArtCount; ++index) {
    UploadTexture(host.EmbeddedArt(index), host);
  }
  for (std::size_t resource = kFirstDiscIllustration;
       resource < kIllustrationCount; ++resource) {
    host.BeginIllustrationRead(resource);
    do {
      host.AdvanceFrameClock();
    } while (!host.IsIllustrationReadComplete());
    host.EndIllustrationRead();
  }
  state.background_brightness = 0;
  state.was_skip_requested = false;
  ResetElements(state);
  AddElement(state, ElementKind::kBackground);
  host.SetDisplayEnabled(true);
  state.script_cursor = 0;
  state.scroll_speed_fixed8 = kScrollSpeedFixed8;
  state.has_finished = false;
  state.script_delay_frames = 0;
  state.elapsed_frames = 0;
  state.is_background_fading = true;
  StartMusic(host);
  RunFrames(state, host);
  host.StopMusic();
  host.SetDisplayEnabled(false);
  for (std::size_t resource = kFirstDiscIllustration;
       resource < kIllustrationCount; ++resource) {
    host.FreeIllustration(resource);
  }
  WaitForMusicEnd(host);
  host.UpdateClearProgress();
  host.ShowClearResults();
}

// decomp: ENDING.PRG _setDrawEnv (src/ENDING/ENDING.PRG/D4.c:1070)
DrawEnvironment ConfigureDrawEnvironment() {
  return {.clear_background = true,
          .dither = false,
          .draw_on_display = false,
          .background = {0, 0, 0},
          .texture_page = kDrawTexturePage};
}

// decomp: ENDING.PRG func_8006A888 (src/ENDING/ENDING.PRG/D4.c:1081)
void RunFrames(State& state, StaffRollHost& host) {
  while (!state.has_finished) {
    state.decoder.budget_start_tick =
        static_cast<std::int16_t>(host.VerticalBlankCounter());
    BeginFrame(host);
    ExecuteScript(state, host);
    DrawElements(state, host);
    DecodeIllustrationStep(state, host);
    if ((host.ButtonsPressed() & kStartButton) != 0 &&
        !state.was_skip_requested) {
      AddElement(state, ElementKind::kFadeOutForSkip);
      state.was_skip_requested = true;
    }
    PresentFrame(kFrameOrderingSlot, host);
    ++state.elapsed_frames;
  }
}

// decomp: ENDING.PRG func_8006A9C0 (src/ENDING/ENDING.PRG/D4.c:1115)
bool UploadTexture(std::span<const std::byte> tim, DisplayHost& host) {
  const auto mode = ReadValue<std::int32_t>(tim, kTimModeOffset);
  if (!mode) {
    VS_LOG_WARN("ending", "Texture upload refused: {} bytes hold no TIM header",
                tim.size());
    return false;
  }
  std::size_t block = kTimFirstBlock;
  if ((static_cast<std::uint32_t>(*mode) & kTimClutFlag) != 0) {
    if (!UploadTimBlock(tim, block, host)) return false;
    const auto length = ReadValue<std::int32_t>(tim, block);
    if (!length || *length < 0) {
      VS_LOG_WARN("ending",
                  "Texture upload refused: the CLUT block length is "
                  "invalid");
      return false;
    }
    block = static_cast<std::size_t>(*length) + kTimFirstBlock;
  }
  return UploadTimBlock(tim, block, host);
}

// decomp: ENDING.PRG _resetElements (src/ENDING/ENDING.PRG/D4.c:1137)
void ResetElements(State& state) {
  for (Element& element : state.elements) element.phase = kPhaseFree;
}

// decomp: ENDING.PRG _renderElements (src/ENDING/ENDING.PRG/D4.c:1146)
int DrawElements(State& state, StaffRollHost& host) {
  int active = 0;
  for (Element& element : state.elements) {
    if (element.phase < 0) {
      element.phase = kPhaseFree;
    } else if (element.phase > 0) {
      ++active;
      DrawElement(element, state, host);
    }
  }
  return active;
}

// decomp: ENDING.PRG _addRenderer (src/ENDING/ENDING.PRG/D4.c:1164)
Element* AddElement(State& state, ElementKind kind) {
  for (std::size_t index = 1; index < state.elements.size(); ++index) {
    if (state.elements[index].phase == kPhaseFree) {
      return StartElement(state.elements[index], kind);
    }
  }
  VS_LOG_WARN("ending", "Credits element refused: all {} slots are in use",
              state.elements.size() - 1);
  return nullptr;
}

// decomp: ENDING.PRG _replaceMainRenderer (src/ENDING/ENDING.PRG/D4.c:1181)
Element* ReserveMainElement(State& state, ElementKind kind) {
  Element& main = state.elements.front();
  return main.phase == kPhaseFree ? StartElement(main, kind) : nullptr;
}

// decomp: ENDING.PRG _parseCreditsScript (src/ENDING/ENDING.PRG/D4.c:1193)
void ExecuteScript(State& state, StaffRollHost& host) {
  if (state.script_delay_frames != 0) {
    --state.script_delay_frames;
    return;
  }
  const auto script = host.Script();
  while (ExecuteOpcode(state, script)) {
  }
}

// decomp: ENDING.PRG func_8006AF44 (src/ENDING/ENDING.PRG/D4.c:1307)
void QueueIllustrationDecode(DecoderState& decoder, std::size_t resource) {
  decoder.phase = DecodePhase::kQueued;
  decoder.resource = resource;
}

// decomp: ENDING.PRG func_8006AF64 (src/ENDING/ENDING.PRG/D4.c:1317)
bool DecodeIllustrationStep(State& state, StaffRollHost& host) {
  DecoderState& decoder = state.decoder;
  if (decoder.phase == DecodePhase::kIdle) return false;
  if (decoder.phase == DecodePhase::kQueued) {
    decoder.phase = DecodePhase::kRunning;
    decoder.input = 0;
    decoder.output = 0;
  }
  ImageDecoder image(host.IllustrationResource(decoder.resource),
                     host.IllustrationWorkspace(), decoder);
  while (host.VerticalBlankCounter() - decoder.budget_start_tick <=
         kDecodeBudgetTicks) {
    switch (image.Step()) {
      case DecodeResult::kContinue:
        break;
      case DecodeResult::kComplete:
        decoder.phase = DecodePhase::kIdle;
        return true;
      case DecodeResult::kRefused:
        decoder.phase = DecodePhase::kIdle;
        return false;
    }
  }
  return false;
}

// decomp: ENDING.PRG func_8006B324 (src/ENDING/ENDING.PRG/D4.c:1390)
void DrawSmallText(ScreenPoint origin, int palette_row_offset,
                   std::span<const std::uint8_t> text, DisplayHost& host) {
  const auto palette = PaletteBits(kSmallTextPaletteX,
                                   palette_row_offset + kSmallTextPaletteRow);
  ScreenPoint position = origin;
  for (const std::uint8_t character : text) {
    if (character == 0) break;
    auto glyph = static_cast<std::uint8_t>(character - kFirstPrintable);
    if (glyph < kSmallTextGlyphs && glyph != 0) {
      if (glyph == kSmallTextAlias) glyph = kSmallTextAliasGlyph;
      if (glyph > kSmallTextLowercase) glyph -= kSmallTextCaseShift;
      host.DrawGlyph(
          {.ordering_slot = kOverlaySlot,
           .position = position,
           .texel = {LowByte((glyph & kGlyphColumnMask) * kSmallGlyphSize),
                     LowByte((glyph & kSmallGlyphRowMask) / 2)},
           .palette = palette});
    }
    position.x = static_cast<std::int16_t>(position.x + kSmallTextAdvance);
  }
  host.ChangeTexturePage({kOverlaySlot, kSmallTextTexturePage});
}

// decomp: ENDING.PRG func_8006B450 (src/ENDING/ENDING.PRG/D4.c:1438)
void InitializeDisplay(const DisplaySettings& settings, DisplayHost& host) {
  host.SetDisplayEnabled(false);
  host.SetGeometryOffset(settings.width / 2, settings.height / 2);
  host.SetProjectionDistance(settings.projection_distance);
  const std::int16_t width = settings.width;
  const auto viewport_height =
      static_cast<std::int16_t>(settings.height - kViewportBorder);
  host.ApplyDisplayLayout({.width = width,
                           .viewport_height = viewport_height,
                           .draw_origins = {{{0, 0}, {width, 0}}},
                           .display_origins = {{{width, 0}, {0, 0}}},
                           .screen = kScreenWindow,
                           .background = settings.background});
  host.ClearVram(kWholeVram, {});
  host.ShowFrameBuffer(host.FrameBuffer());
  host.ResetPresentedFrameCount();
}

// decomp: ENDING.PRG _drawScreen (src/ENDING/ENDING.PRG/D4.c:1480)
int PresentFrame(int ordering_slot, DisplayHost& host) {
  const int buffer = host.FrameBuffer() == 0 ? 1 : 0;
  host.SetFrameBuffer(buffer);
  const int result = host.AdvanceFrameClock();
  host.ShowFrameBuffer(buffer);
  host.DrawOrderingTable(ordering_slot);
  host.CountPresentedFrame();
  return result;
}

// decomp: ENDING.PRG func_8006B884 (src/ENDING/ENDING.PRG/D4.c:1539)
void AllocateRenderBuffers(DisplayHost& host) {
  InitializeDisplay(kStaffRollDisplay, host);
  host.ResetHeap();
  host.AllocateFrameBuffer(FrameBufferKind::kPrimitives, 0,
                           kPrimitiveBufferBytes);
  host.AllocateFrameBuffer(FrameBufferKind::kPrimitives, 1,
                           kPrimitiveBufferBytes);
  host.AllocateFrameBuffer(FrameBufferKind::kOrderingTable, 0,
                           kOrderingTableBytes);
  host.AllocateFrameBuffer(FrameBufferKind::kOrderingTable, 1,
                           kOrderingTableBytes);
}

// decomp: ENDING.PRG func_8006B930 (src/ENDING/ENDING.PRG/D4.c:1551)
void BeginFrame(DisplayHost& host) {
  host.UpdateControllers();
  OrderingTableLayout layout = kOrderingTables;
  layout.buffer = host.FrameBuffer();
  host.BeginOrderingTables(layout);
}

// decomp: ENDING.PRG func_8006B9B4 (src/ENDING/ENDING.PRG/D4.c:1563)
void StartMusic(StaffRollHost& host) {
  host.StartEndingMusic(kEndingMusicByteCount);
}

// decomp: ENDING.PRG _checkStreamXaEnd (src/ENDING/ENDING.PRG/D4.c:1568)
void WaitForMusicEnd(StaffRollHost& host) { host.StopMusic(); }
}  // namespace VS::Ending::Credits
