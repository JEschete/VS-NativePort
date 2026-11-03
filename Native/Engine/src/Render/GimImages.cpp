// Hand-maintained (tools/port/owned.csv).
// The event scripts' GIM images (BATTLE.PRG 5BF94.c:2347-4449): the file's
// tile tables, palettes and texture strips are checked against the file
// before any is used, and the four draw routines build their packets as
// words for a PacketQueue.
#include "Render/GimImages.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "Core/Math/ScalarMath.hpp"
#include "Render/DisplayControl.hpp"
#include "Render/Sprites.hpp"
#include "Render/UiPackets.hpp"

namespace VS::Render {
namespace {
// The CLUT strip and texture strips sit 512 texels right of page 24's x.
constexpr int kClutPageBase = 16;
constexpr int kStripPageBase = 18;
constexpr int kPageWidth = 64;
constexpr int kClutStripY = 511;
constexpr int kTextureStripY = 256;
constexpr int kLayerClutColors = 16;
constexpr std::size_t kLayerClutWords = 64;
// The 256-colour palette follows 64 words of layer CLUTs (5BF94.c:2541).
constexpr std::size_t kSharedPaletteOffset = 64;
constexpr std::size_t kPaletteColors = 256;
constexpr std::size_t kOwnedPaletteOffset = 320;
constexpr std::size_t kStripWords = 0x3FC0;
constexpr std::uint32_t kStripFullSize = 0xFF0040;
constexpr int kSharedClutX = 768;
constexpr int kSharedClutY = 227;
constexpr std::uint16_t kTransparentBlack = 0x8000;

constexpr std::uint32_t kDrawModeCommand = 0xE1000000U;
constexpr std::uint32_t kQuadWords = 11;
constexpr std::uint32_t kQuadCommand = 22;
constexpr std::uint32_t kTileCommand = 48;
constexpr std::uint32_t kCommandBitShift = 25;
constexpr std::uint32_t kLineCommand = 0x40000000U;
constexpr std::uint32_t kBlackTile = 0x60000000U;
constexpr std::uint32_t kFullScreenHeight = 0xF00000U;
constexpr std::uint32_t kTileSize = 64 | (15 << 16);
constexpr std::uint32_t kHalfTileSize = 32 | (15 << 16);
constexpr std::uint32_t kSinglePixelSize = 1 | (1 << 16);
constexpr std::uint32_t kSingleLine = 0x10000;
constexpr std::uint32_t kTileRowTexels = 0xF00;
// Bit 8 of a sprite's brightness word makes it semi-transparent.
constexpr int kOpaqueCompose = 384;
constexpr int kSemiTransparent = 256;
constexpr int kBlendRounding = 7;
constexpr std::uint32_t kFadeTextureMode = 0x120;
constexpr std::uint32_t kFadeWideSize = 0xF00100;
constexpr std::uint32_t kFadeNarrowSize = 0xF00040;
constexpr std::uint32_t kFadeNarrowX = 0x100;
constexpr int kOtherFramebufferX = 0x140;
constexpr int kWipeColumns = 5;
constexpr int kWipeRandomBits = 10;
constexpr int kWipeStartBits = 9;
constexpr int kWipeStepBits = 8;
constexpr int kWipeStepBias = 64;
constexpr int kWipeRestartAngle = 128;
constexpr int kWipeWidthBias = 320;
constexpr int kFixedShift = 12;
constexpr int kColumnShift = 7;
constexpr int kRowShift = 13;
constexpr int kDoubleTileHeight = 30;
constexpr int kSceneOrderEntry = 0x7FE;
constexpr int kOverlayOrderEntry = 6;

// getTPage and getClut fields (LibGpu.h).
constexpr std::uint32_t kTwoBitMask = 3U;
constexpr std::uint32_t kPageModeShift = 7;
constexpr std::uint32_t kPageBlendShift = 5;
constexpr std::uint32_t kPageRowBit = 0x100U;
constexpr std::uint32_t kPageRowShift = 4;
constexpr std::uint32_t kVramColumnMask = 0x3FFU;
constexpr std::uint32_t kPageColumnShift = 6;
constexpr std::uint32_t kPageHighRowBit = 0x200U;
constexpr std::uint32_t kPageHighRowShift = 2;
constexpr std::uint32_t kClutRowShift = 6;
constexpr std::uint32_t kClutColumnShift = 4;
constexpr std::uint32_t kClutColumnMask = 0x3FU;
// Packed points hold x (or u) in the low halfword and y above it; texture
// words hold v in their second byte.
constexpr std::uint32_t kHalfwordBits = 16;
constexpr std::uint32_t kLowHalfMask = 0xFFFFU;
constexpr std::uint32_t kByteBits = 8;
constexpr std::uint32_t kByteMask = 0xFFU;
// 15-bit colours: three 5-bit channels, widened to the top of a byte.
constexpr std::uint32_t kColorChannels = 3;
constexpr std::uint32_t kChannelBits = 5;
constexpr std::uint32_t kChannelMask = 0x1FU;
constexpr std::uint32_t kChannelWiden = 3;
constexpr std::uint32_t kBrightnessShift = 7;
// A tile's atlas cell: bits 0-1 its column, 2-6 its row and 8-10 its page.
constexpr std::uint32_t kCellColumnMask = 3U;
constexpr std::uint32_t kCellRowShift = 2;
constexpr std::uint32_t kCellRowMask = 0x1FU;
constexpr std::uint32_t kCellPageShift = 8;
constexpr std::uint32_t kCellPageMask = 7U;
constexpr std::uint32_t kTileTexelMask = 0x3FU;
// Byte-sized dimensions need at most 256 boundary coordinates per axis.
constexpr std::size_t kGridBoundaries = 256;
constexpr int kWipeSineShift = 3;
constexpr int kHalfTileWidth = 32;
constexpr std::size_t kStripBlockWords = 16;
// GP0 E3-E5, the draw area's top left, bottom right and offset: the 64-wide
// workspace page at y 256, or a 320x240 framebuffer.
constexpr std::uint32_t kWorkspaceAreaTop = 0xE3040000U;
constexpr std::uint32_t kWorkspaceAreaBottom = 0xE407FC00U;
constexpr std::uint32_t kWorkspaceAreaOffset = 0xE5080000U;
constexpr int kWorkspaceAreaRight = kGimTileWidth - 1;
constexpr std::uint32_t kFramebufferAreaTop = 0xE3000000U;
constexpr std::uint32_t kFramebufferAreaBottom = 0xE403BC00U;
constexpr std::uint32_t kFramebufferAreaOffset = 0xE5000000U;
constexpr int kFramebufferAreaRight = kGimScreenWidth - 1;

constexpr std::uint32_t Bits(int value) {
  return static_cast<std::uint32_t>(value);
}

// getTPage (LibGpu.h).
constexpr std::uint32_t TexturePage(int mode, int blend, int x, int y) {
  return ((Bits(mode) & kTwoBitMask) << kPageModeShift) |
         ((Bits(blend) & kTwoBitMask) << kPageBlendShift) |
         ((Bits(y) & kPageRowBit) >> kPageRowShift) |
         ((Bits(x) & kVramColumnMask) >> kPageColumnShift) |
         ((Bits(y) & kPageHighRowBit) << kPageHighRowShift);
}

// getClut (LibGpu.h).
constexpr std::uint32_t ClutId(int x, int y) {
  return (Bits(y) << kClutRowShift) |
         ((Bits(x) >> kClutColumnShift) & kClutColumnMask);
}

constexpr std::uint32_t Pack(int low, int high) {
  return Bits(low) | (Bits(high) << kHalfwordBits);
}

// Pack with x masked to its halfword, so a negative x leaves y intact.
constexpr std::uint32_t PackPoint(int x, int y) {
  return (Bits(x) & kLowHalfMask) | (Bits(y) << kHalfwordBits);
}

// A 15-bit colour as a packet colour scaled by brightness / 128. A channel
// past 255 spills into the next, as in the original.
constexpr std::uint32_t ScaledColor(std::uint32_t color,
                                    std::uint32_t brightness) {
  std::uint32_t packed = 0;
  for (std::uint32_t channel = 0; channel < kColorChannels; ++channel) {
    const std::uint32_t value =
        ((color >> (channel * kChannelBits)) & kChannelMask) << kChannelWiden;
    packed |= ((value * brightness) >> kBrightnessShift)
              << (channel * kByteBits);
  }
  return packed;
}

constexpr std::uint32_t CellColumn(std::uint32_t tile) {
  return tile & kCellColumnMask;
}

constexpr std::uint32_t CellRow(std::uint32_t tile) {
  return (tile >> kCellRowShift) & kCellRowMask;
}

// The x of a cell's texture page within the image's strips.
constexpr int CellPageX(std::uint32_t tile) {
  return static_cast<int>((tile >> kCellPageShift) & kCellPageMask) *
         kPageWidth;
}

// A cell's top-left texel: u in the low byte, v in the second.
constexpr std::uint32_t CellTexel(std::uint32_t tile) {
  return (CellColumn(tile) * kGimTileWidth) |
         ((CellRow(tile) * kGimTileHeight) << kByteBits);
}

// The x of the image's first texture page.
constexpr int StripX(const GimTexture& texture) {
  return (texture.first_page - kStripPageBase) * kPageWidth;
}

// A grid cell's tile-table index, computed at full width.
constexpr std::size_t GridIndex(int row, int columns, int column) {
  return (static_cast<std::size_t>(row) * static_cast<std::size_t>(columns)) +
         static_cast<std::size_t>(column);
}

void EmitSprite(PacketQueue& queue, const SpriteRequest& request,
                std::uint32_t texture) {
  SpritePacket packet = BuildSpritePacket(request, queue.ChainLink());
  packet[kSpriteTextureWord] = texture;
  queue.Emit(packet);
}

void EmitTile(PacketQueue& queue, std::uint32_t command, std::uint32_t position,
              std::uint32_t size) {
  InsertThreeWordPrimitive(
      queue, {.color_command = command, .position = position, .size = size});
}

[[nodiscard]] std::size_t TileCount(const GimLayer& layer) {
  return std::size_t{layer.columns} * layer.rows;
}

struct TileStyle {
  std::uint32_t brightness = kGimFullBrightness;
  // Render_gimBlendSelect: the framebuffer fade blends its tiles.
  std::uint32_t blend = 0;
};

// Where a layer's column and row boundaries fall on screen.
struct TileGrid {
  std::array<std::int16_t, kGridBoundaries> x{};
  std::array<std::int16_t, kGridBoundaries> y{};
  int columns = 0;
  int rows = 0;

