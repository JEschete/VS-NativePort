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
#include "Core/Gte.h"
#include "Core/Log.hpp"
#include "Core/Math/InterpolationLive.hpp"
#include "Core/Math/MatrixLive.hpp"
#include "Core/RandomLive.hpp"
#include "Core/Scratch.hpp"
#include "Effects/ActionRequest.hpp"
#include "Effects/CurvesLive.hpp"
#include "Effects/Package.hpp"
#include "Effects/PackageExtents.hpp"
#include "Effects/ParticleEmitter.hpp"
#include "Effects/ParticlePoolLive.hpp"
#include "Effects/ParticleTextureAnimationLive.hpp"
#include "Effects/Particles.hpp"
#include "Effects/Runtime.h"
#include "Effects/TransformsLiveState.hpp"
#include "Legacy/Psx.h"

namespace VS::Effects {
// decomp: D_800EC328 (BATTLE.PRG); contents: module image (ModuleData.cpp).
std::int32_t emissionCounter = 0;

namespace {
namespace math = Core::Math;
using math::Vec3;
using Particle = LiveParticle;

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
    VS::Core::LogWarn("effects", "particle emission refused: {} ({} refusals)",
                      reason, refused_emissions);
  }
}

[[nodiscard]] ParticleEmissionDefinition Definition(
    const LiveParticleDefinition& definition) {
  const auto vector_range = [](std::span<const std::int16_t> range) {
    return EmissionVectorRange{math::RangePair(range, 0),
                               math::RangePair(range, 1),
                               math::RangePair(range, 2)};
  };
  ParticleEmissionDefinition typed{
      .flags = definition.flags,
      .transparency = static_cast<std::uint8_t>(definition.transparency_curve),
      .texture = definition.texture_index,
      .animation = definition.animation_id,
      .end_event = definition.end_event,
      .tick_event = definition.tick_event,
      .curves = {definition.position_curve, definition.extent_curve,
                 definition.direction_curve, definition.direction_jitter_curve,
                 definition.speed_curve, definition.force_curve,
                 definition.acceleration_curve,
                 definition.attraction_offset_curve,
                 definition.attraction_jitter_curve,
                 definition.attraction_curve, definition.mass_curve,
                 definition.gravity_scale_curve, definition.lifetime_curve,
                 definition.count_curve, definition.interval_curve,
                 definition.inner_extent_curve},
      .position = vector_range(definition.position_range),
      .extent = vector_range(definition.extent_range),
      .inner_extent = math::RangePair(definition.inner_extent_range, 0),
      .count = math::RangePair(definition.count_range, 0),
      .interval = math::RangePair(definition.interval_range, 0),
      .color_curves = {definition.r_curve, definition.g_curve,
                       definition.b_curve},
      .transform = definition.transform_index,
      .attraction_transform = definition.attraction_transform,
      .ring_segments = definition.ring_segments,
      .ring_positions = definition.ring_positions};
  for (std::size_t index = 0; index < typed.vectors.size(); ++index) {
    typed.vectors.at(index) = vector_range(definition.vector_ranges[index]);
  }
  for (std::size_t index = 0; index < typed.pairs.size(); ++index) {
    typed.pairs.at(index) = {math::RangePair(definition.pair_ranges[index], 0),
                             math::RangePair(definition.pair_ranges[index], 1)};
  }
  return typed;
}

[[nodiscard]] ParticleEmissionScratch LoadScratch(
    const LiveParticleEmitterScratch& live) {
  ParticleEmissionScratch values{
      .flags = live.flags,
      .saved_flags = live.saved_flags,
      .offset = {live.vector.vx, live.vector.vy, live.vector.vz},
      .aim = math::VectorOf(live.aim_point),
      .spawn = math::VectorOf(live.spawn_point),
      .aim_matrix = math::MatrixOf(live.aim_matrix),
      .position = math::VectorOf(live.position),
      .extent = math::VectorOf(live.extent),
      .direction = math::VectorOf(live.direction),
      .direction_jitter = math::VectorOf(live.direction_jitter),
      .inner_extent = math::VectorOf(live.inner_extent),
      .base_direction = math::VectorOf(live.base_direction),
      .force_min = math::VectorOf(live.force_min),
      .force_max = math::VectorOf(live.force_max),
      .acceleration_min = math::VectorOf(live.acceleration_min),
      .acceleration_max = math::VectorOf(live.acceleration_max),
      .speed_range = {live.speed_range[0], live.speed_range[1]},
      .speed = live.speed,
      .mass_range = {live.mass_range[0], live.mass_range[1]},
      .gravity_range = {live.gravity_scale_range[0],
                        live.gravity_scale_range[1]},
      .lifetime_range = {live.lifetime_range[0], live.lifetime_range[1]},
      .attraction_offset = math::VectorOf(live.attraction_offset),
      .attraction_jitter = math::VectorOf(live.attraction_jitter),
      .attraction_range = {live.attraction_range[0], live.attraction_range[1]},
      .color_curves = {live.color_curves.red_curve,
                       live.color_curves.green_curve,
                       live.color_curves.blue_curve},
      .age = live.color_curves.age,
      .transform = static_cast<std::uint8_t>(live.transform_index),
      .attraction_transform =
          static_cast<std::uint8_t>(live.attraction_transform),
      .sin_angle0 = live.sin_angle0,
      .sin_angle1 = live.sin_angle1,
      .cos_angle0 = live.cos_angle0,
      .cos_angle1 = live.cos_angle1,
      .transparency = static_cast<std::uint8_t>(live.transparency),
      .ring_segments = live.ring_segments,
      .ring_positions = live.ring_positions};
  // RotMatrix_gte temporarily overlays the first eighteen scratch bytes.
  static_assert(sizeof(math::Mat3) == sizeof(MATRIX::m));
  std::memcpy(&values.direction_matrix, &live, sizeof(values.direction_matrix));
  return values;
}

