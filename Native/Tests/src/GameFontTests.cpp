#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Render/DisplayControl.hpp"
#include "Render/EncodedText.hpp"
#include "Render/GameFont.hpp"
#include "Render/ScaledFont.hpp"
#include "Render/Sprites.hpp"
#include "Render/TextBoxText.hpp"
#include "Render/UiPackets.hpp"

// Contracts of the #276 row 6 font slice: game-encoded text, the variable
// width and compact fonts and the dialogue text box drawers (BATTLE.PRG
// 5BF94.c:1345-2330).
namespace {
namespace Render = VS::Render;
using Bytes = std::vector<std::uint8_t>;
using Packet = std::vector<std::uint32_t>;

constexpr std::uint32_t kChainEnd = 0x123456;
constexpr std::uint32_t kFirstHandle = 0x1000;

// Links each packet in front of the last, as the frame's chain does, so a
// walk from the head gives the draw order.
class ChainQueue final : public Render::PacketQueue {
 public:
  std::vector<Packet> packets;
  bool full = false;

  [[nodiscard]] std::uint32_t ChainLink() const override { return head_; }

  bool Emit(std::span<const std::uint32_t> packet) override {
    if (full) {
      return false;
    }
    packets.emplace_back(packet.begin(), packet.end());
    head_ = kFirstHandle + static_cast<std::uint32_t>(packets.size() - 1);
    return true;
  }

  // The packets from the chain head to kChainEnd.
  [[nodiscard]] std::vector<Packet> DrawOrder() const {
    std::vector<Packet> order;
    std::uint32_t link = head_;
    while (link != kChainEnd && order.size() <= packets.size()) {
      const Packet& packet = packets.at(link - kFirstHandle);
      order.push_back(packet);
      link = packet.front() & Render::kPacketLinkMask;
    }
    return order;
  }

 private:
  std::uint32_t head_ = kChainEnd;
};

class QuadTarget final : public Render::ScaledGlyphTarget {
 public:
  std::vector<Render::ScaledGlyphQuad> quads;

  [[nodiscard]] bool CanDraw() const override { return true; }
  void Draw(const Render::ScaledGlyphQuad& quad) override {
    quads.push_back(quad);
  }
};

constexpr std::uint32_t Position(int x, int y) {
  return static_cast<std::uint32_t>(static_cast<std::uint16_t>(x)) |
         (static_cast<std::uint32_t>(y) << 16);
}

constexpr std::int32_t Packed(int x, int y) {
  return static_cast<std::int32_t>(Position(x, y));
}

constexpr std::uint32_t Tag(std::uint32_t link, std::size_t words) {
  return link | (static_cast<std::uint32_t>(words - 1) << 24);
}

std::optional<std::size_t> Format(Bytes& out, const Bytes& source,
                                  const Render::EncodedTextArguments& args) {
  return Render::FormatEncodedText(out, source, args);
}

Bytes Digits(std::uint8_t code, std::uint8_t parameter, std::int32_t value) {
  const auto view = Render::EncodeTextInteger(code, parameter, value).View();
  return {view.begin(), view.end()};
}

// A width table with every glyph six pixels wide.
std::array<std::uint8_t, Render::kVariableFontGlyphCount> SixPixelWidths() {
  std::array<std::uint8_t, Render::kVariableFontGlyphCount> widths{};
  widths.fill(6);
  return widths;
}

// The glyph id a variable-width packet's texture word selects.
std::uint32_t GlyphOf(const Packet& packet) {
  const std::uint32_t texture = packet.at(Render::kSpriteTextureWord);
  return ((texture & 0xFF) / 12) + (((texture >> 8) & 0xFF) / 12 * 21);
}

std::int16_t XOf(const Packet& packet) {
  return static_cast<std::int16_t>(packet.at(Render::kSpritePositionWord));
}
}  // namespace

