#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Battle/FloatingLabelRendering.hpp"

namespace {
namespace b = VS::Battle;
namespace r = VS::Render;
constexpr b::FloatingLabelScreenPoint kOrigin{100, 100};
constexpr b::FloatingLabelAtlasRect kCaption{150, 64, 26, 12};
constexpr std::array<std::int16_t, 5> kHalfHeights{1, 2, 3, 4, 5};

struct TextDraw {
  std::string text;
  b::FloatingLabelScreenPoint origin;
  std::uint32_t color = 0;
};

class World final : public b::FloatingLabelRenderWorld {
 public:
  static constexpr std::array<b::FloatingLabelAtlasRect, 4> kPieces{
      {{32, 24, 8, 10}, {40, 24, 10, 10}, {50, 24, 6, 10}, {56, 24, 8, 10}}};
  std::optional<b::FloatingLabelAtlasRect> caption = kCaption;
  std::vector<b::FloatingLabelQuad> quads;
  std::vector<TextDraw> text;
  std::optional<std::size_t> refuse_quad;
  std::uint8_t mode = 0;
  std::optional<b::FloatingLabelAtlasRect> ValueCaption(
      std::uint8_t raw_mode) override {
    mode = raw_mode;
    return caption;
  }
  std::optional<std::int16_t> LockedHalfHeight(std::int32_t step) override {
    if (step < 0 || static_cast<std::size_t>(step) >= kHalfHeights.size()) {
      return std::nullopt;
    }
    return kHalfHeights[static_cast<std::size_t>(step)];
  }
  std::optional<b::FloatingLabelAtlasRect> LockedPiece(
      std::size_t piece) override {
    if (piece >= kPieces.size()) return std::nullopt;
    return kPieces[piece];
  }
  bool DrawQuad(const b::FloatingLabelQuad& quad) override {
    if (refuse_quad && quads.size() == refuse_quad.value()) return false;
    quads.push_back(quad);
    return true;
  }
  bool DrawText(std::string_view value, b::FloatingLabelScreenPoint origin,
                std::uint32_t color) override {
    text.push_back({std::string(value), origin, color});
    return true;
  }
};
}  // namespace

TEST_CASE("F00723 lays out caption then the signed-value indicator",
          "[floating-label-rendering][F00723]") {
  constexpr b::FloatingLabelAppearance kLabel{255, 0x8001, 0x83, 3};
  constexpr int kWidth = 28;
  constexpr r::ScreenVertex kCaptionOrigin{72, 94};
  constexpr r::ScreenVertex kSignOrigin{102, 95};
  constexpr std::uint8_t kNegativeSignU = 154;
  World world;
  CHECK(b::DrawValueLabelCaption(world, kOrigin, kLabel) == kWidth);
  REQUIRE(world.quads.size() == 2);
  CHECK(world.mode == kLabel.raw_mode);
  CHECK(world.quads.front().top_left == kCaptionOrigin);
  CHECK(world.quads.back().top_left == kSignOrigin);
  CHECK(world.quads.back().texture.u == kNegativeSignU);
  CHECK_FALSE(world.quads.front().tint.has_value());
}

TEST_CASE("F00723 retains the first draw when the sign is refused",
          "[floating-label-rendering][F00723]") {
  World world;
  world.refuse_quad = 1;
  CHECK_FALSE(b::DrawValueLabelCaption(world, kOrigin, {}).has_value());
  CHECK(world.quads.size() == 1);
  world.caption.reset();
  CHECK_FALSE(b::DrawValueLabelCaption(world, kOrigin, {}).has_value());
  CHECK(world.quads.size() == 1);
}

TEST_CASE("F00724 opening and closing use matching signed half heights",
          "[floating-label-rendering][F00724]") {
  constexpr std::uint8_t kLast = 30;
  constexpr r::ScreenVertex kFirstTop{84, 99};
  World opening;
  World closing;
  CHECK(b::DrawLockedFloatingLabel(opening, kOrigin, {}));
  CHECK(
      b::DrawLockedFloatingLabel(closing, kOrigin, {.elapsed_frames = kLast}));
  REQUIRE(opening.quads.size() == World::kPieces.size());
  REQUIRE(closing.quads.size() == World::kPieces.size());
  CHECK(opening.quads.front().top_left == kFirstTop);
  CHECK(closing.quads.front().top_left == kFirstTop);
  CHECK(opening.quads.front().translucent);
  CHECK(opening.quads.front().raw_texture);
}

TEST_CASE("F00724 clips each piece inclusively and refuses invalid frames",
          "[floating-label-rendering][F00724]") {
  constexpr b::FloatingLabelScreenPoint kLeft{-16, 100};
  constexpr b::FloatingLabelScreenPoint kRight{336, 100};
  constexpr std::uint8_t kInvalid = 255;
  World world;
  CHECK(b::DrawLockedFloatingLabel(world, kLeft, {}));
  REQUIRE(world.quads.size() == 1);
  CHECK(world.quads.front().bottom_right.x == 0);
  CHECK(b::DrawLockedFloatingLabel(world, kRight, {}));
  CHECK(world.quads.size() == 1);
  CHECK_FALSE(
      b::DrawLockedFloatingLabel(world, kOrigin, {.elapsed_frames = kInvalid}));
  CHECK(world.quads.size() == 1);
}

