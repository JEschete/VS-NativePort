#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "Core/Math/Distance.hpp"
#include "Core/Math/Interpolation.hpp"
#include "Core/Math/Matrix.hpp"
#include "Core/Random.hpp"
#include "Effects/ParticleMotion.hpp"

namespace VS::Effects {
inline constexpr std::size_t kEmissionCurveCount = 16;
inline constexpr std::size_t kEmissionVectorRanges = 8;
inline constexpr std::size_t kEmissionPairRanges = 5;
inline constexpr std::size_t kEmissionColorChannels = 3;
inline constexpr std::size_t kEmissionAxes = Core::Math::kPositionAxes;

enum class EmissionCurve : std::size_t {
  kPosition,
  kExtent,
  kDirection,
  kDirectionJitter,
  kSpeed,
  kForce,
  kAcceleration,
  kAttractionOffset,
  kAttractionJitter,
  kAttraction,
  kMass,
  kGravityScale,
  kLifetime,
  kCount,
  kInterval,
  kInnerExtent,
};

enum class EmissionVector : std::size_t {
  kDirection,
  kDirectionJitter,
  kForceMin,
  kForceMax,
  kAccelerationMin,
  kAccelerationMax,
  kAttractionOffset,
  kAttractionJitter,
};

enum class EmissionPair : std::size_t {
  kMass,
  kGravityScale,
  kSpeed,
  kAttraction,
  kLifetime,
};

using EmissionVectorRange = std::array<Core::Math::ScalarSpan, kEmissionAxes>;
using EmissionPairRange = std::array<Core::Math::ScalarSpan, 2>;

struct ParticleEmissionDefinition {
  std::uint32_t flags{};
  std::uint8_t transparency{};
  std::uint8_t texture{};
  std::uint8_t animation{};
  std::uint8_t end_event{};
  std::uint8_t tick_event{};
  std::array<std::uint8_t, kEmissionCurveCount> curves{};
  EmissionVectorRange position{};
  EmissionVectorRange extent{};
  Core::Math::ScalarSpan inner_extent{};
  std::array<EmissionVectorRange, kEmissionVectorRanges> vectors{};
  std::array<EmissionPairRange, kEmissionPairRanges> pairs{};
  Core::Math::ScalarSpan count{};
  Core::Math::ScalarSpan interval{};
  std::array<std::uint8_t, kEmissionColorChannels> color_curves{};
  std::uint8_t transform{};
  std::uint8_t attraction_transform{};
  std::uint8_t ring_segments{};
  std::uint8_t ring_positions{};
};

struct ParticleEmissionParent {
  Core::Math::Vec3 position{};
  std::uint8_t transparency{};
  std::uint8_t variant{};
};

struct ParticleEmissionContext {
  std::uint16_t frame{};
  std::int32_t capacity{};
  std::int32_t used{};
  std::int32_t transparency_override{};
  std::int32_t variant{};
  Core::Math::Vec3 effect_origin{};
  Core::Math::Transform3 transform{};
  Core::Math::Vec3 attraction_translation{};
  // Required only by direction modes 0x4000 and 0x6000, resolved from the
  // instance's source/destination position or checked actor-bone query.
  std::optional<Core::Math::Vec3> aim_target{};
  std::optional<ParticleEmissionParent> parent{};
  // Slot zero and the current package's bound samples, not stale capacity.
  std::span<const std::int32_t> samples{};
};

// Named persistent emitter values. Unselected shape/direction/target modes
// keep their previous values, as the original switches do.
struct ParticleEmissionScratch {
  std::uint32_t flags{};
  std::uint16_t saved_flags{};
  ParticleOffset offset{};
  Core::Math::Vec3 aim{};
  Core::Math::Vec3 spawn{};
  Core::Math::Mat3 direction_matrix{};
  Core::Math::Mat3 aim_matrix{};
  Core::Math::Vec3 position{};
  Core::Math::Vec3 extent{};
  Core::Math::Vec3 direction{};
  Core::Math::Vec3 direction_jitter{};
  Core::Math::Vec3 inner_extent{};
  Core::Math::Vec3 base_direction{};
  Core::Math::Vec3 force_min{};
  Core::Math::Vec3 force_max{};
  Core::Math::Vec3 acceleration_min{};
  Core::Math::Vec3 acceleration_max{};
  Core::Math::Vec2 speed_range{};
  std::int32_t speed{};
  Core::Math::Vec2 mass_range{};
  Core::Math::Vec2 gravity_range{};
  Core::Math::Vec2 lifetime_range{};
  Core::Math::Vec3 attraction_offset{};
  Core::Math::Vec3 attraction_jitter{};
  Core::Math::Vec2 attraction_range{};
  std::array<std::uint8_t, kEmissionColorChannels> color_curves{};
  std::uint8_t age{};
  std::uint8_t transform{};
  std::uint8_t attraction_transform{};
  std::int32_t sin_angle0{};
  std::int32_t sin_angle1{};
  std::int32_t cos_angle0{};
  std::int32_t cos_angle1{};
  std::uint8_t transparency{};
  std::int32_t ring_segments{};
  std::int32_t ring_positions{};
};

struct ParticleEmissionHandle {
  std::size_t index{};
  friend bool operator==(const ParticleEmissionHandle&,
                         const ParticleEmissionHandle&) = default;
};

struct EmittedParticle {
  std::optional<ParticleEmissionHandle> next{};
  ParticleMotionState motion{};
  std::uint32_t flags{};
  std::uint8_t transparency{};
  std::uint8_t variant{};
  std::int16_t lifetime{};
  std::uint8_t transform{};
  std::uint8_t attraction_transform{};
  std::uint8_t end_event{};
  std::uint8_t tick_event{};
  std::array<std::uint8_t, kEmissionColorChannels> color_curves{};
  std::uint8_t age{};
};

class ParticleEmissionHost {
 public:
  ParticleEmissionHost() = default;
  ParticleEmissionHost(const ParticleEmissionHost&) = delete;
  ParticleEmissionHost(ParticleEmissionHost&&) = delete;
  ParticleEmissionHost& operator=(const ParticleEmissionHost&) = delete;
  ParticleEmissionHost& operator=(ParticleEmissionHost&&) = delete;
  virtual ~ParticleEmissionHost() = default;

