// Hand-maintained (tools/port/owned.csv).
#include "Effects/Templates/ParticlesLive.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <span>
#include <type_traits>
#include <utility>

#include "Core/Gte.h"
#include "Core/Log.hpp"
#include "Core/Math/InterpolationLive.hpp"
#include "Core/Math/MatrixLive.hpp"
#include "Core/Math/ScalarMath.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Core/RandomLive.hpp"
#include "Core/Scratch.hpp"
#include "Effects/CurvesLive.hpp"
#include "Effects/Package.hpp"
#include "Effects/PackageExtents.hpp"
#include "Effects/ParticleMotion.hpp"
#include "Effects/ParticleTextureAnimationLive.hpp"
#include "Effects/Particles.hpp"
#include "Effects/Plugins/Plg045.hpp"
#include "Effects/Plugins/Plg057.hpp"
#include "Effects/Plugins/Plg065.hpp"
#include "Effects/Plugins/Plg082.hpp"
#include "Effects/Plugins/Plg087.hpp"
#include "Effects/Plugins/Plg114.hpp"
#include "Effects/Plugins/Plg146.hpp"
#include "Effects/Plugins/Plg147.hpp"
#include "Effects/Plugins/Plg153.hpp"
#include "Effects/Plugins/Plg156.hpp"
#include "Effects/Plugins/Plg168.hpp"
#include "Effects/Plugins/Plg224.hpp"
#include "Effects/Plugins/Plg255.hpp"
#include "Effects/Plugins/Plg263.hpp"
#include "Effects/Plugins/Plg265.hpp"
#include "Effects/Plugins/Plg266.hpp"
#include "Effects/Plugins/Plg300.hpp"
#include "Effects/Plugins/Plg301.hpp"
#include "Effects/Runtime.h"
#include "Effects/Templates/ParticleCore.hpp"
#include "Effects/Templates/ParticleDispatch.hpp"
#include "Effects/TransformsLiveState.hpp"
#include "Legacy/Psx.h"
#include "Render/Camera.hpp"
#include "Render/Frame.hpp"
#include "Render/LegacyPackets.hpp"
#include "Render/Render.hpp"
#include "Render/SpritesLive.hpp"
#include "Render/UiPacketsLive.hpp"
#include "Render/UiPrimitives.hpp"

