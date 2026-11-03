#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <memory>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Core/Math/ScalarMath.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Legacy/Psx.h"
#include "Render/GimImages.hpp"
#include "Render/GimImagesLive.hpp"
#include "Render/PaletteAnimationLive.hpp"
#include "Render/TexturePagesLive.hpp"
#include "Render/UiPacketsLive.hpp"

extern "C" {
#include "Content/DiscLayout.h"
#include "DiscQueue.h"
#include "Render/Display.h"
#include "Render/Frame.h"
#include "Render/Render.h"
#include "Render/UiPrimitives.h"
}

// Contracts of the #276 row 6 GIM slice: the event scripts' tiled images
// (BATTLE.PRG 5BF94.c:2347-4449, 4A0A8.c:609-905).
namespace {
namespace Render = VS::Render;
using Packet = std::vector<std::uint32_t>;

constexpr std::uint32_t kLink = 0x123456;
constexpr std::uint32_t kClut = 0x12340000;

class FakeQueue final : public Render::PacketQueue {
 public:
  std::vector<Packet> packets;

  [[nodiscard]] std::uint32_t ChainLink() const override { return kLink; }
  bool Emit(std::span<const std::uint32_t> packet) override {
    packets.emplace_back(packet.begin(), packet.end());
    return true;
  }
};

constexpr std::uint32_t Tag(std::size_t words) {
  return kLink | (static_cast<std::uint32_t>(words - 1) << 24);
}

constexpr std::uint32_t Position(int x, int y) {
  return static_cast<std::uint32_t>(static_cast<std::uint16_t>(x)) |
         (static_cast<std::uint32_t>(static_cast<std::uint16_t>(y)) << 16);
}

// vs_battle_setSprite (573B8.h): bits 0-7 brightness, bit 8 transparency.
Packet Sprite(std::uint32_t mode, std::uint32_t brightness,
              std::uint32_t position, std::uint32_t size,
              std::uint32_t texture) {
  const std::uint32_t level = brightness & 0xFF;
  const std::uint32_t code = 0x64 + ((brightness >> 7) & 2);
  return {Tag(6),   mode,    level * 0x010101 | (code << 24),
          position, texture, size};
}

// vs_battle_addTile (5BF94.c:4503).
Packet Tile(std::uint32_t color, std::uint32_t position, std::uint32_t size) {
  return {Tag(4), color, position, size};
}

// func_800CBBCC's grid (5BF94.c:4071-4079): the shifts floor.
std::int16_t Coordinate(int index, int count, int scale, int offset,
                        bool vertical) {
  const int value = (index * 2 - count) * scale * (vertical ? 15 : 1);
  const int divisor = vertical ? 8192 : 128;
  const int rounded = value / divisor - (value % divisor < 0 ? 1 : 0);
  return static_cast<std::int16_t>(rounded + offset + (vertical ? 120 : 160));
}

struct TileCase {
  int columns = 0;
  int rows = 0;
  int scale = ONE;
  int offset_x = 0;
  int offset_y = 0;
};

// Every tile numbered from 1, 8-bit texels, pages from 18.
Render::GimLayer NumberedLayer(const TileCase& c, int brightness) {
  Render::GimLayer layer;
  layer.columns = static_cast<std::uint8_t>(c.columns);
  layer.rows = static_cast<std::uint8_t>(c.rows);
  layer.depth = Render::GimTexelDepth::kEightBit;
  layer.placement = {.offset_x = static_cast<std::int16_t>(c.offset_x),
                     .offset_y = static_cast<std::int16_t>(c.offset_y),
                     .scale_and_order = static_cast<std::int16_t>(c.scale),
                     .brightness = static_cast<std::uint8_t>(brightness)};
  for (std::size_t i = 0; i < layer.tiles.size(); ++i) {
    layer.tiles[i] = static_cast<std::uint16_t>(i + 1);
  }
  return layer;
}

Render::GimTexture Texture(int framebuffer = 0) {
  return {.clut_word = kClut,
          .first_page = 18,
          .page_count = 2,
          .framebuffer = framebuffer};
}

// The tile packets func_800CBBCC builds for a numbered layer.
std::vector<Packet> ExpectedTiles(const TileCase& c, int brightness,
                                  int blend) {
  std::vector<Packet> packets;
  if (c.columns * c.rows > 250) {
    return packets;
  }
  const int scale = c.scale & 0x3FFF;
  for (int j = 0; j < c.rows; ++j) {
    const std::int16_t top = Coordinate(j, c.rows, scale, c.offset_y, true);
    const std::int16_t bottom =
        Coordinate(j + 1, c.rows, scale, c.offset_y, true);
    if (top >= 240 || bottom < 0) {
      continue;
    }
    for (int i = 0; i < c.columns; ++i) {
      const std::int16_t left =
          Coordinate(i, c.columns, scale, c.offset_x, false);
      const std::int16_t right =
          Coordinate(i + 1, c.columns, scale, c.offset_x, false);
      if (left >= 320 || right < 0) {
        continue;
      }
      const int tile = i + (j * c.columns) + 1;
      const auto u = static_cast<std::uint32_t>((tile * 64) & 0xC0);
      const auto v = static_cast<std::uint32_t>(((tile * 64) & 0x1F00) * 15);
      const auto page =
          static_cast<std::uint32_t>(getTPage(1, 1, (tile >> 2) & 0x1C0, 256));
      const auto mode = static_cast<std::uint32_t>(page | _get_mode(0, 0, 0));
      if (scale == ONE) {
        packets.push_back(
            Sprite(mode, static_cast<std::uint32_t>(brightness | (blend << 8)),
                   Position(left, top), 64 | (15 << 16), u | v | kClut));
        continue;
      }
      std::uint32_t u1 = u + 64;
      u1 -= u1 >> 8;
      const auto code = static_cast<std::uint32_t>(0x2C + (blend * 2));
      packets.push_back(
          {Tag(11), mode,
           (code << 24) | (static_cast<std::uint32_t>(brightness) * 0x010101),
           Position(left, top), u | v | kClut, Position(right, top),
           u1 | v | (page << 16), Position(left, bottom), u | (v + 0xF00),
           Position(right, bottom), u1 | (v + 0xF00)});
    }
  }
  return packets;
}

// func_800CC128's two framebuffer sprites (5BF94.c:4211-4216).
void AppendFadeSprites(std::vector<Packet>& packets, int brightness,
                       int framebuffer) {
  const std::uint32_t other = framebuffer == 0 ? 320 : 0;
  const auto remainder = static_cast<std::uint32_t>(128 - brightness);
  packets.push_back(
      Sprite(0xE1000120 | (other >> 6), remainder, 0, 256 | (240 << 16), 0));
  packets.push_back(Sprite(0xE1000120 | ((other + 256) >> 6), remainder, 256,
                           64 | (240 << 16), 0));
}

// --- Tiles ---------------------------------------------------------------

TEST_CASE("GIM tiles keep the original's fixed-point grid and packet words",
          "[render-gim]") {
  const std::array<TileCase, 8> cases = {{
      {5, 16, 4096, 0, 0},
      {5, 16, 2048, 0, 0},
      {5, 16, 4095, 0, 0},
      {5, 16, 4097, 0, 0},
      {5, 16, 16383, 0, 0},
      {2, 3, 4096, 17, -9},
      {2, 3, 2048, -17, 9},
      {5, 16, 4096 | 0x4000, 0, 0},
  }};
  for (const TileCase& c : cases) {
    CAPTURE(c.columns, c.rows, c.scale, c.offset_x, c.offset_y);
    FakeQueue queue;
    CHECK(Render::DrawGimTiles(queue, NumberedLayer(c, 128), Texture()));
    CHECK(queue.packets == ExpectedTiles(c, 128, 0));
  }
}

TEST_CASE("GIM scaled tiles clip offscreen rows and columns", "[render-gim]") {
  const std::array<std::pair<int, int>, 5> offsets = {
      {{0, 0}, {-200, -100}, {200, 100}, {5000, 5000}, {-5000, -5000}}};
  for (const auto& [x, y] : offsets) {
    CAPTURE(x, y);
    const TileCase c{5, 16, 8192, x, y};
    FakeQueue queue;
    CHECK(Render::DrawGimTiles(queue, NumberedLayer(c, 128), Texture()));
    CHECK(queue.packets == ExpectedTiles(c, 128, 0));
  }
}

TEST_CASE("GIM grids past the tile table draw nothing", "[render-gim]") {
  // Zero scale keeps every tile on screen, so the whole table is drawn.
  const std::array<std::pair<int, int>, 9> dimensions = {{{0, 0},
                                                          {255, 0},
                                                          {0, 255},
                                                          {250, 1},
                                                          {1, 250},
                                                          {25, 10},
                                                          {251, 1},
                                                          {1, 251},
                                                          {255, 255}}};
  for (const auto& [columns, rows] : dimensions) {
    CAPTURE(columns, rows);
    const TileCase c{columns, rows, 0, 0, 0};
    FakeQueue queue;
    CHECK(Render::DrawGimTiles(queue, NumberedLayer(c, 128), Texture()) ==
          (columns * rows <= 250));
    CHECK(queue.packets == ExpectedTiles(c, 128, 0));
  }
}

// A 2x2 grid at half scale spans x 128-192 and y 112-127; only tile 0 has an
// image.
Render::GimLayer BackdropLayer() {
  Render::GimLayer layer = NumberedLayer({2, 2, 2048, 0, 0}, 64);
  layer.tiles = {};
  layer.tiles[0] = 5;
  layer.backdrop_color = 0x1234;
  return layer;
}

// Brightness 64 halves each backdrop channel (0xA0, 0x88 and 0x20).
constexpr std::uint32_t kHalfBackdrop = 0x60104450;

TEST_CASE("GIM backdrops border the grid", "[render-gim]") {
  FakeQueue queue;
  REQUIRE(Render::DrawGimTiles(queue, BackdropLayer(), Texture()));
  REQUIRE(queue.packets.size() == 8);
  CHECK(queue.packets[0] == Tile(kHalfBackdrop, 0, 320 | (112 << 16)));
  CHECK(queue.packets[1] == Tile(kHalfBackdrop, 127 << 16, 320 | (113 << 16)));
  CHECK(queue.packets[2] == Tile(kHalfBackdrop, 0, 128 | (240 << 16)));
  CHECK(queue.packets[3] == Tile(kHalfBackdrop, 192, 128 | (240 << 16)));
}

TEST_CASE("GIM backdrops fill the grid's empty tiles", "[render-gim]") {
  FakeQueue queue;
  REQUIRE(Render::DrawGimTiles(queue, BackdropLayer(), Texture()));
  REQUIRE(queue.packets.size() == 8);
  CHECK(queue.packets[4].size() == 11);
  CHECK(queue.packets[5] ==
        Tile(kHalfBackdrop, Position(160, 112), 32 | (8 << 16)));
  CHECK(queue.packets[6] ==
        Tile(kHalfBackdrop, Position(128, 120), 32 | (7 << 16)));
  CHECK(queue.packets[7] ==
        Tile(kHalfBackdrop, Position(160, 120), 32 | (7 << 16)));
}

TEST_CASE("GIM transparent black fills empty tiles only when not blending",
          "[render-gim]") {
  Render::GimLayer layer = BackdropLayer();
  layer.backdrop_color = 0x8000;
  FakeQueue opaque;
  REQUIRE(Render::DrawGimTiles(opaque, layer, Texture()));
  CHECK(opaque.packets.size() == 8);
  CHECK(opaque.packets[0][1] == 0x60000000);
  FakeQueue blended;
  REQUIRE(Render::DrawGimFramebufferFade(blended, layer, Texture()));
  REQUIRE(blended.packets.size() == 7);
  CHECK(blended.packets[0][1] == 0x62000000);
  CHECK(blended.packets[4].size() == 11);
  CHECK(blended.packets[5].size() == 6);
}

TEST_CASE("GIM unit-scale sprites keep the unmasked left edge",
          "[render-gim]") {
  // Columns start at x -200, -136, -72, -8, 56 and 120.
  const TileCase c{5, 1, ONE, -200, 0};
  FakeQueue queue;
  REQUIRE(Render::DrawGimTiles(queue, NumberedLayer(c, 128), Texture()));
  REQUIRE(queue.packets.size() == 2);
  // The original OR'd the sign-extended x into the y half.
  CHECK(queue.packets[0][3] == 0xFFFFFFF8);
  CHECK(queue.packets[1][3] == Position(56, 112));
}

// --- Routines ------------------------------------------------------------

TEST_CASE("GIM framebuffer fades blend the tiles over the other framebuffer",
          "[render-gim]") {
  const int framebuffer = GENERATE(0, 1);
  const int scale = GENERATE(2048, 4096);
  const int brightness = GENERATE(0, 64, 128);
  CAPTURE(framebuffer, scale, brightness);
  const TileCase c{5, 16, scale, 0, 0};
  const bool fading = brightness != 128;
  std::vector<Packet> expected = ExpectedTiles(c, brightness, fading ? 1 : 0);
  if (fading) {
    AppendFadeSprites(expected, brightness, framebuffer);
  }
  FakeQueue queue;
  CHECK(Render::DrawGimFramebufferFade(queue, NumberedLayer(c, brightness),
                                       Texture(framebuffer)));
  CHECK(queue.packets == expected);
}

TEST_CASE("GIM wipes and crossfades fall back to the tiles", "[render-gim]") {
  const TileCase unit{5, 16, 4096, 0, 0};
  FakeQueue wipe;
  CHECK(Render::DrawGimScanlineWipe(wipe, NumberedLayer(unit, 32), Texture()));
  CHECK(wipe.packets == ExpectedTiles(unit, 128, 0));
  for (const int scale : {4096, 2048}) {
    const TileCase c{5, 16, scale, 0, 0};
    FakeQueue crossfade;
    CHECK(
        Render::DrawGimCrossfade(crossfade, NumberedLayer(c, 128), Texture()));
    CHECK(crossfade.packets == ExpectedTiles(c, 128, 0));
  }
}

// Lines 0 and 16 are 126 wide; tile 4 is page 1, atlas row 1, atlas column 2.
Render::GimLayer WipeLayer(int scale, int brightness) {
  Render::GimLayer layer;
  layer.columns = 5;
  layer.rows = 16;
  layer.depth = Render::GimTexelDepth::kEightBit;
  layer.backdrop_color = 0x7C1F;
  layer.placement.scale_and_order = static_cast<std::int16_t>(scale);
  layer.placement.brightness = static_cast<std::uint8_t>(brightness);
  layer.wipe_widths[0] = 126;
  layer.wipe_widths[16] = 126;
  layer.tiles[4] = 0x106;
  return layer;
}

std::uint32_t WipeTileMode() {
  return static_cast<std::uint32_t>(getTPage(1, 1, 64, 256) |
                                    _get_mode(0, 0, 0));
}

TEST_CASE("GIM scanline wipes split odd starts and draw empty tiles as lines",
          "[render-gim]") {
  // Half scale shows half of each width: 63 texels from the right.
  FakeQueue queue;
  REQUIRE(Render::DrawGimScanlineWipe(queue, WipeLayer(2048, 1), Texture()));
  REQUIRE(queue.packets.size() == 3);
  CHECK(queue.packets[0] == Sprite(WipeTileMode(), 128, Position(257, 0),
                                   1 | (1 << 16),
                                   (128 + 1) | (15 << 8) | kClut));
  CHECK(queue.packets[1] == Sprite(WipeTileMode(), 128, Position(258, 0),
                                   62 | (1 << 16),
                                   (128 + 2) | (15 << 8) | kClut));
  CHECK(queue.packets[2] ==
        Tile(0x40F800F8, Position(257, 16), Position(319, 16)));
}

TEST_CASE("GIM scanline wipes past unit scale recede from the left",
          "[render-gim]") {
  // Lines 0 and 16 show x 63-319 and the others all of it, one packet per
  // column.
  FakeQueue queue;
  REQUIRE(Render::DrawGimScanlineWipe(queue, WipeLayer(0x1800, 0), Texture()));
  REQUIRE(queue.packets.size() == std::size_t{240} * 5);
  CHECK(queue.packets[0] == Tile(0x40F800F8, Position(63, 0), Position(63, 0)));
  CHECK(queue.packets[4] == Sprite(WipeTileMode(), 128, Position(256, 0),
                                   64 | (1 << 16), 128 | (15 << 8) | kClut));
  CHECK(queue.packets[5] == Tile(0x40F800F8, Position(0, 1), Position(63, 1)));
}

TEST_CASE("GIM draw areas switch between the workspace and the framebuffer",
          "[render-gim]") {
  FakeQueue queue;
  REQUIRE(Render::SetGimWorkspaceDrawArea(queue, 0x100));
  REQUIRE(Render::SetGimFramebufferDrawArea(queue, 320));
  REQUIRE(queue.packets.size() == 2);
  CHECK(queue.packets[0] == Tile(0xE3040100, 0xE407FD3F, 0xE5080100));
  CHECK(queue.packets[1] == Tile(0xE3000140, 0xE403BE7F, 0xE5000140));
}

TEST_CASE("GIM crossfades compose each column in the workspace page",
          "[render-gim]") {
  Render::GimLayer layer;
  layer.columns = 1;
  layer.rows = 1;
  layer.depth = Render::GimTexelDepth::kEightBit;
  layer.placement.brightness = 64;
  layer.tiles[0] = 0x106;
  FakeQueue queue;
  REQUIRE(Render::DrawGimCrossfade(queue, layer, Texture()));

  // The workspace is page 20 (x 256); the other framebuffer is at x 320. The
  // column is 15 lines high from y 113.
  const auto page_mode = [](int blend) {
    return static_cast<std::uint32_t>(getTPage(2, blend, 256, 256) |
                                      _get_mode(0, 0, 0));
  };
  const Packet workspace = Tile(0xE3040100, 0xE407FD3F, 0xE5080100);
  const Packet framebuffer = Tile(0xE3000140, 0xE403BE7F, 0xE5000140);
  const auto screen_mode =
      static_cast<std::uint32_t>(getTPage(2, 0, 320, 0) | _get_mode(0, 0, 0));
  std::vector<Packet> expected;
  expected.push_back(
      Sprite(page_mode(1), 384, Position(0, 113), 64 | (15 << 16), 0));
  expected.push_back(framebuffer);
  for (const int x : {32, 0}) {
    const auto texel = static_cast<std::uint32_t>(x);
    expected.push_back(
        Sprite(page_mode(2), 384, Position(x, 0), 32 | (15 << 16), texel));
    expected.push_back(workspace);
    expected.push_back(Sprite(page_mode(1), (64 | 256) + 7, Position(x, 113),
                              32 | (15 << 16), texel));
    expected.push_back(framebuffer);
  }
  // The tile one line up from the column's top.
  expected.push_back(Sprite(
      static_cast<std::uint32_t>(getTPage(1, 0, 64, 256) | _get_mode(0, 0, 0)),
      128, 0xFFFF0000, 64 | (15 << 16), 128 | (15 << 8) | kClut));
  expected.push_back(workspace);
  for (const int x : {0, 32}) {
    const auto texel = static_cast<std::uint32_t>(x | (113 << 8));
    expected.push_back(Sprite(screen_mode, 128 - 64 + 7, Position(x, 113),
                              32 | (15 << 16), texel));
    expected.push_back(framebuffer);
    expected.push_back(
        Sprite(screen_mode, 128, Position(x, 0), 32 | (15 << 16), texel));
    expected.push_back(workspace);
  }
  expected.push_back(Tile(0x60000000, 0, 64 | (15 << 16)));
  expected.push_back(workspace);
  CHECK(queue.packets == expected);

  layer.columns = 251;
  FakeQueue refused;
  CHECK_FALSE(Render::DrawGimCrossfade(refused, layer, Texture()));
  CHECK(refused.packets.empty());
}

// --- Loading -------------------------------------------------------------

class FakeHost final : public Render::GimHost {
 public:
  struct Upload {
    Render::ScreenRect area;
    std::vector<std::uint16_t> pixels;

