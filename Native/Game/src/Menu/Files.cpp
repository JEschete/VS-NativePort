// Hand-maintained (tools/port/owned.csv).
#include "Menu/Files.hpp"  // NOLINT(build/include): typed API

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <format>
#include <optional>
#include <span>
#include <string_view>

#include "Core/Math/ScalarMath.hpp"
#include "Render/Sprites.hpp"

namespace VS::Menu::Files {
namespace {
using Framework::kByteShift;
using Framework::kHalfwordMask;
using Framework::kHalfwordShift;
using Framework::PackClut;
using Framework::PackUvClut;
using Framework::PackXy;
using Render::ImmediatePacketTarget;

// libpad's button bits; the first byte holds the face and shoulder buttons.
constexpr std::uint32_t kPadTriangle = 0x10;
constexpr std::uint32_t kPadCircle = 0x20;
constexpr std::uint32_t kPadCross = 0x40;
constexpr std::uint32_t kPadSelect = 0x100;
constexpr std::uint32_t kPadStart = 0x800;
constexpr std::uint32_t kPadUp = 0x1000;
constexpr std::uint32_t kPadDown = 0x4000;
constexpr std::uint32_t kPadFirstByte = 0xFF;
constexpr std::uint32_t kPadUpDown = kPadUp | kPadDown;

// Core_TextCode_*: the codes from the first non-printing one, and the space
// and arrow glyphs.
constexpr std::uint8_t kCodeNonPrinting = 0xE5;
constexpr std::uint8_t kCodeConfirm = 0xE6;
constexpr std::uint8_t kCodeTerminator = 0xE7;
constexpr std::uint8_t kCodeNewline = 0xE8;
constexpr std::uint8_t kCodeControl = 0xEC;
constexpr std::uint8_t kCodeSpacing = 0xFA;
constexpr std::uint32_t kGlyphSpace = 0x8F;
constexpr std::uint32_t kGlyphArrow = 0xBC;
// A spacing operand from here moves left by 256 less it.
constexpr std::uint32_t kNegativeSpacing = 0xF0;
constexpr int kSpacingRange = 256;
// The glyph page whose low byte is a spacing.
constexpr std::uint32_t kSpacingGlyphPage = 14;
// An element's text ends at this byte.
constexpr std::uint8_t kElementTextEnd = 0xFF;

// vs_getTpage draw-mode commands and the primitive codes in a colour word.
constexpr std::uint32_t kDitheredAverage = 0xE1000200;
constexpr std::uint32_t kDitheredAdd = 0xE1000220;
constexpr std::uint32_t kGlyphDrawMode = 0xE100002D;
constexpr std::uint32_t kSubtractDrawMode = 0xE1000140;
constexpr std::uint32_t kGameOverDrawMode = 0xE100001D;
constexpr std::uint32_t kTileCode = 0x60000000;
constexpr std::uint32_t kTileSemiCode = 0x62000000;
constexpr std::uint32_t kPolyG4Code = 0x38000000;
constexpr std::uint32_t kPolyG4SemiCode = 0x3A000000;
// vs_getRGB0(SpriteSemiTrans, 128, 128, 128) and vs_getRGB0(Tile, 0, 40, 64).
constexpr std::uint32_t kGlyphColor = 0x66808080;
constexpr std::uint32_t kHpMpBackColor = 0x60402800;

// getTPage(0, 0, 768, 0), getTPage(1, 0, 768, 256) and getTPage(1, 0, 640,
// 256), and the 8-bit page at y 256 the thumbnails' column joins.
constexpr std::uint32_t kInterfacePage = 0xC;
constexpr std::uint32_t kRightBackgroundPage = 0x9C;
constexpr std::uint32_t kLeftBackgroundPage = 0x9A;
constexpr std::uint32_t kThumbnailPage = 0x90;
// (8 - blend) << 19 dims a sprite by 8 a step.
constexpr int kDimmingShift = 19;
constexpr int kPageColumnShift = 6;
constexpr std::uint32_t kPageColumnMask = 0x3FF;

constexpr int kScreenWidth = 320;
constexpr int kDecimalBase = 10;
// vs_getXY(768, 227) and vs_getWH(256, 1): the slot thumbnails' CLUT row.
constexpr std::int32_t kSlotClutPosition = (227 << 16) | 768;
constexpr std::int32_t kClutRowSize = (1 << 16) | 256;

// _drawSaveInfoUI's sprites.
constexpr std::array<std::uint32_t, kLabelCount> kLabelUvClut{
    PackUvClut(248, 8, 832, 223),   PackUvClut(242, 0, 832, 223),
    PackUvClut(224, 120, 848, 223), PackUvClut(128, 56, 848, 223),
    PackUvClut(224, 112, 848, 223), PackUvClut(104, 56, 848, 223),
    PackUvClut(170, 76, 864, 223),  PackUvClut(144, 0, 864, 223),
    PackUvClut(160, 0, 864, 223),   PackUvClut(32, 9, 832, 223),
};
constexpr std::array<std::uint32_t, kLabelCount> kLabelSize{
    PackXy(3, 8),  PackXy(13, 8),  PackXy(17, 8), PackXy(22, 8), PackXy(26, 8),
    PackXy(19, 8), PackXy(10, 10), PackXy(16, 9), PackXy(18, 9), PackXy(5, 7),
};

// _renderInteger's and _drawHPMP's digits.
constexpr std::uint32_t kDigitClut = PackClut(832, 223) << kHalfwordShift;
constexpr std::uint32_t kDigitWidth = 6;
constexpr std::uint32_t kDigitSize = PackXy(6, 10);
constexpr std::uint32_t kDigitAdvance = 5;
constexpr std::uint32_t kLargeDigitShift = 3;
constexpr std::uint32_t kLargeDigitU = 64;
constexpr std::uint32_t kLargeDigitSize = PackXy(7, 12);
constexpr std::uint32_t kLargeDigitAdvance = 6;
constexpr std::uint32_t kLargeDigitRaise = PackXy(0, 1);
// _drawHPMP's digit divisors by digit count; the original read past the
// table for more than three digits.
constexpr std::size_t kDigitDivisorCount = 4;
constexpr std::array<std::uint8_t, kDigitDivisorCount> kDigitDivisors{
    0,
    1,
    10,
    100,
};

// _drawHPMP's bar: 64 steps, filling from the stat's colour towards
// (255, 240, 158).
constexpr std::uint32_t kBarSteps = 64;
constexpr int kBarShift = 6;
constexpr std::uint32_t kHpColor = 0xCFE02D;
constexpr std::uint32_t kMpColor = 0x9F78DC;
constexpr std::uint32_t kFullRed = 255;
constexpr std::uint32_t kFullGreen = 240;
constexpr std::uint32_t kFullBlue = 2 * 79;
constexpr std::uint32_t kByteMask = 0xFF;
constexpr std::size_t kHpMpPacketWords = 13;
constexpr std::uint32_t kBarBackOffset = PackXy(0, 8);
constexpr std::uint32_t kBarBackSize = PackXy(66, 5);
constexpr std::uint32_t kBarTop = PackXy(0, 9);
constexpr std::uint32_t kBarBottom = PackXy(0, 12);
constexpr int kDigitPitch = 6;
constexpr int kMaxDigitPitch = 5;
constexpr int kValueRightEdge = 55;
constexpr std::uint32_t kDotOffset = 1;
constexpr std::uint32_t kMaxValueOffset = 6;

// _fileProcessingAnim and _fileProcessingCompleteAnim.
constexpr std::size_t kTransferPacketWords = 10;
constexpr std::size_t kCompleteTilePacketWords = 5;
constexpr std::size_t kCompleteGradientPacketWords = 9;
constexpr std::uint32_t kTransferTrail = 48;
constexpr std::uint32_t kTransferLeftLimit = 64;
constexpr std::uint32_t kLoadColor = 0x00FFE0;
constexpr std::uint32_t kSaveColor = 0x7F00FF;
constexpr std::uint32_t kTransferHeight = PackXy(0, 32);
constexpr int kTransferStep = 16;
constexpr int kSaveColorShift = 15;
constexpr int kLoadColorShift = 3;
constexpr std::uint32_t kCompleteLeft = 64;
constexpr std::uint32_t kCompleteRight = 320;
constexpr std::uint32_t kCompleteSize = PackXy(256, 32);
constexpr std::uint32_t kCompleteFadeHeight = PackXy(0, -8);
constexpr std::uint32_t kCompleteBandStep = PackXy(0, 40);
constexpr std::uint16_t kTransferTargetReset = 0x180;

// _printCharacter's sheet: 21 glyphs of 12x12 a row, CLUTs from (896, 222).
constexpr std::uint32_t kGlyphColumns = 21;
constexpr int kGlyphCell = 12;
constexpr int kGlyphClutX = 896;
constexpr int kGlyphClutY = 222;
constexpr int kGlyphClutStride = 16;
constexpr std::size_t kGlyphPacketWords = 6;
constexpr auto kGlyphWidths = std::to_array<std::uint8_t>({
    6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,
    6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,
    6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,
    6,  6,  6,  6,  6,  12, 6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,
    6,  6,  6,  6,  6,  6,  6,  6,  6,  12, 6,  6,  6,  6,  6,  6,  6,  6,  6,
    6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,
    6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,
    6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,
    6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,
    6,  6,  6,  12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 8,
    5,  7,  8,  7,  7,  7,  6,  6,  6,  7,  7,  7,  9,  7,  7,  7,  7,  6,  7,
    7,  6,  8,  7,  8,  8,  9,  8,  8,  4,  7,  5,  7,  7,  6,  8,  7,  7,  7,
    6,  8,  6,  8,  8,  7,  6,  7,  6,  8,  8,  7,  7,  8,  7,  5,  6,  7,  6,
    7,  7,  7,  8,  12, 7,  7,  7,  7,  7,  7,  7,  7,  7,  6,  6,  6,  6,  7,
    7,  7,  7,  7,  7,  7,  7,  7,  12, 7,  7,  7,  7,  7,  8,  8,  8,  8,  7,
    7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  12, 12, 12, 12, 12, 12, 12, 12,
    12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12,
    6,  12, 12, 12, 12, 12, 12, 8,  12, 6,  5,  4,  12, 12, 12, 12, 4,  6,  6,
    12, 12, 6,  6,  3,  3,  3,  4,  6,  12, 6,  6,  6,  12, 5,  6,  12, 12, 12,
    12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12,
});
static_assert(kGlyphWidths.size() == kGlyphCount);

// _printString's arrow: four frames of three ticks.
constexpr int kArrowTicks = 12;
constexpr int kArrowFrameShift = 2;
constexpr int kLineHeight = 13;

// _intepolateMenuItemBgColour: blended by 0..8, then two flashing pairs
// selected by the factor's eighth less two.
constexpr std::uint32_t kBlendSteps = 8;
constexpr std::uint32_t kBlendStepMask = 7;
constexpr std::uint32_t kLightFrom = 0x6B4100;
constexpr std::uint32_t kLightTo = 0x6C8219;
constexpr std::uint32_t kDarkFrom = 0x330500;
constexpr std::uint32_t kDarkTo = 0x262801;
constexpr int kFlashRowShift = 3;
constexpr std::uint32_t kFlashFirstRow = 2;
constexpr std::array<std::uint32_t, 2> kFlashLight{0x663040, 0x203840};
constexpr std::array<std::uint32_t, 2> kFlashDark{0x200808, 0x081010};

// _drawFileMenuElement: the slot list spans y 72..152; the box text starts
// 6 in.
constexpr int kSlotListTop = 72;
constexpr std::uint32_t kSlotListSpan = 0x51;
constexpr int kTextIndent = 6;
constexpr int kBoxShadow = 2;
constexpr std::size_t kBoxPacketWords = 13;
constexpr std::size_t kLocationsPerPalette = 32;
constexpr int kLocationCount = 48;
constexpr int kThumbnailWidth = 64;
constexpr std::uint32_t kThumbnailSize = PackXy(64, 32);
constexpr int kThumbnailRowShift = 13;
constexpr int kThumbnailColumn = 8;
constexpr int kThumbnailColumnBit = 8;
constexpr int kThumbnailRowMask = 7;
constexpr int kThumbnailPageMask = 0x30;
constexpr int kThumbnailPageScale = 4;
constexpr int kThumbnailPageX = 832;
constexpr std::uint32_t kIconUvClut = PackUvClut(64, 0, 768, 227);
constexpr std::uint32_t kThumbnailUvClut = PackUvClut(0, 0, 768, 227);
// The thumbnails standing for a missing, temporary, unreadable, new and
// empty file.
constexpr int kThumbnailInUse = -2;
constexpr int kThumbnailTemporary = -4;
constexpr int kThumbnailUnknown = -3;
constexpr int kThumbnailNew = -1;
constexpr int kThumbnailEmpty = -5;
constexpr int kSlotStatusTextDrop = 10;
constexpr int kInUseClut = 3;
constexpr std::uint32_t kSummaryNameDrop = 4;
constexpr std::uint32_t kSummaryStatsDrop = 13;
constexpr std::uint32_t kPercentRaise = 0x10000;
constexpr std::uint32_t kHundred = 100;
constexpr std::uint32_t kSaveCountDivisor = 1000;
constexpr std::uint32_t kClearStarUvClut = PackUvClut(240, 16, 912, 223);
constexpr std::uint32_t kClearStarSize = PackXy(16, 16);
// The summary's columns.
struct SummaryColumns {
  std::uint32_t number = 0;
  std::uint32_t number_value = 0;
  std::uint32_t map = 0;
  std::uint32_t map_colon = 0;
  std::uint32_t map_value = 0;
  std::uint32_t full_percent = 0;
  std::uint32_t percent = 0;
  std::uint32_t save = 0;
  std::uint32_t save_colon = 0;
  std::uint32_t save_value = 0;
  std::uint32_t clear = 0;
  std::uint32_t clear_colon = 0;
  std::uint32_t clear_value = 0;
  std::uint32_t clear_star = 0;
  std::uint32_t time = 0;
  std::uint32_t full_hours = 0;
  std::uint32_t hours = 0;
  std::uint32_t hours_colon = 0;
  std::uint32_t minutes = 0;
  std::uint32_t minutes_colon = 0;
  std::uint32_t seconds = 0;
  std::uint32_t hp = 0;
  std::uint32_t mp = 0;
};
constexpr SummaryColumns kColumns{
    .number = 22,
    .number_value = 9,
    .map = 172,
    .map_colon = 189,
    .map_value = 192,
    .full_percent = 207,
    .percent = 202,
    .save = 217,
    .save_colon = 239,
    .save_value = 242,
    .clear = 267,
    .clear_colon = 293,
    .clear_value = 296,
    .clear_star = 69,
    .time = 240,
    .full_hours = 263,
    .hours = 268,
    .hours_colon = 279,
    .minutes = 282,
    .minutes_colon = 293,
    .seconds = 296,
    .hp = 88,
    .mp = 158,
};

// _cursorFileOpSaturation: the cursor's 16-tick pulse; the completion
// flash reads entries 1..16, the last being MENU7.PRG's next byte (-8 on
// the disc).
constexpr std::size_t kSaturationSteps = 17;
constexpr std::array<int, kSaturationSteps> kSaturation{
    0,   -56, -112, -104, -96, -88, -80, -72, -64,
    -56, -48, -40,  -32,  -24, -16, -8,  -8,
};
constexpr std::uint8_t kCursorColorMask = 0xF;
constexpr int kCompleteFrames = 16;
constexpr int kTransferErrorScale = 20;

// _menuElementStops: the slide's easing; the search stops at entry 16.
constexpr std::size_t kElementStopCount = 20;
constexpr std::array<std::uint8_t, kElementStopCount> kElementStops{
    0,   1,   2,   4,   8,   16,  32,  56,  80,  104,
    128, 152, 176, 200, 224, 248, 255, 255, 255, 255,
};
constexpr std::size_t kStopSearch = 16;
constexpr int kSlideStep = 32;

// _drawFileMenu's frame.
constexpr std::uint32_t kFrameRightPosition = PackXy(256, 176);
constexpr std::uint32_t kFrameLeftPosition = PackXy(0, 176);
constexpr std::uint32_t kFrameUvClut = PackUvClut(0, 176, 768, 227);
constexpr std::uint32_t kFrameRightSize = PackXy(64, 64);
constexpr std::uint32_t kFrameLeftSize = PackXy(256, 64);
constexpr int kEdgeTop = 187;
constexpr int kEdgeLines = 3;
constexpr std::uint32_t kEdgeLeftWidth = 256;
constexpr std::uint32_t kEdgeRightX = 256;
constexpr int kEdgeDimmingShift = 21;
constexpr std::uint32_t kEdgePage = 256;
constexpr std::uint32_t kCautionPosition = PackXy(10, 181);
constexpr std::uint32_t kCautionUvClut = PackUvClut(64, 145, 848, 223);
constexpr std::uint32_t kCautionSize = PackXy(33, 7);
constexpr std::size_t kTilePacketWords = 4;
constexpr std::size_t kTileSemiPacketWords = 5;
constexpr std::uint32_t kMessageBoxPosition = PackXy(0, 187);
constexpr std::uint32_t kMessageBoxSize = PackXy(320, 52);
constexpr std::uint32_t kCursorUvClut = PackUvClut(32, 48, 896, 223);
constexpr std::uint32_t kCursorSize = PackXy(16, 16);
constexpr int kMessageX = 16;
constexpr int kMessageY = 192;
constexpr std::uint32_t kFadeColorStep = 0x080808;
constexpr std::uint32_t kFullScreen = PackXy(320, 224);
// _drawFileMenuBg.
constexpr std::uint32_t kBackgroundRightPosition = PackXy(256, 0);
constexpr std::uint32_t kBackgroundUvClut = PackUvClut(0, 0, 768, 227);
constexpr std::uint32_t kBackgroundRightSize = PackXy(64, 176);
constexpr std::uint32_t kBackgroundLeftSize = PackXy(256, 176);

// The prompts' elements (3, 4): 126 wide, sliding in from the left.
constexpr int kFirstPromptElement = 3;
constexpr int kPromptWidth = 126;
constexpr int kPromptHeight = 12;
constexpr int kPromptTop = 18;
constexpr int kPromptPitch = 16;
constexpr int kPromptCursorTop = 10;

// libcard's event results (Save_CardReadinessResult_*) and maskedHandler's
// (Save_MaskedHandlerResult_*).
constexpr int kReadinessPending = 0;
constexpr int kReadinessReady = 1;
constexpr int kReadinessTimeout = 2;
constexpr int kReadinessUnformatted = 3;
constexpr int kReadinessMask = 3;
constexpr int kMaskedError = -1;
constexpr int kMaskedPending = 0;
constexpr int kMaskedComplete = 1;
constexpr int kEventMaskShift = 4;
// Save_FindSaveResult_*: the search covers both ports, two steps each.
constexpr int kFindTimeout = -2;
constexpr int kFindNotFound = -1;
constexpr int kFindPending = 0;
constexpr int kSearchSteps = 4;
constexpr int kSlotsPerPort = 16;

[[nodiscard]] constexpr std::uint32_t Word(int value) noexcept {
  return static_cast<std::uint32_t>(value);
}

[[nodiscard]] constexpr std::int32_t Packed(std::uint32_t value) noexcept {
  return static_cast<std::int32_t>(value);
}

// An immediate packet's tag: its length and no link.
template <std::size_t N>
void Submit(ImmediatePacketTarget& target,
            std::array<std::uint32_t, N>& packet) {
  packet[0] = static_cast<std::uint32_t>(N - 1) << Render::kPacketLengthShift;
  target.Submit(packet);
}

// A queued packet's tag: its length and the chain head.
template <std::size_t N>
void Emit(Render::PacketQueue& queue, std::array<std::uint32_t, N>& packet) {
  packet[0] = (queue.ChainLink() & Render::kPacketLinkMask) |
              (static_cast<std::uint32_t>(N - 1) << Render::kPacketLengthShift);
  queue.Emit(packet);
}

struct SpriteWords {
  std::uint32_t position = 0;
  std::uint32_t uv_clut = 0;
  std::uint32_t size = 0;
  std::uint32_t page = 0;
};

// _drawSprt; false when the page's dimming leaves the colour outside a byte.
bool Sprite(ImmediatePacketTarget& target, const SpriteWords& sprite) {
  return Render::DrawImmediateSprite(target,
                                     {.position = Packed(sprite.position),
                                      .uv_clut = Packed(sprite.uv_clut),
                                      .size = Packed(sprite.size),
                                      .texture_page = Packed(sprite.page)});
}

template <typename SpriteHost>
void DimmedSprite(SpriteHost& host, const SpriteWords& sprite) {
  if (!Sprite(host, sprite)) {
    host.Refuse(std::format(
        "file menu sprite at 0x{:08X} refused: page 0x{:08X} dims it outside "
        "a colour byte",
        sprite.position, sprite.page));
  }
}

// The text at halfword `offset` of the table, to its end (which reads as the
// terminator); refused past the end.
[[nodiscard]] std::span<const std::uint8_t> TableText(CardHost& host,
                                                      std::size_t offset) {
  const std::span<const std::uint16_t> table = host.TextTable();
  if (offset >= table.size()) {
    host.Refuse(std::format(
        "file menu text offset {} lies outside the {}-halfword table", offset,
        table.size()));
    return {};
  }
  const std::span<const std::uint16_t> tail = table.subspan(offset);
  return {reinterpret_cast<const std::uint8_t*>(tail.data()),
          tail.size_bytes()};
}

// The table's index entry `index`, or none past its end.
[[nodiscard]] std::optional<std::uint16_t> IndexedOffset(CardHost& host,
                                                         std::size_t index) {
  const std::span<const std::uint16_t> table = host.TextTable();
  if (index >= table.size()) {
    host.Refuse(std::format(
        "file menu text index {} lies outside the {}-halfword table", index,
        table.size()));
    return std::nullopt;
  }
  return table[index];
}

[[nodiscard]] std::span<const std::uint8_t> IndexedText(CardHost& host,
                                                        std::size_t index) {
  const std::optional<std::uint16_t> offset = IndexedOffset(host, index);
  return offset ? TableText(host, *offset) : std::span<const std::uint8_t>{};
}

void Play(Host& host, MenuSound sound) { host.PlaySound(sound); }

[[nodiscard]] Element& ElementAt(State& state, int id) {
  return state.elements[static_cast<std::size_t>(id)];
}

[[nodiscard]] bool ValidElement(int id) {
  return id >= 0 && id < static_cast<int>(kElementCount);
}

// _initFileMenuElement's copy: spacing codes keep their operand, the other
// codes from the first non-printing one are dropped and the terminator ends
// the text. The span's end reads as the terminator; the original could store
// a spacing operand at the 33rd byte, which is dropped.
void CopyElementText(std::array<std::uint8_t, kElementTextBytes>& out,
                     std::span<const std::uint8_t> text) {
  std::size_t next = 0;
  const auto read = [&text, &next] {
    return next < text.size() ? text[next++] : kCodeTerminator;
  };
  std::size_t i = 0;
  while (i < kElementTextBytes) {
    std::uint8_t c = read();
    if (c == kCodeSpacing) {
      out[i++] = c;
      c = read();
      if (i == kElementTextBytes) {
        return;
      }
    } else if (c == kCodeTerminator) {
      out[i] = kElementTextEnd;
      return;
    } else if (c >= kCodeNonPrinting) {
      continue;
    }
    out[i++] = c;
  }
}

[[nodiscard]] std::uint32_t FillChannel(std::uint32_t fill, std::uint32_t full,
                                        std::uint32_t empty) {
  return ((fill * full) + (empty * (kBarSteps - fill))) >> kBarShift;
}

// _drawHPMP's bar: the background tile and the gradient filled `fill` of 64.
void DrawHpMpBar(ImmediatePacketTarget& target, std::uint32_t position,
                 std::uint32_t fill, std::uint32_t empty_color) {
  const std::uint32_t full_color =
      FillChannel(fill, kFullRed, empty_color & kByteMask) |
      (FillChannel(fill, kFullGreen, (empty_color >> kByteShift) & kByteMask)
       << kByteShift) |
      (FillChannel(fill, kFullBlue, empty_color >> kHalfwordShift)
       << kHalfwordShift);
  const std::uint32_t bar = position + 1;
  std::array<std::uint32_t, kHpMpPacketWords> packet{0,
                                                     kDitheredAverage,
                                                     kHpMpBackColor,
                                                     position + kBarBackOffset,
                                                     kBarBackSize,
                                                     kPolyG4Code | full_color,
                                                     bar + kBarTop,
                                                     empty_color,
                                                     bar + fill + kBarTop,
                                                     full_color,
                                                     bar + kBarBottom,
                                                     empty_color,
                                                     bar + fill + kBarBottom};
  Submit(target, packet);
}

// _drawFileMenuElement's box: the shadow tile and the gradient.
void DrawElementBox(ImmediatePacketTarget& target, const Element& element,
                    std::uint32_t light, std::uint32_t dark) {
  const std::uint32_t top = Word(element.y) << kHalfwordShift;
  const std::uint32_t bottom = Word(element.y + element.h) << kHalfwordShift;
  const std::uint32_t left = Word(element.x) & kHalfwordMask;
  const std::uint32_t right = Word(element.x + element.w) & kHalfwordMask;
  std::array<std::uint32_t, kBoxPacketWords> packet{
      0,
      kDitheredAverage,
      kTileCode,
      (Word(element.x + kBoxShadow) & kHalfwordMask) |
          (Word(element.y + kBoxShadow) << kHalfwordShift),
      Word(element.w) | (Word(element.h) << kHalfwordShift),
      kPolyG4Code | light,
      left | top,
      dark,
      right | top,
      light,
      left | bottom,
      dark,
      right | bottom};
  Submit(target, packet);
}

// The box's text; the original read a 33rd byte for a spacing code in the
// last one.
void DrawElementText(ImmediatePacketTarget& target, const Element& element) {
  int x = element.x + kTextIndent;
  std::size_t i = 0;
  while (i < kElementTextBytes) {
    const std::uint8_t c = element.text[i];
    if (c == kElementTextEnd) {
      return;
    }
    if (c == kCodeSpacing) {
      ++i;
      if (i < kElementTextBytes) {
        x += element.text[i];
      }
    } else {
      x = DrawGlyph(target, c, x, element.y, 0);
    }
    ++i;
  }
}

// The slot's thumbnail: the location's picture, or the icon of a missing,
// temporary, unreadable, new or empty file, with the location palettes in
// the slot CLUT row.
void DrawSlotThumbnail(Context& ctx, const Element& element,
                       const SlotSummary& summary) {
  int location = element.save_location - 1;
  if (summary.status < kSlotInUse) {
    if (summary.status == kSlotUnavailable) {
      location = kThumbnailInUse;
    } else if (summary.status == kSlotTemporary) {
      location = kThumbnailTemporary;
    } else {
      location = ctx.state.is_saving ? kThumbnailNew : kThumbnailEmpty;
    }
  } else if (location >= kLocationCount) {
    location = kThumbnailUnknown;
  }
  const std::span<const std::uint16_t> palette = ctx.host.LocationPalette(
      static_cast<std::size_t>(Word(location)) >= kLocationsPerPalette ? 1 : 0);
  if (palette.size() < kClutRowColors) {
    ctx.host.Refuse(
        std::format("file menu location palette holds {} of {} colours",
                    palette.size(), kClutRowColors));
  } else {
    ctx.host.UploadImage(kSlotClutPosition, palette, kClutRowSize);
  }
  const std::uint32_t dimming =
      Word(static_cast<int>(kBlendSteps) - element.outer_blend)
      << kDimmingShift;
  SpriteWords sprite{.position = Word(element.x - kThumbnailWidth) |
                                 (Word(element.y) << kHalfwordShift),
                     .size = kThumbnailSize};
  if (location < 0) {
    sprite.uv_clut = (Word(~location) << kThumbnailRowShift) | kIconUvClut;
    sprite.page = dimming | kRightBackgroundPage;
  } else {
    sprite.uv_clut =
        (Word(location & kThumbnailColumnBit) * kThumbnailColumn) |
        (Word(location & kThumbnailRowMask) << kThumbnailRowShift) |
        kThumbnailUvClut;
    sprite.page =
        ((Word(((location & kThumbnailPageMask) * kThumbnailPageScale) +
               kThumbnailPageX) &
          kPageColumnMask) >>
         kPageColumnShift) |
        dimming | kThumbnailPage;
  }
  DimmedSprite(ctx.host, sprite);
}

// A used slot's summary: the location, map completion, saves, clears, the
// play time and the HP and MP bars.
void DrawSlotSummary(Context& ctx, const Element& element,
                     const SlotSummary& summary, std::uint32_t top) {
  Host& host = ctx.host;
  std::uint32_t y = top + (kSummaryNameDrop << kHalfwordShift);
  DrawText(host, ctx.state.text_arrow,
           IndexedText(host, element.save_location + kIndexSaveLocations),
           element.x + kTextIndent,
           element.y + static_cast<int>(kSummaryNameDrop), 0);
  DrawSummaryLabel(host, Packed(y | kColumns.map), Label::kMap);
  DrawSummaryLabel(host, Packed(y | kColumns.map_colon), Label::kColon);
  if (Word(summary.map_completion) == kHundred) {
    DrawInteger(host, Packed(y | kColumns.map_value), kHundred, kHundred);
    DrawSummaryLabel(host, Packed((y - kPercentRaise) | kColumns.full_percent),
                     Label::kPercent);
  } else {
    DrawInteger(host, Packed(y | kColumns.map_value), summary.map_completion,
                kDecimalBase);
    DrawSummaryLabel(host, Packed((y - kPercentRaise) | kColumns.percent),
                     Label::kPercent);
  }
  DrawSummaryLabel(host, Packed(y | kColumns.save), Label::kSave);
  DrawSummaryLabel(host, Packed(y | kColumns.save_colon), Label::kColon);
  DrawInteger(host, Packed(y | kColumns.save_value), summary.save_count,
              kSaveCountDivisor);
  DrawSummaryLabel(host, Packed(y | kColumns.clear), Label::kClear);
  DrawSummaryLabel(host, Packed(y | kColumns.clear_colon), Label::kColon);
  DrawInteger(host, Packed(y | kColumns.clear_value), summary.clear_count,
              kDecimalBase);
  y += kSummaryStatsDrop << kHalfwordShift;
  if (summary.clear_count != 0) {
    DimmedSprite(host, {.position = y | kColumns.clear_star,
                        .uv_clut = kClearStarUvClut,
                        .size = kClearStarSize,
                        .page = kInterfacePage});
  }
  DrawSummaryLabel(host, Packed(y | kColumns.time), Label::kTime);
  const std::uint32_t hours = Word(summary.hours);
  if (hours == kHundred) {
    DrawInteger(host, Packed(y | kColumns.full_hours), hours, kHundred);
  } else {
    DrawInteger(host, Packed(y | kColumns.hours), hours, kDecimalBase);
  }
  DrawSummaryLabel(host, Packed(y | kColumns.hours_colon), Label::kColon);
  DrawInteger(host, Packed(y | kColumns.minutes), Word(summary.minutes),
              kDecimalBase);
  DrawSummaryLabel(host, Packed(y | kColumns.minutes_colon), Label::kColon);
  DrawInteger(host, Packed(y | kColumns.seconds), Word(summary.seconds),
              kDecimalBase);
  DrawHpMpSummary(host, Packed(y | kColumns.hp), Stat::kHp, summary.current_hp,
                  summary.max_hp);
  DrawHpMpSummary(host, Packed(y | kColumns.mp), Stat::kMp, summary.current_mp,
                  summary.max_mp);
}

// The selected slot's transfer: the sweep while the file moves, then the
// 16-frame flash once it has.
void DrawSlotProgress(Context& ctx, const Element& element, std::uint32_t top) {
  const std::int32_t counter = ctx.host.ProgressCounter();
  if (element.selected == 0 || counter == 0) {
    return;
  }
  if (counter < 0) {
    if (counter < -kCompleteFrames) {
      ctx.host.Refuse(std::format(
          "file transfer flash frame {} lies before the {}-frame flash",
          counter, kCompleteFrames));
      return;
    }
    ctx.host.SetProgressCounter(counter + 1);
    const int flash_step = counter + kCompleteFrames + 1;
    DrawTransferComplete(
        ctx, -kSaturation[static_cast<std::size_t>(flash_step)], Packed(top));
    return;
  }
  const TransferProgress transfer = ctx.host.Transfer();
  if (transfer.target == 0) {
    ctx.host.Refuse("file transfer progress has no target");
    return;
  }
  const int position = transfer.position;
  DrawTransferProgress(
      ctx,
      position + (((counter - transfer.previous) *
                   ((transfer.error_offset * kTransferErrorScale) -
                    (position - kScreenWidth))) /
                  transfer.target),
      Packed(top));
}

// A slot box's contents.
void DrawSlot(Context& ctx, const Element& element) {
  Host& host = ctx.host;
  const int slot = element.slot_id;
  std::array<std::uint16_t, kClutRowColors> saved{};
  host.ReadImage(kSlotClutPosition, saved, kClutRowSize);
  const SlotSummary summary = host.Summary(slot);
  DrawSlotThumbnail(ctx, element, summary);
  host.UploadImage(kSlotClutPosition, saved, kClutRowSize);
  const std::uint32_t top = Word(element.y) << kHalfwordShift;
  DrawSummaryLabel(
      host, Packed(Word(element.x - static_cast<int>(kColumns.number)) | top),
      Label::kNumber);
  DrawInteger(
      host,
      Packed(Word(element.x - static_cast<int>(kColumns.number_value)) | top),
      Word(slot + 1), kDecimalBase);
  const int text_x = element.x + kTextIndent;
  const int text_y = element.y + kSlotStatusTextDrop;
  if (summary.status == kSlotUnavailable) {
    DrawText(host, ctx.state.text_arrow, TableText(host, kTextInUse), text_x,
             text_y, kInUseClut);
  } else if (summary.status == kSlotAvailable) {
    if (ctx.state.is_saving) {
      DrawText(host, ctx.state.text_arrow, TableText(host, kTextNew), text_x,
               text_y, 0);
    } else {
      DrawText(host, ctx.state.text_arrow, TableText(host, kTextEmpty), text_x,
               text_y, kInUseClut);
    }
  } else {
    DrawSlotSummary(ctx, element, summary, top);
  }
  DrawSlotProgress(ctx, element, top);
}

// The first stop from the second whose offset from `target` reaches
// `position`, less one; the search ends at the 16th.
[[nodiscard]] std::size_t StopBefore(int target, int position) {
  std::size_t stop = 1;
  while (stop < kStopSearch && target + kElementStops[stop] < position) {
    ++stop;
  }
  return stop - 1;
}

void SetStatic(Element& element) {
  element.state = static_cast<std::uint8_t>(ElementState::kStatic);
}

void SlideX(Element& element) {
  if (element.x < element.target) {
    element.x = static_cast<std::int16_t>(element.x + kSlideStep);
    if (element.x >= element.target) {
      element.x = element.target;
      SetStatic(element);
    }
    return;
  }
  element.x = static_cast<std::int16_t>(
      element.target + kElementStops[StopBefore(element.target, element.x)]);
  if (element.w < kScreenWidth - element.x) {
    element.w = static_cast<std::int16_t>(kScreenWidth - element.x);
  }
  if (element.x == element.target) {
    SetStatic(element);
  }
}

void SlideY(Element& element) {
  if (element.y > element.target) {
    element.y = static_cast<std::int16_t>(
        element.target + kElementStops[StopBefore(element.target, element.y)]);
  }
  if (element.y == element.target) {
    SetStatic(element);
  }
}

void SlideOutLeft(Element& element) {
  if (element.x < element.target) {
    element.x =
        static_cast<std::int16_t>(-kElementStops[StopBefore(0, -element.x)]);
    if (element.x == 0) {
      SetStatic(element);
    }
    return;
  }
  element.x = static_cast<std::int16_t>(element.x - kSlideStep);
  if (element.target >= element.x) {
    element.x = element.target;
    SetStatic(element);
  }
}

void AnimateElement(Element& element, ElementState state) {
  switch (state) {
    case ElementState::kAnimateX:
      SlideX(element);
      break;
    case ElementState::kAnimateY:
      SlideY(element);
      break;
    case ElementState::kAnimateNegX:
      SlideOutLeft(element);
      break;
    default:
      break;
  }
}

// The frame's top edge: three lines copied from the drawn buffer, each
// dimmer.
void DrawFrameEdges(Host& host, std::uint32_t buffer_x) {
  for (int line = 1; line <= kEdgeLines; ++line) {
    const int y = kEdgeTop - line;
    const std::uint32_t dimming =
        (Word(kEdgeLines + 1 - line) << kEdgeDimmingShift) | kEdgePage;
    DimmedSprite(host, {.position = PackXy(0, y),
                        .uv_clut = Word(y) << kByteShift,
                        .size = PackXy(static_cast<int>(kEdgeLeftWidth), 1),
                        .page = (buffer_x >> kPageColumnShift) | dimming});
    DimmedSprite(
        host, {.position = PackXy(static_cast<int>(kEdgeRightX), y),
               .uv_clut = Word(y) << kByteShift,
               .size = PackXy(kScreenWidth - static_cast<int>(kEdgeRightX), 1),
               .page = Word(static_cast<int>(buffer_x + kEdgeRightX) >>
                            kPageColumnShift) |
                       dimming});
  }
}

// The frame's footer: "Caution", the message box, the cursor, the message,
// the file overlay and the screen fade.
void DrawFrameFooter(Context& ctx) {
  Host& host = ctx.host;
  State& state = ctx.state;
  DimmedSprite(host, {.position = kCautionPosition,
                      .uv_clut = kCautionUvClut,
                      .size = kCautionSize,
                      .page = kInterfacePage});
  host.DrawSync();
  std::array<std::uint32_t, kTilePacketWords> box{
      0, kTileCode, kMessageBoxPosition, kMessageBoxSize};
  Submit(host, box);
  const std::int32_t cursor = host.CursorPosition();
  if (cursor != 0) {
    const int saturation = kSaturation[static_cast<std::size_t>(
        state.cursor_color & kCursorColorMask)];
    DimmedSprite(
        host, {.position = Word(cursor),
               .uv_clut = kCursorUvClut,
               .size = kCursorSize,
               .page = (Word(saturation) << kHalfwordShift) | kInterfacePage});
    state.cursor_color =
        static_cast<std::uint8_t>((state.cursor_color + 1) & kCursorColorMask);
    if ((host.Input().pressed & kPadTriangle) != 0) {
      Play(host, MenuSound::kInvalid);
    }
  } else {
    state.cursor_color = 0;
  }
  const std::span<const std::uint8_t> message = host.CardMessage();
  if (!message.empty()) {
    DrawText(host, state.text_arrow, message, kMessageX, kMessageY, 0);
  }
  host.DrawFileOverlay();
  if (state.screen_fade != 0) {
    std::array<std::uint32_t, kTileSemiPacketWords> fade{
        0, kSubtractDrawMode,
        kTileSemiCode | (state.screen_fade * kFadeColorStep), 0, kFullScreen};
    Submit(host, fade);
  }
}

// _promptConfirm's choice.
int ConfirmationInput(Context& ctx) {
  State& state = ctx.state;
  Host& host = ctx.host;
  const int cancelled = state.confirmation_cancelled & 1;
  ElementAt(state, kFirstPromptElement + cancelled).selected = 1;
  ElementAt(state, kFirstPromptElement + 1 - cancelled).selected = 0;
  const Framework::Buttons buttons = host.Input();
  if ((buttons.pressed & (kPadCross | kPadCircle)) != 0) {
    host.SetCursorPosition(0);
    for (int id = kFirstPromptElement; id <= kFirstPromptElement + 1; ++id) {
      Element& element = ElementAt(state, id);
      element.state = static_cast<std::uint8_t>(ElementState::kAnimateNegX);
      element.target = -kPromptWidth;
    }
    if ((buttons.pressed & kPadCircle) != 0 && cancelled == 0) {
      Play(host, MenuSound::kSelect);
      return 1;
    }
    Play(host, MenuSound::kLeave);
    return -1;
  }
  if ((buttons.repeat & kPadUpDown) != 0) {
    Play(host, MenuSound::kChange);
    state.confirmation_cancelled = static_cast<std::uint8_t>(1 - cancelled);
  }
  host.SetCursorPosition(
      (state.confirmation_cancelled * kPromptPitch + kPromptCursorTop)
      << kHalfwordShift);
  return 0;
}

enum ConfirmationStep : std::uint8_t {
  kConfirmationYes,
  kConfirmationNo,
  kConfirmationWait,
  kConfirmationInput,
};

enum FormatStep : std::uint8_t {
  kFormatConfirm,
  kFormatStartEvents,
  kFormatEvents,
  kFormatCard,
};

// _findCurrentSave on both overlays: each port is announced, then checked
// and searched. `menu` also shows the port in the information box
// (MENU7.PRG).
// One readiness event of the port being checked: the search's result once
// it has one (the matching slot or the timeout), nothing to move on.
std::optional<int> CheckSearchedPort(CardHost& host, SaveSearch& search,
                                     int event, int real_port) {
  if (event == kReadinessReady) {
    if (host.ScanSaveDirectory(real_port + 1) == 0) {
      const int found = host.FindMatchingSaveSlot();
      if (found != 0) {
        return found + (real_port * kSlotsPerPort);
      }
    }
  } else if (event == kReadinessTimeout) {
    if (search.step != 1) {
      if (search.timeout_step != 0) {
        return kFindTimeout;
      }
    } else {
      search.timeout_step = search.step;
    }
  }
  return std::nullopt;
}

int StepSaveSearch(CardHost& host, SaveSearch& search, Host* menu) {
  const int real_port = search.step >> 1;
  if ((search.step & 1) == 0) {
    const std::optional<std::uint16_t> offset = IndexedOffset(
        host, kIndexAccessing0 + static_cast<std::size_t>(real_port));
    if (menu != nullptr && offset) {
      menu->SetInformationMessage(*offset);
    }
    host.SetCardMessage(offset);
    host.AdvanceCardReadiness(real_port + 1);
    ++search.step;
  } else {
    const int event = host.AdvanceCardReadiness(0) & kReadinessMask;
    if (event != kReadinessPending) {
      const std::optional<int> result =
          CheckSearchedPort(host, search, event, real_port);
      if (result) {
        return *result;
      }
      ++search.step;
    }
  }
  return search.step != kSearchSteps ? kFindPending : kFindNotFound;
}

// The steps of _showSaveFilesMenu and _showLoadFilesMenu.
enum SaveFilesStep : std::uint8_t {
  kSaveInit,
  kSaveInput,
  kSaveSlotChosen,
  kSaveOverwritePrompt,
  kSaveOverwriteCheck,
  kSaveWrite,
  kSaveValidate,
  kSaveLeaveWithTimer,
  kSaveLeave,
  kSaveFormat,
  kSaveLeaveError,
};

enum LoadFilesStep : std::uint8_t {
  kLoadInit,
  kLoadInput,
  kLoadStart,
  kLoadApply,
  kLoadLoaded,
  kLoadExiting,
};

// The steps both card pickers share; the save picker starts the readiness
// check where the load picker starts maskedHandler.
enum CardStep : std::uint8_t {
  kCardSlideOut,
  kCardSlideUp,
  kCardStartEvents,
  kCardPollEvents,
  kCardLeave,
  kCardReturn,
  kCardScan,
  kCardList,
};

enum GameOverStep : std::uint8_t {
  kGameOverInit,
  kGameOverLoadAssets,
  kGameOverChoices,
  kGameOverOpenFiles,
  kGameOverFilesWait,
  kGameOverSelectFile,
  kGameOverRestartIntro,
  kGameOverToTitle,
};

enum GameOverAssetStep : std::uint8_t {
  kAssetsLoading,
  kAssetsLoaded,
};

// vs_menu7_dataMenu's steps: 0 is closed and the menu opens at 3.
enum DataMenuStep : std::uint8_t {
  kDataClosed = 0,
  kDataOpen = 3,
  kDataLoadWait,
  kDataRows,
  kDataSelect,
  kDataSaveOpen,
  kDataSave,
  kDataLoadOpen,
  kDataLoad,
  kDataClose,
  kDataCloseToMenu,
  kDataFade,
  kDataFadeToMenu,
};

enum ContainerStep : std::uint8_t {
  kContainerNo,
  kContainerWait,
  kContainerInput,
  kContainerReturn,
};

enum ReminderStep : std::uint8_t {
  kReminderYes,
  kReminderNo,
  kReminderWait,
  kReminderInput,
  kReminderReturn,
};

// The slot list: five 256x32 boxes 40 apart from (64, 72), three shown a
// page; the cursor sits 24 in, 10 above its box.
constexpr int kSlotListX = 64;
constexpr int kSlotPitch = 40;
constexpr std::uint32_t kSlotBoxSize = PackXy(256, 32);
constexpr int kSlotCursorX = 24;
constexpr int kSlotCursorTop = 62;
constexpr std::uint8_t kLastVisibleSlot = 2;
constexpr std::uint8_t kLastPage = 2;
constexpr std::uint8_t kLastSlot = 4;
// A finished transfer leaves by itself after 150 ticks; a load settles 16.
constexpr std::uint8_t kLeaveTicks = 150;
constexpr std::uint8_t kLoadSettleTicks = 16;
constexpr std::int32_t kCompleteCounter = -kCompleteFrames;
// VS_SFX_FILEOPCOMPLETE.
constexpr int kFileOperationComplete = 8;
// Save_EventMask_All and Save_EventMask_IgnoreNoEvent.
constexpr int kEventMaskAll = 0x70;
constexpr int kEventMaskIgnoreNoEvent = 0x30;
// The write and read requests: the file, the port above it and the read's
// flag.
constexpr int kPortShift = 16;
constexpr int kReadRequestFlag = 256;
// Input_settings' first word: the bit _containerDataEmpty forces.
constexpr std::uint8_t kMainSettingBit = 0x10;
constexpr int kMainSettingShift = 4;
// ValidateAndApplyLiveLoadedSave's results.
constexpr int kLoadValid = 0;
constexpr int kLoadCorrupt = 1;

// The card pickers: the chosen card slides to x 180 and the other off the
// screen; the lower card (2) rises to y 50. The cards' ids (1, 2), like the
// reminder's options', are 3 less each other.
constexpr std::int16_t kChosenCardX = 180;
constexpr std::int16_t kOffScreenX = 320;
constexpr std::int16_t kRaisedCardY = 50;
constexpr std::uint8_t kLowerCard = 2;
constexpr int kElementPairSum = 3;

// _initGameOver: GAMEOVER.BIN at (832, 256) and the text CLUTs at (832, 384).
constexpr std::int32_t kGameOverImagePosition = Packed(PackXy(832, 256));
constexpr std::int32_t kGameOverImageSize = Packed(PackXy(24, 128));
constexpr std::int32_t kChoiceClutPosition = Packed(PackXy(832, 384));
constexpr std::int32_t kTextClutSize = Packed(PackXy(48, 1));
constexpr std::size_t kTextClutUploadColors = 48;
constexpr std::int32_t kChoiceClutSize = Packed(PackXy(32, 1));
// vs_menu7_gameOver keeps the CLUT row at (640, 511) and loads it as row 3.
constexpr std::int32_t kGameOverClutPosition = Packed(PackXy(640, 511));
constexpr std::int32_t kGameOverClutSize = Packed(PackXy(256, 1));
constexpr int kGameOverClutRow = 3;
constexpr std::uint32_t kRetiredPageFirst = 0x1F;
constexpr int kRetiredPageOwner = 6;
constexpr int kGameOverMusicSlot = 1;
constexpr int kMusicFadeFrames = 60;
constexpr int kGameOverResetDelay = 16;
constexpr int kTitleOption = 2;

// _displayGameOverScreen: the text fades in by 8 to 128 and out over the
// subtracted tile; the chosen option glows over four steps, then pulses
// down to 128 while the other fades.
constexpr int kChoiceFadeStep = 8;
constexpr int kChoiceBright = 128;
constexpr std::uint32_t kFadeTileColorStep = 0x020202;
constexpr std::uint32_t kFadeTileSize = PackXy(320, 240);
constexpr int kGlowSteps = 16;
constexpr int kGlowColorStep = 16;
constexpr int kGlowBlendStep = 4;
constexpr int kGlowPulseFloor = 144;
constexpr int kGlowRest = 128;
constexpr int kSemiTransparentBrightness = 256;
constexpr std::uint32_t kFirstChoicePosition = PackXy(112, 128);
constexpr std::uint32_t kSecondChoicePosition = PackXy(112, 160);
constexpr std::uint32_t kChoiceSize = PackXy(96, 32);
constexpr std::uint32_t kRetryTexture = PackUvClut(0, 0, 832, 384);
constexpr std::uint32_t kTitleTexture = PackUvClut(0, 32, 848, 384);
constexpr std::uint32_t kRetryGlowTexture = PackUvClut(0, 64, 864, 384);
constexpr std::uint32_t kTitleGlowTexture = PackUvClut(0, 96, 864, 384);

// _menuItemTextClut: four 16-colour rows, the plain text first and the
// glow last.
constexpr int kGreenShift = 5;
constexpr int kBlueShift = 10;
constexpr int kAlphaShift = 15;
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
[[nodiscard]] constexpr std::uint16_t Rgb5551(int red, int green,
                                              int blue) noexcept {
  return static_cast<std::uint16_t>(red | (green << kGreenShift) |
                                    (blue << kBlueShift) | (1 << kAlphaShift));
}
constexpr std::size_t kTextClutRows = 4;
constexpr std::size_t kGlowTextRow = 3;
using TextClut = std::array<std::uint16_t, kTextClutRows * kClutColors>;
constexpr TextClut kMenuItemTextClut{
    0,
    Rgb5551(1, 1, 1),
    Rgb5551(1, 1, 3),
    Rgb5551(3, 1, 0),
    Rgb5551(1, 0, 6),
    Rgb5551(6, 1, 1),
    Rgb5551(11, 2, 0),
    Rgb5551(16, 5, 0),
    Rgb5551(19, 8, 0),
    Rgb5551(23, 10, 0),
    Rgb5551(24, 10, 0),
    Rgb5551(24, 11, 0),
    Rgb5551(26, 11, 0),
    Rgb5551(25, 11, 3),
    Rgb5551(27, 14, 0),
    Rgb5551(27, 13, 2),
    0,
    Rgb5551(1, 1, 1),
    Rgb5551(1, 1, 3),
    Rgb5551(3, 1, 0),
    Rgb5551(1, 0, 6),
    Rgb5551(6, 1, 1),
    Rgb5551(11, 2, 0),
    Rgb5551(16, 5, 0),
    Rgb5551(19, 8, 0),
    Rgb5551(23, 10, 0),
    Rgb5551(24, 10, 0),
    Rgb5551(24, 11, 0),
    Rgb5551(26, 11, 0),
    Rgb5551(25, 11, 3),
    Rgb5551(27, 14, 0),
    Rgb5551(27, 13, 2),
    0,
    Rgb5551(0, 0, 2),
    Rgb5551(0, 0, 4),
    Rgb5551(0, 0, 6),
    Rgb5551(0, 0, 8),
    Rgb5551(0, 0, 10),
    Rgb5551(0, 0, 12),
    Rgb5551(0, 0, 14),
    Rgb5551(0, 0, 15),
    Rgb5551(0, 0, 17),
    Rgb5551(0, 0, 20),
    Rgb5551(0, 0, 22),
    Rgb5551(0, 0, 24),
    Rgb5551(0, 0, 23),
    Rgb5551(0, 0, 29),
    Rgb5551(0, 0, 31),
    0,
    Rgb5551(1, 1, 3),
    Rgb5551(2, 2, 7),
    Rgb5551(4, 3, 2),
    Rgb5551(5, 4, 7),
    Rgb5551(8, 6, 6),
    Rgb5551(13, 10, 9),
    Rgb5551(19, 14, 10),
    Rgb5551(23, 18, 12),
    Rgb5551(27, 21, 14),
    Rgb5551(29, 22, 15),
    Rgb5551(31, 23, 13),
    Rgb5551(31, 24, 16),
    Rgb5551(29, 25, 21),
    Rgb5551(31, 26, 18),
    Rgb5551(31, 27, 22),
};
constexpr std::span<const std::uint16_t, kClutColors> kPlainText =
    std::span(kMenuItemTextClut).subspan<0, kClutColors>();
constexpr std::span<const std::uint16_t, kClutColors> kGlowText =
    std::span(kMenuItemTextClut)
        .subspan<kGlowTextRow * kClutColors, kClutColors>();

// _setMenuItemClut's channels, blended in 1/16 steps.
constexpr int kRedMask = 0x1F;
constexpr int kGreenMask = 0x3E0;
constexpr int kBlueMask = 0x7C00;
constexpr int kBlendShift = 4;
constexpr int kOpaqueBit = 0x8000;

// _promptYesNo's rows (30, 31) and the reminder's options: 126 wide at
// y 146 and 162, sliding in from x 320 to 194.
constexpr int kContainerYesRow = 0x1E;
constexpr int kContainerNoRow = 0x1F;
constexpr int kChoiceRowTop = 146;
constexpr std::int16_t kChoiceRowTarget = 194;
constexpr int kContainerCursorBase = 8;
constexpr std::uint32_t kConfirmButtons = kPadTriangle | kPadCircle | kPadCross;
constexpr std::uint32_t kCancelButtons = kPadTriangle | kPadCross;

// _containerItemCapacities in vs_menu_containerIndices' order; the eighth
// entry is MENU7.PRG's next halfword.
constexpr std::array<std::size_t, kContainerSections + 1> kContainerCapacities{
    kContainerWeapons, kContainerBlades, kContainerGrips, kContainerShields,
    kContainerArmor,   kContainerGems,   kContainerMisc,  2,
};
constexpr std::size_t kBladeIndexBase = kContainerWeapons;
constexpr std::size_t kGripIndexBase = kBladeIndexBase + kContainerBlades;
constexpr std::size_t kShieldIndexBase = kGripIndexBase + kContainerGrips;
constexpr std::size_t kArmorIndexBase = kShieldIndexBase + kContainerShields;
constexpr std::size_t kGemIndexBase = kArmorIndexBase + kContainerArmor;
constexpr std::size_t kMiscIndexBase = kGemIndexBase + kContainerGems;
constexpr std::size_t kContainerIndexHalfwords =
    kMiscIndexBase + kContainerMisc;
static_assert(sizeof(ContainerIndices) ==
              kContainerIndexHalfwords * sizeof(std::uint16_t));
// A gem set in a shield links to it with bit 7 set.
constexpr std::uint16_t kShieldLinkBit = 0x80;
using FlatIndices = std::array<std::uint16_t, kContainerIndexHalfwords>;

// _drawPlayTime: the fields' tens 3 apart from column 9, the frames scaled
// to hundredths, and "1" drawn before 100 hours.
constexpr std::size_t kPlayTimeFields = 4;
constexpr std::size_t kHoursField = 3;
constexpr std::size_t kLastTensColumn = 9;
constexpr std::size_t kPlayTimeFieldPitch = 3;
constexpr std::uint32_t kHoursCap = 100;
constexpr int kFrameScaleShift = 14;
constexpr std::uint32_t kFrameScale = 5;
constexpr std::uint32_t kFrameDivisor = 3;
constexpr int kNibbleShift = 4;
constexpr std::int32_t kNibbleMask = 0xF;
constexpr std::uint32_t kPlayTimeHundredPosition = 0xC800D8;
constexpr std::uint32_t kPlayTimePosition = 0xC800E0;
constexpr std::uint32_t kPlayTimeLabelPosition = 0xBC00E0;
constexpr std::array<char, kPlayTimeBytes> kInitialPlayTime{
    '0', '0', ':', '0', '0', ':', '0', '0', ':', '0', '0', '\0',
};

// vs_menu7_dataMenu: two rows (save, load), the menu's row info, the row
// kept while a file screen opens, the cancel that returns to the main menu
// and the information box's caution state.
constexpr std::size_t kDataRowCount = 2;
constexpr int kDataRowInfo = 0x143;
constexpr int kKeepRow = 6;
constexpr int kCancelToMenuRow = -2;
constexpr int kInformationBoxShown = 11;
constexpr int kDataMenuReturnMenu = 7;

// The title screen's display, reminder cursor and exit fade.
constexpr int kReminderCursorX = 180;
constexpr int kReminderCursorRow = 7;
constexpr std::uint32_t kExitPageBase = 0x140;
constexpr int kExitRightX = 256;
constexpr std::uint32_t kExitRightSize = PackXy(64, 224);
constexpr std::uint32_t kExitLeftSize = PackXy(256, 224);
// The list's page and cursor row for `slot`: the last slot is the third row
// of the last page and the others from the second are the second row of the
// page before them. A slot past the list is refused and the cursor starts at
// the top.
void PlaceListCursor(CardHost& host, FileList& list, std::uint8_t slot) {
  list.page = 0;
  list.slot = slot;
  if (slot == kLastSlot) {
    list.page = kLastPage;
    list.slot = kLastVisibleSlot;
  } else if (slot != 0) {
    list.page = static_cast<std::uint8_t>(slot - 1);
    list.slot = 1;
  }
  if (list.slot + list.page > kLastSlot) {
    host.Refuse(std::format("file list slot {} lies outside the {} slots", slot,
                            kSaveSlots));
    list.page = 0;
    list.slot = 0;
  }
}

[[nodiscard]] int CurrentSlot(const FileList& list) {
  return list.slot + list.page;
}

// The five slot boxes; a slot whose status is below `required` is shown
// unavailable.
void InitializeSlotList(Context& ctx, std::uint32_t required) {
  for (int i = 0; i < static_cast<int>(kSaveSlots); ++i) {
    Element* element = InitializeElement(
        ctx, kFirstSlotElement + i,
        PackXy(kSlotListX, (i * kSlotPitch) + kSlotListTop), kSlotBoxSize, {});
    if (element == nullptr) {
      continue;
    }
    const SlotSummary summary = ctx.host.Summary(i);
    element->slot_id = static_cast<std::uint8_t>(i);
    element->slot_unavailable =
        static_cast<std::uint8_t>(summary.status < required);
    element->save_location = summary.save_location;
  }
}

void MoveListCursor(FileList& list, std::uint32_t repeat) {
  if ((repeat & kPadUp) != 0) {
    if (list.slot != 0) {
      --list.slot;
    } else if (list.page != 0) {
      --list.page;
    }
  }
  if ((repeat & kPadDown) != 0) {
    if (list.slot != kLastVisibleSlot) {
      ++list.slot;
    } else if (list.page < kLastPage) {
      ++list.page;
    }
  }
}

// The list's input once Cross is handled: the boxes scroll to the page and
// Circle on a slot whose status reaches `required` picks it (true; the card
// check starts); otherwise the pad moves the cursor.
bool StepListInput(Context& ctx, FileList& list, std::uint32_t required,
                   const Framework::Buttons& buttons) {
  State& state = ctx.state;
  Host& host = ctx.host;
  for (int i = 0; i < static_cast<int>(kSaveSlots); ++i) {
    Element& element = ElementAt(state, kFirstSlotElement + i);
    element.y = static_cast<std::int16_t>(((i - list.page) * kSlotPitch) +
                                          kSlotListTop);
    element.selected = 0;
  }
  const int current = CurrentSlot(list);
  if ((buttons.pressed & kPadCircle) != 0) {
    if (host.Summary(current).status >= required) {
      Play(host, MenuSound::kSelect);
      ElementAt(state, current + kFirstSlotElement).selected = 1;
      host.SetCursorPosition(0);
      CheckCardStep(ctx, list.port + kEventMaskAll);
      return true;
    }
    Play(host, MenuSound::kInvalid);
  }
  MoveListCursor(list, buttons.repeat);
  if (current != CurrentSlot(list)) {
    Play(host, MenuSound::kChange);
  }
  host.SetCursorPosition(
      Packed(PackXy(kSlotCursorX, (list.slot * kSlotPitch) + kSlotCursorTop)));
  ElementAt(state, current + kFirstSlotElement).selected = 1;
  return false;
}

void ClearSlotElements(Context& ctx) {
  for (int id = kFirstSlotElement; id < static_cast<int>(kElementCount); ++id) {
    ClearElement(ctx, id);
  }
}

[[nodiscard]] bool LeavePressed(Host& host) {
  return (host.Input().pressed & kPadFirstByte) != 0;
}

// Input_settings copied out and back; refused unless it is the 32 bytes the
// backups hold.
[[nodiscard]] std::span<std::uint8_t> WholeSettings(Host& host) {
  const std::span<std::uint8_t> settings = host.Settings();
  if (settings.size() != kSettingsBytes) {
    host.Refuse(std::format("Input_settings holds {} bytes, not {}",
                            settings.size(), kSettingsBytes));
    return {};
  }
  return settings;
}

void BackUpSettings(Host& host,
                    std::array<std::uint8_t, kSettingsBytes>& backup) {
  const std::span<std::uint8_t> settings = WholeSettings(host);
  if (!settings.empty()) {
    std::ranges::copy(settings, backup.begin());
  }
}

void RestoreSettings(Host& host,
                     const std::array<std::uint8_t, kSettingsBytes>& backup) {
  const std::span<std::uint8_t> settings = WholeSettings(host);
  if (!settings.empty()) {
    std::ranges::copy(backup, settings.begin());
  }
}

// _showSaveFilesMenu's list: Cross leaves, Circle picks a slot.
void SaveListInput(Context& ctx) {
  FileList& list = ctx.state.save_files;
  Host& host = ctx.host;
  host.SetCardMessage(kTextSelectFile);
  const Framework::Buttons buttons = host.Input();
  if ((buttons.pressed & kPadCross) != 0) {
    Play(host, MenuSound::kLeave);
    host.SetCursorPosition(0);
    list.step = kSaveLeave;
  } else if (StepListInput(ctx, list, kSlotAvailable, buttons)) {
    list.step = kSaveSlotChosen;
  }
}

// A new file for slot `file` (from 1): written next, or the failure shown.
void CreateFileForSave(Context& ctx, int file) {
  FileList& list = ctx.state.save_files;
  if (ctx.host.CreateSaveFile(list.port, file) != 0) {
    list.step = kSaveLeaveWithTimer;
    ctx.host.SetCardMessage(kTextSaveFailed);
  } else {
    list.step = kSaveWrite;
  }
}

// The card check after a slot is picked: an unformatted card asks to
// format it, an available slot gets a new file and a used one asks to
// overwrite it.
void SaveSlotChosen(Context& ctx) {
  FileList& list = ctx.state.save_files;
  Host& host = ctx.host;
  const int checked = CheckCardStep(ctx, 0);
  if (checked == kMaskedPending) {
    return;
  }
  if (host.CardMessageIs(kTextNoData)) {
    UpdateFormatPrompt(ctx, list.port);
    list.step = kSaveFormat;
  } else if (checked > kMaskedError) {
    const int slot = CurrentSlot(list);
    if (host.Summary(slot).status == kSlotAvailable) {
      CreateFileForSave(ctx, slot + 1);
    } else {
      UpdateOverwritePrompt(ctx, true);
      list.step = kSaveOverwritePrompt;
    }
  } else {
    list.step = kSaveLeaveWithTimer;
  }
}

// The card check after an overwrite is confirmed; overwriting the current
// game's own file marks the data unsaved until the write lands.
void CheckOverwrite(Context& ctx) {
  FileList& list = ctx.state.save_files;
  Host& host = ctx.host;
  const int checked = CheckCardStep(ctx, 0);
  if (checked == kMaskedPending) {
    return;
  }
  if (checked > kMaskedError) {
    const bool own_file = host.FindMatchingSaveSlot() == CurrentSlot(list) + 1;
    host.SetDataNotSaved(
        static_cast<std::uint8_t>(host.DataNotSaved() | (own_file ? 1 : 0)));
    list.step = kSaveWrite;
  } else {
    list.step = kSaveLeaveWithTimer;
  }
}

void BeginSaveWrite(Context& ctx) {
  State& state = ctx.state;
  Host& host = ctx.host;
  FileList& list = state.save_files;
  const int slot = CurrentSlot(list);
  host.SetCardMessage(kTextSaving);
  BackUpSettings(host, state.save_settings_backup);
  host.StashSaveImage();
  if (state.container_data_empty) {
    const std::span<std::uint8_t> settings = WholeSettings(host);
    if (!settings.empty()) {
      state.backup_main_setting =
          static_cast<std::uint8_t>((settings[0] >> kMainSettingShift) & 1);
      settings[0] = static_cast<std::uint8_t>(settings[0] | kMainSettingBit);
    }
  }
  host.PackageGameSave(slot);
  host.WriteSaveStep((slot + 1) | ((list.port - 1) << kPortShift));
  list.step = kSaveValidate;
}

// A failed write puts back the settings, the outgoing image and the slot
// (now temporary); a finished one reads the written summary back.
void FinishSaveWrite(Context& ctx, int written) {
  State& state = ctx.state;
  Host& host = ctx.host;
  FileList& list = state.save_files;
  host.ClearDoorEntered();
  const int slot = CurrentSlot(list);
  Element& element = ElementAt(state, slot + kFirstSlotElement);
  if (written < 0) {
    const std::span<std::uint8_t> settings = state.container_data_empty
                                                 ? WholeSettings(host)
                                                 : std::span<std::uint8_t>{};
    if (!settings.empty()) {
      settings[0] = static_cast<std::uint8_t>(
          (settings[0] & ~kMainSettingBit) |
          ((state.backup_main_setting & 1) * kMainSettingBit));
    }
    host.ZeroSummary(slot);
    host.SetSummaryStatus(slot, kSlotTemporary);
    element.save_location = 0;
    host.RestoreSaveImage();
    host.SetProgressCounter(0);
    RestoreSettings(host, state.save_settings_backup);
    host.SetCardMessage(kTextSaveFailed);
  } else {
    host.SetDataNotSaved(0);
    host.SetProgressCounter(kCompleteCounter);
    host.LoadWrittenSummary(slot);
    element.save_location = host.Summary(slot).save_location;
    host.PlaySoundEffect(kFileOperationComplete);
    list.result = 1;
    host.SetCardMessage(kTextSaved);
  }
  list.step = kSaveLeaveWithTimer;
}
// The card check after a file is picked: the file is read, or the list
// waits to leave.
void StartLoad(Context& ctx) {
  FileList& list = ctx.state.load_files;
  const int checked = CheckCardStep(ctx, 0);
  if (checked == kMaskedPending) {
    return;
  }
  if (checked > kMaskedPending) {
    ctx.host.ReadSaveStep((CurrentSlot(list) + 1) |
                          ((list.port - 1) << kPortShift) | kReadRequestFlag);
    list.step = kLoadApply;
    ctx.host.SetCardMessage(kTextLoading);
  } else {
    list.step = kLoadLoaded;
  }
}

// A finished read: a valid file is applied and settles before the list
// leaves; a failed or corrupt one says why. The play time is kept to put
// back once the list closes.
void ApplyLoadedFile(Context& ctx, int read) {
  State& state = ctx.state;
  Host& host = ctx.host;
  FileList& list = state.load_files;
  host.SetProgressCounter(0);
  list.leave_timer = 0;
  if (read < 0) {
    host.SetCardMessage(kTextLoadFailed);
  } else {
    switch (host.ValidateAndApplyLoadedSave(1)) {
      case kLoadValid:
        host.SetProgressCounter(kCompleteCounter);
        host.PlaySoundEffect(kFileOperationComplete);
        list.leave_timer = kLoadSettleTicks;
        list.result = 1;
        host.SetCardMessage(kTextLoaded);
        break;
      case kLoadCorrupt:
        host.SetCardMessage(kTextFileCorrupt);
        break;
      default:
        break;
    }
  }
  state.load_time_backup = host.PlayTime();
  list.step = kLoadLoaded;
}

void StartSlide(Element& element, ElementState state, std::int16_t target) {
  element.state = static_cast<std::uint8_t>(state);
  element.target = target;
}

// The picker's port; a port whose pair lies outside the cards is refused
// and the picker returns.
void InitCardPicker(Host& host, CardPicker& picker, int init_port) {
  picker.port = static_cast<std::uint8_t>(init_port);
  host.SetCardMessage(kTextChecking);
  picker.step = kCardSlideOut;
  if (picker.port > kElementPairSum) {
    host.Refuse(
        std::format("memory card port {} has no card element", picker.port));
    picker.step = kCardReturn;
  }
}

// The picked card slides to the side and the other off the screen.
void SlideCardsOut(State& state, std::uint8_t port) {
  Element& chosen = ElementAt(state, port);
  StartSlide(chosen, ElementState::kAnimateX, kChosenCardX);
  chosen.inner_blend = 1;
  StartSlide(ElementAt(state, kElementPairSum - port), ElementState::kAnimateX,
             kOffScreenX);
}

void SlideCardsAway(State& state) {
  for (int id = 1; id <= kLowerCard; ++id) {
    StartSlide(ElementAt(state, id), ElementState::kAnimateX, kOffScreenX);
  }
}

using ListUpdate = int (*)(Context&, int);

// The start and the steps both pickers share: what the picker returns, or
// none for the steps that differ.
std::optional<int> StepCardPicker(Context& ctx, CardPicker& picker,
                                  int init_port, ListUpdate update_list) {
  State& state = ctx.state;
  if (init_port != 0) {
    InitCardPicker(ctx.host, picker, init_port);
    return 0;
  }
  switch (picker.step) {
    case kCardSlideOut:
      SlideCardsOut(state, picker.port);
      picker.step = kCardSlideUp;
      return 0;
    case kCardSlideUp:
      if (AreAnimationsComplete(state)) {
        if (picker.port == kLowerCard) {
          StartSlide(ElementAt(state, kLowerCard), ElementState::kAnimateY,
                     kRaisedCardY);
        }
        picker.step = kCardStartEvents;
      }
      return 0;
    case kCardLeave:
      if (LeavePressed(ctx.host)) {
        Play(ctx.host, MenuSound::kLeave);
        SlideCardsAway(state);
        picker.step = kCardReturn;
      }
      return 0;
    case kCardReturn:
      return AreAnimationsComplete(state) ? -1 : 0;
    case kCardList: {
      const int result = update_list(ctx, 0);
      if (result < 0) {
        SlideCardsAway(state);
        picker.step = kCardReturn;
        return 0;
      }
      return result > 0 ? 1 : 0;
    }
    default:
      return std::nullopt;
  }
}

// _selectSaveMemoryCard's readiness events: a ready card is scanned, a
// missing one says so and an unformatted one lists five empty slots.
void PollSaveCard(Context& ctx) {
  CardPicker& picker = ctx.state.save_card;
  Host& host = ctx.host;
  switch (host.AdvanceCardReadiness(0) & kReadinessMask) {
    case kReadinessReady:
      picker.step = kCardScan;
      break;
    case kReadinessTimeout:
      picker.step = kCardLeave;
      host.SetCardMessage(kTextInsertError);
      break;
    case kReadinessUnformatted:
      for (int slot = 0; slot < static_cast<int>(kSaveSlots); ++slot) {
        host.ZeroSummary(slot);
        host.SetSummaryStatus(slot, kSlotAvailable);
      }
      host.ClearCardDirectory();
      UpdateSaveFiles(ctx, picker.port);
      picker.step = kCardList;
      break;
    default:
      break;
  }
}

// What a picker's scan needs: the status a slot must reach, the message
// when none does and the list it opens.
struct CardScan {
  std::uint32_t required = kSlotAvailable;
  std::uint16_t empty_text = 0;
  ListUpdate update_list = nullptr;
};

[[nodiscard]] bool AnySlotReaches(Host& host, std::uint32_t required) {
  for (int slot = 0; slot < static_cast<int>(kSaveSlots); ++slot) {
    if (host.Summary(slot).status >= required) {
      return true;
    }
  }
  return false;
}

void ScanCard(Context& ctx, CardPicker& picker, const CardScan& scan) {
  Host& host = ctx.host;
  if (host.ScanSaveDirectory(picker.port) != 0) {
    host.SetCardMessage(kTextLoadFailed);
    picker.step = kCardLeave;
  } else if (!AnySlotReaches(host, scan.required)) {
    host.SetCardMessage(scan.empty_text);
    picker.step = kCardLeave;
  } else {
    scan.update_list(ctx, picker.port);
    picker.step = kCardList;
  }
}

// GAMEOVER.BIN and the first three text CLUT rows go to VRAM; an image
// shorter than the 24x128 rectangle is refused.
void UploadGameOverAssets(Host& host) {
  const std::span<const std::uint16_t> image = host.FinishGameOverLoad();
  if (image.size() < kGameOverImageHalfwords) {
    host.Refuse(std::format("GAMEOVER.BIN holds {} halfwords, not {}",
                            image.size(), kGameOverImageHalfwords));
  } else {
    host.UploadImage(kGameOverImagePosition,
                     image.first(kGameOverImageHalfwords), kGameOverImageSize);
  }
  host.UploadImage(kChoiceClutPosition,
                   std::span(kMenuItemTextClut).first(kTextClutUploadColors),
                   kTextClutSize);
}

// The picked option: the screen darkens under a subtracted tile until it is
// black, then the display is masked and the option returned (from 1).
int FadeOutGameOver(Host& host, GameOverChoices& choices) {
  std::array<std::uint32_t, kTileSemiPacketWords> packet{
      0, kSubtractDrawMode,
      kTileSemiCode |
          (Word(choices.color1 + kChoiceBright) * kFadeTileColorStep),
      0, kFadeTileSize};
  Emit(host.OverlayQueue(), packet);
  choices.color1 += kChoiceFadeStep;
  if (choices.color1 == 0) {
    host.SetDisplayMask(false);
    return choices.option + 1;
  }
  return 0;
}

// The chosen option glows up over four steps, then pulses down to rest
// while the other one's glow fades.
void GlowChoice(GameOverChoices& choices) {
  const bool first = choices.option == 0;
  std::int32_t& blend = first ? choices.blend1 : choices.blend2;
  std::int32_t& color = first ? choices.color2 : choices.color3;
  std::int32_t& other_color = first ? choices.color3 : choices.color2;
  std::int32_t& other_blend = first ? choices.blend2 : choices.blend1;
  if (blend < kGlowSteps) {
    color = blend * kGlowColorStep;
    blend = std::min(blend + kGlowBlendStep, kGlowSteps);
  } else {
    color = color >= kGlowPulseFloor ? color - kGlowColorStep : kGlowRest;
    other_color =
        other_color < kGlowColorStep ? 0 : other_color - kGlowColorStep;
  }
  if (other_blend != 0) {
    --other_blend;
  }
}

// Start or Circle picks the option and fades the music (true); Select and
// up or down switch it.
bool GameOverInput(Host& host, GameOverChoices& choices) {
  const std::uint32_t pressed = host.Input().pressed;
  if ((pressed & (kPadStart | kPadCircle)) != 0) {
    Play(host, MenuSound::kSelect);
    host.FadeMusicSlotVolume(kGameOverMusicSlot, 0, kMusicFadeFrames);
    choices.color1 = -kChoiceBright;
    return true;
  }
  if ((pressed & (kPadSelect | kPadUpDown)) != 0) {
    Play(host, MenuSound::kChange);
    choices.option = 1 - choices.option;
  }
  GlowChoice(choices);
  return false;
}

struct ChoiceSprite {
  int brightness = 0;
  std::uint32_t position = 0;
  std::uint32_t texture = 0;
};

// _drawSprt's packet on GAMEOVER.BIN's page with the choice's CLUT.
void QueueChoiceSprite(Render::PacketQueue& queue, const ChoiceSprite& sprite) {
  Render::SpritePacket packet =
      Render::BuildSpritePacket({.draw_mode = kGameOverDrawMode,
                                 .brightness_transparency = sprite.brightness,
                                 .packed_position = Packed(sprite.position),
                                 .packed_size = Packed(kChoiceSize)},
                                queue.ChainLink());
  packet[Render::kSpriteTextureWord] = sprite.texture;
  queue.Emit(packet);
}

// Both options, then their glows blended over them.
void DrawChoices(Render::PacketQueue& queue, const GameOverChoices& choices) {
  QueueChoiceSprite(queue,
                    {choices.color1, kFirstChoicePosition, kRetryTexture});
  QueueChoiceSprite(queue,
                    {choices.color1, kSecondChoicePosition, kTitleTexture});
  QueueChoiceSprite(queue, {choices.color2 | kSemiTransparentBrightness,
                            kFirstChoicePosition, kRetryGlowTexture});
  QueueChoiceSprite(queue, {choices.color3 | kSemiTransparentBrightness,
                            kSecondChoicePosition, kTitleGlowTexture});
}

// The option once the screen is black: the title, the intro again while it
// has not been cleared, or the file menu.
void GameOverChosen(Context& ctx, std::uint8_t& step) {
  const int option = UpdateGameOverChoices(ctx, false);
  if (option == 0) {
    return;
  }
  ctx.state.game_over_delay = kGameOverResetDelay;
  if (option == kTitleOption) {
    step = kGameOverToTitle;
    return;
  }
  const GameFlags flags = ctx.host.Flags();
  if (Word(flags.intro_state - 1) < 2 && flags.clear_count == 0) {
    ctx.host.LoadIntroSnapshotStep(flags.intro_state);
    step = kGameOverRestartIntro;
  } else {
    step = kGameOverOpenFiles;
  }
}

// The game-over load list: a loaded file enters the battle, a cancel returns
// to the title.
void GameOverFileMenu(Context& ctx, std::uint8_t& step) {
  Host& host = ctx.host;
  host.SetDisplayMask(true);
  const int option = host.UpdateDataLoadMenu(0);
  if (option == 0) {
    DrawBackground(host);
    DrawFrame(ctx, host.FrameBuffer());
    return;
  }
  host.ReleaseMenuResources();
  host.SetDisplayMask(false);
  if (option < 0) {
    ctx.state.game_over_delay = 1;
    step = kGameOverToTitle;
    return;
  }
  host.FreeMusicSlot(kGameOverMusicSlot);
  host.SetGameOver(false);
  host.EnterBattle();
}
// _promptYesNo's row for `choice` (0 yes, 1 no) slides in from the right;
// a text offset past the container strings is refused.
void SlideInContainerRow(Context& ctx, int choice) {
  Host& host = ctx.host;
  const std::span<const std::uint16_t> strings = host.ContainerStrings();
  const std::size_t offset = choice == 0 ? kContainerTextYes : kContainerTextNo;
  if (offset >= strings.size()) {
    host.Refuse(std::format(
        "container text offset {} lies outside the {}-halfword table", offset,
        strings.size()));
    return;
  }
  const std::span<const std::uint16_t> tail = strings.subspan(offset);
  Framework::MenuRow& row = host.SetMenuItem(
      {.row = kContainerYesRow + choice,
       .x = kScreenWidth,
       .y = kChoiceRowTop + (choice * kPromptPitch),
       .w = kPromptWidth,
       .background_width = 0},
      {reinterpret_cast<const std::uint8_t*>(tail.data()), tail.size_bytes()});
  row.state = Framework::RowState::kSlideX;
  row.target0 = kChoiceRowTarget;
}

// _promptYesNo's input: Triangle is refused unless it may cancel; Circle on
// yes accepts (1), anything else leaves (-1).
void ContainerInput(Context& ctx) {
  State& state = ctx.state;
  Host& host = ctx.host;
  const int option = state.container_option;
  host.MenuItem(kContainerYesRow + option).selected = 1;
  host.MenuItem(kContainerNoRow - option).selected = 0;
  const Framework::Buttons buttons = host.Input();
  std::uint32_t pressed = buttons.pressed;
  if (state.container_cancel_with_menu == 0 && (pressed & kPadTriangle) != 0) {
    pressed &= ~kPadTriangle;
    Play(host, MenuSound::kInvalid);
  }
  if ((pressed & kConfirmButtons) != 0) {
    if (option != 0 || (pressed & kCancelButtons) != 0) {
      Play(host, MenuSound::kLeave);
      host.ClearMenuExcept(Framework::kNoRow);
      state.container_option = -1;
    } else {
      Play(host, MenuSound::kSelect);
      host.ClearMenuExcept(0);
      state.container_option = 1;
    }
    state.container_step = kContainerReturn;
    return;
  }
  state.container_cursor = static_cast<std::uint8_t>(host.DrawMainMenuCursor(
      state.container_cursor, option + kContainerCursorBase));
  if ((buttons.repeat & kPadUpDown) != 0) {
    Play(host, MenuSound::kChange);
    state.container_option = 1 - option;
  }
}

// _copyContainer's renumbering: each section's selected item (from 1) maps
// to its new index; a selection past the halfwords is skipped.
[[nodiscard]] FlatIndices TargetIndices(const FlatIndices& selection) {
  FlatIndices remap{};
  std::size_t base = 0;
  for (std::size_t section = 0; section < kContainerSections; ++section) {
    for (std::size_t item = 0; item < kContainerCapacities[section]; ++item) {
      const std::size_t from = selection[base + item];
      if (from != 0 && base + from - 1 < remap.size()) {
        remap[base + from - 1] = static_cast<std::uint16_t>(item + 1);
      }
    }
    base += kContainerCapacities[section];
  }
  return remap;
}

// The new index of item `index` (from 1) of the section at `base`. An index
// of 0 reads the halfword before the section, as the original does; one
// outside the halfwords reads 0.
[[nodiscard]] std::uint16_t Lookup(const FlatIndices& remap, std::size_t base,
                                   int index) {
  const std::ptrdiff_t at = static_cast<std::ptrdiff_t>(base) + index - 1;
  if (at < 0 || at >= std::ssize(remap)) {
    return 0;
  }
  return remap[static_cast<std::size_t>(at)];
}

void RemapGems(const FlatIndices& remap, std::span<std::uint8_t> gems) {
  for (std::uint8_t& gem : gems) {
    if (gem != 0) {
      gem = static_cast<std::uint8_t>(Lookup(remap, kGemIndexBase, gem));
    }
  }
}

// One section of _copyContainer: the selected source records in their new
// order, renumbered and fixed up; a selection past the section is skipped.
template <typename Record, std::size_t N, typename Fix>
void CopySection(std::array<Record, N>& target,
                 const std::array<Record, N>& source,
                 const std::array<std::uint16_t, N>& selection, Fix fix) {
  for (std::size_t i = 0; i < N; ++i) {
    const std::size_t from = selection[i];
    if (from == 0 || from > N) {
      continue;
    }
    Record& record = target[i];
    record = source[from - 1];
    record.index = static_cast<decltype(record.index)>(i + 1);
    fix(record);
  }
}

// vs_menu7_dataMenu's rows: save and load, the save row disabled without a
// save point or while saving is off.
void CreateDataRows(Context& ctx, std::uint8_t& step) {
  Host& host = ctx.host;
  std::array<std::span<const std::uint8_t>, kDataRowCount * 2> strings{};
  std::array<int, kDataRowCount> row_types{};
  for (std::size_t row = 0; row < kDataRowCount; ++row) {
    strings[row * 2] = IndexedText(host, row * 2);
    strings[(row * 2) + 1] = TableText(host, kTextSaveDisabled);
  }
  host.SetSaveLocationId(host.CurrentSavePointId());
  if (host.SaveLocationId() == 0 || host.Flags().opaque_a7 != 0) {
    row_types[0] |= 1;
  }
  const std::uint8_t memory = host.CursorMemory();
  if (step != kDataLoadWait) {
    host.SetCursorMemory(1);
  }
  host.CreateMenuRows(strings, row_types, kDataRowInfo);
  host.SetCursorMemory(memory);
  host.SetFieldRendering(false);
  step = kDataSelect;
}

// The picked row opens its file screen; a cancel closes the menu; while
// saving is off the save row's caution is shown.
void DataMenuSelect(Context& ctx, std::uint8_t& step) {
  Host& host = ctx.host;
  const int row = host.SelectedRow() + 1;
  if (row > 0) {
    host.ClearMenuExcept(kKeepRow);
    ResetElements(ctx);
    switch (row) {
      case 1:
        step = kDataSaveOpen;
        break;
      case 2:
        step = kDataLoadOpen;
        break;
      default:
        break;
    }
  } else if (row < 0) {
    host.SetFieldRendering(true);
    host.ClearMenuExcept(Framework::kNoRow);
    step = row == kCancelToMenuRow ? kDataCloseToMenu : kDataClose;
  } else if (host.SaveLocationId() != 0 && host.Flags().opaque_a7 != 0 &&
             host.HighlightedRow() == 0) {
    host.InitInformationBox(Framework::InformationHeader::kCaution);
    host.SetInformationBoxState(kInformationBoxShown);
  } else {
    host.SetInformationBoxState(0);
  }
}

// The load screen: a cancel brings the rows back and a loaded file enters
// the battle.
void DataMenuLoad(Context& ctx, std::uint8_t& step) {
  Host& host = ctx.host;
  const int result = host.UpdateDataLoadMenu(0);
  if (result < 0) {
    step = kDataRows;
    return;
  }
  if (result > 0) {
    host.ClearScreen();
    host.ReleaseMenuResources();
    step = kDataClosed;
    host.EnterBattle();
    return;
  }
  DrawFrame(ctx, host.FrameBuffer());
}

// _saveScreenConfirmation's input: Cross is refused, Circle returns the
// option (1 yes, 2 no), up and down switch it.
void ReminderInput(TitleHost& host, State& state) {
  const int confirm = state.reminder_confirm;
  host.SetElementSelected(confirm, true);
  host.SetElementSelected(kElementPairSum - confirm, false);
  const Framework::Buttons buttons = host.Input();
  if ((buttons.pressed & kPadCross) != 0) {
    host.PlaySound(MenuSound::kInvalid);
  }
  if ((buttons.pressed & kPadCircle) != 0) {
    host.PlaySound(confirm == 1 ? MenuSound::kSelect : MenuSound::kLeave);
    host.SetCursorPosition(0);
    for (int id = 1; id < kElementPairSum; ++id) {
      host.AnimateElement(id, ElementState::kAnimateX, kOffScreenX);
    }
    state.reminder_step = kReminderReturn;
    return;
  }
  if ((buttons.repeat & kPadUpDown) != 0) {
    host.PlaySound(MenuSound::kChange);
    state.reminder_confirm =
        static_cast<std::uint8_t>(kElementPairSum - confirm);
  }
  host.SetCursorPosition(Packed(
      PackXy(kReminderCursorX,
             ((state.reminder_confirm + kReminderCursorRow) * kPromptPitch) +
                 kPromptCursorTop)));
}

// getTPage for the hidden buffer's 16-bit page at `x`, blended by
// subtraction.
[[nodiscard]] constexpr std::uint32_t ExitPage(int x) noexcept {
  return kExitPageBase | ((Word(x) & kPageColumnMask) >> kPageColumnShift);
}
}  // namespace

// decomp: MENU7.PRG _drawSaveInfoUI (src/MENU/MENU7.PRG/260.c:1222)
void DrawSummaryLabel(Host& host, std::int32_t position, Label label) {
  const auto index = static_cast<std::size_t>(label);
  if (index >= kLabelCount) {
    host.Refuse(std::format("file summary label {} lies outside the {} labels",
                            index, kLabelCount));
    return;
  }
  DimmedSprite(host, {.position = Word(position),
                      .uv_clut = kLabelUvClut[index],
                      .size = kLabelSize[index],
                      .page = kInterfacePage});
}

// decomp: MENU7.PRG _renderInteger (src/MENU/MENU7.PRG/260.c:1239)
// decomp: TITLE.PRG _drawInteger (src/TITLE/TITLE.PRG/22C.c:1129)
// A zero divisor draws nothing; the original divided by it.
void DrawInteger(ImmediatePacketTarget& target, std::int32_t position,
                 std::uint32_t value, std::uint32_t divisor) {
  if (divisor == 0) {
    return;
  }
  std::uint32_t xy = Word(position);
  do {
    Sprite(target, {.position = xy,
                    .uv_clut = ((value / divisor) * kDigitWidth) | kDigitClut,
                    .size = kDigitSize,
                    .page = kInterfacePage});
    value %= divisor;
    divisor /= kDecimalBase;
    xy += kDigitAdvance;
  } while (divisor != 0);
}

// decomp: MENU7.PRG _countDigits (src/MENU/MENU7.PRG/260.c:1253)
// decomp: TITLE.PRG _countDigits (src/TITLE/TITLE.PRG/22C.c:1140)
int DecimalDigitCount(int value) noexcept {
  int digits = 0;
  do {
    value /= kDecimalBase;
    ++digits;
  } while (value != 0);
  return digits;
}

// decomp: MENU7.PRG _drawHPMP (src/MENU/MENU7.PRG/260.c:1270)
// More than three digits in either value is refused after the bar and the
// label; the original read past its divisor table.
void DrawHpMpSummary(Host& host, std::int32_t position, Stat stat,
                     std::uint32_t current, std::uint32_t max) {
  const std::uint32_t fill =
      max != 0 ? ((current * kBarSteps) + max - 1) / max : 0;
  DrawHpMpBar(host, Word(position), fill,
              stat != Stat::kHp ? kMpColor : kHpColor);
  const auto current_digits =
      static_cast<std::size_t>(DecimalDigitCount(static_cast<int>(current)));
  const auto max_digits =
      static_cast<std::size_t>(DecimalDigitCount(static_cast<int>(max)));
  const std::uint32_t bar = Word(position) + 1;
  DrawSummaryLabel(host, Packed(bar - 1),
                   static_cast<Label>(static_cast<int>(stat) +
                                      static_cast<int>(Label::kHp)));
  if (current_digits >= kDigitDivisors.size() ||
      max_digits >= kDigitDivisors.size()) {
    host.Refuse(std::format(
        "file summary stat {} / {} has more digits than the divisor table",
        current, max));
    return;
  }
  std::uint32_t xy =
      bar -
      Word((static_cast<int>(current_digits) * kDigitPitch) - kValueRightEdge) -
      Word(static_cast<int>(max_digits) * kMaxDigitPitch);
  std::uint32_t divisor = kDigitDivisors[current_digits];
  do {
    DimmedSprite(host, {.position = xy - kLargeDigitRaise,
                        .uv_clut = (((current / divisor) << kLargeDigitShift) +
                                    kLargeDigitU) |
                                   kDigitClut,
                        .size = kLargeDigitSize,
                        .page = kInterfacePage});
    current %= divisor;
    divisor /= kDecimalBase;
    xy += kLargeDigitAdvance;
  } while (divisor != 0);
  DrawSummaryLabel(host, Packed(xy + kDotOffset), Label::kDot);
  DrawInteger(host, Packed(xy + kMaxValueOffset), max,
              kDigitDivisors[max_digits]);
}

// decomp: MENU7.PRG _fileProcessingAnim (src/MENU/MENU7.PRG/260.c:1353)
void DrawTransferProgress(Context& context, std::int32_t x, std::int32_t y) {
  std::uint32_t left = Word(x) - kTransferTrail;
  left = std::max(left, kTransferLeftLimit);
  std::uint32_t right = Word(x);
  const std::uint32_t top = Word(y);
  const std::uint32_t bottom = top + kTransferHeight;
  std::uint32_t color1 = 0;
  std::uint32_t color2 = context.state.is_saving ? kSaveColor : kLoadColor;
  for (int band = 0; band < 2; ++band) {
    std::array<std::uint32_t, kTransferPacketWords> packet{
        0,
        kDitheredAdd,
        kPolyG4SemiCode | color1,
        left | top,
        color2,
        right | top,
        color1,
        left | bottom,
        color2,
        right | bottom};
    Submit(context.host, packet);
    left = right;
    right += kTransferStep;
    color1 = color2;
    color2 = 0;
  }
}

// decomp: MENU7.PRG _fileProcessingCompleteAnim (src/MENU/MENU7.PRG/260.c:1400)
void DrawTransferComplete(Context& context, std::int32_t colour,
                          std::int32_t y) {
  const std::uint32_t base = Word(colour);
  std::uint32_t color0 =
      context.state.is_saving
          ? base + (base << kSaveColorShift)
          : (base - Word(colour >> kLoadColorShift)) + (base << kByteShift);
  std::uint32_t color1 = 0;
  std::uint32_t top = Word(y);
  std::array<std::uint32_t, kCompleteTilePacketWords> tile{
      0, kDitheredAdd, kTileSemiCode | color0, top | kCompleteLeft,
      kCompleteSize};
  Submit(context.host, tile);
  for (int band = 0; band < 2; ++band) {
    const std::uint32_t fade_top = top + kCompleteFadeHeight;
    std::array<std::uint32_t, kCompleteGradientPacketWords> packet{
        0,
        kPolyG4SemiCode | color1,
        fade_top | kCompleteLeft,
        color1,
        fade_top | kCompleteRight,
        color0,
        top | kCompleteLeft,
        color0,
        top | kCompleteRight};
    Submit(context.host, packet);
    top += kCompleteBandStep;
    color1 = color0;
    color0 = 0;
  }
}

// decomp: MENU7.PRG _initFileMenu (src/MENU/MENU7.PRG/260.c:1447)
void ResetElements(Context& context) {
  context.host.SetCardMessage(std::nullopt);
  context.state.cursor_color = 0;
  context.host.SetCursorPosition(0);
  context.host.SetProgressCounter(0);
  context.state.screen_fade = 0;
  context.host.SetTransferTarget(kTransferTargetReset);
  context.state.elements.fill(Element{});
}

// decomp: MENU7.PRG _initFileMenuElement (src/MENU/MENU7.PRG/260.c:1463)
Element* InitializeElement(Context& context, int id, std::uint32_t position,
                           std::uint32_t size,
                           std::span<const std::uint8_t> text) {
  if (!ValidElement(id)) {
    context.host.Refuse(
        std::format("file menu element {} lies outside the {} elements", id,
                    kElementCount));
    return nullptr;
  }
  Element& element = ElementAt(context.state, id);
  element = Element{};
  element.state = static_cast<std::uint8_t>(ElementState::kStatic);
  element.slot_id = kElementTextEnd;
  element.x = static_cast<std::int16_t>(position & kHalfwordMask);
  element.y = static_cast<std::int16_t>(position >> kHalfwordShift);
  element.w = static_cast<std::int16_t>(size & kHalfwordMask);
  element.h = static_cast<std::int16_t>(size >> kHalfwordShift);
  if (text.empty()) {
    element.text[0] = kElementTextEnd;
  } else {
    CopyElementText(element.text, text);
  }
  return &element;
}

// decomp: MENU7.PRG _clearFileMenuElement (src/MENU/MENU7.PRG/260.c:1504)
void ClearElement(Context& context, int id) {
  if (!ValidElement(id)) {
    context.host.Refuse(
        std::format("file menu element {} lies outside the {} elements", id,
                    kElementCount));
    return;
  }
  ElementAt(context.state, id) = Element{};
}

// decomp: MENU7.PRG _fileMenuElementsActive (src/MENU/MENU7.PRG/260.c:1512)
bool AreAnimationsComplete(const State& state) {
  return std::ranges::all_of(state.elements, [](const Element& element) {
    return element.state < static_cast<std::uint8_t>(ElementState::kAnimateX);
  });
}

// decomp: MENU7.PRG _printCharacter (src/MENU/MENU7.PRG/260.c:1524)
// A glyph past the width table draws nothing and keeps x; the original read
// past the table.
int DrawGlyph(ImmediatePacketTarget& target, std::uint32_t glyph, int x, int y,
              int clut) {
  if ((glyph >> kByteShift) == kSpacingGlyphPage) {
    return x + static_cast<int>(glyph & kByteMask);
  }
  if (glyph >= kGlyphCount) {
    return x;
  }
  if (glyph != kGlyphSpace) {
    std::array<std::uint32_t, kGlyphPacketWords> packet{
        0,
        kGlyphDrawMode,
        kGlyphColor,
        (Word(x) & kHalfwordMask) | (Word(y) << kHalfwordShift),
        PackUvClut(static_cast<int>(glyph % kGlyphColumns) * kGlyphCell,
                   static_cast<int>(glyph / kGlyphColumns) * kGlyphCell,
                   (clut * kGlyphClutStride) + kGlyphClutX, kGlyphClutY),
        PackXy(kGlyphCell, kGlyphCell)};
    Submit(target, packet);
  }
  return x + kGlyphWidths[glyph];
}

// decomp: MENU7.PRG _findCurrentSave (src/MENU/MENU7.PRG/260.c:1572)
int FindCurrentDataSaveStep(Context& context, bool init) {
  if (init) {
    context.state.data_save_search = {};
    return 0;
  }
  return StepSaveSearch(context.host, context.state.data_save_search,
                        &context.host);
}

// decomp: MENU7.PRG maskedHandler (src/MENU/MENU7.PRG/260.c:1640)
int CheckCardStep(Context& context, int port_mask) {
  CardHost& host = context.host;
  if (port_mask != 0) {
    host.AdvanceCardReadiness(port_mask & kReadinessMask);
    context.state.card_mask =
        static_cast<std::uint8_t>(port_mask >> kEventMaskShift);
    return kMaskedPending;
  }
  const int event = host.AdvanceCardReadiness(0);
  if (event == kReadinessPending) {
    return kMaskedPending;
  }
  switch (event & context.state.card_mask) {
    case kReadinessReady:
      return kMaskedComplete;
    case kReadinessTimeout:
      host.SetCardMessage(kTextInsertError);
      break;
    case kReadinessUnformatted:
      host.SetCardMessage(kTextNoData);
      break;
    default:
      host.SetCardMessage(kTextRemoved);
      break;
  }
  return kMaskedError;
}

// decomp: MENU7.PRG _printString (src/MENU/MENU7.PRG/260.c:1675)
// decomp: TITLE.PRG _printString (src/TITLE/TITLE.PRG/22C.c:1520)
// The span's end ends the text.
void DrawText(ImmediatePacketTarget& target, std::uint8_t& arrow,
              std::span<const std::uint8_t> text, int x, int y, int clut) {
  const int start_x = x;
  int next_x = x;
  std::size_t i = 0;
  while (i < text.size()) {
    const std::uint8_t c = text[i++];
    if (c >= kCodeControl) {
      if (i >= text.size()) {
        return;
      }
      const std::uint32_t control = text[i++];
      if (c == kCodeSpacing) {
        next_x = control >= kNegativeSpacing
                     ? next_x - (kSpacingRange - static_cast<int>(control))
                     : next_x + static_cast<int>(control);
      }
    } else if (c < kCodeNonPrinting) {
      next_x = DrawGlyph(target, c, next_x, y, clut);
    } else if (c == kCodeConfirm) {
      arrow = static_cast<std::uint8_t>((arrow + 1) % kArrowTicks);
      next_x = DrawGlyph(target, kGlyphArrow - (arrow >> kArrowFrameShift),
                         next_x, y, clut);
    } else if (c == kCodeTerminator) {
      return;
    } else if (c == kCodeNewline) {
      y += kLineHeight;
      next_x = start_x;
    }
  }
}

// decomp: MENU7.PRG _intepolateMenuItemBgColour (src/MENU/MENU7.PRG/260.c:1738)
// decomp: TITLE.PRG _intepolateMenuItemBgColour
// (src/TITLE/TITLE.PRG/22C.c:1577)
std::optional<std::uint32_t> ElementBackgroundColor(
    std::uint32_t outer, std::uint32_t inner) noexcept {
  std::uint32_t color1 = 0;
  std::uint32_t color2 = 0;
  if (inner <= kBlendSteps) {
    color1 = Render::BlendRgb(kLightFrom, kLightTo, inner);
    color2 = Render::BlendRgb(kDarkFrom, kDarkTo, inner);
  } else {
    const std::uint32_t row = (inner >> kFlashRowShift) - kFlashFirstRow;
    if (row >= kFlashLight.size()) {
      return std::nullopt;
    }
    color1 = kFlashLight[row];
    color2 = kFlashDark[row];
  }
  return Render::BlendRgb(color1, color2, outer);
}

// decomp: MENU7.PRG _drawFileMenuElement (src/MENU/MENU7.PRG/260.c:1765)
void DrawElement(Context& context, Element& element) {
  if (element.selected != 0) {
    element.outer_blend = kBlendSteps;
  } else if (element.outer_blend != 0) {
    --element.outer_blend;
  }
  if (element.slot_unavailable != 0) {
    element.outer_blend = 0;
  }
  if (element.slot_id < kSaveSlots &&
      Word(element.y - kSlotListTop) >= kSlotListSpan) {
    return;
  }
  const std::uint8_t inner = element.inner_blend;
  const std::optional<std::uint32_t> light =
      ElementBackgroundColor(kBlendSteps - element.outer_blend, inner);
  const std::optional<std::uint32_t> dark =
      ElementBackgroundColor(element.outer_blend, inner);
  if (!light || !dark) {
    context.host.Refuse(std::format(
        "file menu element blend {} lies past the colour tables", inner));
    return;
  }
  if ((inner & kBlendStepMask) != 0) {
    element.inner_blend = static_cast<std::uint8_t>(inner + 1);
  }
  DrawElementBox(context.host, element, *light, *dark);
  DrawElementText(context.host, element);
  if (element.slot_id < kSaveSlots) {
    DrawSlot(context, element);
  }
}

// decomp: MENU7.PRG _drawFileMenu (src/MENU/MENU7.PRG/260.c:1951)
void DrawFrame(Context& context, int frame_buffer) {
  Host& host = context.host;
  const std::uint32_t buffer_x = frame_buffer != 0 ? Word(kScreenWidth) : 0;
  DimmedSprite(host, {.position = kFrameRightPosition,
                      .uv_clut = kFrameUvClut,
                      .size = kFrameRightSize,
                      .page = kRightBackgroundPage});
  DimmedSprite(host, {.position = kFrameLeftPosition,
                      .uv_clut = kFrameUvClut,
                      .size = kFrameLeftSize,
                      .page = kLeftBackgroundPage});
  for (Element& element : context.state.elements) {
    const auto state = static_cast<ElementState>(element.state);
    AnimateElement(element, state);
    if (state != ElementState::kInactive) {
      DrawElement(context, element);
    }
  }
  DrawFrameEdges(host, buffer_x);
  DrawFrameFooter(context);
}

// decomp: MENU7.PRG _drawFileMenuBg (src/MENU/MENU7.PRG/260.c:2078)
// decomp: TITLE.PRG _drawFileMenuBg (src/TITLE/TITLE.PRG/22C.c:1908)
void DrawBackground(ImmediatePacketTarget& target) {
  Sprite(target, {.position = kBackgroundRightPosition,
                  .uv_clut = kBackgroundUvClut,
                  .size = kBackgroundRightSize,
                  .page = kRightBackgroundPage});
  Sprite(target, {.position = 0,
                  .uv_clut = kBackgroundUvClut,
                  .size = kBackgroundLeftSize,
                  .page = kLeftBackgroundPage});
}

// decomp: MENU7.PRG _promptConfirm (src/MENU/MENU7.PRG/260.c:2092)
int UpdateConfirmation(Context& context, bool init) {
  State& state = context.state;
  if (init) {
    state.confirmation_cancelled = 1;
    state.confirmation_step = kConfirmationYes;
    return 0;
  }
  switch (state.confirmation_step) {
    case kConfirmationYes:
    case kConfirmationNo: {
      const int row = state.confirmation_step;
      Element* element = InitializeElement(
          context, kFirstPromptElement + row,
          PackXy(-kPromptWidth, (row * kPromptPitch) + kPromptTop),
          PackXy(kPromptWidth, kPromptHeight),
          IndexedText(context.host,
                      kIndexYesIndent + static_cast<std::size_t>(row)));
      if (element != nullptr) {
        element->state = static_cast<std::uint8_t>(ElementState::kAnimateNegX);
        element->target = 0;
      }
      ++state.confirmation_step;
      break;
    }
    case kConfirmationWait:
      state.confirmation_step += AreAnimationsComplete(state) ? 1 : 0;
      break;
    case kConfirmationInput:
      return ConfirmationInput(context);
    default:
      break;
  }
  return 0;
}

// decomp: MENU7.PRG _promptOverwrite (src/MENU/MENU7.PRG/260.c:2161)
int UpdateOverwritePrompt(Context& context, bool init) {
  State& state = context.state;
  if (init) {
    state.overwrite_initialized = true;
    context.host.SetCardMessage(kTextOverwritePrompt);
    UpdateConfirmation(context, true);
    state.overwrite_step = 0;
    return 0;
  }
  if (state.overwrite_step == 0) {
    state.overwrite_result = UpdateConfirmation(context, false);
    if (state.overwrite_result != 0) {
      state.overwrite_step = 1;
    }
  } else if (state.overwrite_step == 1 && AreAnimationsComplete(state)) {
    return state.overwrite_result;
  }
  return 0;
}

// decomp: MENU7.PRG _promptFormat (src/MENU/MENU7.PRG/260.c:2199)
int UpdateFormatPrompt(Context& context, int init_port) {
  State& state = context.state;
  Host& host = context.host;
  if (init_port != 0) {
    state.format_port = static_cast<std::uint8_t>(init_port);
    host.SetCardMessage(kTextFormatPrompt);
    UpdateConfirmation(context, true);
    state.format_step = kFormatConfirm;
    return 0;
  }
  switch (state.format_step) {
    case kFormatConfirm: {
      const int choice = UpdateConfirmation(context, false);
      if (choice < 0) {
        host.SetCardMessage(kTextFormatCancelled);
        return -1;
      }
      if (choice != 0) {
        state.format_step = kFormatStartEvents;
      }
      return 0;
    }
    case kFormatStartEvents:
      if (AreAnimationsComplete(state)) {
        host.AdvanceCardReadiness(state.format_port);
        state.format_step = kFormatEvents;
      }
      return 0;
    case kFormatEvents: {
      const int event = host.AdvanceCardReadiness(0);
      if (event == kReadinessPending) {
        return 0;
      }
      if (event == kReadinessUnformatted) {
        state.format_step = kFormatCard;
        host.SetCardMessage(kTextFormatting);
        return 0;
      }
      host.SetCardMessage(event == kReadinessTimeout ? kTextInsertError
                                                     : kTextRemoved);
      return -1;
    }
    case kFormatCard:
      if (host.FormatCard((state.format_port - 1) * kSlotsPerPort) == 0) {
        host.SetCardMessage(kTextFormatFailed);
        return -1;
      }
      return 1;
    default:
      return 0;
  }
}
// decomp: MENU7.PRG _showSaveFilesMenu (src/MENU/MENU7.PRG/260.c:2277)
int UpdateSaveFiles(Context& context, int init_port) {
  Host& host = context.host;
  FileList& list = context.state.save_files;
  if (init_port != 0) {
    list.port = static_cast<std::uint8_t>(init_port);
    auto slot = static_cast<std::uint8_t>(host.FindMatchingSaveSlot());
    if (slot != 0) {
      --slot;
    }
    PlaceListCursor(host, list, slot);
    list.result = -1;
    list.leave_timer = 0;
    list.step = kSaveInit;
    return 0;
  }
  switch (list.step) {
    case kSaveInit:
      InitializeSlotList(context, kSlotAvailable);
      list.step = kSaveInput;
      [[fallthrough]];
    case kSaveInput:
      SaveListInput(context);
      break;
    case kSaveSlotChosen:
      SaveSlotChosen(context);
      break;
    case kSaveOverwritePrompt: {
      const int answer = UpdateOverwritePrompt(context, false);
      if (answer == 1) {
        CheckCardStep(context, list.port + kEventMaskAll);
        list.step = kSaveOverwriteCheck;
      } else if (answer != 0) {
        list.step = kSaveInput;
      }
      break;
    }
    case kSaveOverwriteCheck:
      CheckOverwrite(context);
      break;
    case kSaveWrite:
      BeginSaveWrite(context);
      break;
    case kSaveValidate: {
      const int written = host.WriteSaveStep(0);
      host.SetProgressCounter(host.ProgressCounter() + 1);
      if (written != 0) {
        FinishSaveWrite(context, written);
      }
      break;
    }
    case kSaveLeaveWithTimer:
      if (list.result == 1) {
        ++list.leave_timer;
      }
      if (!LeavePressed(host) && list.leave_timer != kLeaveTicks) {
        break;
      }
      Play(host, MenuSound::kLeave);
      list.step = kSaveLeave;
      break;
    case kSaveLeave:
      if (host.ProgressCounter() == 0) {
        ClearSlotElements(context);
        return list.result;
      }
      break;
    case kSaveFormat: {
      const int formatted = UpdateFormatPrompt(context, 0);
      if (formatted < 0) {
        list.step = kSaveLeaveError;
      } else if (formatted > 0) {
        CreateFileForSave(context, CurrentSlot(list) + 1);
      }
      break;
    }
    case kSaveLeaveError:
      if (LeavePressed(host)) {
        Play(host, MenuSound::kLeave);
        ClearSlotElements(context);
        return -1;
      }
      break;
    default:
      break;
  }
  return 0;
}

// decomp: MENU7.PRG _selectSaveMemoryCard (src/MENU/MENU7.PRG/260.c:2537)
int UpdateSaveCard(Context& context, int init_port) {
  CardPicker& picker = context.state.save_card;
  if (const std::optional<int> shared =
          StepCardPicker(context, picker, init_port, UpdateSaveFiles)) {
    return *shared;
  }
  switch (picker.step) {
    case kCardStartEvents:
      if (AreAnimationsComplete(context.state)) {
        context.host.AdvanceCardReadiness(picker.port);
        picker.step = kCardPollEvents;
      }
      break;
    case kCardPollEvents:
      PollSaveCard(context);
      break;
    case kCardScan:
      ScanCard(context, picker,
               {.required = kSlotAvailable,
                .empty_text = kTextCardFull,
                .update_list = UpdateSaveFiles});
      break;
    default:
      break;
  }
  return 0;
}

// decomp: MENU7.PRG _showLoadFilesMenu (src/MENU/MENU7.PRG/260.c:2878)
int UpdateLoadFiles(Context& context, int init_port) {
  State& state = context.state;
  Host& host = context.host;
  FileList& list = state.load_files;
  if (init_port != 0) {
    list.port = static_cast<std::uint8_t>(init_port);
    PlaceListCursor(host, list,
                    static_cast<std::uint8_t>(host.NewestSaveSlot()));
    list.result = -1;
    list.complete_timer = 0;
    list.step = kLoadInit;
    return 0;
  }
  switch (list.step) {
    case kLoadInit:
      InitializeSlotList(context, kSlotInUse);
      list.step = kLoadInput;
      host.SetCardMessage(kTextSelectFile);
      [[fallthrough]];
    case kLoadInput: {
      const Framework::Buttons buttons = host.Input();
      if ((buttons.pressed & kPadCross) != 0) {
        Play(host, MenuSound::kLeave);
        ClearSlotElements(context);
        host.SetCursorPosition(0);
        return -1;
      }
      if (StepListInput(context, list, kSlotInUse, buttons)) {
        list.step = kLoadStart;
      }
      break;
    }
    case kLoadStart:
      StartLoad(context);
      break;
    case kLoadApply: {
      const int read = host.ReadSaveStep(0);
      host.SetProgressCounter(host.ProgressCounter() + 1);
      if (read != 0) {
        ApplyLoadedFile(context, read);
      }
      break;
    }
    case kLoadLoaded:
      if (list.leave_timer != 0) {
        --list.leave_timer;
      }
      if (list.result == 1) {
        ++list.complete_timer;
      }
      if (LeavePressed(host) || list.complete_timer == kLeaveTicks) {
        if (list.result < 0) {
          Play(host, MenuSound::kLeave);
        }
        list.step = kLoadExiting;
      }
      break;
    case kLoadExiting:
      if (list.leave_timer != 0) {
        --list.leave_timer;
        break;
      }
      if (list.result == 1) {
        host.SetPlayTime(state.load_time_backup);
      } else {
        ClearSlotElements(context);
      }
      return list.result;
    default:
      break;
  }
  return 0;
}

// decomp: MENU7.PRG _selectLoadMemoryCard (src/MENU/MENU7.PRG/260.c:3063)
int UpdateLoadCard(Context& context, int init_port) {
  CardPicker& picker = context.state.load_card;
  if (const std::optional<int> shared =
          StepCardPicker(context, picker, init_port, UpdateLoadFiles)) {
    return *shared;
  }
  switch (picker.step) {
    case kCardStartEvents:
      if (AreAnimationsComplete(context.state)) {
        CheckCardStep(context, picker.port + kEventMaskIgnoreNoEvent);
        picker.step = kCardPollEvents;
      }
      break;
    case kCardPollEvents: {
      const int checked = CheckCardStep(context, 0);
      if (checked != kMaskedPending) {
        picker.step = checked < 0 ? kCardLeave : kCardScan;
      }
      break;
    }
    case kCardScan:
      ScanCard(context, picker,
               {.required = kSlotTemporary,
                .empty_text = kTextNoData,
                .update_list = UpdateLoadFiles});
      break;
    default:
      break;
  }
  return 0;
}

// decomp: MENU7.PRG _initGameOver (src/MENU/MENU7.PRG/260.c:3395)
// A load polled before it was requested is refused and frees the buffer.
int LoadGameOverAssetsStep(Context& context, bool begin) {
  Host& host = context.host;
  std::int32_t& step = context.state.game_over_load_step;
  if (begin) {
    host.BeginGameOverLoad();
    step = kAssetsLoading;
    return 0;
  }
  switch (step) {
    case kAssetsLoading: {
      const std::optional<bool> loaded = host.GameOverLoaded();
      if (!loaded) {
        host.Refuse("GAMEOVER.BIN was polled before its read was queued");
        step = kAssetsLoaded;
      } else if (*loaded) {
        UploadGameOverAssets(host);
        step = kAssetsLoaded;
      }
      return 0;
    }
    case kAssetsLoaded:
      host.FreeGameOverImage();
      return 1;
    default:
      return 0;
  }
}

// decomp: MENU7.PRG _setMenuItemClut (src/MENU/MENU7.PRG/260.c:3434)
void BlendGameOverTextClut(std::span<std::uint16_t, kClutColors> clut,
                           int factor,
                           std::span<const std::uint16_t, kClutColors> from,
                           std::span<const std::uint16_t, kClutColors> to) {
  clut[0] = 0;
  for (std::size_t i = 1; i < kClutColors; ++i) {
    const int red = from[i] & kRedMask;
    const int green = from[i] & kGreenMask;
    const int blue = from[i] & kBlueMask;
    const int red_step = ((to[i] & kRedMask) - red) * factor;
    const int green_step = ((to[i] & kGreenMask) - green) * factor;
    const int blue_step = ((to[i] & kBlueMask) - blue) * factor;
    const int blended =
        (((red << kBlendShift) + red_step) & (kRedMask << kBlendShift)) |
        (((green << kBlendShift) + green_step) & (kGreenMask << kBlendShift)) |
        (((blue << kBlendShift) + blue_step) & (kBlueMask << kBlendShift));
    clut[i] = static_cast<std::uint16_t>((blended >> kBlendShift) | kOpaqueBit);
  }
}

// decomp: MENU7.PRG _displayGameOverScreen (src/MENU/MENU7.PRG/260.c:3475)
int UpdateGameOverChoices(Context& context, bool init) {
  Host& host = context.host;
  GameOverChoices& choices = context.state.game_over;
  if (init) {
    choices.color1 = 0;
    choices.color2 = 0;
    choices.color3 = 0;
    choices.blend1 = 0;
    choices.blend2 = 0;
    choices.option = 0;
    return 0;
  }
  if (choices.color1 < 0) {
    return FadeOutGameOver(host, choices);
  }
  if (choices.color1 < kChoiceBright) {
    choices.color1 += kChoiceFadeStep;
  } else if (GameOverInput(host, choices)) {
    return 0;
  }
  BlendGameOverTextClut(choices.clut[0], choices.blend1, kPlainText, kGlowText);
  BlendGameOverTextClut(choices.clut[1], choices.blend2, kPlainText, kGlowText);
  const auto cluts =
      std::bit_cast<std::array<std::uint16_t, kClutColors * 2>>(choices.clut);
  host.UploadImage(kChoiceClutPosition, cluts, kChoiceClutSize);
  DrawChoices(host.OverlayQueue(), choices);
  return 0;
}

// decomp: MENU7.PRG vs_menu7_gameOver (src/MENU/MENU7.PRG/260.c:3603)
int UpdateGameOver(Context& context, std::uint8_t& step) {
  State& state = context.state;
  Host& host = context.host;
  switch (step) {
    case kGameOverInit:
      host.RetireTexturePages(kRetiredPageFirst, 1, kRetiredPageOwner);
      host.SetFieldRendering(false);
      BackUpSettings(host, state.game_over_settings);
      state.game_over_difficulty = host.Flags().difficulty;
      LoadGameOverAssetsStep(context, true);
      step = kGameOverLoadAssets;
      break;
    case kGameOverLoadAssets:
      if (LoadGameOverAssetsStep(context, false) == 0) {
        break;
      }
      step = kGameOverChoices;
      UpdateGameOverChoices(context, true);
      [[fallthrough]];
    case kGameOverChoices:
      GameOverChosen(context, step);
      break;
    case kGameOverOpenFiles:
      host.StepMenuResources(1);
      ResetElements(context);
      host.StoreImage(kGameOverClutPosition, state.game_over_clut,
                      kGameOverClutSize);
      host.LoadClutColors(state.game_over_clut, kGameOverClutRow, 0);
      step = kGameOverFilesWait;
      break;
    case kGameOverFilesWait:
      if (host.StepMenuResources(0) != 0) {
        host.UpdateDataLoadMenu(2);
        step = kGameOverSelectFile;
        DrawBackground(host);
        DrawFrame(context, host.FrameBuffer());
      }
      break;
    case kGameOverSelectFile:
      GameOverFileMenu(context, step);
      break;
    case kGameOverRestartIntro:
      if (host.LoadIntroSnapshotStep(0) != 0) {
        RestoreSettings(host, state.game_over_settings);
        host.SetDifficulty(
            static_cast<std::uint8_t>(state.game_over_difficulty));
        host.FreeMusicSlot(kGameOverMusicSlot);
        host.SetMonoOutput(host.MonoSound());
        host.SetGameOver(false);
        host.EnterBattle();
      }
      break;
    case kGameOverToTitle:
      if (state.game_over_delay != 0) {
        --state.game_over_delay;
      } else {
        host.FreeMusicSlot(kGameOverMusicSlot);
        host.ResetToTitle();
      }
      break;
    default:
      break;
  }
  host.SetGameOver(true);
  return 0;
}

// decomp: MENU7.PRG _promptYesNo (src/MENU/MENU7.PRG/260.c:3727)
int UpdateContainerConfirmation(Context& context, int init) {
  State& state = context.state;
  if (init != 0) {
    SlideInContainerRow(context, 0);
    state.container_step = kContainerNo;
    state.container_option = (init - 1) & 1;
    state.container_cancel_with_menu = static_cast<std::uint8_t>(init >> 2);
    return 0;
  }
  switch (state.container_step) {
    case kContainerNo:
      SlideInContainerRow(context, 1);
      state.container_step = kContainerWait;
      break;
    case kContainerWait:
      if (context.host.RowsIdle()) {
        state.container_step = kContainerInput;
      }
      break;
    case kContainerInput:
      ContainerInput(context);
      break;
    case kContainerReturn:
      if (context.host.RowsIdle()) {
        return state.container_option;
      }
      break;
    default:
      break;
  }
  return 0;
}

// decomp: MENU7.PRG _getContainerIndexOffset (src/MENU/MENU7.PRG/260.c:3804)
// A section past the capacity table stops at its end; the original read
// past it.
std::size_t ContainerIndexSection(int section) noexcept {
  const auto sections = std::min(static_cast<std::size_t>(std::max(section, 0)),
                                 kContainerCapacities.size());
  std::size_t offset = 0;
  for (std::size_t i = 0; i < sections; ++i) {
    offset += kContainerCapacities[i];
  }
  return offset;
}

// decomp: MENU7.PRG _copyContainer (src/MENU/MENU7.PRG/260.c:3821)
// The renumbering lives on the stack; the original took it from the high
// end of the heap.
void CopySelectedContainerItems(ContainerData& target,
                                const ContainerData& source,
                                const ContainerIndices& to_copy) {
  const FlatIndices remap = TargetIndices(std::bit_cast<FlatIndices>(to_copy));
  target = {};
  CopySection(target.weapons, source.weapons, to_copy.weapons,
              [&remap](Items::InventoryWeapon& weapon) {
                weapon.blade = static_cast<std::uint8_t>(
                    Lookup(remap, kBladeIndexBase, weapon.blade));
                weapon.grip = static_cast<std::uint8_t>(
                    Lookup(remap, kGripIndexBase, weapon.grip));
                RemapGems(remap, weapon.gems);
              });
  CopySection(target.blades, source.blades, to_copy.blades,
              [&remap](Items::InventoryBlade& blade) {
                if (blade.assembled_weapon_index != 0) {
                  blade.assembled_weapon_index = static_cast<std::uint8_t>(
                      Lookup(remap, 0, blade.assembled_weapon_index));
                }
              });
  CopySection(target.grips, source.grips, to_copy.grips,
              [&remap](Items::InventoryGrip& grip) {
                if (grip.assembled_weapon_index != 0) {
                  grip.assembled_weapon_index =
                      Lookup(remap, 0, grip.assembled_weapon_index);
                }
              });
  CopySection(target.shields, source.shields, to_copy.shields,
              [&remap](Items::InventoryShield& shield) {
                RemapGems(remap, shield.gems);
              });
  CopySection(target.armor, source.armor, to_copy.armor,
              [](Items::InventoryArmor& /*armor*/) {});
  CopySection(target.gems, source.gems, to_copy.gems,
              [&remap](Items::InventoryGem& gem) {
                const std::uint16_t item = gem.set_item_index;
                if ((item & kShieldLinkBit) != 0) {
                  gem.set_item_index = static_cast<std::uint16_t>(
                      Lookup(remap, kShieldIndexBase, item - kShieldLinkBit) |
                      kShieldLinkBit);
                } else if (item != 0) {
                  gem.set_item_index = Lookup(remap, 0, item);
                }
              });
  CopySection(target.misc, source.misc, to_copy.misc,
              [](Items::InventoryMisc& /*item*/) {});
}

// decomp: MENU7.PRG _drawPlayTime (src/MENU/MENU7.PRG/260.c:4157)
void DrawPlayTime(Context& context) {
  Host& host = context.host;
  std::array<char, kPlayTimeBytes>& text = context.state.play_time;
  const std::uint32_t time = host.PlayTime();
  std::array<std::uint8_t, kPlayTimeFields> fields{};
  for (std::size_t i = 0; i < kPlayTimeFields; ++i) {
    fields[i] =
        static_cast<std::uint8_t>(time >> (static_cast<int>(i) * kByteShift));
  }
  if (fields[kHoursField] >= kHoursCap) {
    fields = {};
    host.DrawCompactText("1", kPlayTimeHundredPosition);
  } else {
    const std::uint32_t scaled =
        (static_cast<std::uint32_t>(fields[0]) << kFrameScaleShift) +
        host.InterfaceRandomBits();
    fields[0] = static_cast<std::uint8_t>(
        ((scaled * kFrameScale) >> kFrameScaleShift) / kFrameDivisor);
  }
  for (std::size_t i = 0; i < kPlayTimeFields; ++i) {
    const std::int32_t digits = Core::Math::SplitDecimalDigit(fields[i]);
    const std::size_t tens = kLastTensColumn - (i * kPlayTimeFieldPitch);
    text[tens] = static_cast<char>('0' + (digits >> kNibbleShift));
    text[tens + 1] = static_cast<char>('0' + (digits & kNibbleMask));
  }
  host.DrawCompactText(std::string_view(text.data(), kPlayTimeBytes - 1),
                       kPlayTimePosition);
  host.DrawCompactText("PLAY    TIME", kPlayTimeLabelPosition);
}

// decomp: MENU7.PRG vs_menu7_dataMenu (src/MENU/MENU7.PRG/260.c:4187)
int UpdateDataMenu(Context& context, std::uint8_t& step) {
  Host& host = context.host;
  if (step < kDataClose) {
    DrawPlayTime(context);
  }
  switch (step) {
    case kDataOpen:
      host.StepMenuResources(1);
      step = kDataLoadWait;
      break;
    case kDataLoadWait:
      if (!host.RowsIdle() || host.StepMenuResources(0) == 0) {
        break;
      }
      [[fallthrough]];
    case kDataRows:
      CreateDataRows(context, step);
      break;
    case kDataSelect:
      DataMenuSelect(context, step);
      break;
    case kDataSaveOpen:
      if (host.RowsIdle()) {
        host.UpdateDataSaveMenu(1);
        step = kDataSave;
      }
      break;
    case kDataSave:
      if (host.UpdateDataSaveMenu(0) != 0) {
        step = kDataRows;
      }
      DrawFrame(context, host.FrameBuffer());
      break;
    case kDataLoadOpen:
      if (host.RowsIdle()) {
        host.UpdateDataLoadMenu(1);
        step = kDataLoad;
      }
      break;
    case kDataLoad:
      DataMenuLoad(context, step);
      break;
    case kDataClose:
    case kDataCloseToMenu:
      host.ClearCommandLabel();
      host.DismissInformationBox();
      host.SetBackgroundFade(0);
      step = step == kDataClose ? kDataFade : kDataFadeToMenu;
      break;
    case kDataFade:
    case kDataFadeToMenu:
      if (host.BackgroundFadeStep() != 0) {
        break;
      }
      host.ReleaseMenuResources();
      if (step == kDataFadeToMenu) {
        host.SetCurrentMenu(kDataMenuReturnMenu);
      }
      step = kDataClosed;
      return 1;
    default:
      break;
  }
  return 0;
}

// decomp: TITLE.PRG _findCurrentSave (src/TITLE/TITLE.PRG/22C.c:1427)
int FindCurrentSaveStep(TitleContext& context, bool init) {
  if (init) {
    context.state.title_save_search = {};
    return 0;
  }
  return StepSaveSearch(context.host, context.state.title_save_search, nullptr);
}

// decomp: TITLE.PRG _initSaveScreen (src/TITLE/TITLE.PRG/22C.c:3074)
void InitializeDisplay(TitleHost& host) {
  host.SetFrameBuffer(0);
  host.PutDisplay(kScreenWidth, 0);
  host.ClearScreen();
  host.SetDisplayMask(true);
}

// decomp: TITLE.PRG _saveScreenSwapBuf (src/TITLE/TITLE.PRG/22C.c:3093)
void SwapBuffers(TitleHost& host) {
  const auto buffer = static_cast<std::uint8_t>(1 - host.FrameBuffer());
  host.SetFrameBuffer(buffer);
  const int offset = buffer * kScreenWidth;
  host.PutDisplay(kScreenWidth - offset, offset);
}

// decomp: TITLE.PRG _saveScreenConfirmation (src/TITLE/TITLE.PRG/22C.c:3226)
// An option whose text index lies past the table is refused and left out.
int UpdateClearSaveReminder(TitleContext& context, bool init) {
  State& state = context.state;
  TitleHost& host = context.host;
  if (init) {
    state.reminder_confirm = 1;
    state.reminder_step = kReminderYes;
    return 0;
  }
  switch (state.reminder_step) {
    case kReminderYes:
    case kReminderNo: {
      const int option = state.reminder_step;
      if (const std::optional<std::uint16_t> offset =
              IndexedOffset(host, kIndexYesOption + state.reminder_step)) {
        host.InitializeElement(
            option + 1,
            PackXy(kScreenWidth, (option * kPromptPitch) + kChoiceRowTop),
            PackXy(kPromptWidth, kPromptHeight), *offset);
        host.AnimateElement(option + 1, ElementState::kAnimateX,
                            kChoiceRowTarget);
      }
      ++state.reminder_step;
      break;
    }
    case kReminderWait:
      if (host.AnimationsComplete()) {
        state.reminder_step = kReminderInput;
      }
      break;
    case kReminderInput:
      ReminderInput(host, state);
      break;
    case kReminderReturn:
      if (host.AnimationsComplete()) {
        return state.reminder_confirm;
      }
      break;
    default:
      break;
  }
  return 0;
}

// decomp: TITLE.PRG _saveScreenExitScreen (src/TITLE/TITLE.PRG/22C.c:3299)
void DrawExitFade(TitleHost& host) {
  const int hidden = (1 - host.FrameBuffer()) * kScreenWidth;
  DimmedSprite(host, {.position = PackXy(kExitRightX, 0),
                      .uv_clut = 0,
                      .size = kExitRightSize,
                      .page = ExitPage(hidden + kExitRightX)});
  DimmedSprite(host, {.position = 0,
                      .uv_clut = 0,
                      .size = kExitLeftSize,
                      .page = ExitPage(hidden)});
  std::array<std::uint32_t, kTilePacketWords> tile{
      0, kTileSemiCode | kFadeColorStep, 0, kFullScreen};
  Submit(host, tile);
}

// MENU7.PRG's initialised statics this State holds; the tables are
// constants here (PORTING_WATCHLIST.md section 1).
void ResetStatics(State& state) {
  state.text_arrow = 0;
  state.container_cursor = 0;
  state.play_time = kInitialPlayTime;
}
}  // namespace VS::Menu::Files
