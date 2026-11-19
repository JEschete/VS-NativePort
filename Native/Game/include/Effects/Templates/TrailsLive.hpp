#pragma once
// Hand-maintained (tools/port/owned.csv).

#include "Effects/Templates/TrailCore.hpp"
#include "Effects/TrailEmitter.hpp"

namespace VS::Effects {
struct LiveEffectInstance;
struct LiveParticleDefinition;
struct LiveParticleEmitterScratch;
struct LiveRendererSlot;
}  // namespace VS::Effects

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
                       const LiveParticleDefinition& definition,
                       LiveParticleEmitterScratch& scratch,
                       LiveEffectInstance& instance, LiveTrailState& state);
// RunLiveTrailStage over a nine-sample emitter (trails9.h).
void RunLiveTrailNineStage(TrailNineStage stage,
                           const LiveParticleDefinition& definition,
                           LiveParticleEmitterScratch& scratch,
                           LiveEffectInstance& instance,
                           LiveTrailNineState& state);

int UpdateLivePlg057Trail(LiveRendererSlot* slot, unsigned int command,
                          int packed);
int UpdateLivePlg073Trail(LiveRendererSlot* slot, unsigned int command,
                          int packed);
int UpdateLivePlg086Trail(LiveRendererSlot* slot, unsigned int command,
                          int packed);
int UpdateLivePlg087Trail(LiveRendererSlot* slot, unsigned int command,
                          int packed);
int UpdateLivePlg128Trail(LiveRendererSlot* slot, unsigned int command,
                          int packed);
int UpdateLivePlg134Trail(LiveRendererSlot* slot, unsigned int command,
                          int packed);
int UpdateLivePlg137Trail(LiveRendererSlot* slot, unsigned int command,
                          int packed);
int UpdateLivePlg141Trail(LiveRendererSlot* slot, unsigned int command,
                          int packed);
int UpdateLivePlg146Trail(LiveRendererSlot* slot, unsigned int command,
                          int packed);
int UpdateLivePlg148Trail(LiveRendererSlot* slot, unsigned int command,
                          int packed);
int UpdateLivePlg242Trail(LiveRendererSlot* slot, unsigned int command,
                          int packed);
int UpdateLivePlg255Trail(LiveRendererSlot* slot, unsigned int command,
                          int packed);
int UpdateLivePlg265Trail(LiveRendererSlot* slot, unsigned int command,
                          int packed);
int UpdateLivePlg266Trail(LiveRendererSlot* slot, unsigned int command,
                          int packed);
int UpdateLivePlg291Trail(LiveRendererSlot* slot, unsigned int command,
                          int packed);

// The trails9.h overlays' renderer-slot entries (trails9.h:685).
int UpdateLivePlg049TrailNine(LiveRendererSlot* slot, unsigned int command,
                              int packed);
int UpdateLivePlg050TrailNine(LiveRendererSlot* slot, unsigned int command,
                              int packed);
int UpdateLivePlg057TrailNine(LiveRendererSlot* slot, unsigned int command,
                              int packed);
int UpdateLivePlg069TrailNine(LiveRendererSlot* slot, unsigned int command,
                              int packed);
int UpdateLivePlg135TrailNine(LiveRendererSlot* slot, unsigned int command,
                              int packed);
int UpdateLivePlg161TrailNine(LiveRendererSlot* slot, unsigned int command,
                              int packed);
int UpdateLivePlg165TrailNine(LiveRendererSlot* slot, unsigned int command,
                              int packed);
int UpdateLivePlg167TrailNine(LiveRendererSlot* slot, unsigned int command,
                              int packed);
int UpdateLivePlg168TrailNine(LiveRendererSlot* slot, unsigned int command,
                              int packed);
int UpdateLivePlg169TrailNine(LiveRendererSlot* slot, unsigned int command,
                              int packed);
int UpdateLivePlg232TrailNine(LiveRendererSlot* slot, unsigned int command,
                              int packed);
}  // namespace VS::Effects::Templates
