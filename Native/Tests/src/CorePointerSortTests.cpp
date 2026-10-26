#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <type_traits>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

extern "C" {
#include "Core/PointerSort.h"
}

static_assert(std::is_same_v<decltype(&Core_SortPointersByDescendingKey),
                             void (*)(void**, int, int)>);

namespace {
struct KeyRecord {
  std::int32_t key = 0;
  std::uint64_t tag = 0;
  bool operator==(const KeyRecord&) const = default;
};
static_assert(offsetof(KeyRecord, key) == 0);

bool Descending(void* left, void* right) {
  return static_cast<KeyRecord*>(left)->key >
         static_cast<KeyRecord*>(right)->key;
}
}  // namespace

TEST_CASE(
    "Pointer key sorting retains signed priorities and original tie order",
    "[core][pointer-sort]") {
  std::array<KeyRecord, 6> records{};
  constexpr std::array kKeys{2, -3, 2, 9, 9, 0};
  constexpr std::array kExpected{4, 3, 2, 0, 5, 1};
  std::array<void*, 8> pointers{};
  pointers.front() = &records.front();
  pointers.back() = &records.back();
  for (std::size_t index = 0; index < records.size(); ++index) {
    records[index] = {kKeys[index], index + 100};
    pointers[index + 1] = &records[index];
  }
  const auto before = records;
  Core_SortPointersByDescendingKey(pointers.data(), 1, 6);
  CHECK(pointers.front() == &records.front());
  CHECK(pointers.back() == &records.back());
  for (std::size_t index = 0; index < kExpected.size(); ++index) {
    CHECK(pointers[index + 1] == &records[kExpected[index]]);
  }
  CHECK(records == before);
}

TEST_CASE("Pointer key sorting accepts original empty and single ranges",
          "[core][pointer-sort]") {
  Core_SortPointersByDescendingKey(nullptr, 0, -1);
  void* single = nullptr;
  Core_SortPointersByDescendingKey(&single, 0, 0);
  CHECK(single == nullptr);
}

TEST_CASE(
    "Pointer key sorting covers full byte-index bounds and signed extrema",
    "[core][pointer-sort]") {
  const int pattern = GENERATE(0, 1, 2, 3);
  std::array<KeyRecord, 256> records{};
  std::array<void*, 256> pointers{};
  for (std::size_t index = 0; index < records.size(); ++index) {
    const int value = static_cast<int>(index) - 128;
    records[index] = {pattern == 0 ? 7 : pattern == 1 ? value : -value, index};
    pointers[index] = &records[index];
  }
  if (pattern == 3) {
    records.front().key = std::numeric_limits<std::int32_t>::min();
    records.back().key = std::numeric_limits<std::int32_t>::max();
  }
  const auto before = records;
  Core_SortPointersByDescendingKey(pointers.data(), 0, 255);
  CHECK(std::ranges::is_sorted(pointers, Descending));
  CHECK(records == before);
  for (auto& record : records) {
    CHECK(std::ranges::count(pointers, &record) == 1);
  }
}

TEST_CASE("Pointer key sorting preserves every outside-range pointer",
          "[core][pointer-sort]") {
  std::array<KeyRecord, 16> records{};
  for (std::size_t index = 0; index < records.size(); ++index) {
    records[index] = {static_cast<int>(index), index};
  }
  for (int left = 0; left <= 16; ++left) {
    for (int right = left - 1; right < 16; ++right) {
      std::array<void*, 16> pointers{};
      for (std::size_t index = 0; index < records.size(); ++index) {
        pointers[index] = &records[index];
      }
      auto expected = pointers;
      if (left < right) {
        std::sort(expected.begin() + left, expected.begin() + right + 1,
                  Descending);
      }
      Core_SortPointersByDescendingKey(pointers.data(), left, right);
      CHECK(pointers == expected);
    }
  }
}
