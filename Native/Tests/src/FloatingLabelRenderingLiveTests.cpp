#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <span>

#include <catch2/catch_test_macros.hpp>

#include "Battle/FloatingLabelQueueLive.hpp"
#include "Battle/FloatingLabelRenderingLive.hpp"
#include "Render/GameFont.hpp"
#include "Render/PageSpace.hpp"
#include "Render/UiTexturedQuads.hpp"
#include "TestSupport/FloatingLabelRenderingLiveFixture.hpp"

namespace {
namespace b = VS::Battle;
namespace r = VS::Render;
namespace f = VS::test_support::floating_label_rendering;
constexpr int kX = 100;
constexpr int kY = 100;
constexpr std::size_t kColorWord = 1;
constexpr std::size_t kPositionWord = 2;
constexpr std::size_t kTextureWord = 3;
constexpr std::size_t kPageWord = 5;
constexpr std::size_t kUvPaddingWord = 7;
constexpr std::uint32_t kRawColor = 0x2DB6C5D4;
constexpr std::uint32_t kSemiRawColor = 0x2FB6C5D4;
constexpr std::uint32_t kUvPadding = 0xA7B60000;
constexpr std::uint32_t kHighHalf = 0xFFFF0000;
constexpr std::size_t kQuadWords = 10;

std::uint32_t Command(const f::Packet& packet) { return packet.at(kColorWord); }
}  // namespace

TEST_CASE("F00723 Live reads its original atlas and preserves untouched words",
          "[floating-label-rendering-live][F00723]") {
  constexpr std::uint32_t kCaptionUv = 0x373D4C66;
  constexpr std::uint32_t kSignUv = 0x37F64CA2;
  constexpr int kWidth = 23;
  constexpr std::uint8_t kCaptionMode = 3;
  constexpr std::uint8_t kDigits = 3;
  f::ProductionTables tables;
  f::LiveFrame frame;
  auto& label = frame.labels.front();
  label.colorFlags = kCaptionMode;
  label.textLength = kDigits;
  const auto before = f::Words(label);
  CHECK(b::DrawLiveValueLabelCaption(kX, kY, &label) == kWidth);
  const auto packets = frame.Traversal(f::kQuadBucket);
  REQUIRE(packets.size() == 2);
  CHECK(packets.front().at(kTextureWord) == kSignUv);
  CHECK(packets.back().at(kTextureWord) == kCaptionUv);
  CHECK(Command(packets.back()) == kRawColor);
  CHECK((packets.back().at(kUvPaddingWord) & kHighHalf) == kUvPadding);
  CHECK(f::Words(label) == before);
}

TEST_CASE("F00724 Live uses the real four-piece atlas and current OT owner",
          "[floating-label-rendering-live][F00724]") {
  constexpr std::uint8_t kFullFrame = 4;
  constexpr std::size_t kPieces = 4;
  constexpr std::uint32_t kLastPieceUv = 0x37F656A0;
  f::ProductionTables tables;
  f::LiveFrame frame;
  frame.labels.front().frame = kFullFrame;
  const auto before = f::Words(frame.labels.front());
  b::DrawLiveLockedFloatingLabel(kX, kY, &frame.labels.front());
  const auto packets = frame.Traversal(f::kQuadBucket);
  REQUIRE(packets.size() == kPieces);
  CHECK(frame.UsedWords() == kPieces * kQuadWords);
  CHECK(Command(packets.front()) == kSemiRawColor);
  CHECK(packets.front().at(kTextureWord) == kLastPieceUv);
  CHECK(f::Words(frame.labels.front()) == before);
}

TEST_CASE("F00724 fully culled graphics require no primitive or OT owner",
          "[floating-label-rendering-live][F00724]") {
  constexpr int kFullyRight = 336;
  f::ProductionTables tables;
  f::LiveFrame frame;
  Render_frameScratch.primitiveCursor = nullptr;
  Render_frameScratch.orderingTable = nullptr;
  b::DrawLiveLockedFloatingLabel(kFullyRight, kY, &frame.labels.front());
  CHECK(Render_frameScratch.primitiveCursor == nullptr);
  CHECK(frame.ordering.at(f::kQuadBucket) == f::kEnd);
}