TEST_CASE("Encoded integers print the original digit glyphs", "[render-font]") {
  using Render::kTextPrintDecimal;
  using Render::kTextPrintHex;
  CHECK(Digits(kTextPrintDecimal, 0, 1234) == Bytes{1, 2, 3, 4});
  CHECK(Digits(kTextPrintDecimal, 3, 0) == Bytes{0});
  CHECK(Digits(kTextPrintDecimal, 0, -5) == Bytes{Render::kTextMinusGlyph, 5});
  CHECK(Digits(kTextPrintHex, 0, 0x1F) == Bytes{1, 15});
  // From parameter 10 the tens digit is the digit count.
  CHECK(Digits(kTextPrintDecimal, 30, 7) == Bytes{0, 0, 7});
  CHECK(Digits(kTextPrintDecimal, 20, 1234) == Bytes{3, 4});
  // Sixteen to the eighth wraps to zero, so nothing is printed.
  CHECK(Digits(kTextPrintHex, 90, 0x1234).empty());
  // INT_MIN negates to itself rather than trapping.
  CHECK(Digits(kTextPrintDecimal, 0, INT32_MIN).front() ==
        Render::kTextMinusGlyph);
}

namespace {
// Text printing integer 2 (42) and string 1 (glyph 9) between glyph 1 and a
// sound code. The arguments view `name`, so a fixture is never copied.
struct FormatFixture {
  Render::EncodedTextArguments args;
  Bytes name{9, Render::kTextTerminator};
  Bytes source{1, Render::kTextPrintDecimal, 2, Render::kTextPrintString,
               1, Render::kTextSoundEffect,  7, Render::kTextTerminator};
  Bytes expected{
      1, 4, 2, 9, Render::kTextSoundEffect, 7, Render::kTextTerminator};
  Bytes out = Bytes(16, 0xAA);

  FormatFixture() {
    args.integers[2] = 42;
    args.strings[1] = name;
  }
};
}  // namespace

TEST_CASE("Encoded text formatting expands integer and string references",
          "[render-font]") {
  FormatFixture fixture;
  CHECK(Render::EncodedTextBytes(fixture.source, fixture.args) ==
        fixture.expected.size());
  CHECK(Format(fixture.out, fixture.source, fixture.args) ==
        fixture.expected.size() - 1);
  CHECK(std::equal(fixture.expected.begin(), fixture.expected.end(),
                   fixture.out.begin()));
  CHECK(fixture.out[fixture.expected.size()] == 0xAA);
}

TEST_CASE("Encoded text formatting leaves a short destination unchanged",
          "[render-font]") {
  FormatFixture fixture;
  Bytes small(fixture.expected.size() - 1, 0xAA);
  CHECK_FALSE(Format(small, fixture.source, fixture.args).has_value());
  CHECK(std::ranges::all_of(small,
                            [](std::uint8_t byte) { return byte == 0xAA; }));
}

TEST_CASE("Encoded text formatting refuses unterminated sources",
          "[render-font]") {
  FormatFixture fixture;
  const Bytes open{1, 2, 3};
  CHECK_FALSE(Format(fixture.out, open, fixture.args).has_value());
  const Bytes cut{1, Render::kTextSpacing};
  CHECK_FALSE(Format(fixture.out, cut, fixture.args).has_value());
}

TEST_CASE("Encoded text formatting refuses unset and out-of-range strings",
          "[render-font]") {
  FormatFixture fixture;
  const Bytes unset{Render::kTextPrintString, 4, Render::kTextTerminator};
  CHECK_FALSE(Format(fixture.out, unset, fixture.args).has_value());
  const Bytes past{Render::kTextPrintString, 10, Render::kTextTerminator};
  CHECK_FALSE(Format(fixture.out, past, fixture.args).has_value());
}

TEST_CASE("Encoded text formatting stops self-references at the nesting limit",
          "[render-font]") {
  FormatFixture fixture;
  const Bytes loop{Render::kTextPrintString, 0, Render::kTextTerminator};
  fixture.args.strings[0] = loop;
  CHECK_FALSE(Format(fixture.out, loop, fixture.args).has_value());
}

TEST_CASE("Encoded line measurement stops at the line's end", "[render-font]") {
  std::array<std::uint8_t, 4> widths{0, 6, 12, 0};
  const Bytes text{1,
                   2,
                   Render::kTextSpacing,
                   6,
                   Render::kTextPrintDecimal,
                   20,
                   Render::kTextChunkSize,
                   3,
                   1,
                   0xE0,
                   Render::kTextNewline,
                   2};
  // 6 + 12 + 6 + two digits (12) + 6, and glyph 0xE0 is past the table.
  CHECK(Render::MeasureEncodedTextLine(text, widths) == 7);
  const Bytes paged{2, Render::kTextPageBreak, 2};
  CHECK(Render::MeasureEncodedTextLine(paged, widths) == 2);
  const Bytes cut{2, Render::kTextSpacing};
  CHECK(Render::MeasureEncodedTextLine(cut, widths) == 2);
}

