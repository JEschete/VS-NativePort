#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Core/Memory/Heap.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Legacy/Psx.h"
#include "Menu/Manual.h"
#include "Menu/ManualIllustration.hpp"
#include "Menu/ManualIllustrationLive.hpp"
#include "Render/Display.hpp"
#include "Render/Frame.hpp"
#include "Render/Render.hpp"
#include "Render/UiPrimitives.hpp"

// Contracts of the manual's illustrations (MENUE.PRG 494.c:481-511, 619-713,
// 883-960): the help asset uploads, the atlas sprites and diagram lines of a
// help page and their visibility animations.
namespace {
namespace manual = VS::Menu::manual;
using manual::DrawResult;
using manual::UploadResult;
using VS::Render::ScreenRect;

class FakeUploader final : public manual::ImageUploader {
 public:
  std::vector<ScreenRect> areas;
  std::vector<const std::uint32_t*> pixels;
  std::vector<std::size_t> sizes;

  void Upload(const ScreenRect& area,
              std::span<const std::uint32_t> words) override {
    areas.push_back(area);
    pixels.push_back(words.data());
    sizes.push_back(words.size());
  }
};

class FakeIllustration final : public manual::IllustrationTarget {
 public:
  std::vector<manual::IllustrationTile> tiles;

  void DrawTile(const manual::IllustrationTile& tile) override {
    tiles.push_back(tile);
  }
};

class FakeLines final : public manual::DiagramLineTarget {
 public:
  std::vector<manual::DiagramLine> lines;