TEST_CASE("F00726 Live marker keeps raw RGB and signed frame wrapping",
          "[floating-label-rendering-live][F00726]") {
  constexpr std::uint32_t kMarkerUv = 0x373E3030;
  constexpr std::uint32_t kMarkerPage = 0x00173040;
  f::LiveFrame frame;
  b::DrawLiveStatusRemovalMarker(kX, kY, -1);
  const auto packets = frame.Traversal(f::kQuadBucket);
  REQUIRE(packets.size() == 1);
  CHECK(Command(packets.front()) == kRawColor);
  CHECK(packets.front().at(kTextureWord) == kMarkerUv);
  CHECK(packets.front().at(kPageWord) == kMarkerPage);
  CHECK(frame.UsedWords() == kQuadWords);
}

TEST_CASE(
    "F00727 Live emits compact mode then five-word glyphs including spaces",
    "[floating-label-rendering-live][F00727]") {
  constexpr std::uint32_t kDrawMode = 0xE100000C;
  constexpr std::size_t kCompactWords = 5;
  constexpr std::uint8_t kStill = 4;
  constexpr std::size_t kGlyphsWithSpaces = 11;
  f::ProductionTables tables;
  f::LiveFrame frame;
  auto& label = frame.labels.front();
  label.frame = kStill;
  label.value = 0;
  const auto before = f::Words(label);
  b::DrawLiveAbilityTimingResult(kX, kY, &label);
  const auto packets = frame.Traversal(f::kTextBucket);
  REQUIRE(packets.size() == kGlyphsWithSpaces + 1);
  CHECK(packets.front().at(kColorWord) == kDrawMode);
  CHECK(packets.back().size() == kCompactWords);
  CHECK(frame.UsedWords() == kCompactWords * kGlyphsWithSpaces + 2);
  CHECK(f::Words(label) == before);
}

TEST_CASE(
    "F00728 Live renders all unsigned chain digits through production fonts",
    "[floating-label-rendering-live][F00728]") {
  constexpr std::uint16_t kMaximum = 65535;
  constexpr std::uint8_t kStill = 4;
  constexpr std::uint32_t kChainCommand = 0x641040A0;
  constexpr std::size_t kGlyphCount = 12;
  f::ProductionTables tables;
  f::LiveFrame frame;
  auto& label = frame.labels.front();
  label.frame = kStill;
  label.value = kMaximum;
  const auto before = f::Words(label);
  b::DrawLiveChainCount(kX, kY, &label);
  const auto packets = frame.Traversal(f::kTextBucket);
  REQUIRE(packets.size() == kGlyphCount + 1);
  CHECK(Command(packets.back()) == kChainCommand);
  CHECK(f::Words(label) == before);
}

TEST_CASE("F00741 Live cue is modulated opaque geometry with no timer mutation",
          "[floating-label-rendering-live][F00741]") {
  constexpr std::uint32_t kCueCommand = 0x2C2040F0;
  constexpr std::uint32_t kCueOrigin = 0x00440059;
  constexpr std::uint32_t kCueUv = 0x377C00C0;
  f::LiveFrame frame;
  Battle_abilityTimingCuePosition = {kX, kY, -1, -2};
  const auto cue = std::bit_cast<std::array<std::uint32_t, 2>>(
      Battle_abilityTimingCuePosition);
  b::DrawLiveAbilityTimingCue();
  const auto packets = frame.Traversal(f::kQuadBucket);
  REQUIRE(packets.size() == 1);
  CHECK(Command(packets.front()) == kCueCommand);
  CHECK(packets.front().at(kPositionWord) == kCueOrigin);
  CHECK(packets.front().at(kTextureWord) == kCueUv);
  CHECK(std::bit_cast<std::array<std::uint32_t, 2>>(
            Battle_abilityTimingCuePosition) == cue);
}

TEST_CASE("Real renderer consumes the timing-cue packet into actual pixels",
          "[floating-label-rendering-live][F00741]") {
  constexpr r::PageSpace::Rect kTexels{496, 256, 6, 32};
  constexpr r::PageSpace::Rect kPalette{960, 221, 16, 1};
  constexpr std::size_t kTexelWords = 192;
  constexpr std::size_t kColors = 16;
  constexpr std::uint16_t kRedTexel = 31;
  constexpr std::uint16_t kTexelIndices = 0x1111;
  std::array<std::uint16_t, kTexelWords> texels{};
  std::array<std::uint16_t, kColors> palette{};
  texels.fill(kTexelIndices);
  palette[1] = kRedTexel;
  r::PageSpace pages;
  pages.Upload(kTexels, texels.data());
  pages.Upload(kPalette, palette.data());
  f::LiveFrame frame;
  Battle_abilityTimingCuePosition = {kX, kY, 0, 0};
  b::DrawLiveAbilityTimingCue();
  const auto packets = frame.Traversal(f::kQuadBucket);
  REQUIRE(packets.size() == 1);
  const auto commands = std::span(packets.front()).subspan(1);
  CHECK(pages.Execute(commands) == commands.size());
  CHECK(pages.At(kX, kY - 1) == kRedTexel);
  CHECK(pages.At(kX, kY) == 0);
}

