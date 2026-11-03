#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Core/Memory/Copy.hpp"
#include "Core/Memory/CopyLive.hpp"
#include "Core/Memory/WordCopy.hpp"
#include "Core/Memory/WordCopyLive.hpp"
#include "Legacy/Psx.h"

namespace {
void CopyHalfwords(void* destination, const void* source, int size) {
  VS::Core::Memory::CopyLiveHalfwords(destination, source, size);
}
void CopyHalfwordBlocks(void* destination, const void* source, int size) {
  VS::Core::Memory::CopyLiveHalfwordBlocks(destination, source, size);
}
const std::array kHalfwordCopies{&CopyHalfwords, &CopyHalfwordBlocks};
}  // namespace

TEST_CASE("Signed short reads preserve aligned and unaligned byte order",
          "[core][memory]") {
  alignas(std::uint16_t) std::array<unsigned char, 5> bytes{0x34, 0x12, 0xFF,
                                                            0x80, 0x7F};
  using VS::Core::Memory::ReadLiveUnalignedShort;
  CHECK(ReadLiveUnalignedShort(bytes.data()) == 0x1234);
  CHECK(ReadLiveUnalignedShort(bytes.data() + 1) == -238);
  CHECK(ReadLiveUnalignedShort(bytes.data() + 2) == -32513);
  CHECK(ReadLiveUnalignedShort(bytes.data() + 3) == 0x7F80);
  CHECK(VS::Core::Memory::ReadUnalignedShort(
            std::as_bytes(std::span(bytes).subspan<1, 2>())) == -238);
}

TEST_CASE("Backward word clear consumes bytes without touching adjacent words",
          "[core][memory]") {
  for (int bytes : {4, 8, 12, 16, 20, 32, 256}) {
    const auto words = static_cast<std::size_t>(bytes / 4);
    std::vector<std::uint32_t> memory(words + 2, 0xA5A5A5A5u);
    VS::Core::Memory::ZeroLiveWordsBackward(memory.data() + 1, bytes);
    CHECK(memory.front() == 0xA5A5A5A5u);
    CHECK(memory.back() == 0xA5A5A5A5u);
    CHECK(std::all_of(memory.begin() + 1, memory.end() - 1,
                      [](auto word) { return word == 0; }));
  }
}

TEST_CASE("Backward copies refuse sizes the original could not finish",
          "[core][memory]") {
  using VS::Core::Memory::CopyBytesBackward;
  using VS::Core::Memory::CopyLiveBytesBackward;
  using VS::Core::Memory::RefusedLiveSizeCount;
  using VS::Core::Memory::ZeroLiveWordsBackward;
  using VS::Core::Memory::ZeroWordsBackward;
  std::array<std::byte, 8> bytes{};
  bytes.fill(std::byte{0x5A});
  const auto original = bytes;
  for (const std::int32_t size : {0, -4, 1, 3, 6}) {
    INFO("size=" << size);
    CHECK(ZeroWordsBackward(bytes, size) == 0);
    CHECK(bytes == original);
  }
  for (const std::int32_t size : {0, -1}) {
    CHECK(CopyBytesBackward(bytes, original, size) == 0);
  }
  const auto refused = RefusedLiveSizeCount();
  ZeroLiveWordsBackward(bytes.data(), 6);
  CopyLiveBytesBackward(bytes.data(), original.data(), 0);
  CHECK(RefusedLiveSizeCount() == refused + 2);
  CHECK(bytes == original);
}

