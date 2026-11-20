#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <variant>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Core/Memory/Heap.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Input/ControllerLiveState.hpp"
#include "Legacy/Psx.h"
#include "Menu/Manual.hpp"
#include "Menu/ManualPage.hpp"
#include "Menu/ManualPageLive.hpp"
#include "Render/Display.hpp"
#include "Render/Frame.hpp"
#include "Render/Render.hpp"
#include "Render/UiPrimitives.hpp"

// Contracts of the manual page's frame (MENUE.PRG 494.c:577-616, 716-881,
// 962-987): content clipping, the page background, the scroll fades, the
// page arrows and the control hint boxes.
namespace {
namespace manual = VS::Menu::manual;
using manual::FlatQuad;
using manual::GradientLine;
using manual::GradientQuad;
using manual::PageArrow;
using manual::PageColor;
using manual::PageDrawResult;
using manual::PagePoint;
using manual::PageTile;
using manual::TexturedQuad;
using VS::Render::ScreenRect;

struct TexturePage {
  std::uint16_t page = 0;
};

struct Command {
  using Primitive = std::variant<GradientQuad, TexturedQuad, PageTile,
                                 GradientLine, FlatQuad, TexturePage>;
  std::int32_t slot = 0;
  Primitive primitive;
};

class RecordingTarget final : public manual::PagePrimitiveTarget {
 public:
  std::vector<Command> commands;

  void DrawGradientQuad(std::int32_t slot, const GradientQuad& quad) override {
    commands.push_back({slot, quad});
  }
  void DrawTexturedQuad(std::int32_t slot, const TexturedQuad& quad) override {
    commands.push_back({slot, quad});
  }
  void DrawTile(std::int32_t slot, const PageTile& tile) override {
    commands.push_back({slot, tile});
  }
  void DrawGradientLine(std::int32_t slot, const GradientLine& line) override {
    commands.push_back({slot, line});
  }
  void DrawFlatQuad(std::int32_t slot, const FlatQuad& quad) override {
    commands.push_back({slot, quad});
  }
  void SetTexturePage(std::int32_t slot, std::uint16_t page) override {
    commands.push_back({slot, TexturePage{page}});
  }

  template <typename T>
  [[nodiscard]] const T& At(std::size_t index) const {
    REQUIRE(index < commands.size());
    const T* primitive = std::get_if<T>(&commands[index].primitive);
    REQUIRE(primitive != nullptr);
    return *primitive;
  }
};

struct ContentEvent {
  enum class Kind : std::uint8_t { kArea, kLines, kIllustration };
  Kind kind = Kind::kArea;
  ScreenRect area{};
  std::size_t offset = 0;
};

class RecordingContent final : public manual::PageContentTarget {
 public:
  std::vector<ContentEvent> events;