void StoreScratch(LiveParticleEmitterScratch& live,
                  const ParticleEmissionScratch& values) {
  std::memcpy(&live, &values.direction_matrix, sizeof(values.direction_matrix));
  live.flags = values.flags;
  live.saved_flags = values.saved_flags;
  live.vector.vx = values.offset.x;
  live.vector.vy = values.offset.y;
  live.vector.vz = values.offset.z;
  math::StoreVector(live.aim_point, values.aim);
  math::StoreVector(live.spawn_point, values.spawn);
  math::StoreMatrix(live.aim_matrix, values.aim_matrix);
  math::StoreVector(live.position, values.position);
  math::StoreVector(live.extent, values.extent);
  math::StoreVector(live.direction, values.direction);
  math::StoreVector(live.direction_jitter, values.direction_jitter);
  math::StoreVector(live.inner_extent, values.inner_extent);
  math::StoreVector(live.base_direction, values.base_direction);
  math::StoreVector(live.force_min, values.force_min);
  math::StoreVector(live.force_max, values.force_max);
  math::StoreVector(live.acceleration_min, values.acceleration_min);
  math::StoreVector(live.acceleration_max, values.acceleration_max);
  live.speed_range[0] = values.speed_range.x;
  live.speed_range[1] = values.speed_range.y;
  live.speed = values.speed;
  live.mass_range[0] = values.mass_range.x;
  live.mass_range[1] = values.mass_range.y;
  live.gravity_scale_range[0] = values.gravity_range.x;
  live.gravity_scale_range[1] = values.gravity_range.y;
  live.lifetime_range[0] = values.lifetime_range.x;
  live.lifetime_range[1] = values.lifetime_range.y;
  math::StoreVector(live.attraction_offset, values.attraction_offset);
  math::StoreVector(live.attraction_jitter, values.attraction_jitter);
  live.attraction_range[0] = values.attraction_range.x;
  live.attraction_range[1] = values.attraction_range.y;
  live.color_curves.red_curve = values.color_curves[0];
  live.color_curves.green_curve = values.color_curves[1];
  live.color_curves.blue_curve = values.color_curves[2];
  live.color_curves.age = values.age;
  live.transform_index = values.transform;
  live.attraction_transform = values.attraction_transform;
  live.sin_angle0 = values.sin_angle0;
  live.sin_angle1 = values.sin_angle1;
  live.cos_angle0 = values.cos_angle0;
  live.cos_angle1 = values.cos_angle1;
  live.transparency = static_cast<int>(
      (static_cast<std::uint32_t>(live.transparency) & ~kByteMask) |
      values.transparency);
  live.ring_segments = values.ring_segments;
  live.ring_positions = values.ring_positions;
}

[[nodiscard]] std::optional<ParticleEmissionHandle> Handle(
    const Particle* particle) {
  if (particle == nullptr || particlePool == nullptr || particleCapacity <= 0) {
    return std::nullopt;
  }
  const auto address = reinterpret_cast<std::uintptr_t>(particle);
  const auto base = reinterpret_cast<std::uintptr_t>(particlePool);
  if (address < base) return std::nullopt;
  const auto offset = address - base;
  const auto index = offset / sizeof(Particle);
  if (offset % sizeof(Particle) != 0 ||
      index >= static_cast<std::size_t>(particleCapacity)) {
    return std::nullopt;
  }
  return ParticleEmissionHandle{index};
}

[[nodiscard]] Particle* At(ParticleEmissionHandle handle) {
  if (particlePool == nullptr ||
      !std::cmp_less(handle.index, particleCapacity)) {
    return nullptr;
  }
  return &particlePool[handle.index];
}

