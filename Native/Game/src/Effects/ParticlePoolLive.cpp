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
#include "Effects/Particles.hpp"
#include "Effects/Runtime.hpp"

using VS::Core::Gte::VECTOR;

namespace VS::Effects {
// decomp: D_800F55F0 (src/BATTLE/BATTLE.PRG/5BF94.c:446)
std::int32_t particleCapacity = 0;
// decomp: D_800F55F4 (src/BATTLE/BATTLE.PRG/5BF94.c:447)
LiveParticle* freeParticles = nullptr;
// decomp: D_800F55F8 (src/BATTLE/BATTLE.PRG/5BF94.c:448)
std::int32_t usedParticleCount = 0;
// decomp: D_800F55FC (src/BATTLE/BATTLE.PRG/5BF94.c:453)
LiveParticle* particlePool = nullptr;
// decomp: D_800F5600 (src/BATTLE/BATTLE.PRG/5BF94.c:449)
std::int32_t particlePoolReferences = 0;
// decomp: D_800F5618 (src/BATTLE/BATTLE.PRG/5BF94.c:451)
std::int32_t particleDrag = 0;
// decomp: D_800F5620 (src/BATTLE/BATTLE.PRG/5BF94.c:452)
VECTOR particleGravity{};

namespace {
using Particle = LiveParticle;
using Instance = LiveEffectInstance;

constexpr std::uint8_t kUnagedParticle = 0xFF;

std::uint64_t refusedCalls = 0;

void Refuse(std::string_view call, std::string_view reason) {
  if (std::has_single_bit(++refusedCalls)) {
    VS::Core::LogWarn("effects", "particle pool: {} refused: {} ({} refusals)",
                      call, reason, refusedCalls);
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
    particle.texture_animation = {};
    particle.age = kUnagedParticle;
  }
};

// The pool globals, loaded for one call and stored back.
class LivePool {
 public:
  LivePool()
      : pool_{.block = particlePool,
              .free = freeParticles,
              .capacity = particleCapacity,
              .used = usedParticleCount,
              .references = particlePoolReferences} {}
  LivePool(const LivePool&) = delete;
  LivePool& operator=(const LivePool&) = delete;
  LivePool(LivePool&&) = delete;
  LivePool& operator=(LivePool&&) = delete;
  ~LivePool() {
    particlePool = pool_.block;
    freeParticles = pool_.free;
    particleCapacity = pool_.capacity;
    usedParticleCount = pool_.used;
    particlePoolReferences = pool_.references;
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
              .count = std::bit_cast<std::uint8_t>(instance.particle_count)} {}
  LiveList(const LiveList&) = delete;
  LiveList& operator=(const LiveList&) = delete;
  LiveList(LiveList&&) = delete;
  LiveList& operator=(LiveList&&) = delete;
  ~LiveList() {
    instance_->particles = list_.first;
    instance_->particle_count = std::bit_cast<char>(list_.count);
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
    Core::Math::StoreVector(particleGravity, forces_.gravity);
    particleDrag = forces_.drag;
  }

  [[nodiscard]] ParticleForces& Forces() { return forces_; }

 private:
  ParticleForces forces_;
};
}  // namespace

// decomp: BATTLE.PRG func_800D268C (src/BATTLE/BATTLE.PRG/5BF94.c:6345)
void LiveResetParticlePoolReferences() {
  LivePool pool;
  ResetParticlePoolReferences(pool.Pool());
}

// decomp: BATTLE.PRG func_800D2698 (src/BATTLE/BATTLE.PRG/5BF94.c:6347)
void LiveAcquireParticlePool(int count) {
  LiveParticleRecords records;
  LivePool pool;
  if (!AcquireParticlePool(pool.Pool(), count, records)) {
    Refuse("acquire", count < 1 ? "count below 1" : "no heap room");
  }
}

// decomp: BATTLE.PRG func_800D278C (src/BATTLE/BATTLE.PRG/5BF94.c:6373)
void LiveReleaseParticlePool() {
  LiveParticleRecords records;
  LivePool pool;
  if (!ReleaseParticlePool(pool.Pool(), records)) {
    Refuse("release", "nothing references the pool");
  }
}

// decomp: BATTLE.PRG func_800D27F0 (src/BATTLE/BATTLE.PRG/5BF94.c:6386)
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

// decomp: BATTLE.PRG func_800D2888 (src/BATTLE/BATTLE.PRG/5BF94.c:6415)
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

// decomp: BATTLE.PRG func_800D2904 (src/BATTLE/BATTLE.PRG/5BF94.c:6433)
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

// decomp: BATTLE.PRG func_800D5260 (src/BATTLE/BATTLE.PRG/5BF94.c:6818)
void LiveSetParticleGravity(VECTOR* gravity) {
  if (gravity == nullptr) {
    Refuse("gravity", "no vector");
    return;
  }
  {
    LiveForces forces;
    SetParticleGravity(forces.Forces(), Core::Math::VectorOf(*gravity));
  }
  particleGravity.pad = gravity->pad;
}

// decomp: BATTLE.PRG func_800D5294 (src/BATTLE/BATTLE.PRG/5BF94.c:6820)
void LiveSetParticleDrag(int* drag) {
  if (drag == nullptr) {
    Refuse("drag", "no value");
    return;
  }
  LiveForces forces;
  SetParticleDrag(forces.Forces(), *drag);
}

ParticleForces LoadLiveParticleForces() {
  return {.gravity = Core::Math::VectorOf(particleGravity),
          .drag = particleDrag};
}
}  // namespace VS::Effects