    bool operator==(const Upload&) const = default;
  };

  std::vector<std::uint16_t> file;
  bool read_finished = false;
  int read_releases = 0;
  int file_frees = 0;
  std::vector<Upload> uploads;
  std::vector<std::vector<std::uint16_t>> clut_loads;
  bool lend_pages = true;
  std::vector<std::pair<std::uint32_t, std::uint32_t>> reserved;
  std::vector<std::size_t> uploads_at_reserve;
  std::vector<std::pair<std::uint32_t, std::uint32_t>> retired;
  std::uint32_t random_value = 0;
  std::vector<int> random_requests;
  std::vector<Render::GimRefusal> refusals;

  [[nodiscard]] bool FileReadFinished() override { return read_finished; }
  void ReleaseFileRead() override { ++read_releases; }
  [[nodiscard]] std::span<const std::uint16_t> FileWords() override {
    return file;
  }
  void FreeFile() override { ++file_frees; }
  void UploadImage(const Render::ScreenRect& area,
                   std::span<const std::uint16_t> pixels) override {
    uploads.push_back({area, {pixels.begin(), pixels.end()}});
  }
  void LoadClutColors(std::span<const std::uint16_t> colors) override {
    clut_loads.emplace_back(colors.begin(), colors.end());
  }
  [[nodiscard]] bool ReservePages(std::uint32_t first,
                                  std::uint32_t count) override {
    reserved.emplace_back(first, count);
    uploads_at_reserve.push_back(uploads.size());
    return lend_pages;
  }
  void RetirePages(std::uint32_t first, std::uint32_t count) override {
    retired.emplace_back(first, count);
  }
  [[nodiscard]] std::uint32_t RandomBits(int bit_count) override {
    random_requests.push_back(bit_count);
    return random_value;
  }
  void Refuse(Render::GimRefusal refusal) override {
    refusals.push_back(refusal);
  }
};

struct LayerSpec {
  std::uint8_t columns = 0;
  std::uint8_t rows = 0;
  std::uint16_t palette_mode = 0;
};

constexpr std::size_t kPaletteWords = 576;
constexpr auto kStripBlocks = static_cast<std::uint8_t>(kPaletteWords / 16);
constexpr std::size_t kStripWords = 0x3FC0;

struct GimFile {
  std::vector<std::uint16_t> words;
  std::size_t palette = 0;
  std::size_t strips = 0;
};

// A file in func_800C8778's layout (5BF94.c:2459-2580): the tile tables, the
// layer, shared and owned palettes, then `strip_rows` tile rows of strips.
GimFile BuildFile(std::span<const LayerSpec> layers, std::uint8_t strip_rows) {
  GimFile file;
  for (std::size_t n = 0; n < layers.size(); ++n) {
    const LayerSpec& spec = layers[n];
    file.words.push_back(
        static_cast<std::uint16_t>(spec.columns | (spec.rows << 8)));
    file.words.push_back(
        n == 0 ? static_cast<std::uint16_t>(strip_rows | (kStripBlocks << 8))
               : 0);
    file.words.push_back(spec.palette_mode);
    file.words.push_back(n == 0 ? static_cast<std::uint16_t>(layers.size() - 1)
                                : 0);
    for (int t = 0; t < spec.columns * spec.rows; ++t) {
      file.words.push_back(static_cast<std::uint16_t>(0x100 + (n * 0x40) + t));
    }
  }
  if (file.words.size() % 2 != 0) {
    file.words.push_back(0xEEEE);
  }
  file.palette = file.words.size();
  for (std::size_t i = 0; i < kPaletteWords; ++i) {
    file.words.push_back(static_cast<std::uint16_t>(0x4000 + i));
  }
  file.strips = file.words.size();
  const std::size_t strip_words = (std::size_t{strip_rows} / 17 * kStripWords) +
                                  (std::size_t{strip_rows} % 17 * 15 * 64);
  for (std::size_t i = 0; i < strip_words; ++i) {
    file.words.push_back(static_cast<std::uint16_t>(0x2000 + (i & 0xFFF)));
  }
  return file;
}

std::vector<std::uint16_t> Words(const GimFile& file, std::size_t offset,
                                 std::size_t count) {
  const auto first = file.words.begin() + static_cast<std::ptrdiff_t>(offset);
  return {first, first + static_cast<std::ptrdiff_t>(count)};
}

TEST_CASE("GIM image sets start loading at full brightness and unit scale",
          "[render-gim]") {
  const Render::GimImageSet set = Render::MakeGimImageSet(7, 20);
  CHECK(set.image_id == 7);
  CHECK(set.first_page == 20);
  CHECK(set.phase == Render::GimLoadPhase::kReading);
  CHECK_FALSE(set.usable);
  for (const Render::GimLayer& layer : set.layers) {
    CHECK(layer.loading);
    CHECK_FALSE(layer.draw_enabled);
    CHECK(layer.placement == Render::GimPlacement{});
  }
}

constexpr std::array<LayerSpec, 1> kOneRow = {{{5, 1, 0}}};

// Three layers: 4-bit on the first CLUT, 8-bit on the shared palette and 8-bit
// with its own palette.
using ThreeLayerSpecs = std::array<LayerSpec, 3>;
constexpr ThreeLayerSpecs kThreeLayers = {{{2, 1, 0}, {1, 2, 1}, {1, 1, 2}}};

struct ThreeLayerLoad {
  GimFile file = BuildFile(kThreeLayers, 20);
  FakeHost host;
  Render::GimImageSet set = Render::MakeGimImageSet(0, 20);