TEST_CASE("Variable-width glyphs build the original sprite packet",
          "[render-font]") {
  auto widths = SixPixelWidths();
  widths[Render::kVariableFontGlyphsPerTable + 5] = 7;
  Render::FontStyle style{.table_select = 1, .palette_offset = 4, .color = 384};
  ChainQueue queue;

  CHECK(Render::DrawVariableWidthGlyph(queue, style, widths, 5, 10, 20) == 17);
  REQUIRE(queue.packets.size() == 1);
  // Glyph 194 is cell (5, 9); CLUT (832 + 16 * 4, 222); brightness 384 is
  // the semi-transparent sprite.
  const Packet expected{Tag(kChainEnd, 6),
                        0xE100002D,
                        0x66808080,
                        Position(10, 20),
                        60U | (108U << 8) | (0x37B8U << 16),
                        12U | (12U << 16)};
  CHECK(queue.packets[0] == expected);

  CHECK(Render::DrawVariableWidthGlyph(queue, style, widths,
                                       Render::kTextSpaceGlyph, 10, 20) == 16);
  CHECK(Render::DrawVariableWidthGlyph(
            queue, style, std::span(widths).first(100), 5, 10, 20) == 10);
  CHECK(queue.packets.size() == 1);

  Render::SetMenuFontStyle(style, 3);
  CHECK(style.table_select == 0);
  CHECK(style.palette_offset == 3);
  CHECK(style.color == Render::kMenuFontColor);
}

namespace {
constexpr std::uint32_t kCompactClut = 0x37F6U << 16;
constexpr std::uint32_t kCompactColor = 0x64000000U | Render::kCompactTextGray;

// 'A' (33) is five wide and 'B' (34) six, "AB" trims one pixel and '*' (10)
// is four wide. The tables view the arrays, so a fixture is never copied.
struct CompactFixture {
  std::array<std::uint8_t, 64> width_pairs{};
  std::array<std::uint8_t, 169> trims{};
  std::array<std::uint8_t, 96> cells{};
  Render::CompactFontTables tables{width_pairs, trims, cells};

  CompactFixture() {
    width_pairs[16] = 0x50;
    width_pairs[17] = 0x06;
    width_pairs[5] = 0x04;
    trims[0] = 0x04;
    cells[33] = 0x15;
    cells[34] = 0x20;
  }

  bool Draw(ChainQueue& queue, const char* text, std::int32_t xy) const {
    return Render::DrawCompactText(queue, tables, text, xy,
                                   Render::kCompactTextGray);
  }
};
}  // namespace

TEST_CASE("Compact text draws its glyphs in order after its draw mode",
          "[render-font]") {
  const CompactFixture font;
  ChainQueue queue;
  REQUIRE(font.Draw(queue, "AB", Packed(10, 20)));
  const auto order = queue.DrawOrder();
  REQUIRE(order.size() == 3);
  CHECK(order[0] == Packet{Tag(kFirstHandle + 1, 2), 0xE100000C});
  CHECK(order[1] == Packet{Tag(kFirstHandle, 5), kCompactColor,
                           Position(10, 20), 0x14U | (86U << 8) | kCompactClut,
                           5U | (10U << 16)});
  CHECK(order[2] == Packet{Tag(kChainEnd, 5), kCompactColor, Position(14, 20),
                           0x20U | (76U << 8) | kCompactClut,
                           6U | (10U << 16)});
}

TEST_CASE("Compact text right-aligns on '#' and centres on '$'",
          "[render-font]") {
  const CompactFixture font;
  ChainQueue right;
  REQUIRE(font.Draw(right, "#AB", Packed(30, 0)));
  CHECK(right.DrawOrder()[1][2] == Position(20, 0));
  ChainQueue centre;
  REQUIRE(font.Draw(centre, "$AB", Packed(30, 0)));
  CHECK(centre.DrawOrder()[1][2] == Position(25, 0));
}

