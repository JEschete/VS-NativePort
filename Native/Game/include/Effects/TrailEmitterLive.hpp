#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstdint>
#include <string_view>

#include "Effects/Templates/TrailCore.hpp"
#include "Effects/TrailEmitter.hpp"

struct Effects_EffectInstance;
struct Effects_ParticleDefinition;
struct Effects_ParticleEmitterScratch;
struct Effects_RendererSlot;
struct Effects_Trail9State;
struct Effects_TrailQuadSet;

// PLG074's emission cursor in the overlay module image (ModuleData.cpp).
extern "C" int Content_plg074TrailEmissionCounter[1];

// Live binding for the five- and nine-sample trail emitters (TrailEmitter.cpp):
// the current effect package and instance, the emitter scratch block, the GTE,
// the BIOS generator, the high-end heap and the overlay's trail stages
// (TrailsLive.hpp). Definitions, curves and transforms are read only within
// the current package's recorded extents (PackageExtents.hpp); an emitter
// whose data the package lacks is refused with a diagnostic instead of
// indexing past it (#271).
namespace VS::Effects {
// One overlay's emitter: its name in diagnostics, its emission cursor
// (VS_TRAIL_CORNER_STATE) and its target, control, init, update and render
// instances.
template <typename Stages>
struct BasicLiveTrailOverlay {
  std::string_view name;
  std::int32_t* corner = nullptr;
  const Stages* stages = nullptr;
};
using LiveTrailOverlay = BasicLiveTrailOverlay<Templates::TrailStages>;
using LiveTrailNineOverlay = BasicLiveTrailOverlay<Templates::TrailNineStages>;

// trails5.h:27 VS_TRAIL_ORIGIN_FUNCTION: places the first spline point of
// each trail, advancing the overlay's emission cursor.
void GenerateLiveTrailOrigins(const Effects_ParticleDefinition& definition,
                              Effects_ParticleEmitterScratch& scratch,
                              Effects_EffectInstance& instance,
                              Effects_TrailQuadSet& state,
                              const LiveTrailOverlay& overlay);
// trails5.h:677 VS_TRAIL_DISPATCH_FUNCTION: one lifecycle command for the
// overlay's renderer slot; returns whether the emitter stays attached.
int UpdateLiveTrailEmitter(Effects_RendererSlot* slot, unsigned int command,
                           int packed, const LiveTrailOverlay& overlay);

// trails9.h:27 VS_TRAIL_ORIGIN_FUNCTION over a nine-sample emitter.
void GenerateLiveTrailNineOrigins(const Effects_ParticleDefinition& definition,
                                  Effects_ParticleEmitterScratch& scratch,
                                  Effects_EffectInstance& instance,
                                  Effects_Trail9State& state,
                                  const LiveTrailNineOverlay& overlay);
// trails9.h:685 VS_TRAIL_DISPATCH_FUNCTION: the lifecycle of a nine-sample
// emitter, which holds at most eight trails and nine transparency samples.
int UpdateLiveTrailNineEmitter(Effects_RendererSlot* slot, unsigned int command,
                               int packed, const LiveTrailNineOverlay& overlay);

// PLG074 func_800FB414: places the first spline point of each trail.
void GenerateLivePlg074TrailOrigins(
    const Effects_ParticleDefinition& definition,
    Effects_ParticleEmitterScratch& scratch, Effects_EffectInstance& instance,
    Effects_TrailQuadSet& state);
// PLG074 func_800FCFEC: the renderer slot entry (EFFPURGE.BIN entry 89), with
// PLG074's own cursor and PLG073's byte-identical stages.
int UpdateLivePlg074TrailEmitter(Effects_RendererSlot* slot,
                                 unsigned int command, int packed);
}  // namespace VS::Effects