  ThreeLayerLoad() { host.file = file.words; }
  // Finishes the read, which parses and uploads the file.
  bool Read() {
    host.read_finished = true;
    return Render::UpdateGimImageSet(set, host);
  }
};

TEST_CASE("GIM files wait for their read before parsing", "[render-gim]") {
  ThreeLayerLoad load;
  CHECK_FALSE(Render::UpdateGimImageSet(load.set, load.host));
  CHECK(load.host.read_releases == 0);
  CHECK(load.set.phase == Render::GimLoadPhase::kReading);

  CHECK_FALSE(load.Read());
  CHECK(load.host.refusals.empty());
  CHECK(load.host.read_releases == 1);
  CHECK(load.set.usable);
  CHECK(load.set.phase == Render::GimLoadPhase::kUploaded);
}

TEST_CASE("GIM files read 4-bit layers on the first CLUT", "[render-gim]") {
  ThreeLayerLoad load;
  CHECK_FALSE(load.Read());
  const Render::GimLayer& layer = load.set.layers[0];
  CHECK(layer.columns == 2);
  CHECK(layer.rows == 1);
  CHECK(layer.tiles[1] == 0x101);
  CHECK(layer.depth == Render::GimTexelDepth::kFourBit);
  CHECK(layer.backdrop_color == 0x4000);
}

TEST_CASE("GIM files read 8-bit layers on the shared palette", "[render-gim]") {
  ThreeLayerLoad load;
  CHECK_FALSE(load.Read());
  const Render::GimLayer& layer = load.set.layers[1];
  CHECK(layer.rows == 2);
  CHECK(layer.tiles[1] == 0x141);
  CHECK(layer.depth == Render::GimTexelDepth::kEightBit);
  CHECK(layer.backdrop_color == 0x4040);
  CHECK_FALSE(layer.owns_palette);
}

TEST_CASE("GIM files read 8-bit layers with their own palette",
          "[render-gim]") {
  ThreeLayerLoad load;
  CHECK_FALSE(load.Read());
  const Render::GimLayer& layer = load.set.layers[2];
  CHECK(layer.tiles[0] == 0x180);
  CHECK(layer.depth == Render::GimTexelDepth::kEightBit);
  CHECK(layer.backdrop_color == 0x4140);
  CHECK(layer.owns_palette);
}

TEST_CASE("GIM files upload their palettes, then lend pages to their strips",
          "[render-gim]") {
  ThreeLayerLoad load;
  // The tables take 6, 6 and 5 words, so the palettes start at 18.
  REQUIRE(load.file.palette == 18);
  CHECK_FALSE(load.Read());

  // The CLUT strip and owned palette at x (20 - 16) * 64, then the strips.
  const GimFile& file = load.file;
  const std::vector<FakeHost::Upload> uploads = {
      {{256, 511, 64, 1}, Words(file, 18, 64)},
      {{256, 511, 256, 1}, Words(file, 18 + 320, 256)},
      {{256, 256, 64, 255}, Words(file, file.strips, kStripWords)},
      {{320, 256, 64, 45},
       Words(file, file.strips + kStripWords, std::size_t{45} * 64)},
  };
  CHECK(load.host.uploads == uploads);
  CHECK(load.host.clut_loads ==
        std::vector<std::vector<std::uint16_t>>{Words(file, 18 + 64, 256)});
  CHECK(load.set.page_count == 2);
  CHECK(load.set.pages_reserved);
  CHECK(load.host.reserved ==
        std::vector<std::pair<std::uint32_t, std::uint32_t>>{{20, 2}});
  CHECK(load.host.uploads_at_reserve == std::vector<std::size_t>{2});
}

TEST_CASE("GIM files free once uploaded and are then ready", "[render-gim]") {
  ThreeLayerLoad load;
  CHECK_FALSE(load.Read());
  CHECK_FALSE(Render::UpdateGimImageSet(load.set, load.host));
  CHECK(load.host.file_frees == 1);
  CHECK(load.set.phase == Render::GimLoadPhase::kReady);
  for (const Render::GimLayer& layer : load.set.layers) {
    CHECK_FALSE(layer.loading);
  }
  CHECK(Render::UpdateGimImageSet(load.set, load.host));
  CHECK(load.host.file_frees == 1);
}

TEST_CASE("GIM files with one layer leave the others empty", "[render-gim]") {
  const std::array<LayerSpec, 1> layers = {{{5, 16, 0}}};
  const GimFile file = BuildFile(layers, 17);
  FakeHost host;
  host.file = file.words;
  host.read_finished = true;
  Render::GimImageSet set = Render::MakeGimImageSet(0, 25);
  CHECK_FALSE(Render::UpdateGimImageSet(set, host));
  CHECK(host.refusals.empty());
  CHECK(set.page_count == 1);
  CHECK(set.layers[1].columns == 0);
  CHECK(set.layers[2].columns == 0);
  // Each 4-bit layer's backdrop is its own CLUT's first colour.
  CHECK(set.layers[1].backdrop_color == 0x4010);
  CHECK(set.layers[2].backdrop_color == 0x4020);
  CHECK(host.uploads.size() == 2);
}

struct RefusedFile {
  std::vector<std::uint16_t> words;
  Render::GimRefusal refusal;
};

// Files whose header, tile tables or sizes do not fit func_800C8778's layout.
RefusedFile RefusedFileCase(std::size_t n) {
  const GimFile whole = BuildFile(kOneRow, 20);
  std::vector<std::uint16_t> wide = whole.words;
  wide[0] = 251 | (1 << 8);
  std::vector<std::uint16_t> many_pages = whole.words;
  many_pages[1] = static_cast<std::uint16_t>(120 | (kStripBlocks << 8));
  std::vector<std::uint16_t> owned = whole.words;
  owned[2] = 2;
  owned.resize(whole.palette + 320 + 255);
  const std::array<RefusedFile, 6> cases = {{
      {{}, Render::GimRefusal::kFileTooShort},
      {{0x0105, 0, 0}, Render::GimRefusal::kFileTooShort},
      {wide, Render::GimRefusal::kTileTableTooLarge},
      {many_pages, Render::GimRefusal::kTooManyPages},
      {Words(whole, 0, whole.words.size() - 1),
       Render::GimRefusal::kFileTooShort},
      {owned, Render::GimRefusal::kFileTooShort},
  }};
  return cases.at(n);
}

struct RefusedLoad {
  FakeHost host;
  Render::GimImageSet set = Render::MakeGimImageSet(0, 25);