TEST_CASE("Compact text steps back on '[', skips lower case and draws '*'",
          "[render-font]") {
  const CompactFixture font;
  ChainQueue marks;
  REQUIRE(font.Draw(marks, "[a*", Packed(10, 5)));
  const auto star = marks.DrawOrder();
  REQUIRE(star.size() == 2);
  CHECK(star[1] == Packet{Tag(kChainEnd, 5), kCompactColor, Position(8, 4),
                          232U | (75U << 8) | kCompactClut, 4U | (11U << 16)});
}

TEST_CASE("Compact text reports a packet the full queue dropped",
          "[render-font]") {
  const CompactFixture font;
  ChainQueue full;
  full.full = true;
  CHECK_FALSE(font.Draw(full, "AB", 0));
}

namespace {
struct ProgressiveFixture {
  std::array<std::uint8_t, Render::kVariableFontGlyphCount> widths =
      SixPixelWidths();
  Render::EncodedTextArguments args;
  Render::FontStyle style;
  Render::TextBoxInput input{.confirm_pressed = false, .tick_speed = 2};
  ChainQueue queue;
  Bytes text;
  Render::TextBoxText box;

  ProgressiveFixture() {
    box.kind = 2;
    box.speed = 10;
    box.brightness = 0x80;
    box.x_indent = 40;
    box.y_indent = 50;
    box.speaker_texture = 0x20;
  }

  Render::ProgressiveTextResult Draw(Bytes encoded) {
    text = std::move(encoded);
    box.text = text;
    return Render::DrawProgressiveTextBoxText(box, input, style, widths, args,
                                              queue);
  }

  [[nodiscard]] std::vector<std::uint32_t> Glyphs() const {
    std::vector<std::uint32_t> glyphs;
    for (const Packet& packet : queue.packets) {
      glyphs.push_back(GlyphOf(packet));
    }
    return glyphs;
  }
};
}  // namespace

TEST_CASE("Progressive text reveals the page and finishes on its terminator",
          "[render-font]") {
  ProgressiveFixture fixture;
  const auto result = fixture.Draw({1, 2, Render::kTextTerminator});
  CHECK(fixture.box.done);
  CHECK(fixture.box.previous_chunks == 2);
  CHECK(fixture.Glyphs() == std::vector<std::uint32_t>{1, 2});
  CHECK(XOf(fixture.queue.packets[1]) == 46);
  CHECK(fixture.style.table_select == 0);
  CHECK(fixture.style.color == 0x80);
  CHECK_FALSE(result.next_page.has_value());
  CHECK_FALSE(result.overran);
}

TEST_CASE("Progressive text in a box of speed 0 draws nothing",
          "[render-font]") {
  ProgressiveFixture idle;
  idle.box.speed = 0;
  idle.Draw({1, Render::kTextTerminator});
  CHECK(idle.queue.packets.empty());
  CHECK_FALSE(idle.box.done);
}

TEST_CASE("Progressive text's glyph budget grows by half the tick speed",
          "[render-font]") {
  ProgressiveFixture slow;
  slow.box.speed = 1;
  slow.Draw({1, 2, Render::kTextTerminator});
  CHECK(slow.Glyphs() == std::vector<std::uint32_t>{1});
  CHECK_FALSE(slow.box.done);
  CHECK(slow.box.speed == 2);
}

TEST_CASE("Progressive text records where the page after a break starts",
          "[render-font]") {
  ProgressiveFixture fixture;
  const auto result =
      fixture.Draw({1, Render::kTextPageBreak, 2, Render::kTextTerminator});
  REQUIRE(result.next_page.has_value());
  const Render::TextPosition page =
      result.next_page.value_or(Render::TextPosition{.source = -2});
  CHECK(page.source == Render::kTextBoxString);
  CHECK(page.offset == 2);
  CHECK(fixture.box.speed == 1);
  CHECK(fixture.Glyphs() == std::vector<std::uint32_t>{1});
}