TEST_CASE("F00722 dependency draws digits after the promoted F00723 caption",
          "[floating-label-rendering-live][F00723]") {
  constexpr std::uint8_t kCaptionMode = 3;
  constexpr signed char kOne = '1';
  constexpr std::uint32_t kDigitCommand = 0x2C808080;
  constexpr std::size_t kPacketCount = 3;
  f::ProductionTables tables;
  f::LiveFrame frame;
  auto& label = frame.labels.front();
  label.colorFlags = kCaptionMode;
  label.textLength = 1;
  label.text[0] = kOne;
  const auto before = f::Words(label);
  Battle_DrawActorValueLabel(kX, kY, &label);
  const auto packets = frame.Traversal(f::kQuadBucket);
  REQUIRE(packets.size() == kPacketCount);
  CHECK(Command(packets.front()) == kDigitCommand);
  CHECK(Command(packets.back()) == kRawColor);
  CHECK(f::Words(label) == before);
}

TEST_CASE("F00725 dependency submits removal marker before its status caption",
          "[floating-label-rendering-live][F00726]") {
  constexpr std::uint8_t kMarkerFrame = 15;
  constexpr std::uint32_t kStatusCommand = 0x2E686868;
  f::ProductionTables tables;
  f::LiveFrame frame;
  auto& label = frame.labels.front();
  label.colorFlags = 1;
  label.frame = kMarkerFrame;
  const auto before = f::Words(label);
  Battle_DrawActorStatusLabel(kX, kY, &label);
  const auto packets = frame.Traversal(f::kQuadBucket);
  REQUIRE(packets.size() == 2);
  CHECK(Command(packets.front()) == kStatusCommand);
  CHECK(Command(packets.back()) == kRawColor);
  CHECK(f::Words(label) == before);
}

TEST_CASE("F00719 dependency decrements cue timing before the F00741 leaf",
          "[floating-label-rendering-live][F00741]") {
  constexpr std::uint32_t kCueCommand = 0x2C2040F0;
  f::LiveFrame frame;
  Battle_floatingLabelCount = 0;
  Battle_abilityTimingCueFrames = 1;
  Battle_UpdateAndDrawFloatingLabels();
  const auto packets = frame.Traversal(f::kQuadBucket);
  REQUIRE(packets.size() == 1);
  CHECK(Battle_abilityTimingCueFrames == 0);
  CHECK(Command(packets.front()) == kCueCommand);
}

TEST_CASE("Mixed frame traverses text before LIFO cue and removal-marker quads",
          "[floating-label-rendering-live][F00726][F00728][F00741]") {
  constexpr std::uint8_t kStill = 4;
  constexpr std::size_t kGlyphs = 7;
  constexpr std::size_t kPackets = 10;
  constexpr std::uint32_t kDrawMode = 0xE100000C;
  constexpr std::uint32_t kCueCommand = 0x2C2040F0;
  constexpr std::size_t kGlyphWords = 5;
  f::ProductionTables tables;
  f::LiveFrame frame;
  ClearOTagR(frame.ordering.data(), f::kOrderingWords);
  frame.labels.front().frame = kStill;
  frame.labels.front().value = 1;
  b::DrawLiveStatusRemovalMarker(kX, kY, 0);
  b::DrawLiveChainCount(kX, kY, &frame.labels.front());
  b::DrawLiveAbilityTimingCue();
  const auto packets = frame.Traversal(f::kOrderingWords - 1);
  REQUIRE(packets.size() == kPackets);
  CHECK(Command(packets.front()) == kDrawMode);
  CHECK(Command(packets.at(kGlyphs + 1)) == kCueCommand);
  CHECK(Command(packets.back()) == kRawColor);
  CHECK(frame.UsedWords() == kQuadWords * 2 + kGlyphs * kGlyphWords + 2);
}
