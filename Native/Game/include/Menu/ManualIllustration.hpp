#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstddef>
#include <cstdint>
#include <span>

#include "Render/DisplayControl.hpp"

// The manual's illustrations (MENUE.PRG 494.c:481-511, 619-713, 883-960):
// the tile and palette uploads of a help asset file, the animated atlas
// sprites and diagram lines of a help text page and the sixteen-frame
// visibility masks that blink them. The cores read bounded spans and hand
// typed upload and draw commands to a target, so they reach no frame
// scratch, primitive cursor or VRAM themselves (ManualIllustrationLive.cpp).
namespace VS::Menu::manual {
// The tile atlas: 48 columns of 4x16 VRAM tiles from (832, 256), each 32
// words of 4-bit or 8-bit texels.
inline constexpr std::int32_t kTileAtlasX = 832;
inline constexpr std::int32_t kTileAtlasY = 256;
inline constexpr std::int16_t kTileVramWidth = 4;
inline constexpr std::int16_t kTileHeight = 16;
inline constexpr std::int32_t kTilesPerAtlasRow = 48;
inline constexpr std::size_t kTileWords = 32;
// Sixteen rows of tiles reach the bottom of VRAM (y 511).
inline constexpr std::int32_t kMaxSpriteTiles = 768;

// The CLUT grid: 16 columns of 16-colour palettes from (768, 496), 8 words
// each.
inline constexpr std::int32_t kClutGridX = 768;
inline constexpr std::int32_t kClutGridY = 496;
inline constexpr std::int16_t kClutWidth = 16;
inline constexpr std::int32_t kClutsPerRow = 16;
inline constexpr std::size_t kClutWords = 8;
// Sixteen rows of palettes reach the bottom of VRAM (y 511).
inline constexpr std::int32_t kMaxCluts = 256;

class ImageUploader {
 public:
  virtual ~ImageUploader() = default;
  // Copies the pixels of `area` (its words, first to last) into VRAM.
  virtual void Upload(const Render::ScreenRect& area,
                      std::span<const std::uint32_t> pixels) = 0;
};

enum class UploadResult : std::uint8_t {
  kUploaded,
  // More images than the VRAM grid holds; nothing was uploaded.
  kTooManyImages,
  // The buffer ends before the last image; nothing was uploaded.
  kShortBuffer,
};

// The VRAM area of atlas tile `index` (0 to kMaxSpriteTiles - 1).
[[nodiscard]] Render::ScreenRect SpriteTileRect(std::int32_t index) noexcept;
// The VRAM area of palette `index` (0 to kMaxCluts - 1).
[[nodiscard]] Render::ScreenRect ClutRect(std::int32_t index) noexcept;

// decomp: MENUE.PRG copySprites (src/MENU/MENUE.PRG/494.c:485; was
// _copySprites). `count` tiles of kTileWords words each; a count of 0 or less
// uploads nothing.
UploadResult UploadSpriteTiles(std::span<const std::uint32_t> words,
                               std::int32_t count, ImageUploader& uploader);
// decomp: MENUE.PRG copyCluts (src/MENU/MENUE.PRG/494.c:502; was _copyCluts).
// `count` palettes of kClutWords words each; a count of 0 or less uploads
// nothing.
UploadResult UploadCluts(std::span<const std::uint32_t> words,
                         std::int32_t count, ImageUploader& uploader);

// A visibility animation: AnimationState (494.c:42), the first seven
// halfwords of every illustration and diagram-line record.
inline constexpr std::size_t kAnimationHalfwords = 7;
// Each record's animation header occupies eight halfwords.
inline constexpr std::size_t kRecordHeaderHalfwords = 8;
inline constexpr std::int16_t kAnimationFrames = 16;

struct VisibilityAnimation {
  // Nonzero when the record animates (read by the draw loops only).
  std::int16_t enabled = 0;
  // Bit n set: frame n is visible.
  std::uint16_t frame_mask = 0;
  std::int16_t frame_duration = 0;
  std::int16_t repeat = 0;
  std::int16_t current_frame = 0;
  std::int16_t timer = 0;
  // 1 while the content is hidden.
  std::int16_t hidden = 0;
};

[[nodiscard]] VisibilityAnimation LoadAnimation(
    std::span<const std::int16_t, kAnimationHalfwords> halfwords) noexcept;
void StoreAnimation(
    const VisibilityAnimation& animation,
    std::span<std::int16_t, kAnimationHalfwords> halfwords) noexcept;

// decomp: MENUE.PRG getAnimationState (src/MENU/MENUE.PRG/494.c:944; was
// _getAnimationState). Past the last frame the stored hidden value is returned
// unchanged; otherwise the current frame's mask bit decides it, and the frame
// advances once the timer reaches the frame duration, restarting after frame 15
// when the animation repeats.
std::int16_t AdvanceVisibilityAnimation(
    VisibilityAnimation& animation) noexcept;

// An illustration record: the animation header, then x, y, width, height,
// an unused tile count, the colour depth (16 for 4-bit tiles) and the packed
// CLUT index, then one atlas tile index per drawn cell, row by row.
enum IllustrationField : std::uint8_t {
  kIllustrationX = kRecordHeaderHalfwords,
  kIllustrationY,
  kIllustrationWidth,
  kIllustrationHeight,
  kIllustrationTileCount,
  kIllustrationColors,
  kIllustrationClut,
  kIllustrationTiles,
};

// One clipped atlas sprite, with the texture page its texels are on.
struct IllustrationTile {
  std::int16_t x = 0;
  std::int16_t y = 0;
  std::uint8_t u = 0;
  std::uint8_t v = 0;
  std::uint16_t clut = 0;
  std::int16_t width = 0;
  std::int16_t height = 0;
  std::uint16_t texture_page = 0;