  void DrawLine(const manual::DiagramLine& line) override {
    lines.push_back(line);
  }
};

struct IllustrationShape {
  std::int16_t x = 0;
  std::int16_t y = 0;
  std::int16_t width = 0;
  std::int16_t height = 0;
  std::int16_t colors = 16;
  std::int16_t clut = 0;
};

std::vector<std::int16_t> IllustrationRecord(
    const IllustrationShape& shape, std::span<const std::int16_t> tiles) {
  std::vector<std::int16_t> record(manual::kIllustrationTiles);
  record[manual::kIllustrationX] = shape.x;
  record[manual::kIllustrationY] = shape.y;
  record[manual::kIllustrationWidth] = shape.width;
  record[manual::kIllustrationHeight] = shape.height;
  record[manual::kIllustrationColors] = shape.colors;
  record[manual::kIllustrationClut] = shape.clut;
  record.insert(record.end(), tiles.begin(), tiles.end());
  return record;
}

// A hiding animation: frame 0's mask bit is clear and lasts one tick.
void HideOnFirstFrame(std::span<std::int16_t> record) {
  record[0] = 1;
  record[1] = 0;
  record[2] = 1;
}

struct LineShape {
  std::int16_t x0 = 0;
  std::int16_t y0 = 0;
  std::int16_t x1 = 0;
  std::int16_t y1 = 0;
  bool hidden = false;
};

void PutWord(std::vector<std::int16_t>& halfwords, std::size_t word,
             std::uint32_t value) {
  halfwords[word * 2] = static_cast<std::int16_t>(value & 0xFFFFU);
  halfwords[(word * 2) + 1] = static_cast<std::int16_t>(value >> 16U);
}

// A help text with a 6-byte text section, a 9-byte illustration section
// (rounded down to 1 and 2 words) and the line records after them.
std::vector<std::int16_t> HelpText(std::span<const LineShape> lines,
                                   std::uint32_t enabled = 1) {
  constexpr std::size_t kCountIndex = 8 + 6;
  std::vector<std::int16_t> text(kCountIndex + 1);
  PutWord(text, 0, 6);
  PutWord(text, 1, 9);
  PutWord(text, 2, enabled);
  text[kCountIndex] = static_cast<std::int16_t>(lines.size());
  for (const LineShape& line : lines) {
    std::array<std::int16_t, manual::kDiagramLineHalfwords> record{};
    if (line.hidden) {
      HideOnFirstFrame(record);
    }
    record[8] = line.x0;
    record[9] = line.y0;
    record[10] = line.x1;
    record[11] = line.y1;
    record[12] = 0x0201;
    record[13] = 0x7F03;
    text.insert(text.end(), record.begin(), record.end());
  }
  return text;
}

std::vector<std::uint32_t> Words(std::size_t count) {
  std::vector<std::uint32_t> words(count);
  for (std::size_t i = 0; i < count; ++i) {
    words[i] = static_cast<std::uint32_t>(i);
  }
  return words;
}

// --- F02992 _copySprites ----------------------------------------------------

TEST_CASE("Sprite tiles fill the atlas 48 tiles per row",
          "[manual-illustration]") {
  const std::vector<std::uint32_t> words = Words(50 * manual::kTileWords);
  FakeUploader uploader;
  CHECK(manual::UploadSpriteTiles(words, 50, uploader) ==
        UploadResult::kUploaded);
  REQUIRE(uploader.areas.size() == 50);
  CHECK(uploader.areas[0] ==
        ScreenRect{.x = 832, .y = 256, .width = 4, .height = 16});
  CHECK(uploader.areas[47] ==
        ScreenRect{.x = 1020, .y = 256, .width = 4, .height = 16});
  CHECK(uploader.areas[49] ==
        ScreenRect{.x = 836, .y = 272, .width = 4, .height = 16});
  CHECK(uploader.pixels[1] == words.data() + 32);
  CHECK(uploader.sizes[49] == manual::kTileWords);
}

TEST_CASE("Sprite tile uploads refuse counts past the atlas or the buffer",
          "[manual-illustration]") {
  const std::vector<std::uint32_t> words =
      Words(std::size_t{manual::kMaxSpriteTiles} * manual::kTileWords);
  FakeUploader uploader;
  CHECK(manual::UploadSpriteTiles(words, 769, uploader) ==
        UploadResult::kTooManyImages);
  CHECK(manual::UploadSpriteTiles(std::span(words).first(63), 2, uploader) ==
        UploadResult::kShortBuffer);
  CHECK(manual::UploadSpriteTiles({}, 0, uploader) == UploadResult::kUploaded);
  CHECK(manual::UploadSpriteTiles({}, -5, uploader) == UploadResult::kUploaded);
  CHECK(uploader.areas.empty());
  CHECK(manual::UploadSpriteTiles(words, 768, uploader) ==
        UploadResult::kUploaded);
  REQUIRE(uploader.areas.size() == 768);
  CHECK(uploader.areas[767] ==
        ScreenRect{.x = 1020, .y = 496, .width = 4, .height = 16});
}

// --- F02993 _copyCluts ------------------------------------------------------

TEST_CASE("Palettes fill the CLUT grid 16 per row", "[manual-illustration]") {
  const std::vector<std::uint32_t> words = Words(18 * manual::kClutWords);
  FakeUploader uploader;
  CHECK(manual::UploadCluts(words, 18, uploader) == UploadResult::kUploaded);
  REQUIRE(uploader.areas.size() == 18);
  CHECK(uploader.areas[0] ==
        ScreenRect{.x = 768, .y = 496, .width = 16, .height = 1});
  CHECK(uploader.areas[15] ==
        ScreenRect{.x = 1008, .y = 496, .width = 16, .height = 1});
  CHECK(uploader.areas[17] ==
        ScreenRect{.x = 784, .y = 497, .width = 16, .height = 1});
  CHECK(uploader.pixels[17] == words.data() + (17 * manual::kClutWords));
  CHECK(uploader.sizes[0] == manual::kClutWords);
}

TEST_CASE("Palette uploads refuse counts past the grid or the buffer",
          "[manual-illustration]") {
  const std::vector<std::uint32_t> words =
      Words(std::size_t{manual::kMaxCluts} * manual::kClutWords);
  FakeUploader uploader;
  CHECK(manual::UploadCluts(words, 257, uploader) ==
        UploadResult::kTooManyImages);
  CHECK(manual::UploadCluts(std::span(words).first(15), 2, uploader) ==
        UploadResult::kShortBuffer);
  CHECK(manual::UploadCluts({}, 0, uploader) == UploadResult::kUploaded);
  CHECK(uploader.areas.empty());
  CHECK(manual::UploadCluts(words, 256, uploader) == UploadResult::kUploaded);
  REQUIRE(uploader.areas.size() == 256);
  CHECK(uploader.areas[255] ==
        ScreenRect{.x = 1008, .y = 511, .width = 16, .height = 1});
}

// --- F03006 _getAnimationState ----------------------------------------------

TEST_CASE("Visibility animation hides the frames whose mask bit is clear",
          "[manual-illustration]") {
  manual::VisibilityAnimation animation{
      .enabled = 1, .frame_mask = 0x0005, .frame_duration = 1};
  CHECK(manual::AdvanceVisibilityAnimation(animation) == 0);
  CHECK(manual::AdvanceVisibilityAnimation(animation) == 1);
  CHECK(manual::AdvanceVisibilityAnimation(animation) == 0);
  CHECK(animation.hidden == 0);
  CHECK(animation.current_frame == 3);
  CHECK(manual::AdvanceVisibilityAnimation(animation) == 1);
  CHECK(animation.hidden == 1);
}

TEST_CASE(
    "Visibility animation advances frames once the timer reaches the "
    "duration",
    "[manual-illustration]") {
  manual::VisibilityAnimation animation{
      .enabled = 1, .frame_mask = 0xFFFF, .frame_duration = 3};
  CHECK(manual::AdvanceVisibilityAnimation(animation) == 0);
  CHECK(animation.timer == 1);
  CHECK(manual::AdvanceVisibilityAnimation(animation) == 0);
  CHECK(animation.current_frame == 0);
  CHECK(manual::AdvanceVisibilityAnimation(animation) == 0);
  CHECK(animation.timer == 0);
  CHECK(animation.current_frame == 1);
  animation.timer = 0x7FFF;
  manual::AdvanceVisibilityAnimation(animation);
  CHECK(animation.timer == -0x8000);
}

TEST_CASE("Visibility animation restarts after frame 15 only when it repeats",
          "[manual-illustration]") {
  manual::VisibilityAnimation once{.enabled = 1,
                                   .frame_mask = 0x8000,
                                   .frame_duration = 1,
                                   .current_frame = 15};
  manual::VisibilityAnimation looping = once;
  looping.repeat = 1;
  CHECK(manual::AdvanceVisibilityAnimation(once) == 0);
  CHECK(once.current_frame == 16);
  CHECK(manual::AdvanceVisibilityAnimation(looping) == 0);
  CHECK(looping.current_frame == 0);
  CHECK(manual::AdvanceVisibilityAnimation(looping) == 1);
  CHECK(looping.current_frame == 1);
}

TEST_CASE("Visibility animation past the last frame keeps its stored state",
          "[manual-illustration]") {
  manual::VisibilityAnimation ended{.enabled = 1,
                                    .frame_duration = 1,
                                    .current_frame = 16,
                                    .timer = 4,
                                    .hidden = 7};
  CHECK(manual::AdvanceVisibilityAnimation(ended) == 7);
  CHECK(ended.timer == 4);
  CHECK(ended.current_frame == 16);
  // A negative frame shifts the mask by its low five bits, as srav does.
  manual::VisibilityAnimation negative{.enabled = 1,
                                       .frame_mask = 0x4000,
                                       .frame_duration = 5,
                                       .current_frame = -18};
  CHECK(manual::AdvanceVisibilityAnimation(negative) == 0);
  negative.current_frame = -17;
  CHECK(manual::AdvanceVisibilityAnimation(negative) == 1);
  CHECK(negative.current_frame == -17);
}

TEST_CASE("Visibility animations load and store their seven halfwords",
          "[manual-illustration]") {
  std::array<std::int16_t, manual::kAnimationHalfwords> halfwords{1, -2, 3, 4,
                                                                  5, 6,  7};
  const manual::VisibilityAnimation animation =
      manual::LoadAnimation(halfwords);
  CHECK(animation.frame_mask == 0xFFFE);
  CHECK(animation.hidden == 7);
  std::array<std::int16_t, manual::kAnimationHalfwords> stored{};
  manual::StoreAnimation(animation, stored);
  CHECK(stored == halfwords);
}

// --- F03000 _drawSprite -----------------------------------------------------

TEST_CASE("Illustration tiles map atlas indices to texture pages and texels",
          "[manual-illustration]") {
  const std::array<std::int16_t, 2> tiles{50, 37};
  std::vector<std::int16_t> record = IllustrationRecord(
      {.x = 10, .width = 20, .height = 16, .clut = 17}, tiles);
  FakeIllustration target;
  CHECK(manual::DrawIllustration(record, 0, target) == DrawResult::kDrawn);
  REQUIRE(target.tiles.size() == 2);
  CHECK(target.tiles[0] == manual::IllustrationTile{.x = 23,
                                                    .y = 55,
                                                    .u = 0x20,
                                                    .v = 16,
                                                    .clut = 0x7C71,
                                                    .width = 16,
                                                    .height = 16,
                                                    .texture_page = 0x1D});
  CHECK(target.tiles[1] == manual::IllustrationTile{.x = 39,
                                                    .y = 55,
                                                    .u = 0x50,
                                                    .v = 0,
                                                    .clut = 0x7C71,
                                                    .width = 4,
                                                    .height = 16,
                                                    .texture_page = 0x1F});
}

TEST_CASE("Eight-bit illustrations use 8-texel tiles and the depth bit",
          "[manual-illustration]") {
  const std::array<std::int16_t, 4> tiles{37, 20, 99, -1};
  std::vector<std::int16_t> record =
      IllustrationRecord({.width = 10, .height = 20, .colors = 256}, tiles);
  FakeIllustration target;
  CHECK(manual::DrawIllustration(record, 0, target) == DrawResult::kDrawn);
  REQUIRE(target.tiles.size() == 4);
  CHECK(target.tiles[0].texture_page == 0x9F);
  CHECK(target.tiles[0].u == 40);
  CHECK(target.tiles[1].width == 2);
  CHECK(target.tiles[1].texture_page == 0x9E);
  CHECK(target.tiles[2].height == 4);
  CHECK(target.tiles[2].y == 71);
}

TEST_CASE("Eight-bit illustration tiles truncate negative indices to zero",
          "[manual-illustration]") {
  const std::array<std::int16_t, 1> tiles{-1};
  std::vector<std::int16_t> record =
      IllustrationRecord({.width = 8, .height = 16, .colors = 256}, tiles);
  FakeIllustration target;
  CHECK(manual::DrawIllustration(record, 0, target) == DrawResult::kDrawn);
  REQUIRE(target.tiles.size() == 1);
  // Index -1 truncates towards zero: column -1, page 29.
  CHECK(target.tiles[0].u == 0xF8);
  CHECK(target.tiles[0].texture_page == 0x9D);
}

TEST_CASE("Illustration palettes pack their grid cell into the CLUT word",
          "[manual-illustration]") {
  CHECK(manual::IllustrationClut(0) == 0x7C30);
  CHECK(manual::IllustrationClut(17) == 0x7C71);
  CHECK(manual::IllustrationClut(255) == 0x7FFF);
  CHECK(manual::IllustrationClut(-1) == 0x7C2F);
}

TEST_CASE(
    "Illustration rows outside the content panel are skipped but "
    "consume their tiles",
    "[manual-illustration]") {
  const std::array<std::int16_t, 2> tiles{1, 2};
  std::vector<std::int16_t> record =
      IllustrationRecord({.width = 8, .height = 32}, tiles);
  FakeIllustration target;
  CHECK(manual::DrawIllustration(record, 2, target) == DrawResult::kDrawn);
  REQUIRE(target.tiles.size() == 1);
  CHECK(target.tiles[0].y == 45);
  CHECK(target.tiles[0].u == 0x20);
  record[manual::kIllustrationY] = 129;
  CHECK(manual::DrawIllustration(record, 0, target) == DrawResult::kDrawn);
  CHECK(target.tiles.size() == 2);
  CHECK(target.tiles[1].y == 184);
}

TEST_CASE("Hidden illustrations draw nothing and advance their animation",
          "[manual-illustration]") {
  const std::array<std::int16_t, 1> tiles{1};
  std::vector<std::int16_t> record =
      IllustrationRecord({.width = 8, .height = 8}, tiles);
  HideOnFirstFrame(record);
  FakeIllustration target;
  CHECK(manual::DrawIllustration(record, 0, target) == DrawResult::kHidden);
  CHECK(target.tiles.empty());
  CHECK(record[4] == 1);
  CHECK(record[6] == 1);
  record[1] = 0x0002;
  CHECK(manual::DrawIllustration(record, 0, target) == DrawResult::kDrawn);
  CHECK(target.tiles.size() == 1);
}

TEST_CASE("Illustrations whose tiles run past the record are refused untouched",
          "[manual-illustration]") {
  const std::array<std::int16_t, 1> tiles{1};
  std::vector<std::int16_t> record =
      IllustrationRecord({.width = 20, .height = 16}, tiles);
  HideOnFirstFrame(record);
  FakeIllustration target;
  CHECK(manual::DrawIllustration(record, 0, target) == DrawResult::kRefused);
  CHECK(record[4] == 0);
  CHECK(record[6] == 0);
  CHECK(manual::DrawIllustration(std::span(record).first(14), 0, target) ==
        DrawResult::kRefused);
  record[manual::kIllustrationWidth] = 0;
  CHECK(manual::DrawIllustration(std::span(record).first(15), 0, target) ==
        DrawResult::kHidden);
  CHECK(target.tiles.empty());
}

// --- F03005 _drawContentLines -----------------------------------------------

TEST_CASE("Diagram lines are offset into the content panel and scrolled",
          "[manual-illustration]") {
  const std::array<LineShape, 1> lines{{{.x1 = 100, .y1 = 50}}};
  std::vector<std::int16_t> text = HelpText(lines);
  FakeLines target;
  CHECK(manual::DrawDiagramLines(text, 0, target) == DrawResult::kDrawn);
  CHECK(manual::DrawDiagramLines(text, 1, target) == DrawResult::kDrawn);
  REQUIRE(target.lines.size() == 2);
  CHECK(target.lines[0] == manual::DiagramLine{.x0 = 13,
                                               .y0 = 55,
                                               .x1 = 113,
                                               .y1 = 105,
                                               .red = 1,
                                               .green = 2,
                                               .blue = 3});
  CHECK(target.lines[1].y0 == 42);
  CHECK(target.lines[1].y1 == 92);
}

TEST_CASE("Diagram lines above or below the content panel are skipped",
          "[manual-illustration]") {
  const std::array<LineShape, 4> lines{{{.y0 = -20, .y1 = -1},
                                        {.y0 = 131, .y1 = 200},
                                        {.y0 = 130, .y1 = 300},
                                        {.y0 = -1, .y1 = 131}}};
  std::vector<std::int16_t> text = HelpText(lines);
  FakeLines target;
  CHECK(manual::DrawDiagramLines(text, 0, target) == DrawResult::kDrawn);
  REQUIRE(target.lines.size() == 2);
  CHECK(target.lines[0].y0 == 185);
  CHECK(target.lines[1].y0 == 54);
  CHECK(target.lines[1].y1 == 186);
}

TEST_CASE("Hidden diagram lines advance their animation but draw nothing",
          "[manual-illustration]") {
  const std::array<LineShape, 2> lines{{{.hidden = true}, {.y1 = 5}}};
  std::vector<std::int16_t> text = HelpText(lines);
  FakeLines target;
  CHECK(manual::DrawDiagramLines(text, 0, target) == DrawResult::kDrawn);
  REQUIRE(target.lines.size() == 1);
  CHECK(target.lines[0].y1 == 60);
  constexpr std::size_t kFirstRecord = 15;
  CHECK(text[kFirstRecord + 4] == 1);
  CHECK(text[kFirstRecord + 6] == 1);
}

TEST_CASE("Disabled or truncated diagram line sections draw nothing",
          "[manual-illustration]") {
  const std::array<LineShape, 1> lines{{{.hidden = true}}};
  FakeLines target;
  std::vector<std::int16_t> text = HelpText(lines, 0);
  CHECK(manual::DrawDiagramLines(text, 0, target) == DrawResult::kDisabled);
  text = HelpText(lines, 0x80000000U);
  CHECK(manual::DrawDiagramLines(text, 0, target) == DrawResult::kDisabled);
  text = HelpText(lines);
  text[14] = 2;
  CHECK(manual::DrawDiagramLines(text, 0, target) == DrawResult::kRefused);
  CHECK(text[15 + 4] == 0);
  PutWord(text, 0, static_cast<std::uint32_t>(-100));
  CHECK(manual::DrawDiagramLines(text, 0, target) == DrawResult::kRefused);
  CHECK(manual::DrawDiagramLines(std::span(text).first(5), 0, target) ==
        DrawResult::kRefused);
  CHECK(target.lines.empty());
}

TEST_CASE("Diagram line offsets wrap like the original's address arithmetic",
          "[manual-illustration]") {
  // Both sections round to 0x1FFFFFFF words: 16 + 4 * 0x3FFFFFFE wraps to 8,
  // so the count is word 2's low half and the records follow it.
  std::vector<std::int16_t> text(5 + manual::kDiagramLineHalfwords);
  PutWord(text, 0, 0x7FFFFFFC);
  PutWord(text, 1, 0x7FFFFFFC);
  PutWord(text, 2, 1);
  text[5 + 9] = 20;
  FakeLines target;
  CHECK(manual::DrawDiagramLines(text, 0, target) == DrawResult::kDrawn);
  REQUIRE(target.lines.size() == 1);
  CHECK(target.lines[0].y0 == 75);
}

// --- Live bindings ----------------------------------------------------------

struct LiveFrameFixture {
  VS::Render::FrameScratch saved_frame = VS::Render::FrameScratchpad();
  void* saved_buffer = VS::Render::legacyPrimitiveBuffers[0];
  void* saved_table = VS::Render::legacyOrderingTables[0];
  int saved_framebuffer = VS::Render::frameBuf;
  int saved_scroll = Menu_Manual_scrollPosition;
  int* saved_help_text = Menu_Manual_helpText;
  std::vector<u_long> storage = std::vector<u_long>(0x20000 / 4, 0xDEADBEEF);
  std::vector<u_long> ordering = std::vector<u_long>(0x2088 / 4);
  alignas(VS::Core::Memory::kHeapUnitBytes)
      std::array<std::byte, std::size_t{16} * 1024> arena{};
  VS::Core::Memory::ScopedLiveGameHeap heap{arena};