  explicit RefusedLoad(std::vector<std::uint16_t> words) {
    host.file = std::move(words);
    host.read_finished = true;
  }
};

TEST_CASE("GIM files that do not fit their layout are refused before upload",
          "[render-gim]") {
  const auto n = static_cast<std::size_t>(GENERATE(0, 1, 2, 3, 4, 5));
  CAPTURE(n);
  const RefusedFile refused = RefusedFileCase(n);
  RefusedLoad load(refused.words);
  CHECK_FALSE(Render::UpdateGimImageSet(load.set, load.host));
  CHECK(load.host.refusals == std::vector<Render::GimRefusal>{refused.refusal});
  CHECK_FALSE(load.set.usable);
  CHECK(load.host.uploads.empty());
  CHECK(load.host.clut_loads.empty());
  CHECK(load.host.reserved.empty());
}

TEST_CASE("GIM refused files still let the scripts waiting on them continue",
          "[render-gim]") {
  const auto n = static_cast<std::size_t>(GENERATE(0, 1, 2, 3, 4, 5));
  CAPTURE(n);
  RefusedLoad load(RefusedFileCase(n).words);
  CHECK_FALSE(Render::UpdateGimImageSet(load.set, load.host));
  CHECK(load.set.phase == Render::GimLoadPhase::kUploaded);
  CHECK_FALSE(Render::UpdateGimImageSet(load.set, load.host));
  CHECK_FALSE(load.set.layers[0].loading);
  FakeQueue queue;
  CHECK_FALSE(Render::DrawGimLayer(queue, load.set, 0, 0));
  CHECK(queue.packets.empty());
  CHECK(Render::ReleaseGimImageSet(load.set, load.host));
  CHECK(load.host.retired.empty());
}

// A crossfade on layer 1 configured while a one-row file loads.
struct CrossfadeLoad {
  GimFile file = BuildFile(kOneRow, 20);
  FakeHost host;
  Render::GimImageSet set = Render::MakeGimImageSet(0, 25);

