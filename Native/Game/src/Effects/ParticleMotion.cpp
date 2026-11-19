// Hand-maintained (tools/port/owned.csv).
#include "Effects/ParticleMotion.hpp"

#include <cstdint>
#include <cstdlib>

#include "Core/Math/ScalarMath.hpp"

namespace VS::Effects {
namespace {
using Core::Math::Vec3;
using Core::Math::WrappingAdd;
using Core::Math::WrappingMultiply;
using Core::Math::WrappingSubtract;
using Core::Math::WrapSigned16;

constexpr std::uint32_t kHalfwordBits = 16;
constexpr std::uint32_t kHalfwordMask = 0xFFFF;
constexpr std::int32_t kFixedShift = 12;
constexpr std::int32_t kFixedOne = 1 << kFixedShift;
constexpr std::int16_t kUnlimitedLifetime = -1;

[[nodiscard]] std::uint32_t StepHalfword(std::uint32_t value,
                                         std::int32_t step) noexcept {
  return (value + static_cast<std::uint32_t>(step)) & kHalfwordMask;
}

[[nodiscard]] Vec3 Fixed(Vec3 value) noexcept {
  return {.x = WrappingMultiply(value.x, kFixedOne),
          .y = WrappingMultiply(value.y, kFixedOne),
          .z = WrappingMultiply(value.z, kFixedOne)};
}

[[nodiscard]] Vec3 Add(Vec3 left, Vec3 right) noexcept {
  return {.x = WrappingAdd(left.x, right.x),
          .y = WrappingAdd(left.y, right.y),
          .z = WrappingAdd(left.z, right.z)};
}

// One axis of the velocity and position step.
void StepAxis(std::int32_t& position, std::int32_t& velocity,
              std::int32_t force, std::int32_t gravity,
              const ParticleMotionState& state, std::int32_t drag) {
  const std::int32_t momentum = WrappingAdd(
      WrappingMultiply(velocity, WrappingSubtract(state.mass, drag)),
      WrappingMultiply(force, kFixedOne));
  velocity = WrappingAdd(
      Core::Math::R3000Quotient(momentum, state.mass),
      WrappingMultiply(gravity, state.gravity_scale) >> kFixedShift);
  position = WrappingAdd(position, velocity);
}

// The Q12 unit direction from the position to the target, scaled by the
// attraction. A zero direction stays zero, as the original skipped
// normalizing it.
[[nodiscard]] Vec3 AttractionPull(const ParticleMotionState& state) {
  const Vec3 direction{.x = state.target.x - (state.position.x >> kFixedShift),
                       .y = state.target.y - (state.position.y >> kFixedShift),
                       .z = state.target.z - (state.position.z >> kFixedShift)};
  const Vec3 unit = Core::Math::NormalizeVector(direction).direction;
  return {.x = WrappingMultiply(unit.x, state.attraction) >> kFixedShift,
          .y = WrappingMultiply(unit.y, state.attraction) >> kFixedShift,
          .z = WrappingMultiply(unit.z, state.attraction) >> kFixedShift};
}

// Counts down a limited lifetime; true when the particle expired.
[[nodiscard]] bool Expired(LiveParticle& particle,
                           const ParticleLifecycle& state,
                           ParticleTickHost& host) {
  if (state.lifetime == kUnlimitedLifetime) {
    return state.texture_timer == 0;
  }
  const auto left = static_cast<std::int16_t>(state.lifetime - 1);
  host.StoreLifetime(particle, left);
  return left == 0;
}

void AgeParticle(LiveParticle& particle, ParticleTickHost& host) {
  const ParticleLifecycle ticked = host.Lifecycle(particle);
  host.StoreAge(particle, static_cast<std::uint8_t>(ticked.age + 1));
  if (ticked.tick_event != 0) {
    host.Emit(particle, ticked.tick_event - 1);
  }
  // The original read the rest of the record after the tick event's emit.
  const ParticleLifecycle state = host.Lifecycle(particle);
  if (!Expired(particle, state, host)) {
    return;
  }
  if (state.end_event != 0) {
    host.Emit(particle, state.end_event - 1);
  }
  host.Release(particle);
}
}  // namespace

// decomp: BATTLE.PRG func_800CFAAC (src/BATTLE/BATTLE.PRG/5BF94.c:5770)
std::uint32_t StepScreenPointToward(std::uint32_t point,
                                    std::uint32_t target) noexcept {
  if (point == target) {
    return point;
  }
  const std::int32_t dx = WrapSigned16(point) - WrapSigned16(target);
  const std::int32_t dy = WrapSigned16(point >> kHalfwordBits) -
                          WrapSigned16(target >> kHalfwordBits);
  const std::int32_t run = std::abs(dx);
  const std::int32_t rise = std::abs(dy);
  const bool x_major = rise < run;
  const bool move_x = x_major || rise < run * 2;
  const bool move_y = !x_major || run < rise * 2;
  std::uint32_t x = point & kHalfwordMask;
  std::uint32_t y = point >> kHalfwordBits;
  if (move_x) {
    x = StepHalfword(x, dx < 0 ? 1 : -1);
  }
  if (move_y) {
    y = StepHalfword(y, dy < 0 ? 1 : -1);
  }
  return x | (y << kHalfwordBits);
}

// decomp: BATTLE.PRG func_800D2A38 (src/BATTLE/BATTLE.PRG/5BF94.c:6915)
ParticleSpawn PlaceParticleSpawn(std::uint32_t flags, ParticleOffset offset,
                                 Vec3 origin, ParticleRotation& rotation) {
  const Vec3 aim = (flags & kParticleRotateSpawnOffset) != 0
                       ? rotation.Rotate(offset)
                       : Vec3{.x = offset.x, .y = offset.y, .z = offset.z};
  return {.aim = aim, .position = Add(origin, Fixed(aim))};
}

// decomp: BATTLE.PRG func_800D52A4 (src/BATTLE/BATTLE.PRG/5BF94.c:7830)
void StepParticleMotion(ParticleMotionState& state,
                        const ParticleForces& forces) {
  StepAxis(state.position.x, state.velocity.x, state.force.x, forces.gravity.x,
           state, forces.drag);
  StepAxis(state.position.y, state.velocity.y, state.force.y, forces.gravity.y,
           state, forces.drag);
  StepAxis(state.position.z, state.velocity.z, state.force.z, forces.gravity.z,
           state, forces.drag);
  const Vec3 pull = state.attraction != 0 ? AttractionPull(state) : Vec3{};
  state.force = Add(state.force, Add(state.acceleration, pull));
}

void IntegrateParticleMotion(LiveParticle* first,
                             ParticleMotionRecords& records,
                             const ParticleForces& forces) {
  for (LiveParticle* particle = first; particle != nullptr;
       particle = records.Next(*particle)) {
    ParticleMotionState state = records.Motion(*particle);
    StepParticleMotion(state, forces);
    records.StoreMotion(*particle, state);
  }
}

// decomp: BATTLE.PRG func_800D4C18 (src/BATTLE/BATTLE.PRG/5BF94.c:7658)
void TickEffectParticles(LiveParticle* first, ParticleTickHost& host) {
  host.Integrate(first);
  host.Draw();
  for (LiveParticle* particle = first; particle != nullptr;) {
    LiveParticle* const next = host.Next(*particle);
    AgeParticle(*particle, host);
    particle = next;
  }
}
}  // namespace VS::Effects
