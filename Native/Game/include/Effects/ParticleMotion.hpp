#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstdint>

#include "Core/Math/Distance.hpp"
#include "Core/Math/ScalarMath.hpp"
#include "Effects/ParticlePool.hpp"

// How an effect's particles move and age (BATTLE.PRG 5BF94.c:5770, 6915,
// 7658 and 7830): a line ribbon's end creeps toward its start, an emitted
// particle is placed from the emitter's offset, and each tick integrates the
// particles' motion and ages them, emitting their tick and end events and
// releasing the expired. The records stay in the game heap:
// ParticleMotionLive.hpp reaches them through the hosts below.
namespace VS::Effects {
struct LiveParticle;

// func_800CFAAC (5BF94.c:5770): packed screen point `point` (x in the low
// halfword, y in the high) moved one pixel toward `target` along the major
// axis, and diagonally when the minor axis is more than half the major. The
// coordinates wrap as halfwords; an equal point stays.
[[nodiscard]] std::uint32_t StepScreenPointToward(
    std::uint32_t point, std::uint32_t target) noexcept;

// A halfword offset, as the emitter scratch's SVECTOR holds it.
struct ParticleOffset {
  std::int16_t x{};
  std::int16_t y{};
  std::int16_t z{};

  friend bool operator==(const ParticleOffset&,
                         const ParticleOffset&) = default;
};

// Emitter scratch flag 18: turn spawn offsets by the GTE rotation.
inline constexpr std::uint32_t kParticleRotateSpawnOffset = 0x40000;

// The GTE rotation the emitter loaded.
class ParticleRotation {
 public:
  virtual ~ParticleRotation() = default;
  [[nodiscard]] virtual Core::Math::Vec3 Rotate(ParticleOffset offset) = 0;

 protected:
  ParticleRotation() = default;
  ParticleRotation(const ParticleRotation&) = default;
  ParticleRotation& operator=(const ParticleRotation&) = default;
  ParticleRotation(ParticleRotation&&) = default;
  ParticleRotation& operator=(ParticleRotation&&) = default;
};

// Where func_800D2A38 places a particle: the aim point it leaves in the
// emitter scratch and the particle's Q12 position.
struct ParticleSpawn {
  Core::Math::Vec3 aim{};
  Core::Math::Vec3 position{};

  friend bool operator==(const ParticleSpawn&, const ParticleSpawn&) = default;
};

// func_800D2A38 (5BF94.c:6915): the emitter `origin` plus the `offset`,
// rotated when `flags` holds kParticleRotateSpawnOffset, in Q12 units.
[[nodiscard]] ParticleSpawn PlaceParticleSpawn(std::uint32_t flags,
                                               ParticleOffset offset,
                                               Core::Math::Vec3 origin,
                                               ParticleRotation& rotation);

// A particle's motion record (LiveParticleMotion) and its attraction.
struct ParticleMotionState {
  std::int16_t mass{};
  std::int16_t gravity_scale{};
  Core::Math::Vec3 position{};
  Core::Math::Vec3 velocity{};
  Core::Math::Vec3 force{};
  Core::Math::Vec3 acceleration{};
  Core::Math::Position3 target{};
  std::int32_t attraction{};

  friend bool operator==(const ParticleMotionState&,
                         const ParticleMotionState&) = default;
};

// One particle's step of func_800D52A4 (5BF94.c:7830): per axis, the
// velocity is the dragged velocity plus the force over the mass, plus the
// scaled gravity, and the position advances by it. The force then gains the
// acceleration and, with an attraction, the Q12 unit direction to the target
// scaled by it; the force is never cleared. A zero mass divides as the R3000
// does and every word wraps.
void StepParticleMotion(ParticleMotionState& state,
                        const ParticleForces& forces);

// The particle records' list links and motion.
class ParticleMotionRecords {
 public:
  virtual ~ParticleMotionRecords() = default;
  [[nodiscard]] virtual LiveParticle* Next(LiveParticle& particle) = 0;
  [[nodiscard]] virtual ParticleMotionState Motion(LiveParticle& particle) = 0;
  virtual void StoreMotion(LiveParticle& particle,
                           const ParticleMotionState& state) = 0;

 protected:
  ParticleMotionRecords() = default;
  ParticleMotionRecords(const ParticleMotionRecords&) = default;
  ParticleMotionRecords& operator=(const ParticleMotionRecords&) = default;
  ParticleMotionRecords(ParticleMotionRecords&&) = default;
  ParticleMotionRecords& operator=(ParticleMotionRecords&&) = default;
};

// func_800D52A4 (5BF94.c:7830): steps each particle of the list from `first`.
void IntegrateParticleMotion(LiveParticle* first,
                             ParticleMotionRecords& records,
                             const ParticleForces& forces);

// The fields of a particle a tick ages it by.
struct ParticleLifecycle {
  std::uint8_t age{};
  // Ticks left; -1 lives until the texture animation stops.
  std::int16_t lifetime{};
  // Events, numbered from 1; 0 emits nothing.
  std::uint8_t end_event{};
  std::uint8_t tick_event{};
  // The texture animation's frame timer.
  std::uint8_t texture_timer{};

  friend bool operator==(const ParticleLifecycle&,
                         const ParticleLifecycle&) = default;
};

// The effect instance's particles and the services a tick reaches.
class ParticleTickHost {
 public:
  virtual ~ParticleTickHost() = default;
  // func_800D52A4 on the list from `first`.
  virtual void Integrate(LiveParticle* first) = 0;
  // Draws the instance's particles (func_800D6E24).
  virtual void Draw() = 0;
  [[nodiscard]] virtual LiveParticle* Next(LiveParticle& particle) = 0;
  [[nodiscard]] virtual ParticleLifecycle Lifecycle(LiveParticle& particle) = 0;
  virtual void StoreAge(LiveParticle& particle, std::uint8_t age) = 0;
  virtual void StoreLifetime(LiveParticle& particle, std::int16_t lifetime) = 0;
  // Emits package event `event` with the particle as the parent
  // (func_800D2ADC).
  virtual void Emit(LiveParticle& particle, std::int32_t event) = 0;
  // Returns the particle to the pool (func_800D2888).
  virtual void Release(LiveParticle& particle) = 0;

 protected:
  ParticleTickHost() = default;
  ParticleTickHost(const ParticleTickHost&) = default;
  ParticleTickHost& operator=(const ParticleTickHost&) = default;
  ParticleTickHost(ParticleTickHost&&) = default;
  ParticleTickHost& operator=(ParticleTickHost&&) = default;
};

// func_800D4C18 (5BF94.c:7658): integrates and draws the list from `first`,
// then ages each particle and emits its tick event. A particle whose lifetime
// runs out, or an unlimited one whose texture animation holds no frame (a
// stopped program), emits its end event and is released. A lifetime of 0 counts
// down to -1 and so becomes unlimited, as in the original.
void TickEffectParticles(LiveParticle* first, ParticleTickHost& host);
}  // namespace VS::Effects
