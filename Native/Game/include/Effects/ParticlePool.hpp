#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstdint>

#include "Core/Math/ScalarMath.hpp"

// The shared particle pool (BATTLE.PRG 5BF94.c:6777-6880) and the forces on
// every particle (5BF94.c:7826-7828). The pool reserves its particles in one
// block from the high end of the game heap and keeps the unused ones on a free
// list; each effect instance takes particles from it into a list of its own,
// newest first, and returns them. The records stay in the heap:
// ParticlePoolLive.hpp edits them through ParticleRecords and keeps the
// original entry points.
namespace VS::Effects {
struct LiveParticle;

// particlePool, freeParticles, particleCapacity,
// usedParticleCount and particlePoolReferences.
struct ParticlePool {
  LiveParticle* block{};
  LiveParticle* free{};
  std::int32_t capacity{};
  std::int32_t used{};
  std::int32_t references{};

  friend bool operator==(const ParticlePool&, const ParticlePool&) = default;
};

// An effect instance's particles (LiveEffectInstance's particles and
// particleCount): the newest and the count, an unsigned byte as the original
// read it.
struct ParticleList {
  LiveParticle* first{};
  std::uint8_t count{};

  friend bool operator==(const ParticleList&, const ParticleList&) = default;
};

// The particle records: game heap records, each linked to the previous and
// the next particle of its list. The free list links forward only.
class ParticleRecords {
 public:
  virtual ~ParticleRecords() = default;
  // A block of `count` records from the high end of the heap; null (refused)
  // when the heap has no room.
  [[nodiscard]] virtual LiveParticle* Reserve(std::int32_t count) = 0;
  virtual void Free(LiveParticle& block) = 0;
  // Record `index` of the block.
  [[nodiscard]] virtual LiveParticle& At(LiveParticle& block,
                                         std::int32_t index) = 0;
  [[nodiscard]] virtual LiveParticle* Previous(LiveParticle& particle) = 0;
  virtual void SetPrevious(LiveParticle& particle, LiveParticle* previous) = 0;
  [[nodiscard]] virtual LiveParticle* Next(LiveParticle& particle) = 0;
  virtual void SetNext(LiveParticle& particle, LiveParticle* next) = 0;
  // Clears the particle's texture animation and marks it unaged (age 0xFF).
  virtual void Renew(LiveParticle& particle) = 0;

 protected:
  ParticleRecords() = default;
  ParticleRecords(const ParticleRecords&) = default;
  ParticleRecords& operator=(const ParticleRecords&) = default;
  ParticleRecords(ParticleRecords&&) = default;
  ParticleRecords& operator=(ParticleRecords&&) = default;
};

// func_800D268C (5BF94.c:6777): forgets every reference, keeping the block;
// the effect runtime starts on a reset heap.
void ResetParticlePoolReferences(ParticlePool& pool);

// func_800D2698 (5BF94.c:6779): the first reference reserves a block of
// `count` particles and links them all free, in order; a later reference only
// counts itself. False (refused), leaving the pool, for a count below 1 or
// when the heap has no room. A single particle is linked alone, where the
// original linked a second one past the block.
[[nodiscard]] bool AcquireParticlePool(ParticlePool& pool, std::int32_t count,
                                       ParticleRecords& records);

// func_800D278C (5BF94.c:6805): the last reference frees the block; an
// earlier one only drops itself. False (refused) when nothing references the
// pool, where the original counted below zero.
[[nodiscard]] bool ReleaseParticlePool(ParticlePool& pool,
                                       ParticleRecords& records);

// func_800D27F0 (5BF94.c:6818): renews the first free particle and links it
// first in `list`. Null (refused) when every particle is in use, where the
// original wrote through the null particle.
[[nodiscard]] LiveParticle* AllocateEffectParticle(ParticlePool& pool,
                                                   ParticleList& list,
                                                   ParticleRecords& records);

// func_800D2888 (5BF94.c:6847): unlinks the particle from `list` and returns
// it first on the free list.
void ReleaseEffectParticle(LiveParticle& particle, ParticlePool& pool,
                           ParticleList& list, ParticleRecords& records);

// func_800D2904 (5BF94.c:6865): returns the whole list to the front of the
// free list. The list keeps its first particle, as in the original; the
// instance is destroyed next.
void ReleaseAllEffectParticles(ParticlePool& pool, ParticleList& list,
                               ParticleRecords& records);

// The forces on every particle (particleGravity and
// particleDrag).
struct ParticleForces {
  Core::Math::Vec3 gravity{};
  std::int32_t drag{};

  friend bool operator==(const ParticleForces&,
                         const ParticleForces&) = default;
};

// func_800D5260 (5BF94.c:7826).
void SetParticleGravity(ParticleForces& forces, Core::Math::Vec3 gravity);

// func_800D5294 (5BF94.c:7828).
void SetParticleDrag(ParticleForces& forces, std::int32_t drag);
}  // namespace VS::Effects