namespace VS::Effects::Templates {
namespace {
namespace math = Core::Math;
using math::Vec3;

constexpr std::uintptr_t kEmitterScratchOffset = 0x1D0;
using InstanceTransforms = decltype(Effects_EffectInstance::transforms);
using TextureTables = decltype(Effects_EffectPackage::textureInfoTable);
constexpr std::size_t kTransformCount = std::extent_v<InstanceTransforms>;
constexpr std::size_t kTextureTableCount = std::extent_v<TextureTables>;
constexpr std::uint32_t kOrderingHighMask = ~kParticleLinkMask;
// vectorRanges[] and pairRanges[] rows of a particle definition the
// particle-set stages read; row 5 holds the depth bias and the cooldown.
constexpr std::size_t kDirectionRange = 0;
constexpr std::size_t kDirectionJitterRange = 1;
constexpr std::size_t kSpeedJitterRange = 2;
constexpr std::size_t kWidthRange = 4;
constexpr std::size_t kTimingRange = 5;
constexpr std::size_t kTargetOffsetRange = 6;
constexpr std::size_t kTargetJitterRange = 7;
constexpr std::size_t kDepthBiasHalfword = 0;
constexpr std::size_t kCooldownHalfword = 2;
constexpr std::size_t kFirstRadiusRange = 0;
constexpr std::size_t kSecondRadiusRange = 1;
constexpr std::size_t kSpeedRange = 2;
// The line blend is the transparency field's low two bits.
constexpr unsigned kBlendMask = 3;

static_assert(sizeof(LiveParticleDefinition) == kParticleDefinitionBytes);
static_assert(sizeof(VS::Render::ParticleQuadPacket) ==
              kParticleQuadWords * sizeof(std::uint32_t));
static_assert(sizeof(VS::Render::ParticleLinePacket) ==
              kParticleLineWords * sizeof(std::uint32_t));
static_assert(sizeof(SVECTOR) * kParticleControlPoints ==
              kParticleControlPointBytes);
static_assert(sizeof(u_long) == sizeof(std::uint32_t));

std::uint64_t refused_curves = 0;
std::uint64_t refused_textures = 0;
std::uint64_t dropped_packets = 0;

// True on the 1st, 2nd, 4th, 8th... occurrence, to keep repeated
// diagnostics readable.
[[nodiscard]] bool ShouldReport(std::uint64_t& count) {
  return std::has_single_bit(++count);
}

LiveParticleEmitterScratch& EmitterScratch() {
  return *reinterpret_cast<LiveParticleEmitterScratch*>(
      reinterpret_cast<std::uintptr_t>(VS::Core::ScratchBase()) +
      kEmitterScratchOffset);
}

// The current package's definition `index`, or null when its recorded
// definition block does not hold it.
const LiveParticleDefinition* PackageDefinition(std::size_t index) {
  if (Effects_package == nullptr || Effects_package->block5Data == nullptr ||
      index >= LiveParticleDefinitionCount()) {
    return nullptr;
  }
  return &Effects_package->block5Data->Definitions()[index];
}

[[nodiscard]] ParticleVectorRange VectorRange(
    std::span<const std::int16_t, math::kVectorRangeHalfwords> ranges) {
  return {math::RangePair(ranges, 0), math::RangePair(ranges, 1),
          math::RangePair(ranges, 2)};
}

[[nodiscard]] ParticlePlaneRange PlaneRange(
    std::span<const std::int16_t> ranges) {
  return {math::RangePair(ranges, 0), math::RangePair(ranges, 1)};
}

[[nodiscard]] ParticleSetCurves TypedCurves(
    const LiveParticleDefinition& definition) {
  return {
      .position = definition.position_curve,
      .extent = definition.extent_curve,
      .direction = definition.direction_curve,
      .direction_jitter = definition.direction_jitter_curve,
      .speed = definition.speed_curve,
      .force = definition.force_curve,
      .acceleration = definition.acceleration_curve,
      .attraction_offset = definition.attraction_offset_curve,
      .attraction_jitter = definition.attraction_jitter_curve,
      .mass = definition.mass_curve,
      .gravity_scale = definition.gravity_scale_curve,
      .interval = definition.interval_curve,
      .inner_extent = definition.inner_extent_curve,
      .color = {definition.r_curve, definition.g_curve, definition.b_curve}};
}

[[nodiscard]] ParticleSetDefinition TypedDefinition(
    const LiveParticleDefinition& definition) {
  const auto& vectors = definition.vector_ranges;
  const auto& pairs = definition.pair_ranges;
  return {.flags = definition.flags,
          .blend = static_cast<std::uint8_t>(definition.transparency_curve &
                                             kBlendMask),
          .curves = TypedCurves(definition),
          .position = VectorRange(definition.position_range),
          .extent = VectorRange(definition.extent_range),
          .inner_extent = math::RangePair(definition.inner_extent_range, 0),
          .interval = math::RangePair(definition.interval_range, 0),
          .direction = VectorRange(vectors[kDirectionRange]),
          .direction_jitter = VectorRange(vectors[kDirectionJitterRange]),
          .speed_jitter = VectorRange(vectors[kSpeedJitterRange]),
          .width = VectorRange(vectors[kWidthRange]),
          .target_offset = VectorRange(vectors[kTargetOffsetRange]),
          .target_jitter = VectorRange(vectors[kTargetJitterRange]),
          .first_radius = PlaneRange(pairs[kFirstRadiusRange]),
          .second_radius = PlaneRange(pairs[kSecondRadiusRange]),
          .speed = PlaneRange(pairs[kSpeedRange]),
          .cooldown = vectors[kTimingRange][kCooldownHalfword],
          .depth_bias = vectors[kTimingRange][kDepthBiasHalfword],
          .ring_segments = definition.ring_segments,
          .ring_positions = definition.ring_positions};
}

[[nodiscard]] ParticlePoint PointOf(const SVECTOR& vector) {
  return {vector.vx, vector.vy, vector.vz};
}

void StorePoint(SVECTOR& vector, ParticlePoint point) {
  vector.vx = point.x;
  vector.vy = point.y;
  vector.vz = point.z;
}

[[nodiscard]] math::Vec2 PairOf(std::span<const int, 2> pair) {
  return {pair[0], pair[1]};
}

void StorePair(std::span<int, 2> pair, math::Vec2 value) {
  pair[0] = value.x;
  pair[1] = value.y;
}

// The overlay global an origin instance keeps its emission cursor in.
std::int32_t& CounterOf(ParticleEmissionCounter counter) {
  using enum ParticleEmissionCounter;
  switch (counter) {
    case kPlg045:
      return Effects::Plg045::particleEmissionCounter;
    case kPlg057:
      return Effects::Plg057::particleEmissionCounter;
    case kPlg065:
      return Effects::Plg065::particleEmissionCounter;
    // PLG152's origin shares PLG082's address.
    case kPlg082:
    case kPlg152:
      return Effects::Plg082::particleEmissionCounter;
    case kPlg087:
      return Effects::Plg087::particleEmissionCounter;
    case kPlg114:
      return Effects::Plg114::particleEmissionCounter;
    case kPlg114Second:
      return Effects::Plg114::particleEmissionCounter2;
    case kPlg146:
      return Effects::Plg146::particleEmissionCounter;
    case kPlg147:
      return Effects::Plg147::particleEmissionCounter;
    case kPlg153:
      return Effects::Plg153::particleEmissionCounter;
    case kPlg156:
      return Effects::Plg156::particleEmissionCounter;
    case kPlg168:
      return Effects::Plg168::particleEmissionCounter;
    case kPlg224:
      return Effects::Plg224::particleEmissionCounter;
    case kPlg255:
      return Effects::Plg255::particleEmissionCounter;
    case kPlg263:
      return Effects::Plg263::particleEmissionCounter;
    case kPlg265:
      return Effects::Plg265::particleEmissionCounter;
    case kPlg265Second:
      return Effects::Plg265::particleEmissionCounter2;
    case kPlg266:
      return Effects::Plg266::particleEmissionCounter;
    case kPlg266Second:
      return Effects::Plg266::particleEmissionCounter2;
    case kPlg300:
      return Effects::Plg300::particleEmissionCounter;
    case kPlg301:
    default:
      return Effects::Plg301::particleEmissionCounter;
  }
}

class LiveParticleHost final : public ParticleSetHost {
 public:
  explicit LiveParticleHost(Effects_RendererSlot& slot) : slot_(slot) {}

