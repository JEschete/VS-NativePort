#include "Effects/ParticleEmitterLive.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <span>
#include <string_view>
#include <utility>

#include "Actors/SkeletonLive.hpp"
#include "Core/Log.hpp"
#include "Core/Math/InterpolationLive.hpp"
#include "Core/Math/MatrixLive.hpp"
#include "Core/RandomLive.hpp"
#include "Effects/CurvesLive.hpp"
#include "Effects/PackageExtents.hpp"
#include "Effects/ParticleEmitter.hpp"
#include "Effects/ParticlePoolLive.hpp"
#include "Effects/ParticleTextureAnimationLive.hpp"
#include "Legacy/Psx.h"

extern "C" {
#include "Core/Gte.h"
#include "Core/Scratch.h"
#include "Effects/Package.h"
#include "Effects/Particles.h"
#include "Effects/Runtime.h"
#include "Effects/Transforms.h"
}

namespace VS::Effects {
namespace {
namespace math = Core::Math;
using math::Vec3;
using Particle = Effects_Particle;

constexpr std::uintptr_t kScratchOffset = 0x1D0;
constexpr std::uint32_t kDirectionMask = 0xE000;
constexpr std::uint32_t kTowardSource = 0x4000;
constexpr std::uint32_t kTowardDestination = 0x6000;
constexpr std::uint32_t kAttractionMask = 0x1C0;
constexpr std::uint32_t kActorAttraction = 0xC0;
constexpr std::uint32_t kByteMask = 0xFF;
constexpr std::uint16_t kActorTarget = 4;
std::uint64_t refused_emissions = 0;

void Refuse(std::string_view reason) {
  if (std::has_single_bit(++refused_emissions)) {
    VS_LOG_WARN("effects", "particle emission refused: {} ({} refusals)",
                reason, refused_emissions);
  }
}

[[nodiscard]] ParticleEmissionDefinition Definition(
    const Effects_ParticleDefinition& definition) {
  const auto vector_range = [](std::span<const std::int16_t> range) {
    return EmissionVectorRange{math::RangePair(range, 0),
                               math::RangePair(range, 1),
                               math::RangePair(range, 2)};
  };
  ParticleEmissionDefinition typed{
      .flags = definition.flags,
      .transparency = static_cast<std::uint8_t>(definition.transparencyCurve),
      .texture = definition.textureIndex,
      .animation = definition.animationId,
      .end_event = definition.endEvent,
      .tick_event = definition.tickEvent,
      .curves = {definition.positionCurve, definition.extentCurve,
                 definition.directionCurve, definition.directionJitterCurve,
                 definition.speedCurve, definition.forceCurve,
                 definition.accelerationCurve, definition.attractionOffsetCurve,
                 definition.attractionJitterCurve, definition.attractionCurve,
                 definition.massCurve, definition.gravityScaleCurve,
                 definition.lifetimeCurve, definition.countCurve,
                 definition.intervalCurve, definition.innerExtentCurve},
      .position = vector_range(definition.positionRange),
      .extent = vector_range(definition.extentRange),
      .inner_extent = math::RangePair(definition.innerExtentRange, 0),
      .count = math::RangePair(definition.countRange, 0),
      .interval = math::RangePair(definition.intervalRange, 0),
      .color_curves = {definition.rCurve, definition.gCurve, definition.bCurve},
      .transform = definition.transformIndex,
      .attraction_transform = definition.attractionTransform,
      .ring_segments = definition.ringSegments,
      .ring_positions = definition.ringPositions};
  for (std::size_t index = 0; index < typed.vectors.size(); ++index) {
    typed.vectors.at(index) = vector_range(definition.vectorRanges[index]);
  }
  for (std::size_t index = 0; index < typed.pairs.size(); ++index) {
    typed.pairs.at(index) = {math::RangePair(definition.pairRanges[index], 0),
                             math::RangePair(definition.pairRanges[index], 1)};
  }
  return typed;
}

[[nodiscard]] ParticleEmissionScratch LoadScratch(
    const Effects_ParticleEmitterScratch& live) {
  ParticleEmissionScratch values{
      .flags = live.flags,
      .saved_flags = live.savedFlags,
      .offset = {live.vector.vx, live.vector.vy, live.vector.vz},
      .aim = math::VectorOf(live.aimPoint),
      .spawn = math::VectorOf(live.spawnPoint),
      .aim_matrix = math::MatrixOf(live.aimMatrix),
      .position = math::VectorOf(live.position),
      .extent = math::VectorOf(live.extent),
      .direction = math::VectorOf(live.direction),
      .direction_jitter = math::VectorOf(live.directionJitter),
      .inner_extent = math::VectorOf(live.innerExtent),
      .base_direction = math::VectorOf(live.baseDirection),
      .force_min = math::VectorOf(live.forceMin),
      .force_max = math::VectorOf(live.forceMax),
      .acceleration_min = math::VectorOf(live.accelerationMin),
      .acceleration_max = math::VectorOf(live.accelerationMax),
      .speed_range = {live.speedRange[0], live.speedRange[1]},
      .speed = live.speed,
      .mass_range = {live.massRange[0], live.massRange[1]},
      .gravity_range = {live.gravityScaleRange[0], live.gravityScaleRange[1]},
      .lifetime_range = {live.lifetimeRange[0], live.lifetimeRange[1]},
      .attraction_offset = math::VectorOf(live.attractionOffset),
      .attraction_jitter = math::VectorOf(live.attractionJitter),
      .attraction_range = {live.attractionRange[0], live.attractionRange[1]},
      .color_curves = {live.colorCurves.fields.redCurve,
                       live.colorCurves.fields.greenCurve,
                       live.colorCurves.fields.blueCurve},
      .age = live.colorCurves.fields.age,
      .transform = static_cast<std::uint8_t>(live.transformIndex),
      .attraction_transform =
          static_cast<std::uint8_t>(live.attractionTransform),
      .sin_angle0 = live.sinAngle0,
      .sin_angle1 = live.sinAngle1,
      .cos_angle0 = live.cosAngle0,
      .cos_angle1 = live.cosAngle1,
      .transparency = static_cast<std::uint8_t>(live.transparency),
      .ring_segments = live.ringSegments,
      .ring_positions = live.ringPositions};
  // RotMatrix_gte temporarily overlays the first eighteen scratch bytes.
  static_assert(sizeof(math::Mat3) == sizeof(MATRIX::m));
  std::memcpy(&values.direction_matrix, &live, sizeof(values.direction_matrix));
  return values;
}

void StoreScratch(Effects_ParticleEmitterScratch& live,
                  const ParticleEmissionScratch& values) {
  std::memcpy(&live, &values.direction_matrix, sizeof(values.direction_matrix));
  live.flags = values.flags;
  live.savedFlags = values.saved_flags;
  live.vector.vx = values.offset.x;
  live.vector.vy = values.offset.y;
  live.vector.vz = values.offset.z;
  math::StoreVector(live.aimPoint, values.aim);
  math::StoreVector(live.spawnPoint, values.spawn);
  math::StoreMatrix(live.aimMatrix, values.aim_matrix);
  math::StoreVector(live.position, values.position);
  math::StoreVector(live.extent, values.extent);
  math::StoreVector(live.direction, values.direction);
  math::StoreVector(live.directionJitter, values.direction_jitter);
  math::StoreVector(live.innerExtent, values.inner_extent);
  math::StoreVector(live.baseDirection, values.base_direction);
  math::StoreVector(live.forceMin, values.force_min);
  math::StoreVector(live.forceMax, values.force_max);
  math::StoreVector(live.accelerationMin, values.acceleration_min);
  math::StoreVector(live.accelerationMax, values.acceleration_max);
  live.speedRange[0] = values.speed_range.x;
  live.speedRange[1] = values.speed_range.y;
  live.speed = values.speed;
  live.massRange[0] = values.mass_range.x;
  live.massRange[1] = values.mass_range.y;
  live.gravityScaleRange[0] = values.gravity_range.x;
  live.gravityScaleRange[1] = values.gravity_range.y;
  live.lifetimeRange[0] = values.lifetime_range.x;
  live.lifetimeRange[1] = values.lifetime_range.y;
  math::StoreVector(live.attractionOffset, values.attraction_offset);
  math::StoreVector(live.attractionJitter, values.attraction_jitter);
  live.attractionRange[0] = values.attraction_range.x;
  live.attractionRange[1] = values.attraction_range.y;
  live.colorCurves.fields.redCurve = values.color_curves[0];
  live.colorCurves.fields.greenCurve = values.color_curves[1];
  live.colorCurves.fields.blueCurve = values.color_curves[2];
  live.colorCurves.fields.age = values.age;
  live.transformIndex = values.transform;
  live.attractionTransform = values.attraction_transform;
  live.sinAngle0 = values.sin_angle0;
  live.sinAngle1 = values.sin_angle1;
  live.cosAngle0 = values.cos_angle0;
  live.cosAngle1 = values.cos_angle1;
  live.transparency = static_cast<int>(
      (static_cast<std::uint32_t>(live.transparency) & ~kByteMask) |
      values.transparency);
  live.ringSegments = values.ring_segments;
  live.ringPositions = values.ring_positions;
}

[[nodiscard]] std::optional<ParticleEmissionHandle> Handle(
    const Particle* particle) {
  if (particle == nullptr || Effects_particlePool == nullptr ||
      Effects_particleCapacity <= 0) {
    return std::nullopt;
  }
  const auto address = reinterpret_cast<std::uintptr_t>(particle);
  const auto base = reinterpret_cast<std::uintptr_t>(Effects_particlePool);
  if (address < base) return std::nullopt;
  const auto offset = address - base;
  const auto index = offset / sizeof(Particle);
  if (offset % sizeof(Particle) != 0 ||
      index >= static_cast<std::size_t>(Effects_particleCapacity)) {
    return std::nullopt;
  }
  return ParticleEmissionHandle{index};
}

[[nodiscard]] Particle* At(ParticleEmissionHandle handle) {
  if (Effects_particlePool == nullptr ||
      !std::cmp_less(handle.index, Effects_particleCapacity)) {
    return nullptr;
  }
  return &Effects_particlePool[handle.index];
}

[[nodiscard]] bool ValidPool(const Effects_EffectInstance& instance) {
  if (Effects_particleCapacity < 0 || Effects_usedParticleCount < 0 ||
      Effects_usedParticleCount > Effects_particleCapacity ||
      (instance.particles != nullptr && !Handle(instance.particles))) {
    return false;
  }
  const auto available = Effects_particleCapacity - Effects_usedParticleCount;
  const Particle* particle = Effects_freeParticles;
  for (int index = 0; index < available; ++index) {
    if (!Handle(particle) || particle == instance.particles) return false;
    particle = particle->next;
  }
  // Also detects cycles and a free-list length inconsistent with the pool.
  return particle == nullptr;
}

class LiveEmissionHost final : public ParticleEmissionHost {
 public:
  explicit LiveEmissionHost(Effects_EffectInstance& instance)
      : instance_(instance) {}

