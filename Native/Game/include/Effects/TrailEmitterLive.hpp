#pragma once
// Hand-maintained (tools/port/owned.csv).

#include "Effects/TrailEmitter.hpp"

struct Effects_EffectInstance;
struct Effects_ParticleDefinition;
struct Effects_ParticleEmitterScratch;
struct Effects_RendererSlot;
struct Effects_TrailQuadSet;

// PLG074's emission cursor in the overlay module image (ModuleData.cpp).
extern "C" int Content_plg074TrailEmissionCounter[1];

// Live binding for the PLG074 trail emitter (TrailEmitter.cpp): the current
// effect package and instance, the emitter scratch block, the GTE, the BIOS
// generator, the high-end heap and PLG073's shared trail stages. Definitions,
// curves and transforms are read only within the current package's recorded
// extents (PackageExtents.hpp); an emitter whose data the package lacks is
// refused with a diagnostic instead of indexing past it (#271).
namespace VS::Effects {
// PLG074 func_800FB414: places the first spline point of each trail.
void GenerateLivePlg074TrailOrigins(
    const Effects_ParticleDefinition& definition,
    Effects_ParticleEmitterScratch& scratch, Effects_EffectInstance& instance,
    Effects_TrailQuadSet& state);
// PLG074 func_800FCFEC: the renderer slot entry (EFFPURGE.BIN entry 89).
int UpdateLivePlg074TrailEmitter(Effects_RendererSlot* slot,
                                 unsigned int command, int packed);
}  // namespace VS::Effects
