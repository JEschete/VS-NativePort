#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <numeric>
#include <span>
#include <type_traits>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Core/PointerSort.hpp"

extern "C" {
#include "Content/ModuleData.h"
}

static_assert(std::is_same_v<decltype(&VS::Core::SortPointersByDescendingKey),
                             bool (*)(std::span<VS::Core::SortKey>)>);

namespace {
using VS::Core::SortKey;
using VS::Core::SortPointersByDescendingKey;

bool Descending(const SortKey& left, const SortKey& right) {
  return left.key > right.key;
}

template <std::size_t kCount>
std::array<SortKey, kCount> KeysFrom(
    const std::array<std::int32_t, kCount>& values) {
  std::array<SortKey, kCount> keys{};
  for (std::size_t index = 0; index < kCount; ++index) {
    keys[index] = {values[index], index};
  }
  return keys;
}

void CheckOnlyRangeIsOrdered(std::size_t left, std::size_t right) {
  std::array<std::int32_t, 16> values{};
  std::iota(values.begin(), values.end(), 0);
  auto keys = KeysFrom<16>(values);
  auto expected = keys;
  std::sort(expected.begin() + static_cast<std::ptrdiff_t>(left),
            expected.begin() + static_cast<std::ptrdiff_t>(right), Descending);
  REQUIRE(
      SortPointersByDescendingKey(std::span(keys).subspan(left, right - left)));
  for (std::size_t index = 0; index < keys.size(); ++index) {
    CHECK(keys[index].position == expected[index].position);
  }
}
}  // namespace

TEST_CASE(
    "Pointer key sorting retains signed priorities and original tie order",
    "[core][pointer-sort]") {
  auto keys = KeysFrom<6>({2, -3, 2, 9, 9, 0});
  constexpr std::array<std::size_t, 6> kExpected{4, 3, 2, 0, 5, 1};
  REQUIRE(SortPointersByDescendingKey(keys));
  for (std::size_t index = 0; index < kExpected.size(); ++index) {
    CHECK(keys[index].position == kExpected[index]);
  }
}

TEST_CASE("Pointer key sorting accepts original empty and single ranges",
          "[core][pointer-sort]") {
  CHECK(SortPointersByDescendingKey({}));
  std::array<SortKey, 1> single{{{7, 3}}};
  CHECK(SortPointersByDescendingKey(single));
  CHECK(single[0].key == 7);
  CHECK(single[0].position == 3);
}

TEST_CASE(
    "Pointer key sorting covers full byte-index bounds and signed extrema",
    "[core][pointer-sort]") {
  const int pattern = GENERATE(0, 1, 2, 3);
  std::array<SortKey, 256> keys{};
  for (std::size_t index = 0; index < keys.size(); ++index) {
    const int value = static_cast<int>(index) - 128;
    keys[index] = {pattern == 0 ? 7 : pattern == 1 ? value : -value, index};
  }
  if (pattern == 3) {
    keys.front().key = std::numeric_limits<std::int32_t>::min();
    keys.back().key = std::numeric_limits<std::int32_t>::max();
  }
  REQUIRE(SortPointersByDescendingKey(keys));
  CHECK(std::ranges::is_sorted(keys, Descending));
  std::array<bool, 256> seen{};
  for (const auto& key : keys) {
    CHECK_FALSE(seen[key.position]);
    seen[key.position] = true;
  }
}

TEST_CASE("Pointer key sorting preserves every outside-range entry",
          "[core][pointer-sort]") {
  for (std::size_t left = 0; left <= 16; ++left) {
    for (std::size_t right = left; right <= 16; ++right) {
      CheckOnlyRangeIsOrdered(left, right);
    }
  }
}

TEST_CASE("Pointer key sorting refuses ranges beyond its byte index unchanged",
          "[core][pointer-sort]") {
  std::vector<SortKey> keys(257);
  for (std::size_t index = 0; index < keys.size(); ++index) {
    keys[index] = {static_cast<std::int32_t>(index), index};
  }
  const auto before = keys;
  CHECK_FALSE(SortPointersByDescendingKey(keys));
  for (std::size_t index = 0; index < keys.size(); ++index) {
    CHECK(keys[index].key == before[index].key);
    CHECK(keys[index].position == before[index].position);
  }
  CHECK(SortPointersByDescendingKey(std::span(keys).first(256)));
}

TEST_CASE("Pointer key sorting repeats deterministically on the same input",
          "[core][pointer-sort]") {
  const auto input = KeysFrom<8>({5, 5, -1, 9, 5, 0, 9, -1});
  auto first = input;
  auto second = input;
  REQUIRE(SortPointersByDescendingKey(first));
  REQUIRE(SortPointersByDescendingKey(second));
  for (std::size_t index = 0; index < input.size(); ++index) {
    CHECK(first[index].position == second[index].position);
  }
}

TEST_CASE("Pointer key sorting has no original-address module entry",
          "[core][pointer-sort][retirement]") {
  // No disc module stores 0x800E6974 or its 0x800E6958 entry as data; the
  // original's only references are three jal instructions in BATTLE.PRG.
  constexpr std::array<unsigned, 2> kOriginalEntries{0x800E6974u, 0x800E6958u};
  const std::span symbols(Content_moduleSymbols, Content_moduleSymbolCount);
  CHECK_FALSE(symbols.empty());
  for (const auto& symbol : symbols) {
    if (std::strcmp(symbol.module, "BATTLE/BATTLE.PRG") != 0) continue;
    CHECK(std::ranges::find(kOriginalEntries, symbol.address) ==
          kOriginalEntries.end());
  }
}
