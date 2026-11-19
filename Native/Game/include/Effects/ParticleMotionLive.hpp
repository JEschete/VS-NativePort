#pragma once
// Hand-maintained (tools/port/owned.csv).

// Live binding for particle motion and ageing (ParticleMotion.hpp): the
// LiveParticle records of an effect instance's list, the emitter scratch,
// the forces ParticlePoolLive.hpp loads and the GTE rotation the emitter set.
// The entries keep the original signatures for the module pointers, the
// emitter and the particle renderers. A missing record is refused with a
// diagnostic.
namespace VS::Effects {
struct LiveEffectInstance;
struct LiveParticle;
struct LiveParticleEmitterScratch;
struct LiveParticleLinePacket;

// func_800CFAAC (5BF94.c:5770): steps a linked line packet's second point
// toward its first.
void LiveStepParticleEndpointTowardStart(LiveParticleLinePacket* packet);
// func_800D2A38 (5BF94.c:6915).
void LiveSetParticleSpawnPosition(LiveParticleEmitterScratch* scratch,
                                  LiveParticle* particle);
// func_800D4C18 (5BF94.c:7658): returns 1, as the original always did. A
// particle is the parent of the events it emits.
int LiveTickEffectParticles(LiveEffectInstance* instance);
// func_800D52A4 (5BF94.c:7830): an empty list is nothing to integrate.
void LiveIntegrateParticleMotion(LiveParticle* first);
}  // namespace VS::Effects
