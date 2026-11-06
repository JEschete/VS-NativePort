#pragma once
// Hand-maintained (tools/port/owned.csv).

#include "Effects/Templates/TrailCore.hpp"

struct Effects_EffectInstance;
struct Effects_ParticleDefinition;
struct Effects_ParticleEmitterScratch;
struct Effects_RendererSlot;
struct Effects_Trail9State;
struct Effects_TrailQuadSet;

// Live bindings for the five- and nine-sample trail template instances
// (TrailTarget.cpp ... TrailRender.cpp). RunLiveTrailStage and
// RunLiveTrailNineStage load one emitter's definition ranges, emitter scratch
// and trail buffer into the typed records, run a stage over them with the
// host bound to the current effect package and instance, the GTE, the BIOS
// generator, the emitter's texture animation and transparency samples and the
// frame's ordering table and primitive buffer, and store the scratch and
// trails back. Curves and textures are read only within the current package's
// recorded extents (PackageExtents.hpp); a stage whose data the package lacks
// samples 0 or draws nothing, with a diagnostic, instead of indexing past it.
// Each overlay's renderer-slot entry keeps the shape the generated renderer
// tables and plugin updates call (EFFPURGE.BIN entry points) and runs the
// emitter lifecycle (TrailEmitterLive.hpp) with its own emission cursor and
// stage set.
namespace VS::Effects::Templates {
void RunLiveTrailStage(TrailStage stage,
                       const Effects_ParticleDefinition& definition,
                       Effects_ParticleEmitterScratch& scratch,
                       Effects_EffectInstance& instance,
                       Effects_TrailQuadSet& state);
// RunLiveTrailStage over a nine-sample emitter (trails9.h; TemplateState.h).
void RunLiveTrailNineStage(TrailNineStage stage,
                           const Effects_ParticleDefinition& definition,
                           Effects_ParticleEmitterScratch& scratch,
                           Effects_EffectInstance& instance,
                           Effects_Trail9State& state);

int UpdateLivePlg057Trail(Effects_RendererSlot* slot, unsigned int command,
                          int packed);
int UpdateLivePlg073Trail(Effects_RendererSlot* slot, unsigned int command,
                          int packed);
int UpdateLivePlg086Trail(Effects_RendererSlot* slot, unsigned int command,
                          int packed);
int UpdateLivePlg087Trail(Effects_RendererSlot* slot, unsigned int command,
                          int packed);
int UpdateLivePlg128Trail(Effects_RendererSlot* slot, unsigned int command,
                          int packed);
int UpdateLivePlg134Trail(Effects_RendererSlot* slot, unsigned int command,
                          int packed);
int UpdateLivePlg137Trail(Effects_RendererSlot* slot, unsigned int command,
                          int packed);
int UpdateLivePlg141Trail(Effects_RendererSlot* slot, unsigned int command,
                          int packed);
int UpdateLivePlg146Trail(Effects_RendererSlot* slot, unsigned int command,
                          int packed);
int UpdateLivePlg148Trail(Effects_RendererSlot* slot, unsigned int command,
                          int packed);
int UpdateLivePlg242Trail(Effects_RendererSlot* slot, unsigned int command,
                          int packed);
int UpdateLivePlg255Trail(Effects_RendererSlot* slot, unsigned int command,
                          int packed);
int UpdateLivePlg265Trail(Effects_RendererSlot* slot, unsigned int command,
                          int packed);
int UpdateLivePlg266Trail(Effects_RendererSlot* slot, unsigned int command,
                          int packed);
int UpdateLivePlg291Trail(Effects_RendererSlot* slot, unsigned int command,
                          int packed);

// The trails9.h overlays' renderer-slot entries (trails9.h:685).
int UpdateLivePlg049TrailNine(Effects_RendererSlot* slot, unsigned int command,
                              int packed);
int UpdateLivePlg050TrailNine(Effects_RendererSlot* slot, unsigned int command,
                              int packed);
int UpdateLivePlg057TrailNine(Effects_RendererSlot* slot, unsigned int command,
                              int packed);
int UpdateLivePlg069TrailNine(Effects_RendererSlot* slot, unsigned int command,
                              int packed);
int UpdateLivePlg135TrailNine(Effects_RendererSlot* slot, unsigned int command,
                              int packed);
int UpdateLivePlg161TrailNine(Effects_RendererSlot* slot, unsigned int command,
                              int packed);
int UpdateLivePlg165TrailNine(Effects_RendererSlot* slot, unsigned int command,
                              int packed);
int UpdateLivePlg167TrailNine(Effects_RendererSlot* slot, unsigned int command,
                              int packed);
int UpdateLivePlg168TrailNine(Effects_RendererSlot* slot, unsigned int command,
                              int packed);
int UpdateLivePlg169TrailNine(Effects_RendererSlot* slot, unsigned int command,
                              int packed);
int UpdateLivePlg232TrailNine(Effects_RendererSlot* slot, unsigned int command,
                              int packed);
}  // namespace VS::Effects::Templates