  [[nodiscard]] std::optional<ParticleEmissionHandle> Allocate() override {
    return Handle(LiveAllocateEffectParticle(&instance_));
  }
  [[nodiscard]] std::optional<EmittedParticle> Read(
      ParticleEmissionHandle handle) const override {
    const Particle* particle = At(handle);
    if (particle == nullptr) return std::nullopt;
    const auto next = Handle(particle->next);
    if (particle->next != nullptr && !next) return std::nullopt;
    return EmittedParticle{
        .next = next,
        .motion = {.mass = particle->motion.mass,
                   .gravity_scale = particle->motion.gravityScale,
                   .position = math::VectorOf(particle->motion.position),
                   .velocity = math::VectorOf(particle->motion.velocity),
                   .force = math::VectorOf(particle->motion.force),
                   .acceleration =
                       math::VectorOf(particle->motion.acceleration),
                   .target = {particle->attractionTarget[0],
                              particle->attractionTarget[1],
                              particle->attractionTarget[2]},
                   .attraction = particle->attraction},
        .flags = static_cast<std::uint32_t>(particle->emitFlags),
        .transparency = particle->transparency,
        .variant = static_cast<std::uint8_t>(particle->opaque61Bit0),
        .lifetime = particle->lifetime,
        .transform = particle->transformIndex,
        .attraction_transform = particle->attractionTransform,
        .end_event = particle->endEvent,
        .tick_event = particle->tickEvent,
        .color_curves = {particle->colorCurves[0], particle->colorCurves[1],
                         particle->colorCurves[2]},
        .age = particle->age};
  }