TEST_CASE("Progressive text animates the arrow on the frame's last glyph",
          "[render-font]") {
  ProgressiveFixture fixture;
  fixture.box.speed = 2;
  fixture.box.arrow_frame = 7;
  fixture.Draw({1, Render::kTextConfirm, 2, Render::kTextTerminator});
  CHECK(fixture.Glyphs() == std::vector<std::uint32_t>{1, 0xBB});
  CHECK(fixture.box.arrow_frame == 9);
}

TEST_CASE("Progressive text skips the arrow on confirm", "[render-font]") {
  ProgressiveFixture fixture;
  fixture.box.speed = 2;
  fixture.input.confirm_pressed = true;
  fixture.Draw({1, Render::kTextConfirm, 2, Render::kTextTerminator});
  CHECK(fixture.Glyphs() == std::vector<std::uint32_t>{1});
  CHECK(fixture.box.speed == 3);
}

TEST_CASE("Progressive text follows integer and string references",
          "[render-font]") {
  ProgressiveFixture fixture;
  fixture.args.integers[3] = 42;
  const Bytes name{7, Render::kTextTerminator};
  fixture.args.strings[1] = name;
  const auto result =
      fixture.Draw({Render::kTextPrintDecimal, 3, Render::kTextPrintString, 1,
                    Render::kTextSoundEffect, 5, 8, Render::kTextTerminator});
  CHECK(fixture.Glyphs() == std::vector<std::uint32_t>{4, 2, 7, 8});
  CHECK(fixture.box.done);
  CHECK(result.missing_strings == 0);
  CHECK(result.sound == 5);
}

TEST_CASE("Progressive text prints unset and out-of-range strings empty",
          "[render-font]") {
  ProgressiveFixture missing;
  const auto counted =
      missing.Draw({Render::kTextPrintString, 4, 1, Render::kTextPrintString,
                    12, 2, Render::kTextTerminator});
  CHECK(missing.Glyphs() == std::vector<std::uint32_t>{1, 2});
  CHECK(counted.missing_strings == 2);
  CHECK(missing.box.done);
}

TEST_CASE("Progressive text stops a self-referencing string at the step cap",
          "[render-font]") {
  ProgressiveFixture loop;
  const Bytes self{Render::kTextPrintString, 0, Render::kTextTerminator};
  loop.args.strings[0] = self;
  const auto capped = loop.Draw(self);
  CHECK(capped.overran);
  CHECK(loop.queue.packets.empty());
  CHECK_FALSE(loop.box.done);
}

TEST_CASE("Progressive text font codes select the table and reset the speaker",
          "[render-font]") {
  ProgressiveFixture styled;
  styled.box.speaker_texture = 0;
  const auto reset =
      styled.Draw({Render::kTextFontControl, 5, 1, Render::kTextTerminator});
  CHECK(styled.Glyphs() ==
        std::vector<std::uint32_t>{1 + Render::kVariableFontGlyphsPerTable});
  CHECK(styled.style.table_select == 1);
  CHECK(reset.reset_speaker);
}

TEST_CASE("Scaled text box text lays out lines about the box frame",
          "[render-font]") {
  auto widths = SixPixelWidths();
  const Bytes text{5, Render::kTextNewline, 5, Render::kTextTerminator};
  Render::TextBoxText box;
  box.text = text;
  box.kind = 2;
  box.speed = 1;
  box.brightness = 0x80;
  box.frame_x = 10;
  box.x_indent = 4;
  box.frame_y = 20;
  box.y_indent = 2;
  Render::FontStyle style;
  QuadTarget target;

  Render::DrawScaledTextBoxText(box, 1 << 16, style, widths, target);
  REQUIRE(target.quads.size() == 2);
  CHECK(target.quads[0].left == target.quads[1].left);
  CHECK(target.quads[1].top - target.quads[0].top == 13);
  CHECK(style.color == 0x2C808080);

  box.speed = 0;
  QuadTarget idle;
  Render::DrawScaledTextBoxText(box, 1 << 16, style, widths, idle);
  CHECK(idle.quads.empty());
}