TEST_CASE("Backward copies stay inside their spans and array extents",
          "[core][memory]") {
  using VS::Core::Memory::TruncatedLiveCopyCount;
  using VS::Core::Memory::ZeroLiveWordsBackward;
  using VS::Core::Memory::ZeroWordsBackward;
  std::array<std::byte, 12> bytes{};
  bytes.fill(std::byte{0x5A});
  CHECK(ZeroWordsBackward(std::span(bytes).first(8), 12) == 8);
  CHECK(bytes[7] == std::byte{0});
  CHECK(bytes[8] == std::byte{0x5A});

  struct Record {
    std::uint32_t words[2];
    std::uint32_t guard;
  } record{{1, 2}, 0xA5A5A5A5u};
  const auto truncated = TruncatedLiveCopyCount();
  ZeroLiveWordsBackward(record.words, 12);
  CHECK(record.words[0] == 0);
  CHECK(record.words[1] == 0);
  CHECK(record.guard == 0xA5A5A5A5u);
  CHECK(TruncatedLiveCopyCount() == truncated + 1);
}

TEST_CASE("Byte copies run from the end like the original loop",
          "[core][memory]") {
  using VS::Core::Memory::CopyLiveBytesBackward;
  std::array<char, 6> higher{'A', 'B', 'C', 'D', 'E', 'F'};
  CopyLiveBytesBackward(higher.data() + 1, higher.data(), 4);
  CHECK(higher == std::array<char, 6>{'A', 'A', 'B', 'C', 'D', 'F'});
  // Copying to a lower address re-reads bytes already written: the original's
  // order, not memmove's.
  std::array<char, 6> lower{'A', 'B', 'C', 'D', 'E', 'F'};
  CopyLiveBytesBackward(lower.data(), lower.data() + 1, 4);
  CHECK(lower == std::array<char, 6>{'E', 'E', 'E', 'E', 'E', 'F'});
}