  void Store(ParticleEmissionHandle handle,
             const EmittedParticle& values) override {
    Particle* particle = At(handle);
    if (particle == nullptr) {
      Refuse("particle store lost its pool handle");
      return;
    }
    particle->motion.mass = values.motion.mass;
    particle->motion.gravityScale = values.motion.gravity_scale;
    math::StoreVector(particle->motion.position, values.motion.position);
    math::StoreVector(particle->motion.velocity, values.motion.velocity);
    math::StoreVector(particle->motion.force, values.motion.force);
    math::StoreVector(particle->motion.acceleration,
                      values.motion.acceleration);
    particle->attractionTarget[0] =
        math::WrapSigned16(static_cast<std::uint32_t>(values.motion.target.x));
    particle->attractionTarget[1] =
        math::WrapSigned16(static_cast<std::uint32_t>(values.motion.target.y));
    particle->attractionTarget[2] =
        math::WrapSigned16(static_cast<std::uint32_t>(values.motion.target.z));
    particle->attraction = values.motion.attraction;
    particle->emitFlags = static_cast<int>(values.flags);
    particle->transparency = values.transparency;
    particle->opaque61Bit0 = values.variant;
    particle->lifetime = values.lifetime;
    particle->transformIndex = values.transform;
    particle->attractionTransform = values.attraction_transform;
    particle->endEvent = values.end_event;
    particle->tickEvent = values.tick_event;
    std::ranges::copy(values.color_curves, particle->colorCurves);
    particle->age = values.age;
  }

