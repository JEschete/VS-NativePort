#include <array>
#include <cstdint>
#include <span>
#include <type_traits>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Battle/FloatingLabelQueueLive.hpp"
#include "Content/ModuleData.h"
#include "TestSupport/FloatingLabelQueueLiveFixture.hpp"

namespace {
namespace b = VS::Battle;
namespace f = VS::test_support::live_floating_labels;

template <typename Function>
Function Entry(unsigned address, Function expected) {
  const auto symbols =
      std::span(Content_moduleSymbols, Content_moduleSymbolCount);
  void* const entry =
      VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG", address);
  const bool matches =
      entry != nullptr && entry == reinterpret_cast<void*>(expected);
  CHECK(matches);
  if (!matches) return nullptr;
  return reinterpret_cast<Function>(entry);
}

static_assert(
    std::is_same_v<decltype(&b::ResetLiveFloatingLabels), void (*)()>);
static_assert(std::is_same_v<decltype(&b::QueueLiveWorldLockedLabelEntry),
                             int (*)(std::uint16_t*)>);
static_assert(std::is_same_v<decltype(&b::AdvanceLiveFloatingLabelQueue),
                             void (*)(Battle_FloatingLabel*)>);
static_assert(std::is_same_v<decltype(&b::ReserveLiveFloatingLabel),
                             Battle_FloatingLabel* (*)()>);
static_assert(
    std::is_same_v<decltype(&b::CompactLiveFloatingLabels), void (*)(int)>);
static_assert(
    std::is_same_v<decltype(&b::RemoveLiveFloatingLabel), void (*)(int)>);
}  // namespace

TEST_CASE(
    "F00712 production module entry performs explicit room feedback reset",
    "[floating-label-queue-module][F00712]") {
  const auto reset = Entry(0x80093AB4, &b::ResetLiveFloatingLabels);
  if (reset == nullptr) return;
  f::State state;
  state.Populate();
  Battle_floatingLabelCount = 31;
  reset();
  CHECK(Battle_floatingLabelCount == 0);
  CHECK(f::Words(state.labels[0][31]) == f::LabelWords{});
}

TEST_CASE("F00713 production module entry preserves its original zero result",
          "[floating-label-queue-module][F00713]") {
  const auto queue = Entry(0x80093B04, &b::QueueLiveWorldLockedLabelEntry);
  if (queue == nullptr) return;
  f::State state;
  std::array<std::uint16_t, 3> position{11, 22, 33};
  CHECK(queue(position.data()) == 0);
  CHECK(Battle_floatingLabelCount == 1);
  CHECK(state.labels[0][0].position.vz == 33);
}

TEST_CASE("F00721 production module entry borrows only its supplied label",
          "[floating-label-queue-module][F00721]") {
  const auto advance = Entry(0x80094AF8, &b::AdvanceLiveFloatingLabelQueue);
  if (advance == nullptr) return;
  f::State state;
  state.labels[0][5].position.pad = 2;
  advance(&state.labels[0][5]);
  CHECK(state.labels[0][5].position.pad == 1);
}

TEST_CASE("F00734 production module entry returns its real reserved slot",
          "[floating-label-queue-module][F00734]") {
  const auto reserve = Entry(0x800962E4, &b::ReserveLiveFloatingLabel);
  if (reserve == nullptr) return;
  f::State state;
  Battle_floatingLabelCount = 31;
  CHECK(reserve() == &state.labels[0][31]);
  CHECK(Battle_floatingLabelCount == 31);
}

TEST_CASE("F00735 production module entry rescans shifted inactive slots",
          "[floating-label-queue-module][F00735]") {
  const auto compact = Entry(0x8009639C, &b::CompactLiveFloatingLabels);
  if (compact == nullptr) return;
  f::State state;
  Battle_floatingLabelCount = 2;
  compact(2);
  CHECK(Battle_floatingLabelCount == 0);
}

TEST_CASE("F00736 production module entry clears its vacated tail",
          "[floating-label-queue-module][F00736]") {
  const auto remove = Entry(0x80096444, &b::RemoveLiveFloatingLabel);
  if (remove == nullptr) return;
  f::State state;
  state.Populate();
  Battle_floatingLabelCount = 1;
  remove(0);
  CHECK(Battle_floatingLabelCount == 0);
  CHECK(f::Words(state.labels[0][0]) == f::LabelWords{});
}

TEST_CASE("Ordinary module-data copying does not invent a label queue reset",
          "[floating-label-queue-module][F00712]") {
  f::State state;
  state.Populate();
  Battle_floatingLabelCount = 2;
  Battle_actorMarkersHidden = 123;
  const auto before = f::Words(state.labels[0][0]);
  std::array<unsigned char, 4> source{1, 2, 3, 4};
  std::array<unsigned char, 4> target{};
  const Content_ModuleDataEntry entry{"BATTLE/BATTLE.PRG", 0, 4, target.data(),
                                      4};
  REQUIRE(VS::Content::CopyModuleDataEntry(entry, source));
  CHECK(target == source);
  CHECK(Battle_floatingLabelCount == 2);
  CHECK(Battle_actorMarkersHidden == 123);
  CHECK(f::Words(state.labels[0][0]) == before);
}

TEST_CASE("Production module saturation retains32 then explicit reset reopens",
          "[floating-label-queue-module][F00712][F00713][F00734]") {
  const auto queue = Entry(0x80093B04, &b::QueueLiveWorldLockedLabelEntry);
  const auto reset = Entry(0x80093AB4, &b::ResetLiveFloatingLabels);
  if (queue == nullptr || reset == nullptr) return;
  f::State state;
  for (std::uint16_t value = 0; value < 48; ++value) {
    std::array<std::uint16_t, 3> position{value, 22, 33};
    REQUIRE(queue(position.data()) == 0);
  }
  CHECK(Battle_floatingLabelCount == 32);
  CHECK(state.labels[0][0].position.vx == 16);
  CHECK(state.labels[0][31].position.vx == 47);
  reset();
  CHECK(Battle_floatingLabelCount == 0);
  CHECK(f::Words(state.labels[0][31]) == f::LabelWords{});
}