  [[nodiscard]] std::optional<ParticleSetDefinition> Definition(
      std::uint8_t index) override;
  void Create(const ParticleSetState& state, std::int32_t capacity,
              ParticleTexturing texturing) override;

  [[nodiscard]] std::optional<ParticleSetState> State() const override {
    const LiveParticleQuadSet* live = LiveState();
    if (live == nullptr) {
      return std::nullopt;
    }
    return ParticleSetState{.lifetime = live->lifetime,
                            .definition_index = live->definition_index,
                            .count = live->count,
                            .cooldown = live->cooldown,
                            .age = live->age};
  }

  void StoreState(const ParticleSetState& state) override {
    LiveParticleQuadSet* live = LiveState();
    if (live == nullptr) {
      return;
    }
    live->lifetime = state.lifetime;
    live->definition_index = state.definition_index;
    live->count = state.count;
    live->cooldown = state.cooldown;
    live->age = state.age;
  }

  void Release() override {
    LiveParticleQuadSet* live = LiveState();
    if (live == nullptr || live->control_points == nullptr) {
      return;
    }
    Core::Memory::FreeLiveFromHighEnd(live->control_points);
    live->control_points = nullptr;
  }

  [[nodiscard]] std::optional<ParticleTickInputs> BeginTick(
      std::uint8_t definition_index) override;
  [[nodiscard]] ParticleSetPoints LoadPoints() const override;
  void StorePoints(const ParticleSetPoints& points) override;
  [[nodiscard]] ParticleSetScratch LoadScratch() const override;
  void PublishScratch(const ParticleSetScratch& scratch) override;
  [[nodiscard]] std::int32_t SampleCurve(std::uint8_t curve,
                                         std::int32_t distance) override;

  [[nodiscard]] std::int32_t Rand() override {
    return VS::Core::NextBiosRand();
  }