  CrossfadeLoad() { host.file = file.words; }
  bool Configure() {
    return Render::ConfigureGimLayer(
        set, 1,
        {.routine = Render::GimDrawRoutine::kCrossfade,
         .placement = {.brightness = 64}},
        host);
  }
  bool Read() {
    host.read_finished = true;
    return Render::UpdateGimImageSet(set, host);
  }
};

using PageRanges = std::vector<std::pair<std::uint32_t, std::uint32_t>>;

TEST_CASE("GIM crossfades configured while loading wait for the page count",
          "[render-gim]") {
  CrossfadeLoad load;
  REQUIRE(load.Configure());
  CHECK(load.set.layers[1].draw_enabled);
  CHECK(load.host.reserved.empty());
  CHECK_FALSE(Render::ReleaseGimImageSet(load.set, load.host));

  CHECK_FALSE(load.Read());
  CHECK(load.host.reserved == PageRanges{{25, 2}, {27, 1}});
  CHECK(load.set.workspace_page == std::optional<std::uint8_t>{27});
  // Reactivating keeps the one workspace page.
  Render::ActivateGimLayer(load.set, 1, load.host);
  CHECK(load.host.reserved.size() == 2);
}

TEST_CASE("GIM sets retire only the pages they were lent", "[render-gim]") {
  CrossfadeLoad load;
  REQUIRE(load.Configure());
  REQUIRE_FALSE(load.Read());
  CHECK_FALSE(Render::ReleaseGimImageSet(load.set, load.host));
  CHECK(load.host.retired.empty());
  CHECK_FALSE(Render::UpdateGimImageSet(load.set, load.host));
  CHECK(Render::ReleaseGimImageSet(load.set, load.host));
  CHECK(load.host.retired == PageRanges{{25, 2}, {27, 1}});
  CHECK(Render::ReleaseGimImageSet(load.set, load.host));
  CHECK(load.host.retired.size() == 2);
}

TEST_CASE("GIM pages the host does not lend are not retired", "[render-gim]") {
  FakeHost refusing;
  refusing.file = BuildFile(kOneRow, 20).words;
  refusing.read_finished = true;
  refusing.lend_pages = false;
  Render::GimImageSet unlent = Render::MakeGimImageSet(0, 25);
  CHECK_FALSE(Render::UpdateGimImageSet(unlent, refusing));
  CHECK(refusing.refusals ==
        std::vector<Render::GimRefusal>{Render::GimRefusal::kPagesUnavailable});
  // The strips are still uploaded, as in the original.
  CHECK(unlent.usable);
  CHECK(refusing.uploads.size() == 3);
  CHECK_FALSE(Render::UpdateGimImageSet(unlent, refusing));
  CHECK(Render::ReleaseGimImageSet(unlent, refusing));
  CHECK(refusing.retired.empty());
}

TEST_CASE("GIM scanline wipes walk their widths from the random stream",
          "[render-gim]") {
  FakeHost host;
  Render::GimImageSet set = Render::MakeGimImageSet(0, 25);
  REQUIRE(Render::ConfigureGimLayer(
      set, 2,
      {.routine = Render::GimDrawRoutine::kScanlineWipe, .placement = {}},
      host));
  // With every draw 0, the first step overshoots below 0 and restarts at
  // 128; each later step adds 64.
  const auto width = [](int angle) {
    return static_cast<std::uint16_t>((VS::Core::Math::Sine(angle) >> 3) + 320);
  };
  const std::array<std::uint16_t, 240>& widths = set.layers[2].wipe_widths;
  CHECK(widths[0] == width(0));
  for (int line = 1; line < 240; ++line) {
    CAPTURE(line);
    CHECK(widths[static_cast<std::size_t>(line)] ==
          width(128 + (64 * (line - 1))));
  }
  std::vector<int> requests = {9, 10, 8, 10, 10};
  for (int line = 1; line < 240; ++line) {
    requests.insert(requests.end(), {8, 10});
  }
  CHECK(host.random_requests == requests);
  CHECK(host.reserved.empty());
}

TEST_CASE("GIM layer setups past the three layers are refused",
          "[render-gim]") {
  FakeHost host;
  Render::GimImageSet set = Render::MakeGimImageSet(0, 25);
  CHECK_FALSE(Render::ConfigureGimLayer(set, 3, {}, host));
  Render::ActivateGimLayer(set, 3, host);
  CHECK(host.refusals ==
        std::vector<Render::GimRefusal>{Render::GimRefusal::kLayerOutOfRange,
                                        Render::GimRefusal::kLayerOutOfRange});
  for (const Render::GimLayer& layer : set.layers) {
    CHECK_FALSE(layer.draw_enabled);
  }
}

TEST_CASE("GIM layers pick their ordering table and CLUT", "[render-gim]") {
  Render::GimImageSet set = Render::MakeGimImageSet(0, 20);
  set.layers[1].placement.scale_and_order = 0x4000 | ONE;
  CHECK(Render::GimLayerOrderingEntry(set, 0) ==
        Render::GimOrderingEntry{.scene = false, .entry = 6});
  CHECK(Render::GimLayerOrderingEntry(set, 1) ==
        Render::GimOrderingEntry{.scene = true, .entry = 0x7FD});
  CHECK(Render::GimLayerOrderingEntry(set, 2) ==
        Render::GimOrderingEntry{.scene = false, .entry = 2});

  const auto clut = [](int x, int y) {
    return static_cast<std::uint32_t>(getClut(x, y)) << 16;
  };
  CHECK(Render::GimLayerClutWord(set, 1) == clut(256 + 16, 511));
  set.layers[1].depth = Render::GimTexelDepth::kEightBit;
  CHECK(Render::GimLayerClutWord(set, 1) == clut(768, 227));
  set.layers[1].owns_palette = true;
  CHECK(Render::GimLayerClutWord(set, 1) == clut(256, 511));
}

// Layer 0 is a full-brightness 5x16 numbered grid; the set is not yet usable.
Render::GimImageSet DrawableSet() {
  Render::GimImageSet set = Render::MakeGimImageSet(0, 18);
  set.layers[0] = NumberedLayer({5, 16, ONE, 0, 0}, 128);
  set.page_count = 2;
  return set;
}

TEST_CASE("GIM layers draw nothing until usable or past the three layers",
          "[render-gim]") {
  Render::GimImageSet set = DrawableSet();
  FakeQueue queue;
  CHECK_FALSE(Render::DrawGimLayer(queue, set, 0, 0));
  set.usable = true;
  CHECK_FALSE(Render::DrawGimLayer(queue, set, 3, 0));
  CHECK(queue.packets.empty());
}

TEST_CASE("GIM layers draw their routine with their CLUT", "[render-gim]") {
  Render::GimImageSet set = DrawableSet();
  set.usable = true;
  FakeQueue queue;
  CHECK(Render::DrawGimLayer(queue, set, 0, 0));
  REQUIRE(queue.packets.size() == 80);
  CHECK(queue.packets[0][4] ==
        ((64U & 0xC0U) | Render::GimLayerClutWord(set, 0)));
}

TEST_CASE("GIM dark layers draw only scanline wipes", "[render-gim]") {
  Render::GimImageSet set = DrawableSet();
  set.usable = true;
  set.layers[0].placement.brightness = 0;
  FakeQueue dark;
  CHECK_FALSE(Render::DrawGimLayer(dark, set, 0, 0));
  CHECK(dark.packets.empty());
  // A wipe at brightness 0 runs from the left.
  set.layers[0].routine = Render::GimDrawRoutine::kScanlineWipe;
  CHECK(Render::DrawGimLayer(dark, set, 0, 0));
  CHECK(dark.packets.size() == 80);
}

// --- Live binding --------------------------------------------------------

struct LiveGimFixture {
  struct alignas(VS::Core::Memory::kHeapUnitBytes) Arena {
    std::array<std::byte, VS::Core::Memory::kHeapUnitBytes * 4096> bytes{};
  };
  std::unique_ptr<Arena> arena = std::make_unique<Arena>();
  VS::Core::Memory::ScopedLiveGameHeap heap{arena->bytes};
  Render::TexturePageTable saved_pages = Render::LiveTexturePages();
  decltype(std::to_array(Content_discRequestSlots)) saved_slots =
      std::to_array(Content_discRequestSlots);
  Content_DiscRequestCounts saved_counts = Content_discRequestCounts;
  Render::ClutRowColors saved_clut = Render::LivePaletteAnimation().colors[3];
  Render_FrameScratch saved_frame = Render_frameScratch;
  void* saved_buffer = Render_legacyPrimitiveBuffers[0];
  void* saved_table = Render_legacyOrderingTables[0];
  int saved_framebuffer = Render_frameBuf;
  RECT clut_strip{};
  RECT texture_strip{};
  std::vector<u_short> saved_clut_strip = std::vector<u_short>(64);
  std::vector<u_short> saved_texture_strip =
      std::vector<u_short>(std::size_t{64} * 15);
  std::vector<u_long> storage = std::vector<u_long>(0x20000 / 4, 0xDEADBEEF);
  std::vector<u_long> ordering = std::vector<u_long>(0x2088 / 4);

