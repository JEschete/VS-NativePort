#include <algorithm>
#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Core/Memory/WordCopy.h"

TEST_CASE("Battle memory clear preserves bounds and uses the full size",
          "[core][memory]") {
  for (int size : {0, 1, 3, 4, 8, 12, 16, 20, 31, 32, 0x484, 0x10004}) {
    INFO("size=" << size);
    const auto words = static_cast<std::size_t>(size / 4);
    std::vector<std::uint32_t> memory(words + 3, 0xA5A5A5A5u);
    Core_Memory_ClearAlignedWords(memory.data() + 1, size);
    CHECK(memory.front() == 0xA5A5A5A5u);
    CHECK(std::all_of(memory.begin() + 1, memory.begin() + 1 + words,
                      [](auto word) { return word == 0; }));
    CHECK(memory[words + 1] == 0xA5A5A5A5u);
    CHECK(memory.back() == 0xA5A5A5A5u);
  }
}

TEST_CASE("Battle memory clear accepts empty and nonpositive ranges",
          "[core][memory]") {
  std::uint32_t word = 0x12345678;
  Core_Memory_ClearAlignedWords(&word, -4);
  Core_Memory_ClearAlignedWords(nullptr, 0);
  CHECK(word == 0x12345678);
}
