// Hand-maintained (tools/port/owned.csv).
#include "Core/Memory/WordCopy.hpp"

#include <algorithm>
#include <array>
#include <cstring>

namespace VS::Core::Memory {
namespace {
constexpr std::size_t kWordBytes = 4;
constexpr std::size_t kHalfwordBytes = 2;
constexpr std::size_t kHalfwordBlockBytes = 8;
constexpr std::uint32_t kZeroSizeMask = 0xFFFFu;

constexpr std::size_t RoundDown(std::size_t bytes, std::size_t unit) {
  return bytes - bytes % unit;
}

std::size_t Bounded(std::size_t bytes, std::size_t unit,
                    std::size_t destination_bytes,
                    std::size_t source_bytes = SIZE_MAX) {
  return RoundDown(std::min({bytes, destination_bytes, source_bytes}), unit);
}

// Moves `bytes` (a multiple of Unit) forward one unit at a time; each unit is
// loaded whole before it is stored, as the assembly's register copies are.
template <std::size_t Unit>
void CopyForward(std::byte* destination, const std::byte* source,
                 std::size_t bytes) {
  for (std::size_t offset = 0; offset < bytes; offset += Unit) {
    std::array<std::byte, Unit> unit{};
    std::memcpy(unit.data(), source + offset, Unit);
    std::memcpy(destination + offset, unit.data(), Unit);
  }
}
}  // namespace

std::size_t AlignedWordBytes(std::uint32_t size) {
  return RoundDown(size, kWordBytes);
}

std::size_t CopyAlignedWords(std::span<std::byte> destination,
                             std::span<const std::byte> source,
                             std::uint32_t size) {
  const std::size_t bytes = Bounded(AlignedWordBytes(size), kWordBytes,
                                    destination.size(), source.size());
  CopyForward<kWordBytes>(destination.data(), source.data(), bytes);
  return bytes;
}

std::size_t ZeroAlignedWordBytes(std::int32_t size) {
  return RoundDown(static_cast<std::uint32_t>(size) & kZeroSizeMask,
                   kWordBytes);
}

std::size_t ZeroAlignedWords(std::span<std::byte> destination,
                             std::int32_t size) {
  const std::size_t bytes =
      Bounded(ZeroAlignedWordBytes(size), kWordBytes, destination.size());
  std::fill_n(destination.begin(), bytes, std::byte{});
  return bytes;
}

std::size_t ClearAlignedWordBytes(std::int32_t size) {
  return size > 0 ? RoundDown(static_cast<std::size_t>(size), kWordBytes) : 0;
}

std::size_t ClearAlignedWords(std::span<std::byte> destination,
                              std::int32_t size) {
  const std::size_t bytes =
      Bounded(ClearAlignedWordBytes(size), kWordBytes, destination.size());
  std::fill_n(destination.begin(), bytes, std::byte{});
  return bytes;
}

std::size_t HalfwordBytes(std::int32_t size) {
  return size > 0 ? RoundDown(static_cast<std::size_t>(size), kHalfwordBytes)
                  : 0;
}

std::size_t CopyHalfwords(std::span<std::byte> destination,
                          std::span<const std::byte> source,
                          std::int32_t size) {
  const std::size_t bytes = Bounded(HalfwordBytes(size), kHalfwordBytes,
                                    destination.size(), source.size());
  const std::size_t leading = bytes % kHalfwordBlockBytes;
  CopyForward<kHalfwordBytes>(destination.data(), source.data(), leading);
  CopyForward<kHalfwordBlockBytes>(destination.data() + leading,
                                   source.data() + leading, bytes - leading);
  return bytes;
}

std::size_t HalfwordBlockBytes(std::int32_t size) {
  return size > 0
             ? RoundDown(static_cast<std::size_t>(size), kHalfwordBlockBytes)
             : 0;
}

std::size_t CopyHalfwordBlocks(std::span<std::byte> destination,
                               std::span<const std::byte> source,
                               std::int32_t size) {
  const std::size_t bytes =
      Bounded(HalfwordBlockBytes(size), kHalfwordBlockBytes, destination.size(),
              source.size());
  CopyForward<kHalfwordBlockBytes>(destination.data(), source.data(), bytes);
  return bytes;
}
}  // namespace VS::Core::Memory
