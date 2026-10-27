#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Core/Memory/WordCopy.h"
#include "Legacy/Psx.h"

extern "C" {
#include "Core/Memory/Copy.h"
}

TEST_CASE("Signed short reads preserve aligned and unaligned byte order",
          "[core][memory]") {
  alignas(std::uint16_t) std::array<unsigned char, 5> bytes{0x34, 0x12, 0xFF,
                                                            0x80, 0x7F};
  CHECK(Core_Memory_ReadUnalignedShort(bytes.data()) == 0x1234);
  CHECK(Core_Memory_ReadUnalignedShort(bytes.data() + 1) == -238);
  CHECK(Core_Memory_ReadUnalignedShort(bytes.data() + 2) == -32513);
  CHECK(Core_Memory_ReadUnalignedShort(bytes.data() + 3) == 0x7F80);
}

TEST_CASE("Backward word clear consumes bytes without touching adjacent words",
          "[core][memory]") {
  for (int bytes : {4, 8, 12, 16, 20, 32, 256}) {
    const auto words = static_cast<std::size_t>(bytes / 4);
    std::vector<std::uint32_t> memory(words + 2, 0xA5A5A5A5u);
    Core_Memory_ZeroWordsBackward(memory.data() + 1, bytes);
    CHECK(memory.front() == 0xA5A5A5A5u);
    CHECK(memory.back() == 0xA5A5A5A5u);
    CHECK(std::all_of(memory.begin() + 1, memory.end() - 1,
                      [](auto word) { return word == 0; }));
  }
}

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

TEST_CASE("Battle halfword copies preserve boundaries and halfword alignment",
          "[core][memory][halfword]") {
  for (const auto copy :
       {Core_Memory_CopyHalfwords, Core_Memory_CopyHalfwordBlocks}) {
    for (const int bytes : {2, 4, 6, 8, 10, 14, 16, 24, 26, 32, 64, 92, 128}) {
      if (copy == Core_Memory_CopyHalfwordBlocks && bytes % 8 != 0) {
        continue;
      }
      CAPTURE(bytes);
      std::vector<std::uint16_t> source(66, 0x1234);
      std::vector<std::uint16_t> destination(66, 0xA5A5);
      for (std::size_t index = 0; index < source.size(); ++index) {
        source[index] = static_cast<std::uint16_t>(index + 0x1200);
      }
      copy(destination.data() + 1, source.data() + 1, bytes);
      const auto count = static_cast<std::size_t>(bytes / 2);
      CHECK(std::equal(source.begin() + 1, source.begin() + 1 + count,
                       destination.begin() + 1));
      CHECK(destination.front() == 0xA5A5);
      CHECK(destination[count + 1] == 0xA5A5);
      CHECK(destination.back() == 0xA5A5);
    }
  }
}

TEST_CASE("Battle block copies retain complete native pointer values",
          "[core][memory][halfword]") {
  struct Record {
    void* pointer;
    std::uint64_t value;
  };
  int object = 0;
  const std::array<Record, 2> source = {Record{&object, 0x1234567887654321ULL},
                                        Record{&object, 0x8877665544332211ULL}};
  for (const auto copy :
       {Core_Memory_CopyHalfwords, Core_Memory_CopyHalfwordBlocks}) {
    std::array<Record, 2> destination{};
    copy(destination.data(), source.data(), sizeof source);
    for (std::size_t index = 0; index < source.size(); ++index) {
      CHECK(destination[index].pointer == source[index].pointer);
      CHECK(destination[index].value == source[index].value);
    }
  }
}

TEST_CASE("Battle halfword copies preserve assembly block overlap order",
          "[core][memory][halfword]") {
  std::array<std::uint16_t, 12> blocks = {1, 2, 3, 4,  5,  6,
                                          7, 8, 9, 10, 11, 12};
  Core_Memory_CopyHalfwordBlocks(blocks.data() + 1, blocks.data(), 16);
  const std::array<std::uint16_t, 12> expected_blocks = {1, 1, 2, 3,  4,  4,
                                                         6, 7, 8, 10, 11, 12};
  CHECK(blocks == expected_blocks);
  std::array<std::uint16_t, 12> prefix = {1, 2, 3, 4,  5,  6,
                                          7, 8, 9, 10, 11, 12};
  Core_Memory_CopyHalfwords(prefix.data() + 1, prefix.data(), 18);
  const std::array<std::uint16_t, 12> expected_prefix = {1, 1, 1, 3, 4,  5,
                                                         5, 7, 8, 9, 11, 12};
  CHECK(prefix == expected_prefix);
}

TEST_CASE("Battle halfword copies bound partial and empty native ranges",
          "[core][memory][halfword]") {
  const std::array<std::uint16_t, 8> source = {1, 2, 3, 4, 5, 6, 7, 8};
  std::array<std::uint16_t, 8> halfwords{};
  Core_Memory_CopyHalfwords(halfwords.data(), source.data(), 3);
  CHECK(halfwords[0] == 1);
  CHECK(halfwords[1] == 0);
  std::array<std::uint16_t, 8> blocks{};
  Core_Memory_CopyHalfwordBlocks(blocks.data(), source.data(), 10);
  CHECK(blocks[3] == 4);
  CHECK(blocks[4] == 0);
  for (const auto copy :
       {Core_Memory_CopyHalfwords, Core_Memory_CopyHalfwordBlocks}) {
    copy(nullptr, nullptr, 0);
    copy(nullptr, nullptr, -8);
    auto identity = source;
    copy(identity.data(), identity.data(), sizeof identity);
    CHECK(identity == source);
  }
}
