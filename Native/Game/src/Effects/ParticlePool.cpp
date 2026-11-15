// Hand-maintained (tools/port/owned.csv).
#include "Effects/ParticlePool.hpp"

#include <cstdint>

#include "Core/Math/ScalarMath.hpp"

namespace VS::Effects {
void ResetParticlePoolReferences(ParticlePool& pool) { pool.references = 0; }

bool AcquireParticlePool(ParticlePool& pool, std::int32_t count,
                         ParticleRecords& records) {
  if (pool.references != 0) {
    pool.references = Core::Math::WrappingAdd(pool.references, 1);
    return true;
  }
  if (count < 1) {
    return false;
  }
  Effects_Particle* block = records.Reserve(count);
  if (block == nullptr) {
    return false;
  }
  Effects_Particle* previous = nullptr;
  for (std::int32_t index = 0; index < count; ++index) {
    Effects_Particle& particle = records.At(*block, index);
    records.SetPrevious(particle, previous);
    records.SetNext(
        particle, index + 1 < count ? &records.At(*block, index + 1) : nullptr);
    previous = &particle;
  }
  pool = {.block = block,
          .free = block,
          .capacity = count,
          .used = 0,
          .references = 1};
  return true;
}

bool ReleaseParticlePool(ParticlePool& pool, ParticleRecords& records) {
  if (pool.references < 1) {
    return false;
  }
  if (pool.references != 1) {
    --pool.references;
    return true;
  }
  if (pool.block != nullptr) {
    records.Free(*pool.block);
    pool.block = nullptr;
  }
  pool.references = 0;
  return true;
}

Effects_Particle* AllocateEffectParticle(ParticlePool& pool, ParticleList& list,
                                         ParticleRecords& records) {
  Effects_Particle* particle = pool.free;
  if (particle == nullptr) {
    return nullptr;
  }
  records.Renew(*particle);
  records.SetPrevious(*particle, nullptr);
  pool.used = Core::Math::WrappingAdd(pool.used, 1);
  pool.free = records.Next(*particle);
  records.SetNext(*particle, list.first);
  if (list.first != nullptr) {
    records.SetPrevious(*list.first, particle);
  }
  list.first = particle;
  ++list.count;
  return particle;
}

void ReleaseEffectParticle(Effects_Particle& particle, ParticlePool& pool,
                           ParticleList& list, ParticleRecords& records) {
  Effects_Particle* next = records.Next(particle);
  Effects_Particle* previous = records.Previous(particle);
  if (next != nullptr) {
    records.SetPrevious(*next, previous);
  }
  if (previous != nullptr) {
    records.SetNext(*previous, next);
  } else {
    list.first = next;
  }
  records.SetNext(particle, pool.free);
  pool.free = &particle;
  pool.used = Core::Math::WrappingSubtract(pool.used, 1);
  --list.count;
}

void ReleaseAllEffectParticles(ParticlePool& pool, ParticleList& list,
                               ParticleRecords& records) {
  if (list.first == nullptr) {
    return;
  }
  Effects_Particle* last = list.first;
  for (Effects_Particle* next = records.Next(*last); next != nullptr;
       next = records.Next(*last)) {
    last = next;
  }
  records.SetNext(*last, pool.free);
  pool.free = list.first;
  pool.used = Core::Math::WrappingSubtract(pool.used, list.count);
  list.count = 0;
}

void SetParticleGravity(ParticleForces& forces, Core::Math::Vec3 gravity) {
  forces.gravity = gravity;
}

void SetParticleDrag(ParticleForces& forces, std::int32_t drag) {
  forces.drag = drag;
}
}  // namespace VS::Effects