  [[nodiscard]] std::int32_t RandomRange(std::int32_t first,
                                         std::int32_t second) override {
    return Core::LiveRandomRangeExclusive(first, second);
  }

  [[nodiscard]] std::int32_t EmissionCounter(
      ParticleEmissionCounter counter) const override {
    return CounterOf(counter);
  }

  void StoreEmissionCounter(ParticleEmissionCounter counter,
                            std::int32_t value) override {
    CounterOf(counter) = value;
  }

  void BeginProjection(bool effect_view) override {
    if (!effect_view) {
      Render::LoadLiveFrameViewMatrix();
      return;
    }
    const MATRIX& view =
        instance_->transforms[definition_->transform_index].view;
    Core_Gte_SetRotMatrix(&view);
    Core_Gte_SetTransMatrix(&view);
  }

  [[nodiscard]] ParticleProjection Project(ParticlePoint point) override {
    const SVECTOR vertex{point.x, point.y, point.z, 0};
    Core_Gte_LoadV0(&vertex);
    Core_Gte_Rtps();
    Core_GteWord screen = 0;
    Core_GteWord depth = 0;
    Core_Gte_StoreSxy2(&screen);
    Core_Gte_StoreSz3Otz(&depth);
    return {.screen = static_cast<std::uint32_t>(screen),
            .depth = static_cast<std::int32_t>(depth)};
  }

  [[nodiscard]] std::optional<ParticleTexture> AdvanceTexture() override;
  void SubmitQuad(const ParticleQuad& quad, std::int32_t depth) override;
  void SubmitLine(const ParticleLine& line, std::int32_t depth) override;

 private:
  [[nodiscard]] LiveParticleQuadSet* LiveState() const {
    return static_cast<LiveParticleQuadSet*>(slot_.rendererState);
  }

  // The control point records the set's buffer holds, at most
  // kParticleSetCapacity ribbons of them.
  [[nodiscard]] std::span<SVECTOR> ControlPoints() const;
  // Room for `words` words linked at `depth` of the scene table, or nothing.
  [[nodiscard]] std::optional<std::pair<u_long*, u_long*>> Reserve(
      std::int32_t depth, std::size_t words);
  void Link(u_long* entry, u_long* packet, std::size_t words);

