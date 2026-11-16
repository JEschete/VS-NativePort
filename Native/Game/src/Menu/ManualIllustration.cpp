// Hand-maintained (tools/port/owned.csv).
#include "Menu/ManualIllustration.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>

#include "Core/Math/ScalarMath.hpp"

namespace VS::Menu::manual {
namespace {
using Core::Math::WrappingAdd;
using Core::Math::WrappingMultiply;
using Core::Math::WrappingSubtract;
using Core::Math::WrapSigned16;

// Help text pages scroll by one 13-pixel text line.
constexpr std::int32_t kLineHeight = 13;
// The content panel's left edge and the y of its first text line.
constexpr std::int32_t kContentLeft = 13;
constexpr std::int32_t kContentTop = 55;
// Illustration rows outside y 39-184 are skipped.
constexpr std::int32_t kSpriteVisibleTop = 39;
constexpr std::uint32_t kSpriteVisibleRows = 146;
// A diagram line is drawn when an end lies above y 186.
constexpr std::int16_t kLineVisibleBottom = 186;
// 4-bit tiles are 16 texels wide, 8-bit tiles 8.
constexpr std::int16_t kFourBitColors = 16;
constexpr std::int32_t kFourBitTileTexels = 16;
constexpr std::int32_t kEightBitTileTexels = 8;
constexpr int kFourBitTexelShift = 4;
constexpr int kEightBitTexelShift = 3;
constexpr std::int32_t kAtlasColumns = 16;
constexpr std::int32_t kAtlasRowTiles = 48;
constexpr int kAtlasRowShift = 4;
// Atlas tiles sit on texture pages 27-31 (x 704-1023 at y 256); 8-bit
// pages add the colour-depth bit.
constexpr std::int32_t kAtlasPages = 3;
constexpr std::uint16_t kAtlasFirstPage = 0x1D;
constexpr std::uint16_t kEightBitPage = 0x80;
// The palette's packed CLUT word: x / 16 in bits 0-5, y in bits 6-14.
constexpr std::int32_t kClutColumnBase = 0x30;
constexpr int kClutRowShift = 6;
// A diagram-line record's fields after its animation header.
enum DiagramLineField : std::uint8_t {
  kLineX0 = kRecordHeaderHalfwords,
  kLineY0,
  kLineX1,
  kLineY1,
  kLineRedGreen,
  kLineBlue,
};
// AnimationState_t's halfwords.
enum AnimationField : std::uint8_t {
  kAnimationEnabled,
  kAnimationFrameMask,
  kAnimationFrameDuration,
  kAnimationRepeat,
  kAnimationCurrentFrame,
  kAnimationTimer,
  kAnimationHidden,
};
constexpr unsigned kHalfwordBits = 16;
constexpr int kByteBits = 8;
constexpr std::uint32_t kByteMask = 0xFF;
constexpr std::size_t kLineCountWord = 2;
constexpr std::int32_t kSectionWordBytes = 4;
constexpr std::int32_t kHeaderBytes = 16;

[[nodiscard]] std::int32_t ReadWord(std::span<const std::int16_t> halfwords,
                                    std::size_t word) noexcept {
  const auto low = static_cast<std::uint16_t>(halfwords[word * 2]);
  const auto high = static_cast<std::uint16_t>(halfwords[(word * 2) + 1]);
  return static_cast<std::int32_t>(
      static_cast<std::uint32_t>(low) |
      (static_cast<std::uint32_t>(high) << kHalfwordBits));
}

// The tile cells a record reads: whole 16-row bands times whole columns.
[[nodiscard]] std::int64_t IllustrationCells(std::int16_t width,
                                             std::int16_t height,
                                             std::int32_t stride) noexcept {
  if (width <= 0 || height <= 0) {
    return 0;
  }
  const std::int64_t rows = (height + kTileHeight - 1) / kTileHeight;
  const std::int64_t columns = (width + stride - 1) / stride;
  return rows * columns;
}

// The record's animation, advanced: true when it hides the content.
bool AdvanceRecordAnimation(std::span<std::int16_t> record) noexcept {
  if (record[kAnimationEnabled] == 0) {
    return false;
  }
  const std::span<std::int16_t, kAnimationHalfwords> header =
      record.first<kAnimationHalfwords>();
  VisibilityAnimation animation = LoadAnimation(header);
  const std::int16_t hidden = AdvanceVisibilityAnimation(animation);
  StoreAnimation(animation, header);
  return hidden != 0;
}

[[nodiscard]] IllustrationTile BuildTile(std::int16_t atlas_index,
                                         bool four_bit) noexcept {
  const std::int32_t column_group = atlas_index / kAtlasColumns;
  const std::int32_t column = atlas_index - (column_group * kAtlasColumns);
  auto page = static_cast<std::uint16_t>((column_group % kAtlasPages) +
                                         kAtlasFirstPage);
  const int texel_shift = four_bit ? kFourBitTexelShift : kEightBitTexelShift;
  if (!four_bit) {
    page |= kEightBitPage;
  }
  const std::int32_t row = atlas_index / kAtlasRowTiles;
  IllustrationTile tile;
  tile.u = static_cast<std::uint8_t>(static_cast<std::uint32_t>(column)
                                     << static_cast<unsigned>(texel_shift));
  tile.v = static_cast<std::uint8_t>(static_cast<std::uint32_t>(row)
                                     << static_cast<unsigned>(kAtlasRowShift));
  tile.texture_page = page;
  return tile;
}
}  // namespace

Render::ScreenRect SpriteTileRect(std::int32_t index) noexcept {
  const std::int32_t row = index / kTilesPerAtlasRow;
  const std::int32_t column = index - (row * kTilesPerAtlasRow);
  return {
      .x = static_cast<std::int16_t>((column * kTileVramWidth) + kTileAtlasX),
      .y = static_cast<std::int16_t>((row * kTileHeight) + kTileAtlasY),
      .width = kTileVramWidth,
      .height = kTileHeight};
}

Render::ScreenRect ClutRect(std::int32_t index) noexcept {
  const std::int32_t row = index / kClutsPerRow;
  const std::int32_t column = index - (row * kClutsPerRow);
  return {.x = static_cast<std::int16_t>((column * kClutWidth) + kClutGridX),
          .y = static_cast<std::int16_t>(row + kClutGridY),
          .width = kClutWidth,
          .height = 1};
}

namespace {
template <typename RectOf>
UploadResult UploadGrid(std::span<const std::uint32_t> words,
                        std::int32_t count, std::int32_t limit,
                        std::size_t image_words, RectOf rect_of,
                        ImageUploader& uploader) {
  if (count <= 0) {
    return UploadResult::kUploaded;
  }
  if (count > limit) {
    return UploadResult::kTooManyImages;
  }
  const auto images = static_cast<std::size_t>(count);
  if (words.size() / image_words < images) {
    return UploadResult::kShortBuffer;
  }
  for (std::size_t index = 0; index < images; ++index) {
    uploader.Upload(rect_of(static_cast<std::int32_t>(index)),
                    words.subspan(index * image_words, image_words));
  }
  return UploadResult::kUploaded;
}
}  // namespace

// decomp: MENUE.PRG _copySprites (494.c:481).
UploadResult UploadSpriteTiles(std::span<const std::uint32_t> words,
                               std::int32_t count, ImageUploader& uploader) {
  return UploadGrid(words, count, kMaxSpriteTiles, kTileWords, SpriteTileRect,
                    uploader);
}

// decomp: MENUE.PRG _copyCluts (494.c:498).
UploadResult UploadCluts(std::span<const std::uint32_t> words,
                         std::int32_t count, ImageUploader& uploader) {
  return UploadGrid(words, count, kMaxCluts, kClutWords, ClutRect, uploader);
}

VisibilityAnimation LoadAnimation(
    std::span<const std::int16_t, kAnimationHalfwords> halfwords) noexcept {
  return {
      .enabled = halfwords[kAnimationEnabled],
      .frame_mask = static_cast<std::uint16_t>(halfwords[kAnimationFrameMask]),
      .frame_duration = halfwords[kAnimationFrameDuration],
      .repeat = halfwords[kAnimationRepeat],
      .current_frame = halfwords[kAnimationCurrentFrame],
      .timer = halfwords[kAnimationTimer],
      .hidden = halfwords[kAnimationHidden]};
}

void StoreAnimation(
    const VisibilityAnimation& animation,
    std::span<std::int16_t, kAnimationHalfwords> halfwords) noexcept {
  halfwords[kAnimationEnabled] = animation.enabled;
  halfwords[kAnimationFrameMask] =
      static_cast<std::int16_t>(animation.frame_mask);
  halfwords[kAnimationFrameDuration] = animation.frame_duration;
  halfwords[kAnimationRepeat] = animation.repeat;
  halfwords[kAnimationCurrentFrame] = animation.current_frame;
  halfwords[kAnimationTimer] = animation.timer;
  halfwords[kAnimationHidden] = animation.hidden;
}

// decomp: MENUE.PRG _getAnimationState (494.c:940).
std::int16_t AdvanceVisibilityAnimation(
    VisibilityAnimation& animation) noexcept {
  if (animation.current_frame >= kAnimationFrames) {
    return animation.hidden;
  }
  // srav uses the low five bits of the frame, so a negative frame shifts
  // by its value modulo 32.
  constexpr std::uint32_t kShiftMask = 31;
  const std::uint32_t shift =
      static_cast<std::uint32_t>(animation.current_frame) & kShiftMask;
  const std::uint32_t visible =
      (static_cast<std::uint32_t>(animation.frame_mask) >> shift) & 1U;
  animation.timer =
      WrapSigned16(static_cast<std::uint32_t>(animation.timer) + 1U);
  if (animation.timer >= animation.frame_duration) {
    animation.timer = 0;
    animation.current_frame =
        WrapSigned16(static_cast<std::uint32_t>(animation.current_frame) + 1U);
    if (animation.current_frame >= kAnimationFrames && animation.repeat != 0) {
      animation.current_frame = 0;
    }
  }
  animation.hidden = static_cast<std::int16_t>(visible ^ 1U);
  return animation.hidden;
}

std::uint16_t IllustrationClut(std::int16_t palette) noexcept {
  const std::int32_t row = palette / kClutsPerRow;
  const std::int32_t column = palette - (row * kClutsPerRow);
  const std::uint32_t clut_row = static_cast<std::uint32_t>(row + kClutGridY)
                                 << static_cast<unsigned>(kClutRowShift);
  return static_cast<std::uint16_t>(
      static_cast<std::uint32_t>(column + kClutColumnBase) | clut_row);
}

// decomp: MENUE.PRG _drawSprite (494.c:629).
DrawResult DrawIllustration(std::span<std::int16_t> record,
                            std::int32_t scroll_position,
                            IllustrationTarget& target) {
  if (record.size() < kIllustrationTiles) {
    return DrawResult::kRefused;
  }
  const std::int16_t origin_x = record[kIllustrationX];
  const std::int16_t origin_y = record[kIllustrationY];
  const std::int16_t width = record[kIllustrationWidth];
  const std::int16_t height = record[kIllustrationHeight];
  const bool four_bit = record[kIllustrationColors] == kFourBitColors;
  const std::int32_t stride =
      four_bit ? kFourBitTileTexels : kEightBitTileTexels;
  const std::int64_t cells = IllustrationCells(width, height, stride);
  if (static_cast<std::int64_t>(record.size() - kIllustrationTiles) < cells) {
    return DrawResult::kRefused;
  }
  if (AdvanceRecordAnimation(record)) {
    return DrawResult::kHidden;
  }
  const std::uint16_t clut = IllustrationClut(record[kIllustrationClut]);
  const std::int32_t scroll_pixels =
      WrappingMultiply(scroll_position, kLineHeight);
  std::size_t cell = kIllustrationTiles;
  // The original's static inline _min (494.c:619) and _add_int (494.c:627)
  // survive only inlined here; std::min and WrappingAdd replace them.
  for (std::int32_t band = 0; band < height; band += kTileHeight) {
    const std::int32_t tile_height =
        std::min<std::int32_t>(height - band, kTileHeight);
    for (std::int32_t column = 0; column < width; ++cell, column += stride) {
      const std::int32_t tile_width = std::min(width - column, stride);
      const std::int32_t y = WrappingSubtract(
          WrappingAdd(origin_y, band + kContentTop), scroll_pixels);
      if (static_cast<std::uint32_t>(WrappingSubtract(y, kSpriteVisibleTop)) >=
          kSpriteVisibleRows) {
        continue;
      }
      IllustrationTile tile = BuildTile(record[cell], four_bit);
      tile.x = WrapSigned16(
          static_cast<std::uint32_t>(origin_x + column + kContentLeft));
      tile.y = static_cast<std::int16_t>(y);
      tile.clut = clut;
      tile.width = static_cast<std::int16_t>(tile_width);
      tile.height = static_cast<std::int16_t>(tile_height);
      target.DrawTile(tile);
    }
  }
  return DrawResult::kDrawn;
}

// decomp: MENUE.PRG _drawContentLines (494.c:883).
DrawResult DrawDiagramLines(std::span<std::int16_t> help_text,
                            std::int32_t scroll_position,
                            DiagramLineTarget& target) {
  if (help_text.size() < (kLineCountWord + 1) * 2) {
    return DrawResult::kRefused;
  }
  if (ReadWord(help_text, kLineCountWord) <= 0) {
    return DrawResult::kDisabled;
  }
  // Both section sizes are rounded towards zero to whole words; the byte
  // offset wraps like the original's address arithmetic.
  const std::int32_t section_words =
      WrappingAdd(ReadWord(help_text, 0) / kSectionWordBytes,
                  ReadWord(help_text, 1) / kSectionWordBytes);
  const std::int32_t count_offset = WrappingAdd(
      WrappingMultiply(section_words, kSectionWordBytes), kHeaderBytes);
  if (count_offset < 0) {
    return DrawResult::kRefused;
  }
  const auto count_index =
      static_cast<std::size_t>(count_offset) / sizeof(std::int16_t);
  if (count_index >= help_text.size()) {
    return DrawResult::kRefused;
  }
  const std::int16_t count = help_text[count_index];
  const std::span<std::int16_t> records = help_text.subspan(count_index + 1);
  if (count > 0 && records.size() / kDiagramLineHalfwords <
                       static_cast<std::size_t>(count)) {
    return DrawResult::kRefused;
  }
  const std::int32_t scroll_pixels = WrappingSubtract(
      WrappingMultiply(scroll_position, kLineHeight), kContentTop);
  for (std::int16_t index = 0; index < count; ++index) {
    const std::span<std::int16_t> record =
        records.subspan(static_cast<std::size_t>(index) * kDiagramLineHalfwords,
                        kDiagramLineHalfwords);
    const bool hidden = AdvanceRecordAnimation(record);
    const auto x_of = [&](std::size_t field) {
      return WrapSigned16(
          static_cast<std::uint32_t>(WrappingAdd(record[field], kContentLeft)));
    };
    const auto y_of = [&](std::size_t field) {
      return WrapSigned16(static_cast<std::uint32_t>(
          WrappingSubtract(record[field], scroll_pixels)));
    };
    DiagramLine line;
    line.x0 = x_of(kLineX0);
    line.y0 = y_of(kLineY0);
    line.x1 = x_of(kLineX1);
    line.y1 = y_of(kLineY1);
    const auto red_green = static_cast<std::uint16_t>(record[kLineRedGreen]);
    line.red = static_cast<std::uint8_t>(red_green & kByteMask);
    line.green = static_cast<std::uint8_t>(red_green >> kByteBits);
    line.blue = static_cast<std::uint8_t>(
        static_cast<std::uint16_t>(record[kLineBlue]) & kByteMask);
    if (line.y0 < kContentTop && line.y1 < kContentTop) {
      continue;
    }
    if ((line.y0 < kLineVisibleBottom || line.y1 < kLineVisibleBottom) &&
        !hidden) {
      target.DrawLine(line);
    }
  }
  return DrawResult::kDrawn;
}
}  // namespace VS::Menu::manual