  void SetDrawArea(const ScreenRect& area) override {
    events.push_back({.kind = ContentEvent::Kind::kArea, .area = area});
  }
  void DrawDiagramLines() override {
    events.push_back({.kind = ContentEvent::Kind::kLines});
  }
  void DrawIllustration(std::size_t offset) override {
    events.push_back(
        {.kind = ContentEvent::Kind::kIllustration, .offset = offset});
  }
};

manual::PageCorners Corners(std::int16_t left, std::int16_t top,
                            std::int16_t right, std::int16_t bottom) {
  return {{{left, top}, {right, top}, {left, bottom}, {right, bottom}}};
}

void PutWord(std::vector<std::int16_t>& halfwords, std::size_t word,
             std::uint32_t value) {
  halfwords[word * 2] = static_cast<std::int16_t>(value & 0xFFFFU);
  halfwords[(word * 2) + 1] = static_cast<std::int16_t>(value >> 16U);
}

// A help text whose 10-byte text section rounds to 2 words, so the
// illustration list starts at halfword 12: a count, then each record's
// halfword offset from the list.
std::vector<std::int16_t> HelpTextWithIllustrations(
    std::span<const std::int16_t> offsets, std::size_t trailing = 8) {
  std::vector<std::int16_t> text(12);
  PutWord(text, 0, 10);
  text.push_back(static_cast<std::int16_t>(offsets.size()));
  text.insert(text.end(), offsets.begin(), offsets.end());
  text.resize(text.size() + trailing);
  return text;
}

void CheckPanelGradient(const RecordingTarget& target) {
  REQUIRE(target.commands.size() == 2);
  const auto& quad = target.At<GradientQuad>(0);
  CHECK(target.commands[0].slot == manual::kPagePanelSlot);
  CHECK(quad.corners == Corners(8, 52, 317, 187));
  const PageColor left{0x00, 0x20, 0x50};
  const PageColor right{0x00, 0x05, 0x33};
  CHECK(quad.colors ==
        std::array<PageColor, manual::kQuadCorners>{left, right, left, right});
  CHECK(target.commands[1].slot == manual::kPagePanelSlot);
  CHECK(target.At<TexturePage>(1).page == 0x60);
}

void CheckPanelGradientWords(const GradientQuad& quad) {
  CHECK(manual::PackGradientQuad(quad)[0] == 0x38502000U);
  CHECK(manual::PackGradientQuad(quad)[manual::kBottomRight] ==
        ((52U << 16U) | 317U));
  CHECK(manual::PackTexturePage(0x60) ==
        std::array<std::uint32_t, 2>{0xE1000260U, 0});
}

TEST_CASE("Page background is a left to right gradient over a subtractive page",
          "[manual-page]") {
  RecordingTarget target;
  CHECK(manual::DrawPageBackground(
            {.x = 8, .y = 52, .width = 310, .height = 136}, 0, target) ==
        PageDrawResult::kDrawn);
  CheckPanelGradient(target);
  CheckPanelGradientWords(target.At<GradientQuad>(0));

  RecordingTarget last;
  CHECK(manual::DrawPageBackground({.x = -1, .y = 0, .width = 0, .height = 0},
                                   3, last) == PageDrawResult::kDrawn);
  CHECK(last.At<GradientQuad>(0).corners == Corners(-1, 0, -2, -1));
  CHECK(last.At<GradientQuad>(0).colors[1] == PageColor{0x10, 0x10, 0x08});
}

TEST_CASE("Page background refuses gradients outside its palette",
          "[manual-page]") {
  RecordingTarget target;
  CHECK(manual::DrawPageBackground({}, -1, target) == PageDrawResult::kRefused);
  CHECK(manual::DrawPageBackground({}, manual::kPageGradients, target) ==
        PageDrawResult::kRefused);
  CHECK(target.commands.empty());
}

TEST_CASE("Page arrows bounce up and down from their resting rows",
          "[manual-page]") {
  CHECK(manual::PageArrowTop(PageArrow::kUp, 0) == 40);
  CHECK(manual::PageArrowTop(PageArrow::kUp, 8) == 36);
  CHECK(manual::PageArrowTop(PageArrow::kUp, 9) == 36);
  CHECK(manual::PageArrowTop(PageArrow::kUp, 15) == 37);
  CHECK(manual::PageArrowTop(PageArrow::kDown, 0) == 184);
  CHECK(manual::PageArrowTop(PageArrow::kDown, 8) == 188);
  CHECK(manual::PageArrowTop(PageArrow::kDown, 9) == 188);
  CHECK(manual::PageArrowTop(PageArrow::kDown, 15) == 187);
  // Arithmetic shifts: a negative counter rounds down.
  CHECK(manual::PageArrowTop(PageArrow::kUp, -1) == 41);
  CHECK(manual::PageArrowTop(PageArrow::kDown, -3) == 182);
}

void CheckDownArrow(const TexturedQuad& down) {
  CHECK(down.corners == Corners(152, 185, 168, 201));
  CHECK(down.texels[0] == manual::PageTexel{16, 48});
  CHECK(down.texels[manual::kBottomRight] == manual::PageTexel{32, 64});
  CHECK(down.clut == 0x37F9);
  CHECK(down.texture_page == 0x000C);
  CHECK(down.raw_texture);
  CHECK_FALSE(down.semi_transparent);
}

void CheckUpArrow(const TexturedQuad& up) {
  CHECK(up.corners == Corners(152, 37, 168, 53));
  CHECK(up.texels[0] == manual::PageTexel{0, 48});
  CHECK(up.texels[1] == manual::PageTexel{16, 48});
  CHECK(up.clut == 0x37F8);
  const auto packed = manual::PackTexturedQuad(up);
  CHECK(packed[0] == 0x2D000000U);
  CHECK(packed[2] == ((0x37F8U << 16U) | (48U << 8U)));
  CHECK(packed[4] == ((0x000CU << 16U) | (48U << 8U) | 16U));
}

TEST_CASE("Page arrows pick their texture and palette by direction and Square",
          "[manual-page]") {
  RecordingTarget target;
  manual::DrawPageArrow(PageArrow::kDown, {.animation_state = 2}, target);
  manual::DrawPageArrow(PageArrow::kUp,
                        {.animation_state = 12, .square_held = true}, target);
  REQUIRE(target.commands.size() == 2);
  CHECK(target.commands[0].slot == manual::kPageFadeSlot);
  CheckDownArrow(target.At<TexturedQuad>(0));
  CheckUpArrow(target.At<TexturedQuad>(1));
}

void CheckFadeChunks(const RecordingTarget& target, std::size_t row,
                     std::int16_t top) {
  const auto level = manual::kScrollFadeLevels.at(row);
  const auto& first = target.At<TexturedQuad>(1 + (row * 3));
  const auto& last = target.At<TexturedQuad>(3 + (row * 3));
  const auto bottom = static_cast<std::int16_t>(top + 1);
  CHECK(first.corners == Corners(0, top, 128, bottom));
  CHECK(last.corners == Corners(256, top, 320, bottom));
  CHECK(last.texels[1] ==
        manual::PageTexel{64, static_cast<std::uint8_t>(top)});
  CHECK(last.texels[2] ==
        manual::PageTexel{0, static_cast<std::uint8_t>(bottom)});
  CHECK(first.color == PageColor{level, level, level});
}

void CheckFadeTile(const RecordingTarget& target, std::size_t row,
                   std::int16_t top) {
  const auto level = manual::kScrollFadeLevels.at(row);
  const auto& tile = target.At<PageTile>(1 + 48 + row);
  CHECK(tile.position == PagePoint{0, top});
  CHECK(tile.color == PageColor{level, level, level});
}

void CheckFadeRows(const RecordingTarget& target, std::int16_t first_row,
                   std::int16_t step) {
  REQUIRE(target.commands.size() == 1 + 48 + 16 + 1);
  for (std::size_t index = 1; index < target.commands.size(); ++index) {
    CHECK(target.commands[index].slot == manual::kPageFadeSlot);
  }
  for (std::int16_t row = 0; row < 16; ++row) {
    const auto top = static_cast<std::int16_t>(first_row + (row * step));
    CheckFadeChunks(target, static_cast<std::size_t>(row), top);
    CheckFadeTile(target, static_cast<std::size_t>(row), top);
  }
}

void CheckUpperFadeChunkPages(const RecordingTarget& target) {
  const auto& chunk = target.At<TexturedQuad>(2);
  CHECK(chunk.clut == 0x38F0);
  CHECK(chunk.semi_transparent);
  CHECK_FALSE(chunk.raw_texture);
  CHECK(manual::PackTexturedQuad(chunk)[0] == 0x2E808080U);
  // The third chunk ORs into the page bits and lands back on x 640.
  CHECK(target.At<TexturedQuad>(1).texture_page == 0xBA);
  CHECK(target.At<TexturedQuad>(2).texture_page == 0xBB);
  CHECK(target.At<TexturedQuad>(3).texture_page == 0xBA);
}

void CheckUpperFadeTiles(const RecordingTarget& target) {
  const auto& tile = target.At<PageTile>(49);
  CHECK(tile.width == 320);
  CHECK(tile.height == 1);
  CHECK(tile.semi_transparent);
  CHECK(manual::PackPageTile(tile)[0] == 0x62808080U);
  CHECK(target.At<TexturePage>(65).page == 0x40);
}

TEST_CASE("Upper scroll fade dims rows 52 to 67 below the up arrow",
          "[manual-page]") {
  RecordingTarget target;
  manual::DrawUpperScrollFade({.animation_state = 0}, target);
  CHECK(target.commands[0].slot == manual::kPageFadeSlot);
  CHECK(target.At<TexturedQuad>(0).corners == Corners(152, 40, 168, 56));
  CheckFadeRows(target, 52, 1);
  CheckUpperFadeChunkPages(target);
  CheckUpperFadeTiles(target);
}

TEST_CASE("Lower scroll fade dims rows 187 up to 172 above the down arrow",
          "[manual-page]") {
  RecordingTarget target;
  manual::DrawLowerScrollFade({.animation_state = 10}, target);
  CHECK(target.At<TexturedQuad>(0).corners == Corners(152, 188, 168, 204));
  CHECK(target.At<TexturedQuad>(0).texels[0] == manual::PageTexel{16, 48});
  CheckFadeRows(target, 187, -1);
  CHECK(target.At<TexturePage>(65).page == 0x40);
}

TEST_CASE("Scroll fades show for the no-scroll first middle and last pages",
          "[manual-page]") {
  using manual::ScrollFades;
  CHECK(manual::VisibleScrollFades(0, 0) == ScrollFades{});
  CHECK(manual::VisibleScrollFades(0, 12) ==
        ScrollFades{.upper = false, .lower = true});
  CHECK(manual::VisibleScrollFades(4, 12) ==
        ScrollFades{.upper = true, .lower = true});
  CHECK(manual::VisibleScrollFades(12, 12) ==
        ScrollFades{.upper = true, .lower = false});
}

void CheckControlLines(const RecordingTarget& target) {
  const auto& first = target.At<GradientLine>(0);
  CHECK(target.commands[0].slot == manual::kPagePanelSlot);
  CHECK(first.from == PagePoint{16, 18});
  CHECK(first.to == PagePoint{111, 18});
  CHECK(first.from_color == PageColor{64, 56, 32});
  CHECK(first.to_color == PageColor{16, 16, 8});
  const auto& last = target.At<GradientLine>(10);
  CHECK(last.from == PagePoint{16, 28});
  CHECK(last.to == PagePoint{101, 28});
}

void CheckControlShadow(const RecordingTarget& target) {
  const auto& shadow = target.At<FlatQuad>(11);
  CHECK(target.commands[11].slot == manual::kPagePanelSlot);
  CHECK(shadow.corners ==
        manual::PageCorners{{{18, 20}, {113, 20}, {18, 31}, {102, 31}}});
  CHECK(manual::PackFlatQuad(shadow)[0] == 0x28000000U);
  CHECK(manual::PackGradientLine(target.At<GradientLine>(0)) ==
        std::array<std::uint32_t, 4>{0x50203840U, (18U << 16U) | 16U,
                                     0x00081010U, (18U << 16U) | 111U});
}

TEST_CASE("Control hint boxes shade one shorter line per row over a shadow",
          "[manual-page]") {
  RecordingTarget target;
  CHECK(manual::DrawControlsBackground(
            {.x = 16, .y = 18, .width = 96, .height = 12}, target) ==
        PageDrawResult::kDrawn);
  REQUIRE(target.commands.size() == 12);
  CheckControlLines(target);
  CheckControlShadow(target);
}

TEST_CASE("Control hint boxes bound their rows", "[manual-page]") {
  RecordingTarget flat;
  CHECK(manual::DrawControlsBackground(
            {.x = 0, .y = 5, .width = 4, .height = 1}, flat) ==
        PageDrawResult::kDrawn);
  REQUIRE(flat.commands.size() == 1);
  CHECK(flat.At<FlatQuad>(0).corners[manual::kBottomRight] == PagePoint{5, 7});

  // y + h - 1 wraps below y: the original drew no rows.
  RecordingTarget wrapped;
  constexpr std::int32_t kMax = std::numeric_limits<std::int32_t>::max();
  CHECK(manual::DrawControlsBackground(
            {.x = 0, .y = kMax - 1, .width = 4, .height = 4}, wrapped) ==
        PageDrawResult::kDrawn);
  CHECK(wrapped.commands.size() == 1);

  RecordingTarget tall;
  CHECK(manual::DrawControlsBackground(
            {.x = 0,
             .y = 0,
             .width = 4,
             .height = static_cast<std::int32_t>(manual::kMaxControlRows) + 1},
            tall) == PageDrawResult::kDrawn);
  CHECK(tall.commands.size() ==
        static_cast<std::size_t>(manual::kMaxControlRows) + 1);
  RecordingTarget refused;
  CHECK(manual::DrawControlsBackground(
            {.x = 0,
             .y = 0,
             .width = 4,
             .height = static_cast<std::int32_t>(manual::kMaxControlRows) + 2},
            refused) == PageDrawResult::kRefused);
  CHECK(refused.commands.empty());
}

void CheckContentOrder(const RecordingContent& target, const ScreenRect& clip) {
  REQUIRE(target.events.size() == 5);
  CHECK(target.events[0].kind == ContentEvent::Kind::kArea);
  CHECK(target.events[0].area == clip);
  CHECK(target.events[1].kind == ContentEvent::Kind::kLines);
  CHECK(target.events[2].kind == ContentEvent::Kind::kIllustration);
  CHECK(target.events[2].offset == 15);
  CHECK(target.events[3].offset == 19);
  CHECK(target.events[4].kind == ContentEvent::Kind::kArea);
}

TEST_CASE("Page content clips its lines and illustrations to the panel",
          "[manual-page]") {
  const std::array<std::int16_t, 2> offsets{3, 7};
  const std::vector<std::int16_t> text = HelpTextWithIllustrations(offsets);
  RecordingContent target;
  const ScreenRect clip{.x = 0, .y = 8, .width = 320, .height = 224};
  CHECK(manual::DrawPageContent(text, clip, target) == PageDrawResult::kDrawn);
  CheckContentOrder(target, clip);
  CHECK(target.events[4].area ==
        ScreenRect{.x = 13, .y = 55, .width = 300, .height = 130});
}

TEST_CASE("Page content follows the right buffer and skips empty lists",
          "[manual-page]") {
  const std::array<std::int16_t, 2> offsets{3, 7};
  const std::vector<std::int16_t> text = HelpTextWithIllustrations(offsets);
  RecordingContent right;
  CHECK(manual::DrawPageContent(text,
                                {.x = 320, .y = 8, .width = 320, .height = 224},
                                right) == PageDrawResult::kDrawn);
  REQUIRE(right.events.size() == 5);
  CHECK(right.events[4].area.x == 333);

  RecordingContent empty;
  CHECK(manual::DrawPageContent(HelpTextWithIllustrations({}),
                                {.width = 320, .height = 224},
                                empty) == PageDrawResult::kDrawn);
  CHECK(empty.events.size() == 3);
}

TEST_CASE("Page content refuses illustration lists past the help text",
          "[manual-page]") {
  RecordingContent target;
  const ScreenRect clip{.width = 320, .height = 224};
  const std::array<std::int16_t, 1> inside{1};
  std::vector<std::int16_t> text = HelpTextWithIllustrations(inside, 0);
  CHECK(manual::DrawPageContent(text, clip, target) == PageDrawResult::kDrawn);
  target.events.clear();
  text[13] = 2;
  CHECK(manual::DrawPageContent(text, clip, target) ==
        PageDrawResult::kRefused);
  text[13] = 1;
  text[12] = 2;
  CHECK(manual::DrawPageContent(text, clip, target) ==
        PageDrawResult::kRefused);
  PutWord(text, 0, 0x7FFFFFF0);
  CHECK(manual::DrawPageContent(text, clip, target) ==
        PageDrawResult::kRefused);
  PutWord(text, 0, 0xFFFFFFF0);
  CHECK(manual::DrawPageContent(text, clip, target) ==
        PageDrawResult::kRefused);
  CHECK(manual::DrawPageContent(std::span(text).first(1), clip, target) ==
        PageDrawResult::kRefused);
  CHECK(target.events.empty());
}

// --- Live bindings ----------------------------------------------------------

// The globals the Live page entries read or write, restored on exit.
class PageGlobalsSnapshot {
 public:
  PageGlobalsSnapshot()
      : scratch_(VS::Render::FrameScratchpad()),
        back_env_(VS::Render::legacyDrawEnvironments[1]),
        primitives_(VS::Render::legacyPrimitiveBuffers[0]),
        ordering_(VS::Render::legacyOrderingTables[0]),
        help_(manual::helpText),
        frame_index_(VS::Render::frameBuf),
        arrow_state_(manual::pageArrowAnimState),
        held_(VS::Input::previousButtons) {}
  PageGlobalsSnapshot(const PageGlobalsSnapshot&) = delete;
  PageGlobalsSnapshot& operator=(const PageGlobalsSnapshot&) = delete;
  PageGlobalsSnapshot(PageGlobalsSnapshot&&) = delete;
  PageGlobalsSnapshot& operator=(PageGlobalsSnapshot&&) = delete;
  ~PageGlobalsSnapshot() {
    VS::Input::previousButtons = held_;
    manual::pageArrowAnimState = arrow_state_;
    VS::Render::frameBuf = frame_index_;
    manual::helpText = help_;
    VS::Render::legacyOrderingTables[0] = ordering_;
    VS::Render::legacyPrimitiveBuffers[0] = primitives_;
    VS::Render::legacyDrawEnvironments[1] = back_env_;
    VS::Render::FrameScratchpad() = scratch_;
  }