  friend bool operator==(const IllustrationTile&,
                         const IllustrationTile&) = default;
};

class IllustrationTarget {
 public:
  virtual ~IllustrationTarget() = default;
  // Queues the raw-texture sprite, then its texture page in front of it.
  virtual void DrawTile(const IllustrationTile& tile) = 0;
};

enum class DrawResult : std::uint8_t {
  kDrawn,
  // The visibility animation hid the content.
  kHidden,
  // Disabled content (diagram lines whose flag is not positive).
  kDisabled,
  // The record or its tile indices run past the span; nothing was drawn
  // and no animation advanced.
  kRefused,
};

// The packed CLUT word of an illustration's palette index (16 per row from
// (768, 496)).
[[nodiscard]] std::uint16_t IllustrationClut(std::int16_t palette) noexcept;

// decomp: MENUE.PRG drawSprite (src/MENU/MENUE.PRG/494.c:633; was _drawSprite).
// Draws the record's tiles that fall in the content panel's rows 39-184 after
// scrolling by 13 pixels per line.
DrawResult DrawIllustration(std::span<std::int16_t> record,
                            std::int32_t scroll_position,
                            IllustrationTarget& target);

// A diagram-line record: the animation header, x0, y0, x1, y1 and the red,
// green and blue bytes (the fourth byte is padding).
inline constexpr std::size_t kDiagramLineHalfwords = 14;

struct DiagramLine {
  std::int16_t x0 = 0;
  std::int16_t y0 = 0;
  std::int16_t x1 = 0;
  std::int16_t y1 = 0;
  std::uint8_t red = 0;
  std::uint8_t green = 0;
  std::uint8_t blue = 0;

  friend bool operator==(const DiagramLine&, const DiagramLine&) = default;
};

class DiagramLineTarget {
 public:
  virtual ~DiagramLineTarget() = default;
  // Queues the flat line.
  virtual void DrawLine(const DiagramLine& line) = 0;
};

// decomp: MENUE.PRG drawContentLines (src/MENU/MENUE.PRG/494.c:887; was
// _drawContentLines). `help_text` is the help text file as halfwords: words 0
// and 1 are the text and illustration section sizes in bytes, word 2 enables
// the lines, and the line section follows the 16-byte header and both sections.
DrawResult DrawDiagramLines(std::span<std::int16_t> help_text,
                            std::int32_t scroll_position,
                            DiagramLineTarget& target);
}  // namespace VS::Menu::manual