TEST_CASE("F00726 retains signed animation wrapping and never screen-culls",
          "[floating-label-rendering][F00726]") {
  constexpr b::FloatingLabelScreenPoint kOffscreen{-100, -100};
  constexpr std::uint8_t kWrappedU = 48;
  constexpr std::uint8_t kMarkerWidth = 16;
  constexpr std::uint16_t kPalette = 0x373E;
  World world;
  REQUIRE(b::DrawStatusRemovalMarker(world, kOffscreen, -1));
  REQUIRE(world.quads.size() == 1);
  CHECK(world.quads.front().texture.u == kWrappedU);
  CHECK(world.quads.front().texture.width == kMarkerWidth);
  CHECK(world.quads.front().texture.clut == kPalette);
  CHECK_FALSE(world.quads.front().tint.has_value());
}

TEST_CASE("F00726 a missing render destination is a failure",
          "[floating-label-rendering][F00726]") {
  World world;
  world.refuse_quad = 0;
  CHECK_FALSE(b::DrawStatusRemovalMarker(
      world, kOrigin, std::numeric_limits<std::int32_t>::min()));
  CHECK(world.quads.empty());
}

TEST_CASE("F00727 selects all success phrases without narrowing the raw mode",
          "[floating-label-rendering][F00727]") {
  constexpr std::array<std::string_view, 8> kExpected{
      "$COOL!",    "$GOOD!",         "$EXCELLENT!", "$RIGHT   ON!",
      "$PERFECT!", "$WELL   TIMED!", "$NICE!",      "$GREAT!"};
  constexpr std::uint8_t kStill = 4;
  constexpr std::uint8_t kFlags = 255;
  constexpr std::uint32_t kColor = 0x408040;
  World world;
  for (std::size_t index = 0; index < kExpected.size(); ++index) {
    const b::FloatingLabelAppearance label{kStill, 2, kFlags,
                                           static_cast<std::uint8_t>(index)};
    REQUIRE(b::DrawAbilityTimingResult(world, kOrigin, label));
    CHECK(world.text.back().text == kExpected[index]);
    CHECK(world.text.back().color == kColor);
  }
}

TEST_CASE("F00727 closing priority and phrase bounds retain prior text",
          "[floating-label-rendering][F00727]") {
  constexpr b::FloatingLabelAppearance kClosing{42, 4, 255, 255};
  constexpr b::FloatingLabelAppearance kInvalid{46, 2, 0, 0};
  constexpr int kShifted = 99;
  constexpr std::uint32_t kColor = 0x008080;
  World world;
  REQUIRE(b::DrawAbilityTimingResult(world, kOrigin, kClosing));
  REQUIRE(world.text.size() == 1);
  CHECK(world.text.front().origin.x == kShifted);
  CHECK(world.text.front().text == "$TOO   SLOW!");
  CHECK(world.text.front().color == kColor);
  CHECK_FALSE(b::DrawAbilityTimingResult(world, kOrigin, kInvalid));
  CHECK(world.text.size() == 1);
}

TEST_CASE("F00728 slides singular opening but not plural opening",
          "[floating-label-rendering][F00728]") {
  constexpr int kOpeningX = 36;
  World world;
  REQUIRE(b::DrawChainCount(world, kOrigin, {.value = 1}));
  REQUIRE(b::DrawChainCount(world, kOrigin, {.value = 2}));
  REQUIRE(world.text.size() == 2);
  CHECK(world.text.front().text == "$1 CHAIN");
  CHECK(world.text.front().origin.x == kOpeningX);
  CHECK(world.text.back().text == "$2 CHAINS");
  CHECK(world.text.back().origin.x == kOrigin.x);
}

TEST_CASE("F00728 renders the full unsigned payload and refuses overrun steps",
          "[floating-label-rendering][F00728]") {
  constexpr b::FloatingLabelAppearance kMaximum{90, 65535, 255, 255};
  constexpr b::FloatingLabelAppearance kInvalid{91, 2, 0, 0};
  constexpr std::uint32_t kColor = 0x1040A0;
  constexpr int kClosingX = 36;
  World world;
  REQUIRE(b::DrawChainCount(world, kOrigin, kMaximum));
  REQUIRE(world.text.size() == 1);
  CHECK(world.text.back().text == "$65535 CHAINS");
  CHECK(world.text.back().origin.x == kClosingX);
  CHECK(world.text.back().color == kColor);
  CHECK_FALSE(b::DrawChainCount(world, kOrigin, kInvalid));
  CHECK(world.text.size() == 1);
}

TEST_CASE("F00741 wraps screen halfwords and modulates the cue texture",
          "[floating-label-rendering][F00741]") {
  constexpr b::FloatingLabelScreenPoint kEdge{32767, -32768};
  constexpr r::ScreenVertex kTop{32756, 32736};
  constexpr r::ScreenVertex kBottom{-32758, -32768};
  constexpr r::QuadColor kTint{240, 64, 32};
  constexpr std::uint16_t kPalette = 0x377C;
  World world;
  REQUIRE(b::DrawAbilityTimingCue(world, kEdge));
  REQUIRE(world.quads.size() == 1);
  CHECK(world.quads.front().top_left == kTop);
  CHECK(world.quads.front().bottom_right == kBottom);
  CHECK(world.quads.front().tint == kTint);
  CHECK(world.quads.front().texture.clut == kPalette);
  CHECK_FALSE(world.quads.front().raw_texture);
  CHECK_FALSE(world.quads.front().translucent);
}

TEST_CASE("F00741 does not turn a refused cue into success",
          "[floating-label-rendering][F00741]") {
  World world;
  world.refuse_quad = 0;
  CHECK_FALSE(b::DrawAbilityTimingCue(world, kOrigin));
  CHECK(world.quads.empty());
}