  // The pool renews and prepends each allocation. Read/Store address checked
  // handles; Store changes values, never list links.
  [[nodiscard]] virtual std::optional<ParticleEmissionHandle> Allocate() = 0;
  [[nodiscard]] virtual std::optional<EmittedParticle> Read(
      ParticleEmissionHandle particle) const = 0;
  virtual void Store(ParticleEmissionHandle particle,
                     const EmittedParticle& values) = 0;
  virtual void InitializeTexture(ParticleEmissionHandle particle,
                                 std::uint8_t texture,
                                 std::uint8_t animation) = 0;
  virtual void Release(ParticleEmissionHandle particle) = 0;
};

enum class ParticleEmissionStatus : std::uint8_t {
  kEmitted,
  kIntervalSkipped,
  kEmpty,
  kMissingSamples,
  kInvalidPool,
  kMissingAim,
  kAllocationRefused,
  kInvalidLinks,
};

struct ParticleEmissionResult {
  ParticleEmissionStatus status{};
  std::int32_t count{};
  bool clear_translation{};
  // The last rotation loaded, when the emission reached its spawn phase.
  std::optional<Core::Math::Mat3> rotation{};
};

// decomp: BATTLE.PRG func_800D2ADC (5BF94.c:6935-7502).
// Allocation order is oldest first; every subsequent pass visits newest
// first. RNG order, low-word products, halfword stores and unhandled switch
// modes follow the original. Empty/full pools retain the #73 no-emission
// contract. Failed allocations return only this call's new particles.
// Polygon filling keeps the native predecessor's SquareRootFixed contract;
// negative root inputs remain a #271 review boundary, not original parity.
[[nodiscard]] ParticleEmissionResult EmitParticleEvent(
    const ParticleEmissionDefinition& definition,
    const ParticleEmissionContext& context, ParticleEmissionScratch& scratch,
    std::int32_t& emission_counter, Core::Rand15& random,
    ParticleEmissionHost& host);
}  // namespace VS::Effects
