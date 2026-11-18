// Hand-maintained (tools/port/owned.csv).
#include "Effects/ParticlePoolLive.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string_view>

#include "Core/Log.hpp"
#include "Core/Math/ScalarMath.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Effects/EffectScript.hpp"
#include "Effects/EffectScriptLive.hpp"
#include "Effects/ParticlePool.hpp"
#include "Effects/Particles.h"
#include "Effects/Runtime.h"

namespace VS::Effects {
namespace {
using Particle = Effects_Particle;
using Instance = Effects_EffectInstance;

constexpr std::uint8_t kUnagedParticle = 0xFF;

std::uint64_t refused_calls = 0;

void Refuse(std::string_view call, std::string_view reason) {
  if (std::has_single_bit(++refused_calls)) {
    VS_LOG_WARN("effects", "particle pool: {} refused: {} ({} refusals)", call,
                reason, refused_calls);
  }
}

// The records of the game heap.
class LiveParticleRecords final : public ParticleRecords {
 public:
  Particle* Reserve(std::int32_t count) override {
    void* storage = Core::Memory::AllocateLiveFromHighEnd(
        sizeof(Particle) * static_cast<std::size_t>(count));
    if (storage == nullptr) {
      return nullptr;
    }
    auto* block = static_cast<Particle*>(storage);
    std::uninitialized_default_construct_n(block, count);
    return block;
  }
  void Free(Particle& block) override {
    Core::Memory::FreeLiveFromHighEnd(&block);
  }
  Particle& At(Particle& block, std::int32_t index) override {
    return std::span(&block, static_cast<std::size_t>(index) + 1).back();
  }
  Particle* Previous(Particle& particle) override { return particle.previous; }
  void SetPrevious(Particle& particle, Particle* previous) override {
    particle.previous = previous;
  }
  Particle* Next(Particle& particle) override { return particle.next; }
  void SetNext(Particle& particle, Particle* next) override {
    particle.next = next;
  }
  void Renew(Particle& particle) override {
    // Clears the texture animation, as func_800D6CCC
    // (LiveClearParticleTextureState) does.
    particle.textureAnimation = {};
    particle.age = kUnagedParticle;
  }
};

// The pool globals, loaded for one call and stored back.
class LivePool {
 public:
  LivePool()
      : pool_{.block = Effects_particlePool,
              .free = Effects_freeParticles,
              .capacity = Effects_particleCapacity,
              .used = Effects_usedParticleCount,
              .references = Effects_particlePoolReferences} {}
  LivePool(const LivePool&) = delete;
  LivePool& operator=(const LivePool&) = delete;
  LivePool(LivePool&&) = delete;
  LivePool& operator=(LivePool&&) = delete;
  ~LivePool() {
    Effects_particlePool = pool_.block;
    Effects_freeParticles = pool_.free;
    Effects_particleCapacity = pool_.capacity;
    Effects_usedParticleCount = pool_.used;
    Effects_particlePoolReferences = pool_.references;
  }

  [[nodiscard]] ParticlePool& Pool() { return pool_; }

 private:
  ParticlePool pool_;
};

// An instance's particles, loaded for one call and stored back.
class LiveList {
 public:
  explicit LiveList(Instance& instance)
      : instance_(&instance),
        list_{.first = instance.particles,
              .count = std::bit_cast<std::uint8_t>(instance.particleCount)} {}
  LiveList(const LiveList&) = delete;
  LiveList& operator=(const LiveList&) = delete;
  LiveList(LiveList&&) = delete;
  LiveList& operator=(LiveList&&) = delete;
  ~LiveList() {
    instance_->particles = list_.first;
    instance_->particleCount = std::bit_cast<char>(list_.count);
  }

  [[nodiscard]] ParticleList& List() { return list_; }

 private:
  Instance* instance_;
  ParticleList list_;
};

// The forces, loaded for one call and stored back.
class LiveForces {
 public:
  LiveForces() : forces_(LoadLiveParticleForces()) {}
  LiveForces(const LiveForces&) = delete;
  LiveForces& operator=(const LiveForces&) = delete;
  LiveForces(LiveForces&&) = delete;
  LiveForces& operator=(LiveForces&&) = delete;
  ~LiveForces() {
    Effects_particleGravity.x = forces_.gravity.x;
    Effects_particleGravity.y = forces_.gravity.y;
    Effects_particleGravity.z = forces_.gravity.z;
    Effects_particleDrag = forces_.drag;
  }