  Effects_RendererSlot& slot_;
  const LiveParticleDefinition* definition_ = nullptr;
  Effects_EffectInstance* instance_ = nullptr;
};

std::optional<ParticleSetDefinition> LiveParticleHost::Definition(
    std::uint8_t index) {
  const LiveParticleDefinition* definition = PackageDefinition(index);
  if (definition == nullptr) {
    VS::Core::LogWarn("effects",
                      "particle set: definition {} is outside the package's {} "
                      "definitions; set refused",
                      index, LiveParticleDefinitionCount());
    return std::nullopt;
  }
  return TypedDefinition(*definition);
}

// decomp: particleDispatch.h:14-44 (the create command). The original
// allocated unchecked and left the record's other fields as the heap held
// them; a set without room is refused here, and a capacity that is not
// positive allocates no control points.
void LiveParticleHost::Create(const ParticleSetState& state,
                              std::int32_t capacity,
                              ParticleTexturing texturing) {
  const LiveParticleDefinition* definition =
      PackageDefinition(state.definition_index);
  if (definition == nullptr) {
    return;
  }
  const bool textured = texturing == ParticleTexturing::kTextured;
  // The untextured sets allocate only the record's prefix.
  const std::size_t bytes =
      textured ? sizeof(LiveParticleQuadSet)
               : offsetof(LiveParticleQuadSet, texture_animation);
  void* storage = Core::Memory::AllocateLiveFromHighEnd(bytes);
  if (storage == nullptr) {
    VS::Core::LogError("effects", "particle set: heap exhausted for state");
    slot_.rendererState = nullptr;
    return;
  }
  std::memset(storage, 0, bytes);
  auto* live = static_cast<LiveParticleQuadSet*>(storage);
  slot_.rendererState = live;
  StoreState(state);
  if (capacity > 0) {
    live->control_points =
        static_cast<SVECTOR*>(Core::Memory::AllocateLiveFromHighEnd(
            static_cast<std::size_t>(capacity) * kParticleControlPointBytes));
    if (live->control_points == nullptr) {
      VS::Core::LogError(
          "effects", "particle set: heap exhausted for {} ribbons; set refused",
          capacity);
      Core::Memory::FreeLiveFromHighEnd(live);
      slot_.rendererState = nullptr;
      return;
    }
  }
  if (textured) {
    // Clearing the record also cleared the texture animation, as
    // func_800D6CCC did.
    VS::Effects::LiveInitializeParticleTextureAnimation(
        &live->texture_animation, definition->texture_index,
        definition->animation_id);
  }
}

std::optional<ParticleTickInputs> LiveParticleHost::BeginTick(
    std::uint8_t definition_index) {
  instance_ = Effects_currentInstance;
  if (instance_ == nullptr) {
    VS::Core::LogWarn("effects",
                      "particle set: no current effect instance; set stopped");
    return std::nullopt;
  }
  definition_ = PackageDefinition(definition_index);
  if (definition_ == nullptr) {
    VS::Core::LogWarn("effects",
                      "particle set: the current package no longer holds "
                      "definition {}; set stopped",
                      definition_index);
    return std::nullopt;
  }
  if (definition_->transform_index >= kTransformCount) {
    VS::Core::LogWarn(
        "effects",
        "particle set: definition {} names transform {} of {}; set "
        "stopped",
        definition_index, definition_->transform_index, kTransformCount);
    return std::nullopt;
  }
  const bool targets_transform =
      (definition_->flags & kParticleTargetMask) ==
      static_cast<std::uint32_t>(ParticleTargetAnchor::kTransform);
  if (targets_transform &&
      definition_->attraction_transform >= kTransformCount) {
    VS::Core::LogWarn(
        "effects",
        "particle set: definition {} targets transform {} of {}; set "
        "stopped",
        definition_index, definition_->attraction_transform, kTransformCount);
    return std::nullopt;
  }
  const math::Transform3 world = math::TransformOf(
      instance_->transforms[definition_->transform_index].world);
  const Vec3 target_translation =
      targets_transform
          ? math::TransformOf(
                instance_->transforms[definition_->attraction_transform].world)
                .translation
          : Vec3{};
  // Effects_effectOrigin: the action request's position.
  return ParticleTickInputs{
      .definition = TypedDefinition(*definition_),
      .effect_origin = math::VectorOf(Effects_actionRequest.position),
      .world = world,
      .target_translation = target_translation,
      .near_clip = VS::Render::nearClip};
}

std::span<SVECTOR> LiveParticleHost::ControlPoints() const {
  const LiveParticleQuadSet* live = LiveState();
  if (live == nullptr || live->control_points == nullptr) {
    return {};
  }
  const auto bytes = Core::Memory::LivePayloadCapacity(live->control_points);
  if (!bytes) {
    return {};
  }
  const std::size_t ribbons =
      std::min(*bytes / kParticleControlPointBytes, kParticleSetCapacity);
  return {live->control_points, ribbons * kParticleControlPoints};
}

ParticleSetPoints LiveParticleHost::LoadPoints() const {
  const std::span<SVECTOR> records = ControlPoints();
  ParticleSetPoints points{.capacity = records.size() / kParticleControlPoints};
  for (std::size_t index = 0; index < records.size(); ++index) {
    points.sets.at(index / kParticleControlPoints)
        .at(index % kParticleControlPoints) = PointOf(records[index]);
  }
  return points;
}

void LiveParticleHost::StorePoints(const ParticleSetPoints& points) {
  const std::span<SVECTOR> records = ControlPoints();
  for (std::size_t index = 0; index < records.size(); ++index) {
    StorePoint(records[index], points.sets.at(index / kParticleControlPoints)
                                   .at(index % kParticleControlPoints));
  }
}

ParticleSetScratch LiveParticleHost::LoadScratch() const {
  const LiveParticleEmitterScratch& scratch = EmitterScratch();
  return {.flags = scratch.flags,
          .position = math::VectorOf(scratch.position),
          .extent = math::VectorOf(scratch.extent),
          .aim_point = math::VectorOf(scratch.aim_point),
          .spawn_point = math::VectorOf(scratch.spawn_point),
          .direction_jitter = math::VectorOf(scratch.direction_jitter),
          .base_direction = math::VectorOf(scratch.base_direction),
          .vector = PointOf(scratch.vector),
          .speed_jitter = PointOf(scratch.force_min),
          .width = PointOf(scratch.acceleration_min),
          .target_offset = PointOf(scratch.attraction_offset),
          .target_jitter = PointOf(scratch.attraction_jitter),
          .speed = PairOf(scratch.speed_range),
          .first_radius = PairOf(scratch.mass_range),
          .second_radius = PairOf(scratch.gravity_scale_range),
          .aim = math::MatrixOf(scratch.aim_matrix),
          .sine = {scratch.sin_angle0, scratch.sin_angle1},
          .cosine = {scratch.cos_angle0, scratch.cos_angle1},
          .ring_segments = scratch.ring_segments,
          .ring_positions = scratch.ring_positions};
}

void LiveParticleHost::PublishScratch(const ParticleSetScratch& values) {
  LiveParticleEmitterScratch& scratch = EmitterScratch();
  scratch.flags = values.flags;
  math::StoreVector(scratch.position, values.position);
  math::StoreVector(scratch.extent, values.extent);
  math::StoreVector(scratch.aim_point, values.aim_point);
  math::StoreVector(scratch.spawn_point, values.spawn_point);
  math::StoreVector(scratch.direction_jitter, values.direction_jitter);
  math::StoreVector(scratch.base_direction, values.base_direction);
  StorePoint(scratch.vector, values.vector);
  StorePoint(scratch.force_min, values.speed_jitter);
  StorePoint(scratch.acceleration_min, values.width);
  StorePoint(scratch.attraction_offset, values.target_offset);
  StorePoint(scratch.attraction_jitter, values.target_jitter);
  StorePair(scratch.speed_range, values.speed);
  StorePair(scratch.mass_range, values.first_radius);
  StorePair(scratch.gravity_scale_range, values.second_radius);
  math::StoreMatrix(scratch.aim_matrix, values.aim);
  scratch.sin_angle0 = values.sine[0];
  scratch.sin_angle1 = values.sine[1];
  scratch.cos_angle0 = values.cosine[0];
  scratch.cos_angle1 = values.cosine[1];
  scratch.ring_segments = values.ring_segments;
  scratch.ring_positions = values.ring_positions;
}

// Curve 0 samples 0 without reading the package, as the original's sampler
// does. A curve the package's recorded curve block lacks, or one without
// entries (the sampler's modulo divisor), samples 0 with a diagnostic.
std::int32_t LiveParticleHost::SampleCurve(std::uint8_t curve,
                                           std::int32_t distance) {
  if (curve == 0) {
    return 0;
  }
  const std::size_t bound = LiveEffectCurveCount();
  if (curve > bound || Effects_package->curveBlock->Size(curve - 1) == 0) {
    if (ShouldReport(refused_curves)) {
      VS::Core::LogWarn(
          "effects",
          "particle set: curve {} is outside the package's {} bound "
          "curves or empty; sampled as 0 ({} refusals)",
          curve, bound, refused_curves);
    }
    return 0;
  }
  // The promoted curve sampler (#276 row 38) through its Live entry.
  return VS::Effects::LiveSampleEffectCurve(curve, distance);
}

// The original stepped the animation and indexed the frame's texture
// unchecked; a texture table the package lacks, or a frame without texture
// info, leaves this tick undrawn here.
std::optional<ParticleTexture> LiveParticleHost::AdvanceTexture() {
  LiveParticleTextureAnimation& animation = LiveState()->texture_animation;
  const std::size_t table = animation.texture_index;
  const bool bound = Effects_package->textures != nullptr &&
                     table < kTextureTableCount &&
                     std::cmp_less(table, Effects_package->block1TableCount) &&
                     Effects_package->textureInfoTable[table] != nullptr;
  if (bound) {
    VS::Effects::LiveAdvanceParticleTextureAnimation(&animation);
  }
  if (!bound || animation.texture_info == nullptr) {
    if (ShouldReport(refused_textures)) {
      VS::Core::LogWarn(
          "effects",
          "particle set: texture table {} of {} has no frame; set "
          "drawn without quads ({} refusals)",
          table, Effects_package->block1TableCount, refused_textures);
    }
    return std::nullopt;
  }
  const LivePackageTexture& texture =
      Effects_package->textures[animation.texture_info->first_texture];
  return ParticleTexture{.clut = texture.clut,
                         .tpage = texture.tpage,
                         .u = texture.u,
                         .v = texture.v,
                         .w = texture.w,
                         .h = texture.h};
}

// decomp: particleRender.h:166-198 and particleLineRender.h:111-126 (addPrim
// at the segment's depth). The original wrote past the primitive buffer when
// it was full; a packet without room or outside the scene table is dropped
// here.
std::optional<std::pair<u_long*, u_long*>> LiveParticleHost::Reserve(
    std::int32_t depth, std::size_t words) {
  u_long* const entry =
      Render::LiveFrameOrderingEntry(Render::LiveOrderingBase::kScene, depth);
  u_long* const packet =
      entry != nullptr ? Render::ReserveLivePrimitiveWords(words) : nullptr;
  if (packet == nullptr) {
    if (ShouldReport(dropped_packets)) {
      VS::Core::LogWarn(
          "effects",
          "particle set: segment at depth {} has no ordering entry or "
          "buffer room; dropped ({} drops)",
          depth, dropped_packets);
    }
    return std::nullopt;
  }
  return std::pair{entry, packet};
}

void LiveParticleHost::Link(u_long* entry, u_long* packet, std::size_t words) {
  *entry = (*entry & kOrderingHighMask) |
           (LegacyGpu_Handle(packet) & kParticleLinkMask);
  VS::Render::FrameScratchpad().primitive_cursor = packet + words;
}

void LiveParticleHost::SubmitQuad(const ParticleQuad& quad,
                                  std::int32_t depth) {
  const auto reserved = Reserve(depth, kParticleQuadWords);
  if (!reserved) {
    return;
  }
  const auto [entry, packet] = *reserved;
  // The encoder keeps the high halfwords of the u2v2 and u3v3 words the
  // buffer held.
  const std::span<u_long, kParticleQuadWords> stored(packet,
                                                     kParticleQuadWords);
  std::array<std::uint32_t, kParticleQuadWords> words{};
  std::ranges::copy(stored, words.begin());
  EncodeParticleQuadPacket(quad, static_cast<std::uint32_t>(*entry), words);
  std::ranges::copy(words, stored.begin());
  Link(entry, packet, kParticleQuadWords);
}

void LiveParticleHost::SubmitLine(const ParticleLine& line,
                                  std::int32_t depth) {
  const auto reserved = Reserve(depth, kParticleLineWords);
  if (!reserved) {
    return;
  }
  const auto [entry, packet] = *reserved;
  // The original stepped the linked packet's second point (func_800CFAAC);
  // the step is applied before encoding, which the link does not read.
  ParticleLine stepped = line;
  stepped.screens[1] =
      VS::Effects::StepScreenPointToward(line.screens[1], line.screens[0]);
  std::ranges::copy(
      EncodeParticleLinePacket(stepped, static_cast<std::uint32_t>(*entry)),
      packet);
  Link(entry, packet, kParticleLineWords);
}

using ParticleInstance = bool (*)(ParticleCommand, std::int32_t,
                                  ParticleSetHost&);

int RunParticleSet(Effects_RendererSlot* slot, unsigned int command, int packed,
                   ParticleInstance instance) {
  if (slot == nullptr) {
    VS::Core::LogError("effects",
                       "particle set: renderer called without a slot; ignored");
    return 0;
  }
  LiveParticleHost host(*slot);
  return instance(static_cast<ParticleCommand>(command), packed, host) ? 1 : 0;
}
}  // namespace

// Each entry runs the instance in ParticleDispatch.cpp, which carries the
// decomp provenance.
int UpdateLivePlg045ParticleDispatch(Effects_RendererSlot* slot,
                                     unsigned int command, int packed) {
  return RunParticleSet(slot, command, packed, &Plg045::ParticleDispatch);
}

int UpdateLivePlg057ParticleDispatch(Effects_RendererSlot* slot,
                                     unsigned int command, int packed) {
  return RunParticleSet(slot, command, packed, &Plg057::ParticleDispatch);
}

int UpdateLivePlg065ParticleDispatch(Effects_RendererSlot* slot,
                                     unsigned int command, int packed) {
  return RunParticleSet(slot, command, packed, &Plg065::ParticleDispatch);
}

int UpdateLivePlg082ParticleDispatch(Effects_RendererSlot* slot,
                                     unsigned int command, int packed) {
  return RunParticleSet(slot, command, packed, &Plg082::ParticleDispatch);
}

int UpdateLivePlg087ParticleDispatch(Effects_RendererSlot* slot,
                                     unsigned int command, int packed) {
  return RunParticleSet(slot, command, packed, &Plg087::ParticleDispatch);
}

int UpdateLivePlg114ParticleDispatch(Effects_RendererSlot* slot,
                                     unsigned int command, int packed) {
  return RunParticleSet(slot, command, packed, &Plg114::ParticleDispatch);
}

int UpdateLivePlg114ParticleDispatch2(Effects_RendererSlot* slot,
                                      unsigned int command, int packed) {
  return RunParticleSet(slot, command, packed, &Plg114::ParticleDispatch2);
}

int UpdateLivePlg146ParticleDispatch(Effects_RendererSlot* slot,
                                     unsigned int command, int packed) {
  return RunParticleSet(slot, command, packed, &Plg146::ParticleDispatch);
}

int UpdateLivePlg147ParticleDispatch(Effects_RendererSlot* slot,
                                     unsigned int command, int packed) {
  return RunParticleSet(slot, command, packed, &Plg147::ParticleDispatch);
}

int UpdateLivePlg152ParticleDispatch(Effects_RendererSlot* slot,
                                     unsigned int command, int packed) {
  return RunParticleSet(slot, command, packed, &Plg152::ParticleDispatch);
}

int UpdateLivePlg153ParticleDispatch(Effects_RendererSlot* slot,
                                     unsigned int command, int packed) {
  return RunParticleSet(slot, command, packed, &Plg153::ParticleDispatch);
}

int UpdateLivePlg156ParticleDispatch(Effects_RendererSlot* slot,
                                     unsigned int command, int packed) {
  return RunParticleSet(slot, command, packed, &Plg156::ParticleDispatch);
}

int UpdateLivePlg168ParticleDispatch(Effects_RendererSlot* slot,
                                     unsigned int command, int packed) {
  return RunParticleSet(slot, command, packed, &Plg168::ParticleDispatch);
}

int UpdateLivePlg224ParticleDispatch(Effects_RendererSlot* slot,
                                     unsigned int command, int packed) {
  return RunParticleSet(slot, command, packed, &Plg224::ParticleDispatch);
}

int UpdateLivePlg255ParticleDispatch(Effects_RendererSlot* slot,
                                     unsigned int command, int packed) {
  return RunParticleSet(slot, command, packed, &Plg255::ParticleDispatch);
}

int UpdateLivePlg263ParticleDispatch(Effects_RendererSlot* slot,
                                     unsigned int command, int packed) {
  return RunParticleSet(slot, command, packed, &Plg263::ParticleDispatch);
}

int UpdateLivePlg265ParticleDispatch(Effects_RendererSlot* slot,
                                     unsigned int command, int packed) {
  return RunParticleSet(slot, command, packed, &Plg265::ParticleDispatch);
}

int UpdateLivePlg265ParticleDispatch2(Effects_RendererSlot* slot,
                                      unsigned int command, int packed) {
  return RunParticleSet(slot, command, packed, &Plg265::ParticleDispatch2);
}

int UpdateLivePlg266ParticleDispatch(Effects_RendererSlot* slot,
                                     unsigned int command, int packed) {
  return RunParticleSet(slot, command, packed, &Plg266::ParticleDispatch);
}

int UpdateLivePlg266ParticleDispatch2(Effects_RendererSlot* slot,
                                      unsigned int command, int packed) {
  return RunParticleSet(slot, command, packed, &Plg266::ParticleDispatch2);
}

int UpdateLivePlg300ParticleDispatch(Effects_RendererSlot* slot,
                                     unsigned int command, int packed) {
  return RunParticleSet(slot, command, packed, &Plg300::ParticleDispatch);
}

int UpdateLivePlg301ParticleDispatch(Effects_RendererSlot* slot,
                                     unsigned int command, int packed) {
  return RunParticleSet(slot, command, packed, &Plg301::ParticleDispatch);
}
}  // namespace VS::Effects::Templates