 private:
  VS::Render::FrameScratch scratch_;
  DRAWENV back_env_;
  void* primitives_;
  void* ordering_;
  int* help_;
  int frame_index_;
  int arrow_state_;
  int held_;
};

constexpr std::size_t kPacketWords = 0x8000;
constexpr std::size_t kTableWords = 0x822;
constexpr std::size_t kFrameSlotBase = 18;

// A primitive buffer, ordering table and game heap bound as frame 0.
struct PageFrame {
  PageGlobalsSnapshot globals;
  std::vector<u_long> storage = std::vector<u_long>(kPacketWords, 0xDEADBEEF);
  std::vector<u_long> ordering = std::vector<u_long>(kTableWords);
  alignas(VS::Core::Memory::kHeapUnitBytes)
      std::array<std::byte, std::size_t{16} * 1024> heap_bytes{};
  VS::Core::Memory::ScopedLiveGameHeap heap{heap_bytes};

  PageFrame() {
    ClearOTagR(ordering.data(), static_cast<int>(ordering.size()));
    VS::Render::legacyOrderingTables[0] = ordering.data();
    VS::Render::legacyPrimitiveBuffers[0] = storage.data();
    VS::Render::FrameScratchpad().ordering_table =
        ordering.data() + kFrameSlotBase;
    VS::Render::FrameScratchpad().primitive_cursor = storage.data();
    VS::Render::frameBuf = 0;
    VS::Input::previousButtons = 0;
    manual::pageArrowAnimState = 0;
  }