TEST_CASE("Glyph blending mixes the cell's colours into the pixels",
          "[render-font]") {
  std::array<std::uint16_t, Render::kGlyphCellPixels> cell{};
  cell.fill(0x1111);
  cell[0] = 0x0011;  // Pixels 2 and 3 of the first row are clear.
  std::array<std::uint16_t, Render::kGlyphPixelExpansionCount> expansion{};
  expansion[1] = 0x7FFF;
  std::vector<std::uint16_t> pixels(std::size_t{12} * 12, 0);

  Render::BlendGlyphIntoPixels(pixels, 12, cell, expansion,
                               Render::kFullGlyphAlpha);
  CHECK(pixels[0] == 0x7FFF);
  CHECK(pixels[2] == 0);
  CHECK(pixels[143] == 0x7FFF);

  std::ranges::fill(pixels, 0);
  Render::BlendGlyphIntoPixels(pixels, 12, cell, expansion, 0x8000);
  CHECK(pixels[0] == 0x3DEF);

  std::vector<std::uint16_t> short_span(20, 0);
  Render::BlendGlyphIntoPixels(short_span, 12, cell, expansion,
                               Render::kFullGlyphAlpha);
  CHECK(short_span[19] == 0x7FFF);
}

namespace {
class FakeVram final : public Render::VramPixelTarget {
 public:
  std::vector<Render::ScreenRect> reads;
  std::vector<Render::ScreenRect> uploads;
  std::vector<std::uint16_t> uploaded;

  void Read(const Render::ScreenRect& area,
            std::span<std::uint16_t> pixels) override {
    reads.push_back(area);
    std::ranges::fill(pixels, area.width == 3 ? 0x1111 : 0x1234);
  }
  void Upload(const Render::ScreenRect& area,
              std::span<const std::uint16_t> pixels) override {
    uploads.push_back(area);
    uploaded.assign(pixels.begin(), pixels.end());
  }
};

// One table-one glyph in a 4x2 box at (32, 32); the reads return 0x1111 for
// a glyph cell and 0x1234 for the framebuffer. The tables and the box view
// the fixture's members, so a fixture is never copied.
struct BlendFixture {
  std::array<std::uint8_t, Render::kVariableFontGlyphCount> widths =
      SixPixelWidths();
  std::array<std::uint16_t, Render::kGlyphPixelExpansionCount> expansion{};
  Render::VariableFontTables font{widths, expansion};
  Bytes text{0, Render::kTextTerminator};
  Render::TextBoxText box;
  Render::FontStyle style;
  FakeVram vram;

  BlendFixture() {
    expansion[1] = 0x7C00;
    box.text = text;
    box.x = box.x_indent = 32;
    box.y = box.y_indent = 32;
    box.chars_per_line = 4;
    box.line_count = 2;
    box.speed = 1;
  }

  bool Blend() {
    return Render::BlendTextBoxIntoFramebuffer(
        box, {.alpha = 0x10000, .framebuffer_x = 320, .skip_pressed = true},
        style, font, vram);
  }
};
}  // namespace

TEST_CASE("Framebuffer text uploads the box once and finishes it on skip",
          "[render-font]") {
  BlendFixture fixture;
  REQUIRE(fixture.Blend());
  REQUIRE(fixture.vram.uploads.size() == 1);
  CHECK(fixture.vram.uploads[0].x == 352);
  CHECK(fixture.vram.uploads[0].width == 58);
  CHECK(fixture.vram.uploads[0].height == 30);
  CHECK(fixture.style.table_select == 1);
  CHECK(fixture.box.speed == Render::kTextBoxSkipSpeed);
}

TEST_CASE("Framebuffer text blends table-one glyph cells into the box",
          "[render-font]") {
  BlendFixture fixture;
  REQUIRE(fixture.Blend());
  // Glyph 189 is the first cell of table one.
  REQUIRE(fixture.vram.reads.size() == 2);
  CHECK(fixture.vram.reads[1].x == 832);
  CHECK(fixture.vram.reads[1].y == 108);
  CHECK(fixture.vram.uploaded[0] == 0x7C00);
  CHECK(fixture.vram.uploaded[12] == 0x1234);
}

TEST_CASE("Framebuffer text refuses a box off the screen", "[render-font]") {
  BlendFixture fixture;
  fixture.box.x = -1;
  CHECK_FALSE(Render::BlendTextBoxIntoFramebuffer(
      fixture.box, {.alpha = 0x10000}, fixture.style, fixture.font,
      fixture.vram));
  CHECK(fixture.vram.uploads.empty());
}
