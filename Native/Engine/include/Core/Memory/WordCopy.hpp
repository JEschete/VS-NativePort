#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstddef>
#include <cstdint>
#include <span>

// The SLUS and BATTLE.PRG word and halfword copies, written in MIPS assembly
// in the original. They move whole units forward from the lowest address, so
// an overlapping copy to a lower address behaves like memmove and one to a
// higher address repeats the leading units, as on the console. Every copy is
// bounded by the spans: units past either extent are not touched and the
// return value is the number of bytes written (#268).
namespace VS::Core::Memory {
// SLUS_010.40 vs_main_memcpy (main.c:11669): the size's low two bits are
// ignored.
std::size_t CopyAlignedWords(std::span<std::byte> destination,
                             std::span<const std::byte> source,
                             std::uint32_t size);
// The bytes CopyAlignedWords writes for `size` when nothing bounds it.
[[nodiscard]] std::size_t AlignedWordBytes(std::uint32_t size);

// SLUS_010.40 vs_main_bzero (main.c:11699): only the low 16 bits of the size
// are read and its low two bits are ignored.
std::size_t ZeroAlignedWords(std::span<std::byte> destination,
                             std::int32_t size);
[[nodiscard]] std::size_t ZeroAlignedWordBytes(std::int32_t size);

// BATTLE.PRG func_800E4288 (7B5DC.s:344-367): the full size, low two bits
// ignored. Native extension: nonpositive sizes write nothing instead of
// entering the assembly's invalid-size loop.
std::size_t ClearAlignedWords(std::span<std::byte> destination,
                              std::int32_t size);
[[nodiscard]] std::size_t ClearAlignedWordBytes(std::int32_t size);

// BATTLE.PRG vs_battle_copyAligned (573B8.c:242): leading halfwords, then
// eight-byte blocks. Native extension: nonpositive sizes write nothing and an
// odd trailing byte is ignored.
std::size_t CopyHalfwords(std::span<std::byte> destination,
                          std::span<const std::byte> source, std::int32_t size);
[[nodiscard]] std::size_t HalfwordBytes(std::int32_t size);

// BATTLE.PRG vs_battle_memcpy (573B8.c:253): each block's four halfwords are
// loaded before any is stored. Native extension: nonpositive sizes and an
// incomplete trailing block write nothing.
std::size_t CopyHalfwordBlocks(std::span<std::byte> destination,
                               std::span<const std::byte> source,
                               std::int32_t size);
[[nodiscard]] std::size_t HalfwordBlockBytes(std::int32_t size);
}  // namespace VS::Core::Memory