  [[nodiscard]] int Left(int column) const {
    return x[static_cast<std::size_t>(column)];
  }
  [[nodiscard]] int Top(int row) const {
    return y[static_cast<std::size_t>(row)];
  }
};

TileGrid PlaceTileGrid(const GimLayer& layer, int scale) {
  TileGrid grid;
  grid.columns = layer.columns;
  grid.rows = layer.rows;
  const int origin_x = layer.placement.offset_x + (kGimScreenWidth / 2);
  for (int i = 0; i <= grid.columns; ++i) {
    grid.x[static_cast<std::size_t>(i)] = static_cast<std::int16_t>(
        (((i * 2 - grid.columns) * scale) >> kColumnShift) + origin_x);
  }
  const int origin_y = layer.placement.offset_y + (kGimScreenHeight / 2);
  for (int i = 0; i <= grid.rows; ++i) {
    grid.y[static_cast<std::size_t>(i)] = static_cast<std::int16_t>(
        (((i * 2 - grid.rows) * scale * kGimTileHeight) >> kRowShift) +
        origin_y);
  }
  return grid;
}

// The backdrop above, below, left and right of the grid.
void EmitBackdropBorder(PacketQueue& queue, const TileGrid& grid,
                        std::uint32_t background) {
  const int top = grid.Top(0);
  if (top > 0) {
    EmitTile(queue, background, 0, Pack(kGimScreenWidth, top));
  }
  const int bottom = grid.Top(grid.rows);
  if (bottom < kGimScreenHeight) {
    EmitTile(queue, background, Bits(bottom) << kHalfwordBits,
             Bits(kGimScreenWidth) |
                 (kFullScreenHeight - (Bits(bottom) << kHalfwordBits)));
  }
  const int left = grid.Left(0);
  if (left > 0) {
    EmitTile(queue, background, 0, Bits(left) | kFullScreenHeight);
  }
  const int right = grid.Left(grid.columns);
  if (right < kGimScreenWidth) {
    EmitTile(queue, background, Bits(right) & kLowHalfMask,
             Bits(kGimScreenWidth - right) | kFullScreenHeight);
  }
}

struct TileCorners {
  int left = 0;
  int top = 0;
  int right = 0;
  int bottom = 0;
};

// The colours every tile of one draw shares.
struct TileShading {
  std::uint32_t quad_color = 0;
  int sprite_brightness = 0;
  bool unit_scale = false;
};

// A tile as a 64x15 sprite at unit scale, else as a scaled textured quad.
void EmitTileImage(PacketQueue& queue, const GimLayer& layer,
                   const GimTexture& texture, std::uint32_t tile,
                   const TileCorners& corners, const TileShading& shading) {
  const std::uint32_t u = CellColumn(tile) * kGimTileWidth;
  const std::uint32_t v = (CellRow(tile) * kGimTileHeight) << kByteBits;
  const std::uint32_t page =
      TexturePage(static_cast<int>(layer.depth), 1,
                  StripX(texture) + CellPageX(tile), kTextureStripY);
  if (shading.unit_scale) {
    // The left edge is not masked, so a tile left of the screen sets the
    // position's high half, as in the original.
    EmitSprite(
        queue,
        {.draw_mode = page | kDrawModeCommand,
         .brightness_transparency = shading.sprite_brightness,
         .packed_position = static_cast<int>(Pack(corners.left, corners.top)),
         .packed_size = static_cast<int>(kTileSize)},
        u | v | texture.clut_word);
    return;
  }
  const std::uint32_t x0 = Bits(corners.left) & kLowHalfMask;
  std::uint32_t u1 = u + kGimTileWidth;
  // A right edge at texel 256 wraps back to 255.
  u1 -= u1 >> kByteBits;
  const std::array<std::uint32_t, kQuadWords> quad = {
      (queue.ChainLink() & kPacketLinkMask) |
          ((kQuadWords - 1) << kPacketLengthShift),
      page | kDrawModeCommand,
      shading.quad_color,
      x0 | (Bits(corners.top) << kHalfwordBits),
      u | v | texture.clut_word,
      Pack(corners.right, corners.top),
      u1 | v | (page << kHalfwordBits),
      x0 | (Bits(corners.bottom) << kHalfwordBits),
      u | (v + kTileRowTexels),
      Pack(corners.right, corners.bottom),
      u1 | (v + kTileRowTexels),
  };
  queue.Emit(quad);
}

// func_800CBBCC (5BF94.c:4056) with the brightness and blend its callers
// set: the backdrop's border, then each visible tile, linked in the
// original's order.
bool DrawTiles(PacketQueue& queue, const GimLayer& layer,
               const GimTexture& texture, const TileStyle& style) {
  if (TileCount(layer) > kGimTileCapacity) {
    return false;
  }
  const int scale = layer.placement.scale_and_order & kGimScaleMask;
  const TileGrid grid = PlaceTileGrid(layer, scale);
  const std::uint32_t alpha = style.brightness;
  const std::uint32_t color = layer.backdrop_color;
  std::uint32_t background = 0;
  if (color != 0) {
    background = ScaledColor(color, alpha) |
                 ((style.blend + kTileCommand) << kCommandBitShift);
    EmitBackdropBorder(queue, grid, background);
  }
  const TileShading shading{
      .quad_color = alpha | (alpha << kByteBits) | (alpha << kHalfwordBits) |
                    ((style.blend + kQuadCommand) << kCommandBitShift),
      .sprite_brightness = static_cast<int>(alpha | (style.blend << kByteBits)),
      .unit_scale = scale == kGimUnitScale};
  // An empty tile shows the backdrop, except a blended draw's transparent
  // black.
  const bool fill_empty =
      color != 0 && (color != kTransparentBlack || style.blend == 0);
  for (int j = 0; j < grid.rows; ++j) {
    const int top = grid.Top(j);
    const int bottom = grid.Top(j + 1);
    if (top >= kGimScreenHeight || bottom < 0) {
      continue;
    }
    for (int i = 0; i < grid.columns; ++i) {
      const TileCorners corners{.left = grid.Left(i),
                                .top = top,
                                .right = grid.Left(i + 1),
                                .bottom = bottom};
      if (corners.left >= kGimScreenWidth || corners.right < 0) {
        continue;
      }
      const std::uint32_t tile = layer.tiles[GridIndex(j, grid.columns, i)];
      if (tile != 0) {
        EmitTileImage(queue, layer, texture, tile, corners, shading);
      } else if (fill_empty) {
        EmitTile(
            queue, background, PackPoint(corners.left, corners.top),
            Pack(corners.right - corners.left, corners.bottom - corners.top));
      }
    }
  }
  return true;
}

// The page after the image's, once the file has set the page count.
void ReserveWorkspacePage(GimImageSet& set, GimHost& host) {
  if (set.phase == GimLoadPhase::kReading || !set.usable ||
      set.workspace_page.has_value()) {
    return;
  }
  const std::uint32_t page = std::uint32_t{set.first_page} + set.page_count;
  if (host.ReservePages(page, 1)) {
    set.workspace_page = static_cast<std::uint8_t>(page);
  }
}
}  // namespace

GimImageSet MakeGimImageSet(int image_id, int first_page) {
  GimImageSet set;
  set.image_id = static_cast<std::int16_t>(image_id);
  set.first_page = static_cast<std::uint8_t>(first_page);
  for (GimLayer& layer : set.layers) {
    layer.loading = true;
  }
  return set;
}

void ActivateGimLayer(GimImageSet& set, std::size_t layer, GimHost& host) {
  if (layer >= kGimLayerCount) {
    host.Refuse(GimRefusal::kLayerOutOfRange);
    return;
  }
  GimLayer& target = set.layers[layer];
  target.draw_enabled = true;
  if (target.routine == GimDrawRoutine::kScanlineWipe) {
    auto angle = static_cast<int>(host.RandomBits(kWipeStartBits));
    auto goal = static_cast<int>(host.RandomBits(kWipeRandomBits));
    for (std::uint16_t& width : target.wipe_widths) {
      width = static_cast<std::uint16_t>(
          (Core::Math::Sine(angle) >> kWipeSineShift) + kWipeWidthBias);
      const int step =
          static_cast<int>(host.RandomBits(kWipeStepBits)) - kWipeStepBias;
      const auto next = static_cast<int>(host.RandomBits(kWipeRandomBits));
      if (goal < angle) {
        angle -= step;
        if (goal >= angle) {
          goal = next;
        }
      } else {
        angle += step;
        if (angle >= goal) {
          goal = next;
        }
      }
      if (angle < 0) {
        angle = kWipeRestartAngle;
        goal = static_cast<int>(host.RandomBits(kWipeRandomBits));
      }
    }
  }
  if (target.routine == GimDrawRoutine::kCrossfade) {
    ReserveWorkspacePage(set, host);
  }
}

bool ConfigureGimLayer(GimImageSet& set, std::size_t layer,
                       const GimLayerSetup& setup, GimHost& host) {
  if (layer >= kGimLayerCount) {
    host.Refuse(GimRefusal::kLayerOutOfRange);
    return false;
  }
  if (static_cast<int>(setup.routine) >= kGimDrawRoutineCount) {
    host.Refuse(GimRefusal::kRoutineOutOfRange);
    return false;
  }
  set.layers[layer].routine = setup.routine;
  set.layers[layer].placement = setup.placement;
  ActivateGimLayer(set, layer, host);
  return true;
}

bool ReleaseGimImageSet(GimImageSet& set, GimHost& host) {
  if (set.phase != GimLoadPhase::kReady) {
    return false;
  }
  if (set.pages_reserved) {
    host.RetirePages(set.first_page, set.page_count);
    set.pages_reserved = false;
  }
  if (set.workspace_page.has_value()) {
    host.RetirePages(*set.workspace_page, 1);
    set.workspace_page.reset();
  }
  return true;
}

namespace {
// The image's tile tables, palettes and texture strips, checked against the
// file before any is used.
struct GimFileLayout {
  std::array<std::uint16_t, kGimLayerCount> palette_modes{};
  std::size_t palette = 0;
  std::size_t strips = 0;
  std::uint32_t strip_rows = 0;
  std::uint32_t page_count = 0;
};

// One layer's grid word, palette mode and tiles at `offset`.
std::optional<GimRefusal> ReadTileTable(std::span<const std::uint16_t> file,
                                        std::size_t& offset, GimLayer& layer,
                                        std::uint16_t& palette_mode) {
  if (file.size() < offset || file.size() - offset < kGimTileHeaderWords) {
    return GimRefusal::kFileTooShort;
  }
  const std::uint16_t grid = file[offset];
  const auto columns = static_cast<std::uint8_t>(grid & kByteMask);
  const auto rows = static_cast<std::uint8_t>(grid >> kByteBits);
  const std::size_t tiles = std::size_t{columns} * rows;
  if (tiles > kGimTileCapacity) {
    return GimRefusal::kTileTableTooLarge;
  }
  if (file.size() - offset - kGimTileHeaderWords < tiles) {
    return GimRefusal::kFileTooShort;
  }
  layer.columns = columns;
  layer.rows = rows;
  palette_mode = file[offset + 2];
  std::ranges::copy(file.subspan(offset + kGimTileHeaderWords, tiles),
                    layer.tiles.begin());
  offset += kGimTileHeaderWords + tiles;
  return std::nullopt;
}

// 5BF94.c:2459-2547: layer 0's table, then layer 1's when header word 3 is
// set and layer 2's when it is 2. A layer the file does not describe keeps
// an empty table and palette mode 0 (the original read uninitialized heap).
std::optional<GimRefusal> ReadLayout(GimImageSet& set,
                                     std::span<const std::uint16_t> file,
                                     GimFileLayout& layout) {
  std::size_t offset = 0;
  if (auto refusal =
          ReadTileTable(file, offset, set.layers[0], layout.palette_modes[0])) {
    return refusal;
  }
  const std::uint16_t extra_layers = file[3];
  if (extra_layers != 0) {
    if (auto refusal = ReadTileTable(file, offset, set.layers[1],
                                     layout.palette_modes[1])) {
      return refusal;
    }
    if (extra_layers == 2) {
      if (auto refusal = ReadTileTable(file, offset, set.layers[2],
                                       layout.palette_modes[2])) {
        return refusal;
      }
    }
  }
  layout.palette = (offset + 1) & ~std::size_t{1};
  const bool owned_palette =
      std::ranges::find(layout.palette_modes, 2) != layout.palette_modes.end();
  const std::size_t palette_end = layout.palette + kOwnedPaletteOffset +
                                  (owned_palette ? kPaletteColors : 0);
  if (palette_end > file.size()) {
    return GimRefusal::kFileTooShort;
  }
  const std::uint16_t strip_word = file[1];
  layout.strip_rows = strip_word & kByteMask;
  layout.page_count =
      (layout.strip_rows + kGimTileRowsPerPage - 1) / kGimTileRowsPerPage;
  if (layout.page_count > kGimMaxPageCount) {
    return GimRefusal::kTooManyPages;
  }
  layout.strips =
      layout.palette +
      (static_cast<std::size_t>(strip_word >> kByteBits) * kStripBlockWords);
  const std::size_t full = layout.strip_rows / kGimTileRowsPerPage;
  const std::size_t partial = layout.strip_rows % kGimTileRowsPerPage;
  const std::size_t strip_end = layout.strips + (full * kStripWords) +
                                (partial * kGimTileHeight * kGimTileWidth);
  if (strip_end > file.size()) {
    return GimRefusal::kFileTooShort;
  }
  return std::nullopt;
}

void UploadArea(GimHost& host, std::span<const std::uint16_t> file,
                std::size_t offset, std::uint32_t position,
                std::uint32_t size) {
  const std::optional<ScreenRect> area = ImageRectangle(
      static_cast<std::int32_t>(position), static_cast<std::int32_t>(size));
  if (area.has_value()) {
    const std::size_t words = static_cast<std::size_t>(area->width) *
                              static_cast<std::size_t>(area->height);
    host.UploadImage(*area, file.subspan(offset, words));
  }
}

// 5BF94.c:2523-2580 for a checked layout.
void UploadImage(GimImageSet& set, std::span<const std::uint16_t> file,
                 const GimFileLayout& layout, GimHost& host) {
  int strip_x = ((set.first_page - kClutPageBase) * kPageWidth);
  const std::uint32_t clut_position = Pack(strip_x, kClutStripY);
  UploadArea(host, file, layout.palette, clut_position,
             kLayerClutWords | kSingleLine);
  for (std::size_t j = 0; j < kGimLayerCount; ++j) {
    GimLayer& layer = set.layers[j];
    switch (layout.palette_modes[j]) {
      case 0:
        layer.depth = GimTexelDepth::kFourBit;
        layer.backdrop_color = file[layout.palette + (j * kLayerClutColors)];
        break;
      case 1:
        layer.depth = GimTexelDepth::kEightBit;
        layer.backdrop_color = file[layout.palette + kSharedPaletteOffset];
        break;
      case 2:
        layer.depth = GimTexelDepth::kEightBit;
        layer.backdrop_color = file[layout.palette + kOwnedPaletteOffset];
        layer.owns_palette = true;
        UploadArea(host, file, layout.palette + kOwnedPaletteOffset,
                   clut_position, kPaletteColors | kSingleLine);
        break;
      default:
        break;
    }
  }
  host.LoadClutColors(
      file.subspan(layout.palette + kSharedPaletteOffset, kPaletteColors));
  set.page_count = static_cast<std::uint8_t>(layout.page_count);
  set.pages_reserved = host.ReservePages(set.first_page, set.page_count);
  if (!set.pages_reserved) {
    host.Refuse(GimRefusal::kPagesUnavailable);
  }
  std::size_t offset = layout.strips;
  for (auto rows = static_cast<int>(layout.strip_rows); rows > 0;
       rows -= kGimTileRowsPerPage) {
    const std::uint32_t size = rows >= kGimTileRowsPerPage
                                   ? kStripFullSize
                                   : Pack(kGimTileWidth, rows * kGimTileHeight);
    UploadArea(host, file, offset, Pack(strip_x, kTextureStripY), size);
    offset += kStripWords;
    strip_x += kPageWidth;
  }
}
}  // namespace

bool UpdateGimImageSet(GimImageSet& set, GimHost& host) {
  switch (set.phase) {
    case GimLoadPhase::kReading: {
      if (!host.FileReadFinished()) {
        return false;
      }
      host.ReleaseFileRead();
      const std::span<const std::uint16_t> file = host.FileWords();
      GimFileLayout layout;
      if (const auto refusal = ReadLayout(set, file, layout)) {
        host.Refuse(*refusal);
        set.usable = false;
      } else {
        UploadImage(set, file, layout, host);
        set.usable = true;
      }
      set.phase = GimLoadPhase::kUploaded;
      const bool crossfading =
          std::ranges::any_of(set.layers, [](const GimLayer& layer) {
            return layer.draw_enabled &&
                   layer.routine == GimDrawRoutine::kCrossfade;
          });
      if (crossfading) {
        ReserveWorkspacePage(set, host);
      }
      return false;
    }
    case GimLoadPhase::kUploaded:
      host.FreeFile();
      for (GimLayer& layer : set.layers) {
        layer.loading = false;
      }
      set.phase = GimLoadPhase::kReady;
      return false;
    case GimLoadPhase::kReady:
      return true;
  }
  return false;
}

bool DrawGimTiles(PacketQueue& queue, const GimLayer& layer,
                  const GimTexture& texture) {
  return DrawTiles(queue, layer, texture,
                   {.brightness = layer.placement.brightness, .blend = 0});
}

bool DrawGimFramebufferFade(PacketQueue& queue, const GimLayer& layer,
                            const GimTexture& texture) {
  const int brightness = layer.placement.brightness;
  if (brightness == kGimFullBrightness) {
    return DrawGimTiles(queue, layer, texture);
  }
  const bool drawn = DrawTiles(queue, layer, texture,
                               {.brightness = Bits(brightness), .blend = 1});
  const std::uint32_t other_x =
      texture.framebuffer == 0 ? Bits(kOtherFramebufferX) : 0U;
  // Above full brightness the remainder is negative and its bit 8 makes the
  // sprites semi-transparent, as in the original.
  const int remainder = kGimFullBrightness - brightness;
  EmitSprite(queue,
             {.draw_mode = (other_x >> kPageColumnShift) | kFadeTextureMode |
                           kDrawModeCommand,
              .brightness_transparency = remainder,
              .packed_position = 0,
              .packed_size = static_cast<int>(kFadeWideSize)},
             0);
  EmitSprite(queue,
             {.draw_mode = ((other_x + kFadeNarrowX) >> kPageColumnShift) |
                           kFadeTextureMode | kDrawModeCommand,
              .brightness_transparency = remainder,
              .packed_position = static_cast<int>(kFadeNarrowX),
              .packed_size = static_cast<int>(kFadeNarrowSize)},
             0);
  return drawn;
}

namespace {
// Each line's visible span [left, right) for a wipe at `scale`.
struct WipeSpans {
  std::array<std::int16_t, kGimScanlineCount> left{};
  std::array<std::int16_t, kGimScanlineCount> right{};
};

WipeSpans PlaceWipe(const GimLayer& layer, int scale) {
  // Brightness picks the side the wipe runs from.
  const bool from_right = layer.placement.brightness != 0;
  WipeSpans spans;
  for (std::size_t y = 0; y < kGimScanlineCount; ++y) {
    const int width = layer.wipe_widths[y];
    if (scale < kGimUnitScale) {
      const int column = (width * scale) >> kFixedShift;
      spans.left[y] =
          static_cast<std::int16_t>(from_right ? kGimScreenWidth - column : 0);
      spans.right[y] =
          static_cast<std::int16_t>(from_right ? kGimScreenWidth : column);
    } else {
      const int column = (width * (scale - kGimUnitScale)) >> kFixedShift;
      spans.left[y] = static_cast<std::int16_t>(from_right ? 0 : column);
      spans.right[y] = static_cast<std::int16_t>(
          from_right ? kGimScreenWidth - column : kGimScreenWidth);
    }
  }
  return spans;
}

// One tile column's part [x, end) of line y.
struct WipeSpan {
  int column = 0;
  int x = 0;
  int end = 0;
  int y = 0;
};

void EmitWipeSpan(PacketQueue& queue, const GimLayer& layer,
                  const GimTexture& texture, WipeSpan span) {
  const std::uint32_t tile =
      layer
          .tiles[GridIndex(span.y / kGimTileHeight, kWipeColumns, span.column)];
  if (tile == 0) {
    if (layer.backdrop_color != 0) {
      EmitTile(
          queue,
          ScaledColor(layer.backdrop_color, kGimFullBrightness) | kLineCommand,
          PackPoint(span.x, span.y), Pack(span.end - 1, span.y));
    }
    return;
  }
  const std::uint32_t mode =
      TexturePage(static_cast<int>(layer.depth), 1,
                  StripX(texture) +
                      (static_cast<int>(tile >> kCellPageShift) * kPageWidth),
                  kTextureStripY) |
      kDrawModeCommand;
  const std::uint32_t line_texel = Bits(span.y % kGimTileHeight) << kByteBits;
  // An odd start draws one texel, then the rest from the even x.
  for (bool odd = true; odd;) {
    odd = (span.x & 1) != 0;
    const std::uint32_t size =
        odd ? kSinglePixelSize : Bits(span.end - span.x) | kSingleLine;
    const std::uint32_t texel =
        (CellTexel(tile) + (Bits(span.x) & kTileTexelMask) + line_texel) |
        texture.clut_word;
    EmitSprite(queue,
               {.draw_mode = mode,
                .brightness_transparency = kGimFullBrightness,
                .packed_position = static_cast<int>(Pack(span.x, span.y)),
                .packed_size = static_cast<int>(size)},
               texel);
    if (odd) {
      ++span.x;
    }
  }
}
}  // namespace

bool DrawGimScanlineWipe(PacketQueue& queue, const GimLayer& layer,
                         const GimTexture& texture) {
  const int scale = layer.placement.scale_and_order & kGimScaleMask;
  if (scale == kGimUnitScale) {
    return DrawTiles(queue, layer, texture,
                     {.brightness = kGimFullBrightness, .blend = 0});
  }
  const WipeSpans spans = PlaceWipe(layer, scale);
  for (int y = 0; y < kGimScreenHeight; ++y) {
    const auto line = static_cast<std::size_t>(y);
    for (int column = 0; column < kWipeColumns; ++column) {
      const int column_left = column * kGimTileWidth;
      const int column_right = column_left + kGimTileWidth;
      const int x = spans.left[line];
      const int end = spans.right[line];
      if (x >= column_right || end <= column_left) {
        continue;
      }
      EmitWipeSpan(queue, layer, texture,
                   {.column = column,
                    .x = std::max(x, column_left),
                    .end = std::min(end, column_right),
                    .y = y});
    }
  }
  return true;
}

bool SetGimWorkspaceDrawArea(PacketQueue& queue, int page_x) {
  return InsertThreeWordPrimitive(
      queue,
      {.color_command = Bits(page_x) | kWorkspaceAreaTop,
       .position = Bits(page_x + kWorkspaceAreaRight) | kWorkspaceAreaBottom,
       .size = Bits(page_x) | kWorkspaceAreaOffset});
}

bool SetGimFramebufferDrawArea(PacketQueue& queue, int page_x) {
  return InsertThreeWordPrimitive(
      queue, {.color_command = Bits(page_x) | kFramebufferAreaTop,
              .position =
                  Bits(page_x + kFramebufferAreaRight) | kFramebufferAreaBottom,
              .size = Bits(page_x) | kFramebufferAreaOffset});
}

namespace {
// What every column of a crossfade shares.
struct Crossfade {
  int framebuffer_x = 0;
  int workspace_x = 0;
  int center = 0;
  int top = 0;
  int height = 0;
  int inverse = 0;
  int blended = 0;
  std::uint32_t compose_mode = 0;
  std::uint32_t copy_mode = 0;
};

// The column's screen area into the workspace, then the shown frame's
// half-tiles blended over it from the bottom up.
void CopyFrameColumn(PacketQueue& queue, const Crossfade& fade, int column_x) {
  EmitSprite(
      queue,
      {.draw_mode = fade.compose_mode,
       .brightness_transparency = kOpaqueCompose,
       .packed_position = static_cast<int>(Pack(column_x, fade.top)),
       .packed_size = static_cast<int>(Pack(kGimTileWidth, fade.height))},
      0);
  SetGimFramebufferDrawArea(queue, fade.framebuffer_x);
  for (int y = fade.height - kGimTileHeight; y >= 0; y -= kGimTileHeight) {
    for (int x = kHalfTileWidth; x >= 0; x -= kHalfTileWidth) {
      const std::uint32_t texel = Bits(x) | (Bits(y) << kByteBits);
      EmitSprite(queue,
                 {.draw_mode = fade.copy_mode,
                  .brightness_transparency = kOpaqueCompose,
                  .packed_position = static_cast<int>(Pack(x, y)),
                  .packed_size = static_cast<int>(kHalfTileSize)},
                 texel);
      SetGimWorkspaceDrawArea(queue, fade.workspace_x);
      EmitSprite(queue,
                 {.draw_mode = fade.compose_mode,
                  .brightness_transparency = fade.blended,
                  .packed_position =
                      static_cast<int>(Pack(x + column_x, fade.top + y)),
                  .packed_size = static_cast<int>(kHalfTileSize)},
                 texel);
      SetGimFramebufferDrawArea(queue, fade.framebuffer_x);
    }
  }
}

// The column's tiles, drawn at the top of the framebuffer.
void DrawColumnTiles(PacketQueue& queue, const Crossfade& fade,
                     const GimLayer& layer, const GimTexture& texture,
                     int column) {
  for (int y = 0; y < layer.rows; ++y) {
    const std::uint32_t tile = layer.tiles[GridIndex(y, layer.columns, column)];
    if (tile == 0) {
      continue;
    }
    const int tile_y =
        (((y * kDoubleTileHeight) - fade.height) >> 1) + fade.center - fade.top;
    EmitSprite(
        queue,
        {.draw_mode =
             TexturePage(static_cast<int>(layer.depth), 0,
                         StripX(texture) + CellPageX(tile), kTextureStripY) |
             kDrawModeCommand,
         .brightness_transparency = kGimFullBrightness,
         .packed_position = static_cast<int>(Bits(tile_y) << kHalfwordBits),
         .packed_size = static_cast<int>(kTileSize)},
        CellTexel(tile) | texture.clut_word);
  }
}

// The composed column back onto the screen, blended by the brightness's
// inverse, then the drawn tiles' area cleared to black.
void BlendColumnBack(PacketQueue& queue, const Crossfade& fade, int column_x) {
  SetGimWorkspaceDrawArea(queue, fade.workspace_x);
  const std::uint32_t framebuffer_mode =
      TexturePage(2, 0, fade.framebuffer_x + column_x, 0) | kDrawModeCommand;
  for (int y = 0; y < fade.height; y += kGimTileHeight) {
    for (int x = 0; x < kGimTileWidth; x += kHalfTileWidth) {
      const std::uint32_t texel = Bits(x) | (Bits(fade.top + y) << kByteBits);
      EmitSprite(queue,
                 {.draw_mode = framebuffer_mode,
                  .brightness_transparency = fade.inverse,
                  .packed_position =
                      static_cast<int>(Pack(x + column_x, fade.top + y)),
                  .packed_size = static_cast<int>(kHalfTileSize)},
                 texel);
      SetGimFramebufferDrawArea(queue, fade.framebuffer_x);
      EmitSprite(queue,
                 {.draw_mode = framebuffer_mode,
                  .brightness_transparency = kGimFullBrightness,
                  .packed_position = static_cast<int>(Pack(x, y)),
                  .packed_size = static_cast<int>(kHalfTileSize)},
                 texel);
      SetGimWorkspaceDrawArea(queue, fade.workspace_x);
    }
  }
  EmitTile(queue, kBlackTile, 0, Pack(kGimTileWidth, fade.height));
  SetGimWorkspaceDrawArea(queue, fade.workspace_x);
}
}  // namespace

bool DrawGimCrossfade(PacketQueue& queue, const GimLayer& layer,
                      const GimTexture& texture) {
  const int brightness = layer.placement.brightness;
  if (brightness == kGimFullBrightness) {
    return DrawGimTiles(queue, layer, texture);
  }
  // The original read past the tile table for larger grids.
  if (TileCount(layer) > kGimTileCapacity) {
    return false;
  }
  Crossfade fade;
  fade.framebuffer_x = texture.framebuffer == 0 ? kGimScreenWidth : 0;
  fade.workspace_x =
      (texture.first_page + texture.page_count - kClutPageBase) * kPageWidth;
  fade.height = layer.rows * kGimTileHeight;
  fade.center = layer.placement.offset_y + (kGimScreenHeight / 2);
  fade.top = fade.center - (fade.height >> 1);
  fade.inverse = kGimFullBrightness - brightness + kBlendRounding;
  fade.blended = (brightness | kSemiTransparent) + kBlendRounding;
  fade.compose_mode =
      TexturePage(2, 1, fade.workspace_x, kTextureStripY) | kDrawModeCommand;
  fade.copy_mode =
      TexturePage(2, 2, fade.workspace_x, kTextureStripY) | kDrawModeCommand;
  for (int column = 0; column < layer.columns; ++column) {
    const int column_x = column * kGimTileWidth;
    CopyFrameColumn(queue, fade, column_x);
    DrawColumnTiles(queue, fade, layer, texture, column);
    BlendColumnBack(queue, fade, column_x);
  }
  return true;
}

GimOrderingEntry GimLayerOrderingEntry(const GimImageSet& set,
                                       std::size_t layer) {
  const auto index = static_cast<int>(layer);
  if ((set.layers.at(layer).placement.scale_and_order & kGimSceneOrderFlag) !=
      0) {
    return {.scene = true, .entry = kSceneOrderEntry - index};
  }
  return {.scene = false, .entry = kOverlayOrderEntry - (2 * index)};
}

std::uint32_t GimLayerClutWord(const GimImageSet& set, std::size_t layer) {
  const GimLayer& target = set.layers.at(layer);
  const int clut_x = (set.first_page - kClutPageBase) * kPageWidth;
  std::uint32_t clut = 0;
  if (target.depth == GimTexelDepth::kEightBit) {
    clut = target.owns_palette ? ClutId(clut_x, kClutStripY)
                               : ClutId(kSharedClutX, kSharedClutY);
  } else {
    clut = ClutId(clut_x + (static_cast<int>(layer) * kLayerClutColors),
                  kClutStripY);
  }
  return clut << kHalfwordBits;
}

bool DrawGimLayer(PacketQueue& queue, const GimImageSet& set, std::size_t layer,
                  int framebuffer) {
  if (layer >= kGimLayerCount || !set.usable) {
    return false;
  }
  const GimLayer& target = set.layers[layer];
  if (target.placement.brightness == 0 &&
      target.routine != GimDrawRoutine::kScanlineWipe) {
    return false;
  }
  const GimTexture texture{.clut_word = GimLayerClutWord(set, layer),
                           .first_page = set.first_page,
                           .page_count = set.page_count,
                           .framebuffer = framebuffer};
  switch (target.routine) {
    case GimDrawRoutine::kTiles:
      return DrawGimTiles(queue, target, texture);
    case GimDrawRoutine::kFramebufferFade:
      return DrawGimFramebufferFade(queue, target, texture);
    case GimDrawRoutine::kScanlineWipe:
      return DrawGimScanlineWipe(queue, target, texture);
    case GimDrawRoutine::kCrossfade:
      return DrawGimCrossfade(queue, target, texture);
  }
  return false;
}
}  // namespace VS::Render
