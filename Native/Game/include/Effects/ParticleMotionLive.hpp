#pragma once
// Hand-maintained (tools/port/owned.csv).

struct Effects_EffectInstance;
struct Effects_Particle;
struct Effects_ParticleEmitterScratch;
struct Effects_ParticleEndpointView;

// Live binding for particle motion and ageing (ParticleMotion.hpp): the
// Effects_Particle records of an effect instance's list, the emitter scratch,
// the forces ParticlePoolLive.hpp loads and the GTE rotation the emitter set.
// The entries keep the original signatures for the module pointers, the
// emitter and the particle renderers. A missing record is refused with a
// diagnostic.
namespace VS::Effects {
// func_800CFAAC (5BF94.c:5770): steps a linked line packet's second point.
void LiveStepParticleEndpointTowardStart(
    Effects_ParticleEndpointView* endpoints);
// func_800D2A38 (5BF94.c:6915).
void LiveSetParticleSpawnPosition(Effects_ParticleEmitterScratch* scratch,
                                  Effects_Particle* particle);
// func_800D4C18 (5BF94.c:7658): returns 1, as the original always did. A
// particle's events are emitted with a parent made of its position,
// transparency and variant, the fields func_800D2ADC reads of it.
int LiveTickEffectParticles(Effects_EffectInstance* instance);
// func_800D52A4 (5BF94.c:7830): an empty list is nothing to integrate.
void LiveIntegrateParticleMotion(Effects_Particle* first);
}  // namespace VS::Effects