  LiveFrameFixture() {
    VS::Render::frameBuf = 0;
    VS::Render::legacyPrimitiveBuffers[0] = storage.data();
    VS::Render::legacyOrderingTables[0] = ordering.data();
    VS::Render::FrameScratchpad().primitive_cursor = storage.data();
    VS::Render::FrameScratchpad().ordering_table = ordering.data() + 18;
    ClearOTagR(ordering.data(), static_cast<int>(ordering.size()));
    Menu_Manual_scrollPosition = 0;
  }
  LiveFrameFixture(const LiveFrameFixture&) = delete;
  LiveFrameFixture& operator=(const LiveFrameFixture&) = delete;
  LiveFrameFixture(LiveFrameFixture&&) = delete;
  LiveFrameFixture& operator=(LiveFrameFixture&&) = delete;
  ~LiveFrameFixture() {
    VS::Render::FrameScratchpad() = saved_frame;
    VS::Render::legacyPrimitiveBuffers[0] = saved_buffer;
    VS::Render::legacyOrderingTables[0] = saved_table;
    VS::Render::frameBuf = saved_framebuffer;
    Menu_Manual_scrollPosition = saved_scroll;
    Menu_Manual_helpText = saved_help_text;
  }

  template <typename T>
  T* Allocate(std::span<const T> contents) {
    auto* payload = static_cast<T*>(
        VS::Core::Memory::AllocateLiveFromHighEnd(contents.size_bytes()));
    std::ranges::copy(contents, payload);
    return payload;
  }
};

TEST_CASE("Live illustrations queue a sprite and texture page per tile",
          "[manual-illustration-live]") {
  LiveFrameFixture frame;
  const std::array<std::int16_t, 1> tiles{37};
  const std::vector<std::int16_t> record =
      IllustrationRecord({.x = 10, .y = 20, .width = 8, .height = 8}, tiles);
  std::int16_t* data = frame.Allocate<std::int16_t>(record);
  REQUIRE(data != nullptr);
  Menu_Manual_scrollPosition = 1;

  manual::LiveDrawIllustration(data);
  CHECK(VS::Render::FrameScratchpad().primitive_cursor ==
        frame.storage.data() + 7);
  CHECK(nextPrim(&frame.ordering[18]) == frame.storage.data() + 5);
  CHECK(nextPrim(frame.storage.data() + 5) == frame.storage.data());
  CHECK(frame.storage[6] == 0xE100021FU);
  CHECK(frame.storage[1] == 0x65000000U);
  CHECK(frame.storage[2] == ((62U << 16U) | 23U));
  CHECK(frame.storage[3] == ((0x7C30U << 16U) | 0x50U));
  CHECK(frame.storage[4] == ((8U << 16U) | 8U));
}

TEST_CASE("Live illustrations outside the game heap are refused",
          "[manual-illustration-live]") {
  LiveFrameFixture frame;
  const std::array<std::int16_t, 1> tiles{37};
  std::vector<std::int16_t> record =
      IllustrationRecord({.width = 8, .height = 8}, tiles);
  const std::uint64_t refusals = manual::LiveIllustrationRefusalCount();
  manual::LiveDrawIllustration(record.data());
  manual::LiveDrawIllustration(nullptr);
  const std::vector<std::int16_t> short_record = IllustrationRecord(
      {.width = 200, .height = 8}, std::span(tiles).first(1));
  // Thirteen tiles do not fit the payload's 16-byte rounding.
  manual::LiveDrawIllustration(frame.Allocate<std::int16_t>(short_record));
  CHECK(manual::LiveIllustrationRefusalCount() == refusals + 3);
  CHECK(VS::Render::FrameScratchpad().primitive_cursor == frame.storage.data());
  CHECK(frame.storage[0] == 0xDEADBEEF);
}

TEST_CASE("Live diagram lines queue flat lines from the help text",
          "[manual-illustration-live]") {
  LiveFrameFixture frame;
  const std::array<LineShape, 2> lines{
      {{.x1 = 100, .y1 = 50}, {.y0 = -20, .y1 = -1}}};
  const std::vector<std::int16_t> text = HelpText(lines);
  Menu_Manual_helpText =
      reinterpret_cast<int*>(frame.Allocate<std::int16_t>(text));
  REQUIRE(Menu_Manual_helpText != nullptr);

  manual::LiveDrawDiagramLines();
  CHECK(VS::Render::FrameScratchpad().primitive_cursor ==
        frame.storage.data() + 4);
  CHECK(nextPrim(&frame.ordering[18]) == frame.storage.data());
  CHECK(frame.storage[1] == 0x40030201U);
  CHECK(frame.storage[2] == ((55U << 16U) | 13U));
  CHECK(frame.storage[3] == ((105U << 16U) | 113U));
}

TEST_CASE("Live diagram lines without a heap help text are refused",
          "[manual-illustration-live]") {
  LiveFrameFixture frame;
  const std::array<LineShape, 1> lines{{{.y1 = 5}}};
  std::vector<std::int16_t> text = HelpText(lines);
  const std::uint64_t refusals = manual::LiveIllustrationRefusalCount();
  Menu_Manual_helpText = nullptr;
  manual::LiveDrawDiagramLines();
  Menu_Manual_helpText = reinterpret_cast<int*>(text.data());
  manual::LiveDrawDiagramLines();
  text[14] = 3;
  Menu_Manual_helpText =
      reinterpret_cast<int*>(frame.Allocate<std::int16_t>(text));
  manual::LiveDrawDiagramLines();
  CHECK(manual::LiveIllustrationRefusalCount() == refusals + 3);
  CHECK(VS::Render::FrameScratchpad().primitive_cursor == frame.storage.data());
}

TEST_CASE("Live uploads copy heap tiles and palettes into VRAM",
          "[manual-illustration-live]") {
  LiveFrameFixture frame;
  std::array<std::uint32_t, manual::kTileWords> saved{};
  RECT tile{};
  setRECT(&tile, 832, 256, 4, 16);
  StoreImage(&tile, reinterpret_cast<u_long*>(saved.data()));
  const std::vector<std::uint32_t> words = Words(manual::kTileWords);
  const std::uint64_t refusals = manual::LiveIllustrationRefusalCount();

  manual::LiveUploadSpriteTiles(
      reinterpret_cast<const u_long*>(frame.Allocate<std::uint32_t>(words)), 1);
  std::array<std::uint32_t, manual::kTileWords> uploaded{};
  StoreImage(&tile, reinterpret_cast<u_long*>(uploaded.data()));
  LoadImage(&tile, reinterpret_cast<u_long*>(saved.data()));
  CHECK(std::ranges::equal(uploaded, words));
  CHECK(manual::LiveIllustrationRefusalCount() == refusals);
}

TEST_CASE("Live uploads outside the heap or past their buffer are refused",
          "[manual-illustration-live]") {
  LiveFrameFixture frame;
  std::vector<std::uint32_t> words = Words(manual::kClutWords);
  const std::uint64_t refusals = manual::LiveIllustrationRefusalCount();
  manual::LiveUploadCluts(reinterpret_cast<u_long*>(words.data()), 1);
  auto* heap_words =
      reinterpret_cast<u_long*>(frame.Allocate<std::uint32_t>(words));
  manual::LiveUploadCluts(heap_words, 2);
  manual::LiveUploadCluts(heap_words, 257);
  manual::LiveUploadSpriteTiles(heap_words, 1);
  CHECK(manual::LiveIllustrationRefusalCount() == refusals + 4);
  manual::LiveUploadCluts(nullptr, 0);
  CHECK(manual::LiveIllustrationRefusalCount() == refusals + 4);
}

TEST_CASE("Live visibility animation updates the original state record",
          "[manual-illustration-live]") {
  Menu_Manual_AnimationState state{.reserved0 = 1,
                                   .frameMask = 0x0002,
                                   .frameDuration = 1,
                                   .repeat = 0,
                                   .currentFrame = 0,
                                   .timer = 0,
                                   .isActive = 0};
  CHECK(manual::LiveAdvanceVisibilityAnimation(&state) == 1);
  CHECK(state.isActive == 1);
  CHECK(state.currentFrame == 1);
  CHECK(manual::LiveAdvanceVisibilityAnimation(&state) == 0);
  CHECK(state.reserved0 == 1);
  const std::uint64_t refusals = manual::LiveIllustrationRefusalCount();
  CHECK(manual::LiveAdvanceVisibilityAnimation(nullptr) == 0);
  CHECK(manual::LiveIllustrationRefusalCount() == refusals + 1);
}
}  // namespace
