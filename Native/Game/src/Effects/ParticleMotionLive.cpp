// Hand-maintained (tools/port/owned.csv).
#include "Effects/ParticleMotionLive.hpp"

#include <bit>
#include <cstdint>
#include <string_view>

#include "Core/Log.hpp"
#include "Core/Math/ScalarMath.hpp"
#include "Effects/ParticleMotion.hpp"
#include "Effects/ParticlePoolLive.hpp"
#include "Effects/ParticleRenderLive.hpp"
#include "Legacy/Psx.h"

extern "C" {
#include "Core/Gte.h"
#include "Effects/Particles.h"
#include "Effects/Runtime.h"
}

namespace VS::Effects {
namespace {
using Core::Math::Vec3;
using Particle = Effects_Particle;
using Instance = Effects_EffectInstance;

constexpr std::uint32_t kHalfwordBits = 16;
constexpr std::uint32_t kHalfwordMask = 0xFFFF;

std::uint64_t refused_calls = 0;

void Refuse(std::string_view call, std::string_view reason) {
  if (std::has_single_bit(++refused_calls)) {
    VS_LOG_WARN("effects", "particle motion: {} refused: {} ({} refusals)",
                call, reason, refused_calls);
  }
}

[[nodiscard]] std::uint32_t PackPoint(std::int16_t x, std::int16_t y) {
  return (static_cast<std::uint32_t>(static_cast<std::uint16_t>(x))) |
         (static_cast<std::uint32_t>(static_cast<std::uint16_t>(y))
          << kHalfwordBits);
}

template <typename Vector>
[[nodiscard]] Vec3 Wide(const Vector& vector) {
  return {.x = vector.vx, .y = vector.vy, .z = vector.vz};
}

template <typename Vector>
void StoreWide(Vector& vector, Vec3 value) {
  vector.vx = value.x;
  vector.vy = value.y;
  vector.vz = value.z;
}

// The GTE rotation the emitter loaded (func_80041508, ApplyRotMatrix).
class LiveRotation final : public ParticleRotation {
 public:
  Vec3 Rotate(ParticleOffset offset) override {
    SVECTOR input{offset.x, offset.y, offset.z, 0};
    VECTOR output{};
    Core_Gte_ApplyRotMatrix(&input, &output);
    return Wide(output);
  }
};

class LiveMotionRecords final : public ParticleMotionRecords {
 public:
  Particle* Next(Particle& particle) override { return particle.next; }
  ParticleMotionState Motion(Particle& particle) override {
    const Effects_ParticleMotion& motion = particle.motion;
    return {.mass = motion.mass,
            .gravity_scale = motion.gravityScale,
            .position = Wide(motion.position),
            .velocity = Wide(motion.velocity),
            .force = Wide(motion.force),
            .acceleration = Wide(motion.acceleration),
            .target = {.x = particle.attractionTarget[0],
                       .y = particle.attractionTarget[1],
                       .z = particle.attractionTarget[2]},
            .attraction = particle.attraction};
  }
  void StoreMotion(Particle& particle,
                   const ParticleMotionState& state) override {
    // The step changes only the position, velocity and force.
    StoreWide(particle.motion.position, state.position);
    StoreWide(particle.motion.velocity, state.velocity);
    StoreWide(particle.motion.force, state.force);
  }
};

class LiveTickHost final : public ParticleTickHost {
 public:
  explicit LiveTickHost(Instance& instance) : instance_(&instance) {}

  void Integrate(Particle* first) override {
    LiveIntegrateParticleMotion(first);
  }
  void Draw() override { DrawLiveEffectParticleList(instance_->particles); }
  Particle* Next(Particle& particle) override { return particle.next; }
  ParticleLifecycle Lifecycle(Particle& particle) override {
    return {.age = particle.age,
            .lifetime = particle.lifetime,
            .end_event = particle.endEvent,
            .tick_event = particle.tickEvent,
            .texture_timer = particle.textureAnimation.frameTimer};
  }
  void StoreAge(Particle& particle, std::uint8_t age) override {
    particle.age = age;
  }
  void StoreLifetime(Particle& particle, std::int16_t lifetime) override {
    particle.lifetime = lifetime;
  }
  void Emit(Particle& particle, std::int32_t event) override {
    // The original passed the particle itself; func_800D2ADC reads only the
    // position, transparency and variant of its parent.
    Effects_PluginEffectState parent{};
    StoreWide(parent.position, Wide(particle.motion.position));
    parent.transparency = particle.transparency;
    parent.variant = particle.opaque61Bit0;
    Effects_EmitParticleEvent(instance_, event, 0, 0, &parent);
  }
  void Release(Particle& particle) override {
    LiveReleaseEffectParticle(&particle, instance_);
  }

 private:
  Instance* instance_;
};
}  // namespace

// decomp: BATTLE.PRG func_800CFAAC (src/BATTLE/BATTLE.PRG/5BF94.c:5770)
void LiveStepParticleEndpointTowardStart(
    Effects_ParticleEndpointView* endpoints) {
  if (endpoints == nullptr) {
    Refuse("step endpoint", "no line");
    return;
  }
  const std::uint32_t stepped =
      StepScreenPointToward(PackPoint(endpoints->startX, endpoints->startY),
                            PackPoint(endpoints->endX, endpoints->endY));
  endpoints->startX = static_cast<std::int16_t>(stepped & kHalfwordMask);
  endpoints->startY = static_cast<std::int16_t>(stepped >> kHalfwordBits);
}

// decomp: BATTLE.PRG func_800D2A38 (src/BATTLE/BATTLE.PRG/5BF94.c:6915)
void LiveSetParticleSpawnPosition(Effects_ParticleEmitterScratch* scratch,
                                  Particle* particle) {
  if (scratch == nullptr || particle == nullptr) {
    Refuse("spawn position", "no emitter scratch or particle");
    return;
  }
  LiveRotation rotation;
  const ParticleSpawn spawn =
      PlaceParticleSpawn(scratch->flags,
                         {.x = scratch->vector.vx,
                          .y = scratch->vector.vy,
                          .z = scratch->vector.vz},
                         Wide(scratch->position), rotation);
  StoreWide(scratch->aimPoint, spawn.aim);
  StoreWide(particle->motion.position, spawn.position);
}

// decomp: BATTLE.PRG func_800D4C18 (src/BATTLE/BATTLE.PRG/5BF94.c:7658)
int LiveTickEffectParticles(Instance* instance) {
  if (instance == nullptr) {
    Refuse("tick", "no instance");
    return 1;
  }
  LiveTickHost host(*instance);
  TickEffectParticles(instance->particles, host);
  return 1;
}

// decomp: BATTLE.PRG func_800D52A4 (src/BATTLE/BATTLE.PRG/5BF94.c:7830)
void LiveIntegrateParticleMotion(Particle* first) {
  LiveMotionRecords records;
  IntegrateParticleMotion(first, records, LoadLiveParticleForces());
}
}  // namespace VS::Effects
