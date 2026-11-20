#pragma once
// Hand-maintained (tools/port/owned.csv).

#include "Effects/ParticlePool.hpp"

namespace VS::Core::Gte {
struct VECTOR;
}  // namespace VS::Core::Gte

// Live binding for the particle pool and forces (ParticlePool.hpp): the pool
// is the particlePool globals, a list an effect instance's particles
// and particleCount, and the forces particleGravity and
// particleDrag. The entries keep the original signatures for the
// effect runtime, resources, instances, emitter and module pointers. A missing
// instance, particle or argument is refused with a diagnostic, as are a count
// below 1, an unbalanced release and an allocation from a full pool.
namespace VS::Effects {
struct LiveEffectInstance;
struct LiveParticle;

// func_800D268C (5BF94.c:6777).
void LiveResetParticlePoolReferences();
// func_800D2698 (5BF94.c:6779).
void LiveAcquireParticlePool(int count);
// func_800D278C (5BF94.c:6805).
void LiveReleaseParticlePool();
// func_800D27F0 (5BF94.c:6818): reports diagnostic 0x14 and returns null when
// every particle is in use.
LiveParticle* LiveAllocateEffectParticle(LiveEffectInstance* instance);
// func_800D2888 (5BF94.c:6847).
void LiveReleaseEffectParticle(LiveParticle* particle,
                               LiveEffectInstance* instance);
// func_800D2904 (5BF94.c:6865).
void LiveReleaseAllEffectParticles(LiveEffectInstance* instance);
// func_800D5260 (5BF94.c:7826): the gravity's fourth word, which nothing
// reads, is copied as in the original.
void LiveSetParticleGravity(VS::Core::Gte::VECTOR* gravity);
// func_800D5294 (5BF94.c:7828).
void LiveSetParticleDrag(int* drag);
// The forces particleGravity and particleDrag hold.
[[nodiscard]] ParticleForces LoadLiveParticleForces();
}  // namespace VS::Effects