TEST_CASE("Sound word copies load each group of four before storing it",
          "[core][memory]") {
  using VS::Core::Memory::CopyLiveWords;
  using VS::Core::Memory::CopyWords;
  std::array<std::uint32_t, 10> words{0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
  CopyLiveWords(words.data(), words.data() + 1, 8 * 4 + 3);
  CHECK(words == std::array<std::uint32_t, 10>{0, 0, 1, 2, 3, 3, 5, 6, 7, 9});

  std::array<std::uint32_t, 6> source{1, 2, 3, 4, 5, 6};
  std::array<std::uint32_t, 6> destination{};
  CHECK(CopyWords(std::as_bytes(std::span(source)),
                  std::as_writable_bytes(std::span(destination)), 24) == 24);
  CHECK(destination == source);
}

TEST_CASE("Sound word swaps exchange whole words in pairs", "[core][memory]") {
  using VS::Core::Memory::SwapLiveWords;
  using VS::Core::Memory::SwapWords;
  std::array<std::uint32_t, 5> first{1, 2, 3, 4, 5};
  std::array<std::uint32_t, 5> second{10, 20, 30, 40, 50};
  SwapLiveWords(first.data(), second.data(), 5 * 4 + 2);
  CHECK(first == std::array<std::uint32_t, 5>{10, 20, 30, 40, 50});
  CHECK(second == std::array<std::uint32_t, 5>{1, 2, 3, 4, 5});

  std::array<std::uint32_t, 3> same{7, 8, 9};
  SwapLiveWords(same.data(), same.data(), 12);
  CHECK(same == std::array<std::uint32_t, 3>{7, 8, 9});
  CHECK(SwapWords(std::as_writable_bytes(std::span(first)).first(4),
                  std::as_writable_bytes(std::span(second)), 20) == 4);
  CHECK(first[0] == 1);
  CHECK(second[0] == 10);
}

TEST_CASE("Battle memory clear preserves bounds and uses the full size",
          "[core][memory]") {
  for (int size : {0, 1, 3, 4, 8, 12, 16, 20, 31, 32, 0x484, 0x10004}) {
    INFO("size=" << size);
    const auto words = static_cast<std::size_t>(size / 4);
    std::vector<std::uint32_t> memory(words + 3, 0xA5A5A5A5u);
    VS::Core::Memory::ClearLiveAlignedWords(memory.data() + 1, size);
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
  VS::Core::Memory::ClearLiveAlignedWords(&word, -4);
  VS::Core::Memory::ClearLiveAlignedWords(nullptr, 0);
  CHECK(word == 0x12345678);
}

TEST_CASE("Battle halfword copies preserve boundaries and halfword alignment",
          "[core][memory][halfword]") {
  for (const auto copy : kHalfwordCopies) {
    for (const int bytes : {2, 4, 6, 8, 10, 14, 16, 24, 26, 32, 64, 92, 128}) {
      if (copy == kHalfwordCopies[1] && bytes % 8 != 0) {
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
  for (const auto copy : kHalfwordCopies) {
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
  CopyHalfwordBlocks(blocks.data() + 1, blocks.data(), 16);
  const std::array<std::uint16_t, 12> expected_blocks = {1, 1, 2, 3,  4,  4,
                                                         6, 7, 8, 10, 11, 12};
  CHECK(blocks == expected_blocks);
  std::array<std::uint16_t, 12> prefix = {1, 2, 3, 4,  5,  6,
                                          7, 8, 9, 10, 11, 12};
  CopyHalfwords(prefix.data() + 1, prefix.data(), 18);
  const std::array<std::uint16_t, 12> expected_prefix = {1, 1, 1, 3, 4,  5,
                                                         5, 7, 8, 9, 11, 12};
  CHECK(prefix == expected_prefix);
}

TEST_CASE("Battle halfword copies bound partial and empty native ranges",
          "[core][memory][halfword]") {
  const std::array<std::uint16_t, 8> source = {1, 2, 3, 4, 5, 6, 7, 8};
  std::array<std::uint16_t, 8> halfwords{};
  CopyHalfwords(halfwords.data(), source.data(), 3);
  CHECK(halfwords[0] == 1);
  CHECK(halfwords[1] == 0);
  std::array<std::uint16_t, 8> blocks{};
  CopyHalfwordBlocks(blocks.data(), source.data(), 10);
  CHECK(blocks[3] == 4);
  CHECK(blocks[4] == 0);
  for (const auto copy : kHalfwordCopies) {
    copy(nullptr, nullptr, 0);
    copy(nullptr, nullptr, -8);
    auto identity = source;
    copy(identity.data(), identity.data(), sizeof identity);
    CHECK(identity == source);
  }
}

TEST_CASE("Word copies keep the original size rules", "[core][memory]") {
  CHECK(VS::Core::Memory::AlignedWordBytes(0x1007) == 0x1004);
  CHECK(VS::Core::Memory::ZeroAlignedWordBytes(0x12347) == 0x2344);
  CHECK(VS::Core::Memory::ZeroAlignedWordBytes(-1) == 0xFFFC);
  CHECK(VS::Core::Memory::ClearAlignedWordBytes(0x12347) == 0x12344);
  CHECK(VS::Core::Memory::ClearAlignedWordBytes(-4) == 0);
  CHECK(VS::Core::Memory::HalfwordBytes(7) == 6);
  CHECK(VS::Core::Memory::HalfwordBlockBytes(15) == 8);
}

TEST_CASE("Aligned word copies move forward through overlap",
          "[core][memory]") {
  std::array<std::uint32_t, 6> lower = {1, 2, 3, 4, 5, 6};
  VS::Core::Memory::CopyLiveAlignedWords(lower.data(), lower.data() + 1, 20);
  CHECK(lower == std::array<std::uint32_t, 6>{2, 3, 4, 5, 6, 6});
  // A higher destination repeats the leading word, as the assembly loop does.
  std::array<std::uint32_t, 6> higher = {1, 2, 3, 4, 5, 6};
  VS::Core::Memory::CopyLiveAlignedWords(higher.data() + 1, higher.data(), 20);
  CHECK(higher == std::array<std::uint32_t, 6>{1, 1, 1, 1, 1, 1});
}

TEST_CASE("Zeroed words read only the low sixteen size bits",
          "[core][memory]") {
  std::array<std::uint32_t, 4> words = {1, 2, 3, 4};
  VS::Core::Memory::ZeroLiveAlignedWords(words.data(), 0x10008);
  CHECK(words == std::array<std::uint32_t, 4>{0, 0, 3, 4});
}

TEST_CASE("Word copies stop at the span extents", "[core][memory][bounds]") {
  std::array<std::byte, 12> destination{};
  destination.fill(std::byte{0xA5});
  const std::array<std::byte, 16> source{};
  CHECK(VS::Core::Memory::CopyAlignedWords(destination, source, 64) == 12);
  CHECK(VS::Core::Memory::CopyAlignedWords(std::span(destination).first(10),
                                           source, 64) == 8);
  CHECK(VS::Core::Memory::ZeroAlignedWords(std::span(destination).first(7),
                                           64) == 4);
  CHECK(VS::Core::Memory::ClearAlignedWords(destination, 64) == 12);
  CHECK(VS::Core::Memory::CopyHalfwords(std::span(destination).first(11),
                                        source, 64) == 10);
  CHECK(VS::Core::Memory::CopyHalfwordBlocks(destination, source, 64) == 8);
}

TEST_CASE("Live copies bound array arguments by their extent",
          "[core][memory][bounds]") {
  struct Record {
    std::uint8_t palettes[8];
    std::uint32_t guard;
  };
  Record record{};
  record.guard = 0xA5A5A5A5u;
  std::array<std::uint8_t, 32> data{};
  data.fill(0x11);
  const std::uint64_t before = VS::Core::Memory::TruncatedLiveCopyCount();
  VS::Core::Memory::CopyLiveAlignedWords(record.palettes, data.data(), 32);
  CHECK(record.palettes[7] == 0x11);
  CHECK(record.guard == 0xA5A5A5A5u);
  VS::Core::Memory::ZeroLiveAlignedWords(record.palettes, 32);
  VS::Core::Memory::ClearLiveAlignedWords(record.palettes, 32);
  VS::Core::Memory::CopyLiveHalfwords(record.palettes, data.data(), 32);
  VS::Core::Memory::CopyLiveHalfwordBlocks(record.palettes, data.data(), 32);
  CHECK(record.guard == 0xA5A5A5A5u);
  CHECK(VS::Core::Memory::TruncatedLiveCopyCount() == before + 5);

  const std::uint8_t small_source[4] = {1, 2, 3, 4};
  std::array<std::uint8_t, 8> destination{};
  VS::Core::Memory::CopyLiveAlignedWords(destination.data(), small_source, 8);
  CHECK(destination == std::array<std::uint8_t, 8>{1, 2, 3, 4, 0, 0, 0, 0});
  CHECK(VS::Core::Memory::TruncatedLiveCopyCount() == before + 6);

  // Requests within the extent are not reported.
  VS::Core::Memory::CopyLiveAlignedWords(record.palettes, data.data(), 8);
  CHECK(VS::Core::Memory::TruncatedLiveCopyCount() == before + 6);
}

TEST_CASE("Live copies across consecutive fields use the record tail",
          "[core][memory][bounds]") {
  struct Record {
    std::uint32_t header;
    std::uint8_t overlays[8];
    std::uint32_t following[2];
  };
  Record record{};
  std::array<std::uint8_t, 32> data{};
  data.fill(0x22);
  const auto tail = VS::Core::Memory::RecordTail(record, record.overlays);
  CHECK(tail.size() == sizeof record - sizeof record.header);
  const std::uint64_t before = VS::Core::Memory::TruncatedLiveCopyCount();
  VS::Core::Memory::CopyLiveAlignedWords(tail, data.data(), 16);
  CHECK(record.header == 0u);
  CHECK(record.following[1] == 0x22222222u);
  CHECK(VS::Core::Memory::TruncatedLiveCopyCount() == before);
  VS::Core::Memory::CopyLiveAlignedWords(tail, data.data(), 32);
  CHECK(VS::Core::Memory::TruncatedLiveCopyCount() == before + 1);
}
