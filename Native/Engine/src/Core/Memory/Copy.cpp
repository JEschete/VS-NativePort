// Hand-maintained (tools/port/owned.csv).
#include "Core/Memory/Copy.hpp"

#include <algorithm>
#include <array>
#include <cstring>

namespace VS::Core::Memory {
namespace {
constexpr std::size_t kWordBytes = 4;
constexpr std::size_t kWordGroup = 4;
constexpr std::size_t kSwapPair = 2;
constexpr unsigned kByteBits = 8;

constexpr std::size_t WholeWordBytes(std::size_t bytes) {
  return bytes - bytes % kWordBytes;
}

// Loads `count` words whole before storing any of them, as the assembly's
// register copies do, so an overlapping block reads its original contents.
void MoveWords(std::byte* destination, const std::byte* source,
               std::size_t count) {
  std::array<std::byte, kWordBytes * kWordGroup> held{};
  std::memcpy(held.data(), source, count * kWordBytes);
  std::memcpy(destination, held.data(), count * kWordBytes);
}

// Both words are loaded before the second is stored, then the first.
void ExchangeWords(std::byte* first, std::byte* second, std::size_t count) {
  std::array<std::byte, kWordBytes * kSwapPair> from_first{};
  std::array<std::byte, kWordBytes * kSwapPair> from_second{};
  const std::size_t bytes = count * kWordBytes;
  std::memcpy(from_first.data(), first, bytes);
  std::memcpy(from_second.data(), second, bytes);
  std::memcpy(second, from_first.data(), bytes);
  std::memcpy(first, from_second.data(), bytes);
}
}  // namespace

// BATTLE.PRG vs_battle_getShort (573B8.c:146).
std::int16_t ReadUnalignedShort(std::span<const std::byte, 2> source) noexcept {
  const auto low = std::to_integer<std::uint16_t>(source[0]);
  const auto high = std::to_integer<std::uint16_t>(source[1]);
  return static_cast<std::int16_t>(low | (high << kByteBits));
}

std::size_t ZeroWordsBackwardBytes(std::int32_t size) noexcept {
  if (size <= 0) {
    return 0;
  }
  const auto bytes = static_cast<std::size_t>(size);
  return bytes % kWordBytes == 0 ? bytes : 0;
}

// BATTLE.PRG vs_battle_rMemzero (5BF94.c:4476).
std::size_t ZeroWordsBackward(std::span<std::byte> destination,
                              std::int32_t size) noexcept {
  const std::size_t bytes = std::min(ZeroWordsBackwardBytes(size),
                                     WholeWordBytes(destination.size()));
  for (std::size_t end = bytes; end != 0; end -= kWordBytes) {
    std::fill_n(destination.begin() + static_cast<std::ptrdiff_t>(end) -
                    static_cast<std::ptrdiff_t>(kWordBytes),
                kWordBytes, std::byte{});
  }
  return bytes;
}

std::size_t CopyBytesBackwardBytes(std::int32_t size) noexcept {
  return size > 0 ? static_cast<std::size_t>(size) : std::size_t{0};
}

// BATTLE.PRG vs_battle_rMemcpy (5BF94.c:4486). Byte by byte from the end, so
// an overlapping copy to a lower address repeats the original's order rather
// than memmove's.
std::size_t CopyBytesBackward(std::span<std::byte> destination,
                              std::span<const std::byte> source,
                              std::int32_t size) noexcept {
  const std::size_t bytes = std::min(
      {CopyBytesBackwardBytes(size), destination.size(), source.size()});
  for (std::size_t index = bytes; index != 0; --index) {
    destination[index - 1] = source[index - 1];
  }
  return bytes;
}

std::size_t WordBytes(std::uint32_t size) noexcept {
  return WholeWordBytes(size);
}

// SLUS_010.40 Sound_memcpy32 (25AC.c:3965).
std::size_t CopyWords(std::span<const std::byte> source,
                      std::span<std::byte> destination,
                      std::uint32_t size) noexcept {
  const std::size_t bytes = WholeWordBytes(
      std::min({WordBytes(size), destination.size(), source.size()}));
  std::size_t words = bytes / kWordBytes;
  std::size_t offset = 0;
  for (; words >= kWordGroup; words -= kWordGroup) {
    MoveWords(destination.data() + offset, source.data() + offset, kWordGroup);
    offset += kWordBytes * kWordGroup;
  }
  for (; words != 0; --words) {
    MoveWords(destination.data() + offset, source.data() + offset, 1);
    offset += kWordBytes;
  }
  return bytes;
}

// SLUS_010.40 Sound_memswp32 (25AC.c:3993). Each pair loads first[0],
// second[0], first[1], second[1] before storing second then first.
std::size_t SwapWords(std::span<std::byte> first, std::span<std::byte> second,
                      std::uint32_t size) noexcept {
  const std::size_t bytes =
      WholeWordBytes(std::min({WordBytes(size), first.size(), second.size()}));
  std::size_t words = bytes / kWordBytes;
  std::size_t offset = 0;
  for (; words >= kSwapPair; words -= kSwapPair) {
    ExchangeWords(first.data() + offset, second.data() + offset, kSwapPair);
    offset += kWordBytes * kSwapPair;
  }
  if (words != 0) {
    ExchangeWords(first.data() + offset, second.data() + offset, 1);
  }
  return bytes;
}
}  // namespace VS::Core::Memory