  [[nodiscard]] ParticleForces& Forces() { return forces_; }

 private:
  ParticleForces forces_;
};
}  // namespace

// decomp: BATTLE.PRG func_800D268C (src/BATTLE/BATTLE.PRG/5BF94.c:6777)
void LiveResetParticlePoolReferences() {
  LivePool pool;
  ResetParticlePoolReferences(pool.Pool());
}

// decomp: BATTLE.PRG func_800D2698 (src/BATTLE/BATTLE.PRG/5BF94.c:6779)
void LiveAcquireParticlePool(int count) {
  LiveParticleRecords records;
  LivePool pool;
  if (!AcquireParticlePool(pool.Pool(), count, records)) {
    Refuse("acquire", count < 1 ? "count below 1" : "no heap room");
  }
}

// decomp: BATTLE.PRG func_800D278C (src/BATTLE/BATTLE.PRG/5BF94.c:6805)
void LiveReleaseParticlePool() {
  LiveParticleRecords records;
  LivePool pool;
  if (!ReleaseParticlePool(pool.Pool(), records)) {
    Refuse("release", "nothing references the pool");
  }
}

// decomp: BATTLE.PRG func_800D27F0 (src/BATTLE/BATTLE.PRG/5BF94.c:6818)
Particle* LiveAllocateEffectParticle(Instance* instance) {
  if (instance == nullptr) {
    Refuse("allocate", "no instance");
    return nullptr;
  }
  LiveParticleRecords records;
  LivePool pool;
  LiveList list(*instance);
  Particle* particle =
      AllocateEffectParticle(pool.Pool(), list.List(), records);
  if (particle == nullptr) {
    LiveReportEffectDiagnostic(
        static_cast<int>(EffectDiagnostic::kParticlePoolExhausted));
  }
  return particle;
}

// decomp: BATTLE.PRG func_800D2888 (src/BATTLE/BATTLE.PRG/5BF94.c:6847)
void LiveReleaseEffectParticle(Particle* particle, Instance* instance) {
  if (particle == nullptr || instance == nullptr) {
    Refuse("release particle", "no particle or instance");
    return;
  }
  LiveParticleRecords records;
  LivePool pool;
  LiveList list(*instance);
  ReleaseEffectParticle(*particle, pool.Pool(), list.List(), records);
}

// decomp: BATTLE.PRG func_800D2904 (src/BATTLE/BATTLE.PRG/5BF94.c:6865)
void LiveReleaseAllEffectParticles(Instance* instance) {
  if (instance == nullptr) {
    Refuse("release all", "no instance");
    return;
  }
  LiveParticleRecords records;
  LivePool pool;
  LiveList list(*instance);
  ReleaseAllEffectParticles(pool.Pool(), list.List(), records);
}

// decomp: BATTLE.PRG func_800D5260 (src/BATTLE/BATTLE.PRG/5BF94.c:7826)
void LiveSetParticleGravity(Effects_ParticleGravity* gravity) {
  if (gravity == nullptr) {
    Refuse("gravity", "no vector");
    return;
  }
  {
    LiveForces forces;
    SetParticleGravity(forces.Forces(),
                       {.x = gravity->x, .y = gravity->y, .z = gravity->z});
  }
  Effects_particleGravity.reservedC = gravity->reservedC;
}

// decomp: BATTLE.PRG func_800D5294 (src/BATTLE/BATTLE.PRG/5BF94.c:7828)
void LiveSetParticleDrag(int* drag) {
  if (drag == nullptr) {
    Refuse("drag", "no value");
    return;
  }
  LiveForces forces;
  SetParticleDrag(forces.Forces(), *drag);
}

ParticleForces LoadLiveParticleForces() {
  return {.gravity = {.x = Effects_particleGravity.x,
                      .y = Effects_particleGravity.y,
                      .z = Effects_particleGravity.z},
          .drag = Effects_particleDrag};
}
}  // namespace VS::Effects
