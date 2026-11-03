#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstddef>
#include <cstdint>
#include <span>

// The BATTLE.PRG backward copies and halfword read, and the SLUS sound word
// copies. Every operation is bounded by its spans: units past either extent
// are not touched and the return value is the number of bytes written. Sizes
// the original turned into a runaway loop are refused and write nothing
// (#277).
namespace VS::Core::Memory {
// BATTLE.PRG vs_battle_getShort (573B8.c:146): the little-endian halfword at
// any alignment.
[[nodiscard]] std::int16_t ReadUnalignedShort(
    std::span<const std::byte, 2> source) noexcept;

// BATTLE.PRG vs_battle_rMemzero (5BF94.c:4476): clears whole words from the
// last one down. The original only stops when it reaches the destination, so
// sizes that are not a positive multiple of four are refused.
std::size_t ZeroWordsBackward(std::span<std::byte> destination,
                              std::int32_t size) noexcept;
// The bytes ZeroWordsBackward writes for `size` when nothing bounds it.
[[nodiscard]] std::size_t ZeroWordsBackwardBytes(std::int32_t size) noexcept;

// BATTLE.PRG vs_battle_rMemcpy (5BF94.c:4486): copies bytes from the last one
// down, so an overlapping copy to a higher address behaves like memmove. The
// original's do-while copies past the start for a size of zero, so
// nonpositive sizes are refused.
std::size_t CopyBytesBackward(std::span<std::byte> destination,
                              std::span<const std::byte> source,
                              std::int32_t size) noexcept;
[[nodiscard]] std::size_t CopyBytesBackwardBytes(std::int32_t size) noexcept;

// SLUS_010.40 Sound_memcpy32 (25AC.c:3965): size / 4 words forward, each
// group of four loaded before any is stored. The source comes first, as in
// the original.
std::size_t CopyWords(std::span<const std::byte> source,
                      std::span<std::byte> destination,
                      std::uint32_t size) noexcept;
[[nodiscard]] std::size_t WordBytes(std::uint32_t size) noexcept;

// SLUS_010.40 Sound_memswp32 (25AC.c:3993): exchanges size / 4 words, two
// pairs at a time.
std::size_t SwapWords(std::span<std::byte> first, std::span<std::byte> second,
                      std::uint32_t size) noexcept;
}  // namespace VS::Core::Memory
