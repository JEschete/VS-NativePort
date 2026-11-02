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
}  // namespace VS::Effects
