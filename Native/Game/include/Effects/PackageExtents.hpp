#pragma once

#include <cstddef>
#include <span>

// Extents of an effect package's particle definition (block 5) and curve
// (block 4) blocks. The original binders store bare pointers, so a package
// without one of the blocks keeps the previous package's pointer into a
// reused buffer and nothing bounds a definition or curve index. The live
// record lets typed renderers refuse indexes the current package lacks.
namespace VS::Effects {
inline constexpr std::size_t kParticleDefinitionBytes = 0xCC;
inline constexpr std::size_t kParticleDefinitionHeaderBytes = 4;
inline constexpr std::size_t kEffectCurveCapacity = 32;

// Whole definitions in a block-5 payload after its reserved header word.
[[nodiscard]] std::size_t CountParticleDefinitions(std::size_t payload_bytes);

// Curves of a block-4 payload {u16 count, u16 data offset, u8 sizes[count]}
// whose bytes lie inside the payload, counted from the first, at most the
// package's curve capacity.
[[nodiscard]] std::size_t CountContainedEffectCurves(
    std::span<const std::byte> payload);

// Clears the current package's record before its blocks are bound
// (func_800D8060, 6F014.c:746).
void ForgetLivePackageExtents();
// Records the current package's block payloads as they are bound.
void RecordLiveParticleDefinitions(std::span<const std::byte> payload);
void RecordLiveEffectCurves(std::span<const std::byte> payload);
// Counts for the current package's bound blocks; zero when the bound pointer
// is not the recorded block.
[[nodiscard]] std::size_t LiveParticleDefinitionCount();
[[nodiscard]] std::size_t LiveEffectCurveCount();

// Bound block-8 program payloads. Effect instances keep the bare program
// pointer the original stored, so the script loop looks the extent up by
// that pointer. A block recorded again at the same
// address replaces the extent; a package that binds no block 8 keeps the
// previous package's pointer and extent, as the original kept the pointer.
// With every extent in use, the oldest one that neither the action nor the
// secondary package record still binds is replaced, so the resident
// secondary programs survive any number of loaded packages.
void RecordLiveEffectPrograms(std::span<const std::byte> payload);
// Withdraws the program block starting at `programs`.
void ForgetLiveEffectPrograms(const void* programs);
// The recorded program block starting at `programs`; empty when none is.
[[nodiscard]] std::span<const std::byte> LiveEffectPrograms(
    const void* programs);

// Bound block-2 particle animation tables. A texture animation keeps a bare
// pointer into its table, as the original did, so its program's extent is
// looked up by the table containing that pointer. Recording a table withdraws
// every recorded table it overlaps, so a reused buffer has one extent; with
// every extent in use, the oldest one neither package record binds is
// replaced.
void RecordLiveAnimationTable(std::span<std::byte> table);
// Withdraws the animation table starting at `table`.
void ForgetLiveAnimationTable(const void* table);
// The recorded animation table starting at `table`; empty when none is.
[[nodiscard]] std::span<std::byte> LiveAnimationTable(const void* table);
// The bytes from `program` to the end of the recorded animation table that
// contains it; empty when none does.
[[nodiscard]] std::span<const std::byte> LiveAnimationProgram(
    const void* program);
}  // namespace VS::Effects
