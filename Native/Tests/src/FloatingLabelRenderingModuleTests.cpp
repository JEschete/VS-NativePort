#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <type_traits>

#include <catch2/catch_test_macros.hpp>

#include "Battle/FloatingLabelRenderingLive.hpp"
#include "Content/ModuleData.h"
#include "TestSupport/FloatingLabelRenderingLiveFixture.hpp"

namespace {
namespace b = VS::Battle;
namespace f = VS::test_support::floating_label_rendering;
constexpr std::uint32_t kCaptionEntry = 0x80094E18;
constexpr std::uint32_t kLockedEntry = 0x8009506C;
constexpr std::uint32_t kRemovalEntry = 0x800957D0;
constexpr std::uint32_t kTimingEntry = 0x800958A4;
constexpr std::uint32_t kChainEntry = 0x80095A4C;
constexpr std::uint32_t kCueEntry = 0x800970BC;
constexpr int kX = 100;
constexpr int kY = 100;
constexpr std::uint8_t kCaptionMode = 3;
constexpr std::uint8_t kStill = 4;
constexpr std::size_t kQuadWords = 10;

template <typename Function>
Function Resolve(std::uint32_t address, Function expected) {
  const auto entries =
      std::span(Content_moduleSymbols, Content_moduleSymbolCount);
  void* const target =
      VS::Content::ResolveModuleAddress(entries, "BATTLE/BATTLE.PRG", address);
  const bool matches =
      target != nullptr && target == reinterpret_cast<void*>(expected);
  CHECK(matches);
  return matches ? reinterpret_cast<Function>(target) : nullptr;
}

using LabelDraw = void (*)(int, int, Battle_FloatingLabel*);
using CaptionDraw = int (*)(int, int, Battle_FloatingLabel*);
using MarkerDraw = void (*)(int, int, int);
static_assert(
    std::is_same_v<decltype(&b::DrawLiveValueLabelCaption), CaptionDraw>);
static_assert(
    std::is_same_v<decltype(&b::DrawLiveLockedFloatingLabel), LabelDraw>);
static_assert(
    std::is_same_v<decltype(&b::DrawLiveStatusRemovalMarker), MarkerDraw>);
static_assert(
    std::is_same_v<decltype(&b::DrawLiveAbilityTimingResult), LabelDraw>);
static_assert(std::is_same_v<decltype(&b::DrawLiveChainCount), LabelDraw>);
static_assert(
    std::is_same_v<decltype(&b::DrawLiveAbilityTimingCue), void (*)()>);
}  // namespace

TEST_CASE("F00723 module callback retains the integer caption-width ABI",
          "[floating-label-rendering-module][F00723]") {
  constexpr int kCaptionWidth = 14;
  f::ProductionTables tables;
  f::LiveFrame frame;
  frame.labels.front().colorFlags = kCaptionMode;
  const auto entry = Resolve(kCaptionEntry, &b::DrawLiveValueLabelCaption);
  if (entry == nullptr) return;
  CHECK(entry(kX, kY, &frame.labels.front()) == kCaptionWidth);
  CHECK(frame.UsedWords() == kQuadWords * 2);
}

TEST_CASE("F00724 module callback retains its label-pointer and void ABI",
          "[floating-label-rendering-module][F00724]") {
  constexpr std::size_t kFourPieces = 4;
  f::ProductionTables tables;
  f::LiveFrame frame;
  frame.labels.front().frame = kStill;
  const auto entry = Resolve(kLockedEntry, &b::DrawLiveLockedFloatingLabel);
  if (entry == nullptr) return;
  entry(kX, kY, &frame.labels.front());
  CHECK(frame.Traversal(f::kQuadBucket).size() == kFourPieces);
}

TEST_CASE("F00726 module callback consumes a signed full-width animation step",
          "[floating-label-rendering-module][F00726]") {
  f::LiveFrame frame;
  const auto entry = Resolve(kRemovalEntry, &b::DrawLiveStatusRemovalMarker);
  if (entry == nullptr) return;
  entry(kX, kY, -1);
  CHECK(frame.UsedWords() == kQuadWords);
}

TEST_CASE("F00727 module callback invokes the actual compact font renderer",
          "[floating-label-rendering-module][F00727]") {
  f::ProductionTables tables;
  f::LiveFrame frame;
  const auto entry = Resolve(kTimingEntry, &b::DrawLiveAbilityTimingResult);
  if (entry == nullptr) return;
  entry(kX, kY, &frame.labels.front());
  CHECK(frame.Traversal(f::kTextBucket).size() > 1);
}

TEST_CASE("F00728 module callback retains unsigned chain payload wording",
          "[floating-label-rendering-module][F00728]") {
  f::ProductionTables tables;
  f::LiveFrame frame;
  frame.labels.front().value = 1;
  const auto entry = Resolve(kChainEntry, &b::DrawLiveChainCount);
  if (entry == nullptr) return;
  entry(kX, kY, &frame.labels.front());
  CHECK(frame.Traversal(f::kTextBucket).size() > 1);
}

TEST_CASE("F00741 module callback remains a no-argument void leaf",
          "[floating-label-rendering-module][F00741]") {
  f::LiveFrame frame;
  const auto entry = Resolve(kCueEntry, &b::DrawLiveAbilityTimingCue);
  if (entry == nullptr) return;
  entry();
  CHECK(frame.UsedWords() == kQuadWords);
}

TEST_CASE(
    "Ordinary BATTLE reload uses current font owners without resetting BSS",
    "[floating-label-rendering-module][F00727][F00728]") {
  constexpr std::size_t kFontAndAtlasOwners = 7;
  constexpr std::size_t kCapitalC = 'C' - ' ';
  constexpr std::uint8_t kSuccess = 2;
  constexpr std::uint8_t kChangedCell = 255;
  f::ProductionTables tables;
  f::LiveFrame frame;
  auto& label = frame.labels.front();
  label.frame = kStill;
  label.value = kSuccess;
  const auto before = f::Words(label);
  b::DrawLiveAbilityTimingResult(kX, kY, &label);
  const auto baseline = frame.Traversal(f::kTextBucket);
  frame.ClearDraws();
  Render_compactGlyphIndices[kCapitalC] = kChangedCell;
  b::DrawLiveAbilityTimingResult(kX, kY, &label);
  CHECK(frame.Traversal(f::kTextBucket) != baseline);
  tables.Reload();
  frame.ClearDraws();
  b::DrawLiveAbilityTimingResult(kX, kY, &label);
  CHECK(frame.Traversal(f::kTextBucket) == baseline);
  CHECK(f::Words(label) == before);
  CHECK(Battle_floatingLabelCount == 1);
  CHECK(tables.OwnerCount() == kFontAndAtlasOwners);
}