  void InitializeTexture(ParticleEmissionHandle handle, std::uint8_t texture,
                         std::uint8_t animation) override {
    if (Particle* particle = At(handle)) {
      LiveInitializeParticleTextureAnimation(&particle->textureAnimation,
                                             texture, animation);
    } else {
      Refuse("texture initialization lost its pool handle");
    }
  }
  void Release(ParticleEmissionHandle handle) override {
    if (Particle* particle = At(handle)) {
      LiveReleaseEffectParticle(particle, &instance_);
    } else {
      Refuse("particle release lost its pool handle");
    }
  }

 private:
  Effects_EffectInstance& instance_;
};

[[nodiscard]] bool BoundCurves(const ParticleEmissionDefinition& definition) {
  const auto bound = LiveEffectCurveCount();
  return std::ranges::all_of(definition.curves, [bound](std::uint8_t curve) {
    return curve == 0 ||
           (curve <= bound && Effects_package->curves[curve - 1] != nullptr &&
            Effects_package->curveBlock->curveSizes[curve - 1] != 0);
  });
}

[[nodiscard]] std::optional<Vec3> Aim(
    const Effects_ActionEffectTarget& target) {
  if (target.type != kActorTarget) return math::VectorOf(target.value.position);
  SVECTOR point{};
  if (Actors::LiveActorBonePosition(target.value.actor.actor,
                                    target.value.actor.part, &point, 0) < 0) {
    return std::nullopt;
  }
  return math::VectorOf(point);
}

[[nodiscard]] std::optional<ParticleEmissionContext> Context(
    const Effects_EffectInstance& instance,
    const ParticleEmissionDefinition& definition) {
  const std::span transforms(instance.transforms);
  if (definition.transform >= transforms.size() ||
      ((definition.flags & kAttractionMask) == kActorAttraction &&
       definition.attraction_transform >= transforms.size())) {
    Refuse("definition names an unavailable transform");
    return std::nullopt;
  }
  if (!BoundCurves(definition)) {
    Refuse("definition names an unbound or empty curve");
    return std::nullopt;
  }
  if (!ValidPool(instance)) {
    Refuse("particle pool counts or free-list links are inconsistent");
    return std::nullopt;
  }
  ParticleEmissionContext context{
      .frame = static_cast<std::uint16_t>(instance.frame),
      .capacity = Effects_particleCapacity,
      .used = Effects_usedParticleCount,
      .effect_origin = math::VectorOf(*Effects_effectOrigin),
      .transform = math::TransformOf(transforms[definition.transform].world),
      .samples = LiveEffectCurveSamples().first(LiveEffectCurveCount() + 1)};
  if ((definition.flags & kAttractionMask) == kActorAttraction) {
    context.attraction_translation =
        math::TransformOf(transforms[definition.attraction_transform].world)
            .translation;
  }
  const auto direction = definition.flags & kDirectionMask;
  if (direction == kTowardSource || direction == kTowardDestination) {
    context.aim_target =
        Aim(direction == kTowardSource ? instance.context.emitFrom
                                       : instance.context.emitTo);
    if (!context.aim_target) {
      Refuse("aim endpoint has no current actor bone");
      return std::nullopt;
    }
  }
  return context;
}
}  // namespace

// decomp: BATTLE.PRG func_800D2ADC (src/BATTLE/BATTLE.PRG/5BF94.c:6935-7502).
// Preserves the existing event-call parameter order.
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
void EmitLiveParticleEvent(Effects_EffectInstance* instance, int event,
                           int transparency_override, int particle_variant,
                           const Effects_PluginEffectState* parent) {
  if (instance == nullptr || Effects_package == nullptr ||
      Effects_package->block5Data == nullptr || event < 0 ||
      !std::cmp_less(event, LiveParticleDefinitionCount())) {
    Refuse("missing instance or definition outside the current package");
    return;
  }
  const auto definition =
      Definition(Effects_package->block5Data->definitions[event]);
  auto context = Context(*instance, definition);
  if (!context) return;
  context->transparency_override = transparency_override;
  context->variant = particle_variant;
  if (parent != nullptr) {
    context->parent = ParticleEmissionParent{
        .position = math::VectorOf(parent->position),
        .transparency = static_cast<std::uint8_t>(parent->transparency),
        .variant = static_cast<std::uint8_t>(parent->variant)};
  }
  auto& live_scratch = *reinterpret_cast<Effects_ParticleEmitterScratch*>(
      reinterpret_cast<std::uintptr_t>(Core_ScratchBase()) + kScratchOffset);
  auto scratch = LoadScratch(live_scratch);
  LiveEmissionHost host(*instance);
  const auto result =
      EmitParticleEvent(definition, *context, scratch, Effects_emissionCounter,
                        Core::BiosRand(), host);
  StoreScratch(live_scratch, scratch);
  if (result.clear_translation) Core_Gte_ClearTranslation();
  if (result.rotation) {
    MATRIX rotation{};
    math::StoreMatrix(rotation, *result.rotation);
    Core_Gte_SetRotMatrix(&rotation);
  }
  if (result.status >= ParticleEmissionStatus::kMissingSamples) {
    Refuse("checked emission core declined its inputs or allocation");
  }
}
}  // namespace VS::Effects