  LiveGimFixture() {
    Render::ForgetLiveGimImages();
    Render::InitializeLiveTexturePages();
    setRECT(&clut_strip, 576, 511, 64, 1);
    setRECT(&texture_strip, 576, 256, 64, 15);
    StoreImage(&clut_strip, reinterpret_cast<u_long*>(saved_clut_strip.data()));
    StoreImage(&texture_strip,
               reinterpret_cast<u_long*>(saved_texture_strip.data()));
    Render_frameBuf = 0;
    Render_legacyPrimitiveBuffers[0] = storage.data();
    Render_legacyOrderingTables[0] = ordering.data();
    Render_frameScratch.primitiveCursor = storage.data();
    Render_frameScratch.orderingTable = ordering.data() + 18;
    Render_frameScratch.overlayOrderingTable = ordering.data() + 2;
    ClearOTagR(ordering.data(), static_cast<int>(ordering.size()));
  }
  LiveGimFixture(const LiveGimFixture&) = delete;
  LiveGimFixture& operator=(const LiveGimFixture&) = delete;
  ~LiveGimFixture() {
    Render::ForgetLiveGimImages();
    Render::LiveTexturePages() = saved_pages;
    std::ranges::copy(saved_slots, std::begin(Content_discRequestSlots));
    Content_discRequestCounts = saved_counts;
    Render::LoadLiveClutRow(3, 0, saved_clut);
    LoadImage(&clut_strip, reinterpret_cast<u_long*>(saved_clut_strip.data()));
    LoadImage(&texture_strip,
              reinterpret_cast<u_long*>(saved_texture_strip.data()));
    Render_frameScratch = saved_frame;
    Render_legacyPrimitiveBuffers[0] = saved_buffer;
    Render_legacyOrderingTables[0] = saved_table;
    Render_frameBuf = saved_framebuffer;
  }
};

TEST_CASE("Live GIM requests without an image set do nothing", "[render-gim]") {
  LiveGimFixture live;
  const std::uint64_t refused = Render::RefusedLiveGimRequestCount();
  CHECK_FALSE(Render::LiveGimImagesPresent());
  CHECK(Render::LiveGimImages() == nullptr);
  CHECK(Render::LiveGimImageLoading() == 0);
  CHECK(Render::RefusedLiveGimRequestCount() == refused + 1);
  CHECK_FALSE(Render::LiveGimLayerPlacement(0).has_value());
  Render::ConfigureLiveGimLayer(0, 0, 1, 2, ONE, 128);
  Render::PlaceLiveGimLayer(0, 1, 2, ONE, 128);
  Render::UpdateLiveGimImages();
  Render::ReleaseLiveGimImages();
  CHECK(Render::LiveGimImages() == nullptr);
  CHECK(Render_frameScratch.primitiveCursor == live.storage.data());

  Render::QueueLiveGimImage(-1, 25);
  Render::QueueLiveGimImage(Render::kGimImageCount, 25);
  CHECK_FALSE(Render::LiveGimImagesPresent());
  CHECK(Render::RefusedLiveGimRequestCount() == refused + 3);
}

// Queues STAFF011.GIM (image 0) at page 25 and fills its disc request with a
// one-row file.
Content_DiscQueueSlot& QueueStaffImage() {
  Render::QueueLiveGimImage(0, 25);
  const auto slot = std::ranges::find_if(
      Content_discRequestSlots, [](const Content_DiscQueueSlot& request) {
        return request.state == Content_DiscRequestState_Enqueued &&
               request.cdFile.lba == VS_STAFF011_GIM_LBA;
      });
  REQUIRE(slot != std::end(Content_discRequestSlots));
  const GimFile file = BuildFile(kOneRow, 1);
  REQUIRE(file.words.size() <= std::size_t{13} * 1024);
  std::ranges::copy(file.words, static_cast<std::uint16_t*>(slot->vram));
  return *slot;
}

// Marks the read loaded, then uploads and frees the file.
void FinishStaffRead(Content_DiscQueueSlot& slot) {
  slot.state = Content_DiscRequestState_Loaded;
  Render::UpdateLiveGimImages();
  Render::UpdateLiveGimImages();
}

TEST_CASE("Live GIM images queue one disc read at a time", "[render-gim]") {
  const LiveGimFixture live;
  const Content_DiscQueueSlot& slot = QueueStaffImage();
  REQUIRE(Render::LiveGimImagesPresent());
  CHECK(Render::LiveGimImageLoading() == 1);
  CHECK(slot.cdFile.size == 13 * 2048);
  // A second request while one is held is ignored.
  const Render::GimImageSet* const set = Render::LiveGimImages();
  Render::QueueLiveGimImage(1, 25);
  CHECK(Render::LiveGimImages() == set);
}

TEST_CASE("Live GIM layers keep their placement while the file loads",
          "[render-gim]") {
  const LiveGimFixture live;
  QueueStaffImage();
  Render::ConfigureLiveGimLayer(0, 0, 0, 0, ONE, 128);
  Render::PlaceLiveGimLayer(0, 0, 0, ONE | 0x10000, 128);
  CHECK(Render::LiveGimLayerPlacement(0).has_value());
  // Placements keep the low 16 bits of the scale and order word.
  const Render::GimPlacement placement =
      Render::LiveGimLayerPlacement(0).value_or(Render::GimPlacement{});
  CHECK(placement.scale_and_order == ONE);
  Render::UpdateLiveGimImages();
  REQUIRE(Render::LiveGimImages() != nullptr);
  CHECK(Render::LiveGimImages()->phase == Render::GimLoadPhase::kReading);
}

TEST_CASE("Live GIM layer setups past the four routines are refused",
          "[render-gim]") {
  const LiveGimFixture live;
  QueueStaffImage();
  const std::uint64_t refused = Render::RefusedLiveGimRequestCount();
  Render::ConfigureLiveGimLayer(0, Render::kGimDrawRoutineCount, 0, 0, ONE,
                                128);
  Render::ConfigureLiveGimLayer(0, -1, 0, 0, ONE, 128);
  CHECK(Render::RefusedLiveGimRequestCount() == refused + 2);
  REQUIRE(Render::LiveGimImages() != nullptr);
  CHECK_FALSE(Render::LiveGimImages()->layers[0].draw_enabled);
}

TEST_CASE("Live GIM images upload once their read is loaded", "[render-gim]") {
  const LiveGimFixture live;
  Content_DiscQueueSlot& slot = QueueStaffImage();
  slot.state = Content_DiscRequestState_Loaded;
  Render::UpdateLiveGimImages();
  CHECK(slot.state == Content_DiscRequestState_Free);
  REQUIRE(Render::LiveGimImages() != nullptr);
  CHECK(Render::LiveGimImages()->usable);
  CHECK(Render::LiveTexturePages()[25] ==
        Render::TexturePage{.state = Render::TexturePageState::kOwned,
                            .owner = Render::kGimTextureOwner});
  CHECK(Render::LivePaletteAnimation().colors[3][0] == 0x4040);
  Render::UpdateLiveGimImages();
  CHECK(Render::LiveGimImageLoading() == 0);
  CHECK(Render_frameScratch.primitiveCursor == live.storage.data());
}

TEST_CASE("Live GIM images draw into the overlay table", "[render-gim]") {
  LiveGimFixture live;
  Content_DiscQueueSlot& slot = QueueStaffImage();
  Render::ConfigureLiveGimLayer(0, 0, 0, 0, ONE, 128);
  FinishStaffRead(slot);
  Render::UpdateLiveGimImages();
  // The backdrop bands above and below the row (colour 0x4000, full blue),
  // then the five tiles, link in front of the overlay table's entry 6.
  CHECK(Render_frameScratch.primitiveCursor == live.storage.data() + 38);
  CHECK(nextPrim(&live.ordering[8]) == live.storage.data() + 32);
  CHECK(nextPrim(live.storage.data()) == &live.ordering[7]);
  CHECK(live.storage[1] == 0x60800000);
}

TEST_CASE("Live GIM images release their pages to retire", "[render-gim]") {
  const LiveGimFixture live;
  FinishStaffRead(QueueStaffImage());
  Render::ReleaseLiveGimImages();
  CHECK_FALSE(Render::LiveGimImagesPresent());
  CHECK(Render::LiveTexturePages()[25].state ==
        Render::TexturePageState::kRetiring);
}
}  // namespace