[[nodiscard]] bool ValidPool(const Effects_EffectInstance& instance) {
  if (particleCapacity < 0 || usedParticleCount < 0 ||
      usedParticleCount > particleCapacity ||
      (instance.particles != nullptr && !Handle(instance.particles))) {
    return false;
  }
  const auto available = particleCapacity - usedParticleCount;
  const Particle* particle = freeParticles;
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
                   .gravity_scale = particle->motion.gravity_scale,
                   .position = math::VectorOf(particle->motion.position),
                   .velocity = math::VectorOf(particle->motion.velocity),
                   .force = math::VectorOf(particle->motion.force),
                   .acceleration =
                       math::VectorOf(particle->motion.acceleration),
                   .target = {particle->attraction_target[0],
                              particle->attraction_target[1],
                              particle->attraction_target[2]},
                   .attraction = particle->attraction},
        .flags = static_cast<std::uint32_t>(particle->emit_flags),
        .transparency = particle->transparency,
        .variant = static_cast<std::uint8_t>(particle->variant),
        .lifetime = particle->lifetime,
        .transform = particle->transform_index,
        .attraction_transform = particle->attraction_transform,
        .end_event = particle->end_event,
        .tick_event = particle->tick_event,
        .color_curves = {particle->color_curves[0], particle->color_curves[1],
                         particle->color_curves[2]},
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
    particle->motion.gravity_scale = values.motion.gravity_scale;
    math::StoreVector(particle->motion.position, values.motion.position);
    math::StoreVector(particle->motion.velocity, values.motion.velocity);
    math::StoreVector(particle->motion.force, values.motion.force);
    math::StoreVector(particle->motion.acceleration,
                      values.motion.acceleration);
    particle->attraction_target[0] =
        math::WrapSigned16(static_cast<std::uint32_t>(values.motion.target.x));
    particle->attraction_target[1] =
        math::WrapSigned16(static_cast<std::uint32_t>(values.motion.target.y));
    particle->attraction_target[2] =
        math::WrapSigned16(static_cast<std::uint32_t>(values.motion.target.z));
    particle->attraction = values.motion.attraction;
    particle->emit_flags = static_cast<int>(values.flags);
    particle->transparency = values.transparency;
    particle->variant = values.variant;
    particle->lifetime = values.lifetime;
    particle->transform_index = values.transform;
    particle->attraction_transform = values.attraction_transform;
    particle->end_event = values.end_event;
    particle->tick_event = values.tick_event;
    std::ranges::copy(values.color_curves, particle->color_curves.begin());
    particle->age = values.age;
  }

  void InitializeTexture(ParticleEmissionHandle handle, std::uint8_t texture,
                         std::uint8_t animation) override {
    if (Particle* particle = At(handle)) {
      LiveInitializeParticleTextureAnimation(&particle->texture_animation,
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
            Effects_package->curveBlock->Size(curve - 1) != 0);
  });
}

[[nodiscard]] std::optional<Vec3> Aim(const LiveActionEffectTarget& target) {
  if (target.type != kActorTarget) return math::VectorOf(target.position);
  SVECTOR point{};
  if (Actors::LiveActorBonePosition(target.Actor().actor, target.Actor().part,
                                    &point, 0) < 0) {
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
      .capacity = particleCapacity,
      .used = usedParticleCount,
      .effect_origin = math::VectorOf(Effects_actionRequest.position),
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
        Aim(direction == kTowardSource ? instance.context.emit_from
                                       : instance.context.emit_to);
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
                           const LiveParticle* parent) {
  if (instance == nullptr || Effects_package == nullptr ||
      Effects_package->block5Data == nullptr || event < 0 ||
      !std::cmp_less(event, LiveParticleDefinitionCount())) {
    Refuse("missing instance or definition outside the current package");
    return;
  }
  const auto definition =
      Definition(Effects_package->block5Data->Definitions()[event]);
  auto context = Context(*instance, definition);
  if (!context) return;
  context->transparency_override = transparency_override;
  context->variant = particle_variant;
  if (parent != nullptr) {
    context->parent = ParticleEmissionParent{
        .position = math::VectorOf(parent->motion.position),
        .transparency = static_cast<std::uint8_t>(parent->transparency),
        .variant = static_cast<std::uint8_t>(parent->variant)};
  }
  auto& live_scratch = *reinterpret_cast<LiveParticleEmitterScratch*>(
      reinterpret_cast<std::uintptr_t>(VS::Core::ScratchBase()) +
      kScratchOffset);
  auto scratch = LoadScratch(live_scratch);
  LiveEmissionHost host(*instance);
  const auto result = EmitParticleEvent(
      definition, *context, scratch, emissionCounter, Core::BiosRand(), host);
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
