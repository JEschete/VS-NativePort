#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstddef>
#include <span>

#include "Effects/Transforms.hpp"

struct Effects_EffectInstance;

// Live binding for the effect transforms (Transforms.hpp) over an effect
// instance's transform records: the definitions are the current package's
// bound block 10, counted by its block-9 transform count but never past the
// block or the instance's sixteen records (the original indexed both by the
// count alone). The host reads the actors' bones and placements, the render
// camera, the action origin, the curve samples, the BIOS rand() and the
// GTE. The entries keep the original signatures for the copied effect
// script; a null instance is refused with a diagnostic.
namespace VS::Effects {
// The bound block-10 definitions the current package's block 9 counts,
// clamped to the block and to kEffectTransformSlots, decoded into `out`;
// returns how many.
std::size_t LiveTransformDefinitions(
    std::span<TransformDefinition, kEffectTransformSlots> out);

// The host the entries run the core over.
[[nodiscard]] TransformHost& LiveTransformHost();

// func_800CFEF0 (5BF94.c:5940).
void LiveInitializeTransformStates(Effects_EffectInstance* instance);
// func_800D0D08 (5BF94.c:6227).
void LiveUpdateEffectTransforms(Effects_EffectInstance* instance);
}  // namespace VS::Effects