  [[nodiscard]] u_long* Word(std::size_t index) {
    return storage.data() + index;
  }
  [[nodiscard]] u_long* Slot(std::int32_t slot) {
    return ordering.data() + kFrameSlotBase + slot;
  }
  [[nodiscard]] std::size_t Used() const {
    return static_cast<std::size_t>(
        static_cast<const u_long*>(
            VS::Render::FrameScratchpad().primitive_cursor) -
        storage.data());
  }
};

std::uint32_t TagLength(const u_long* packet) {
  return static_cast<std::uint32_t>(*packet) >> 24U;
}

void CheckLiveBackgroundPackets(PageFrame& frame) {
  CHECK(nextPrim(frame.Slot(7)) == frame.Word(9));
  CHECK(nextPrim(frame.Word(9)) == frame.Word(0));
  CHECK(TagLength(frame.Word(0)) == 8);
  CHECK(frame.storage[1] == 0x38502000U);
  CHECK(frame.storage[8] == ((187U << 16U) | 317U));
  CHECK(TagLength(frame.Word(9)) == 2);
  CHECK(frame.storage[10] == 0xE1000260U);
  CHECK(frame.storage[11] == 0U);
}

TEST_CASE(
    "Live page background links a gradient and a page into the panel slot",
    "[manual-page-live]") {
  PageFrame frame;
  manual::LiveDrawPageBackground(8, 52, 310, 136, 0);
  CHECK(frame.Used() == 12);
  CheckLiveBackgroundPackets(frame);

  const std::uint64_t refusals = manual::LivePageRefusalCount();
  manual::LiveDrawPageBackground(8, 52, 310, 136, 4);
  CHECK(manual::LivePageRefusalCount() == refusals + 1);
  CHECK(frame.Used() == 12);
}

constexpr std::size_t kFadeWords = 10 + (48 * 10) + (16 * 4) + 3;

void CheckLiveUpperFade(PageFrame& frame) {
  CHECK(frame.Used() == kFadeWords);
  CHECK(nextPrim(frame.Slot(-7)) == frame.Word(kFadeWords - 3));
  CHECK(frame.storage[1] == 0x2D000000U);
  CHECK(frame.storage[2] == ((38U << 16U) | 152U));
  CHECK((frame.storage[3] >> 16U) == 0x37F8U);
  CHECK(frame.storage[11] == 0x2E808080U);
  CHECK(frame.storage[kFadeWords - 2] == 0xE1000240U);
}

void CheckLiveLowerFade(PageFrame& frame) {
  CHECK(frame.Used() == 2 * kFadeWords);
  CHECK(frame.storage[kFadeWords + 2] == ((186U << 16U) | 152U));
  CHECK((frame.storage[kFadeWords + 3] >> 16U) == 0x37F9U);
  CHECK((frame.storage[kFadeWords + 3] & 0xFFU) == 16U);
  CHECK(frame.storage[kFadeWords + 12] == ((187U << 16U) | 0U));
}

TEST_CASE(
    "Live scroll fades link the arrow rows tiles and page into the fade slot",
    "[manual-page-live]") {
  PageFrame frame;
  VS::Input::previousButtons = VS::Legacy::kPadRleft;
  manual::pageArrowAnimState = 4;
  manual::LiveDrawUpperScrollFade();
  CheckLiveUpperFade(frame);

  VS::Input::previousButtons = 0;
  manual::LiveDrawLowerScrollFade();
  CheckLiveLowerFade(frame);
}

TEST_CASE("Live page arrows refuse unknown types and full buffers",
          "[manual-page-live]") {
  PageFrame frame;
  const std::uint64_t refusals = manual::LivePageRefusalCount();
  manual::LiveDrawPageArrow(manual::PageArrow::kDown);
  CHECK(frame.Used() == 10);
  CHECK(nextPrim(frame.Slot(-7)) == frame.Word(0));
  // A generated caller passing a value outside the enum is refused.
  // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
  manual::LiveDrawPageArrow(static_cast<manual::PageArrow>(2));
  CHECK(manual::LivePageRefusalCount() == refusals + 1);

  // Room for the arrow but not the whole fade: nothing is linked.
  VS::Render::FrameScratchpad().primitive_cursor =
      frame.Word(frame.storage.size() - 100);
  manual::LiveDrawUpperScrollFade();
  CHECK(manual::LivePageRefusalCount() == refusals + 2);
  CHECK(frame.Used() == frame.storage.size() - 100);
  CHECK(nextPrim(frame.Slot(-7)) == frame.Word(0));

  VS::Render::FrameScratchpad().primitive_cursor = frame.Word(0);
  VS::Render::FrameScratchpad().ordering_table = frame.ordering.data() + 3;
  manual::LiveDrawPageArrow(manual::PageArrow::kUp);
  CHECK(manual::LivePageRefusalCount() == refusals + 3);
  CHECK(frame.Used() == 0);
}

void CheckLiveControlPackets(PageFrame& frame) {
  CHECK(frame.Used() == (11 * 5) + 6);
  CHECK(nextPrim(frame.Slot(7)) == frame.Word(55));
  CHECK(nextPrim(frame.Word(5)) == frame.Word(0));
  CHECK(TagLength(frame.Word(0)) == 4);
  CHECK(frame.storage[1] == 0x50203840U);
  CHECK(TagLength(frame.Word(55)) == 5);
  CHECK(frame.storage[60] == ((31U << 16U) | 102U));
}

TEST_CASE(
    "Live control hint boxes link their lines and shadow into the panel slot",
    "[manual-page-live]") {
  PageFrame frame;
  manual::LiveDrawControlsBackground(16, 18, 96, 12);
  CheckLiveControlPackets(frame);

  const std::uint64_t refusals = manual::LivePageRefusalCount();
  manual::LiveDrawControlsBackground(0, 0, 4, 1000);
  CHECK(manual::LivePageRefusalCount() == refusals + 1);
  CHECK(frame.Used() == 61);
}

void CheckLiveContentAreas(PageFrame& frame) {
  CHECK(frame.Used() == 6);
  CHECK(nextPrim(frame.Slot(0)) == frame.Word(3));
  CHECK(nextPrim(frame.Word(3)) == frame.Word(0));
  CHECK(TagLength(frame.Word(0)) == 2);
  CHECK(frame.storage[1] == 0xE3000140U);
  CHECK(frame.storage[2] == (0xE4000000U | (239U << 10U) | 639U));
  CHECK(frame.storage[4] == (0xE3000000U | (55U << 10U) | 333U));
  CHECK(frame.storage[5] == (0xE4000000U | (184U << 10U) | 632U));
}

TEST_CASE("Live page content restores the back buffer clip around the panel",
          "[manual-page-live]") {
  PageFrame frame;
  // No illustrations, and a diagram line section with no lines.
  std::vector<std::int16_t> text = HelpTextWithIllustrations({}, 16);
  auto* payload = static_cast<std::int16_t*>(
      VS::Core::Memory::AllocateLiveFromHighEnd(text.size() * 2));
  REQUIRE(payload != nullptr);
  std::ranges::copy(text, payload);
  manual::helpText = reinterpret_cast<int*>(payload);
  setRECT(&VS::Render::legacyDrawEnvironments[1].clip, 320, 0, 320, 240);

  manual::LiveDrawPageContent();
  CheckLiveContentAreas(frame);

  const std::uint64_t refusals = manual::LivePageRefusalCount();
  payload[12] = 100;
  manual::LiveDrawPageContent();
  manual::helpText = reinterpret_cast<int*>(text.data());
  manual::LiveDrawPageContent();
  CHECK(manual::LivePageRefusalCount() == refusals + 2);
  CHECK(frame.Used() == 6);
}
}  // namespace
