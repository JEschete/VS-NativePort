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

#include "Core/Log.hpp"
#include "Core/Math/InterpolationLive.hpp"
#include "Core/Math/MatrixLive.hpp"
#include "Core/Math/ScalarMath.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Core/RandomLive.hpp"
#include "Effects/CurvesLive.hpp"
#include "Effects/PackageExtents.hpp"
#include "Effects/Templates/ParticleCore.hpp"
#include "Effects/Templates/ParticleDispatch.hpp"
#include "Legacy/Psx.h"
#include "Render/SpritesLive.hpp"
#include "Render/UiPacketsLive.hpp"

extern "C" {
#include "Core/BiosRand.h"
#include "Core/Gte.h"
#include "Core/Scratch.h"
#include "Effects/Package.h"
#include "Effects/Particles.h"
#include "Effects/Runtime.h"
#include "Effects/Transforms.h"
#include "Render/Camera.h"
#include "Render/Frame.h"
#include "Render/LegacyPackets.h"
#include "Render/Render.h"
#include "Render/UiPrimitives.h"

// The overlays' emission cursors (Effects/Plugins/Plg*.h); PLG152's origin
// shares PLG082's address.
extern int Effects_Plg045_particleEmissionCounter;
extern int Effects_Plg057_particleEmissionCounter;
extern int Effects_Plg065_particleEmissionCounter;
extern int Effects_Plg082_particleEmissionCounter;
extern int Effects_Plg087_particleEmissionCounter;
extern int Effects_Plg114_particleEmissionCounter;
extern int Effects_Plg114_particleEmissionCounter2;
extern int Effects_Plg146_particleEmissionCounter;
extern int Effects_Plg147_particleEmissionCounter;
extern int Effects_Plg153_particleEmissionCounter;
extern int Effects_Plg156_particleEmissionCounter;
extern int Effects_Plg168_particleEmissionCounter;
extern int Effects_Plg224_particleEmissionCounter;
extern int Effects_Plg255_particleEmissionCounter;
extern int Effects_Plg263_particleEmissionCounter;
extern int Effects_Plg265_particleEmissionCounter;
extern int Effects_Plg265_particleEmissionCounter2;
extern int Effects_Plg266_particleEmissionCounter;
extern int Effects_Plg266_particleEmissionCounter2;
extern int Effects_Plg300_particleEmissionCounter;
extern int Effects_Plg301_particleEmissionCounter;
}

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

static_assert(sizeof(Effects_ParticleDefinition) == kParticleDefinitionBytes);
static_assert(sizeof(Render_ParticleQuadPacket) ==
              kParticleQuadWords * sizeof(std::uint32_t));
static_assert(sizeof(Render_ParticleLinePacket) ==
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

Effects_ParticleEmitterScratch& EmitterScratch() {
  return *reinterpret_cast<Effects_ParticleEmitterScratch*>(
      reinterpret_cast<std::uintptr_t>(Core_ScratchBase()) +
      kEmitterScratchOffset);
}

// The current package's definition `index`, or null when its recorded
// definition block does not hold it.
const Effects_ParticleDefinition* PackageDefinition(std::size_t index) {
  if (Effects_package == nullptr || Effects_package->block5Data == nullptr ||
      index >= LiveParticleDefinitionCount()) {
    return nullptr;
  }
  return &Effects_package->block5Data->definitions[index];
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
    const Effects_ParticleDefinition& definition) {
  return {.position = definition.positionCurve,
          .extent = definition.extentCurve,
          .direction = definition.directionCurve,
          .direction_jitter = definition.directionJitterCurve,
          .speed = definition.speedCurve,
          .force = definition.forceCurve,
          .acceleration = definition.accelerationCurve,
          .attraction_offset = definition.attractionOffsetCurve,
          .attraction_jitter = definition.attractionJitterCurve,
          .mass = definition.massCurve,
          .gravity_scale = definition.gravityScaleCurve,
          .interval = definition.intervalCurve,
          .inner_extent = definition.innerExtentCurve,
          .color = {definition.rCurve, definition.gCurve, definition.bCurve}};
}

[[nodiscard]] ParticleSetDefinition TypedDefinition(
    const Effects_ParticleDefinition& definition) {
  const auto& vectors = definition.vectorRanges;
  const auto& pairs = definition.pairRanges;
  return {.flags = definition.flags,
          .blend = static_cast<std::uint8_t>(definition.transparencyCurve &
                                             kBlendMask),
          .curves = TypedCurves(definition),
          .position = VectorRange(definition.positionRange),
          .extent = VectorRange(definition.extentRange),
          .inner_extent = math::RangePair(definition.innerExtentRange, 0),
          .interval = math::RangePair(definition.intervalRange, 0),
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
          .ring_segments = definition.ringSegments,
          .ring_positions = definition.ringPositions};
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
int& CounterOf(ParticleEmissionCounter counter) {
  using enum ParticleEmissionCounter;
  switch (counter) {
    case kPlg045:
      return Effects_Plg045_particleEmissionCounter;
    case kPlg057:
      return Effects_Plg057_particleEmissionCounter;
    case kPlg065:
      return Effects_Plg065_particleEmissionCounter;
    case kPlg082:
    case kPlg152:
      return Effects_Plg082_particleEmissionCounter;
    case kPlg087:
      return Effects_Plg087_particleEmissionCounter;
    case kPlg114:
      return Effects_Plg114_particleEmissionCounter;
    case kPlg114Second:
      return Effects_Plg114_particleEmissionCounter2;
    case kPlg146:
      return Effects_Plg146_particleEmissionCounter;
    case kPlg147:
      return Effects_Plg147_particleEmissionCounter;
    case kPlg153:
      return Effects_Plg153_particleEmissionCounter;
    case kPlg156:
      return Effects_Plg156_particleEmissionCounter;
    case kPlg168:
      return Effects_Plg168_particleEmissionCounter;
    case kPlg224:
      return Effects_Plg224_particleEmissionCounter;
    case kPlg255:
      return Effects_Plg255_particleEmissionCounter;
    case kPlg263:
      return Effects_Plg263_particleEmissionCounter;
    case kPlg265:
      return Effects_Plg265_particleEmissionCounter;
    case kPlg265Second:
      return Effects_Plg265_particleEmissionCounter2;
    case kPlg266:
      return Effects_Plg266_particleEmissionCounter;
    case kPlg266Second:
      return Effects_Plg266_particleEmissionCounter2;
    case kPlg300:
      return Effects_Plg300_particleEmissionCounter;
    case kPlg301:
    default:
      return Effects_Plg301_particleEmissionCounter;
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
    const Effects_ParticleQuadSet* live = LiveState();
    if (live == nullptr) {
      return std::nullopt;
    }
    return ParticleSetState{.lifetime = live->opaque0,
                            .definition_index = live->definitionIndex,
                            .count = live->count,
                            .cooldown = live->opaque8,
                            .age = live->age};
  }

  void StoreState(const ParticleSetState& state) override {
    Effects_ParticleQuadSet* live = LiveState();
    if (live == nullptr) {
      return;
    }
    live->opaque0 = state.lifetime;
    live->definitionIndex = state.definition_index;
    live->count = state.count;
    live->opaque8 = state.cooldown;
    live->age = state.age;
  }

  void Release() override {
    Effects_ParticleQuadSet* live = LiveState();
    if (live == nullptr || live->controlPoints == nullptr) {
      return;
    }
    Core::Memory::FreeLiveFromHighEnd(live->controlPoints);
    live->controlPoints = nullptr;
  }

  [[nodiscard]] std::optional<ParticleTickInputs> BeginTick(
      std::uint8_t definition_index) override;
  [[nodiscard]] ParticleSetPoints LoadPoints() const override;
  void StorePoints(const ParticleSetPoints& points) override;
  [[nodiscard]] ParticleSetScratch LoadScratch() const override;
  void PublishScratch(const ParticleSetScratch& scratch) override;
  [[nodiscard]] std::int32_t SampleCurve(std::uint8_t curve,
                                         std::int32_t distance) override;

  [[nodiscard]] std::int32_t Rand() override { return Core_Random_Rand(); }

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
        instance_->transforms[definition_->transformIndex].view;
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
  [[nodiscard]] Effects_ParticleQuadSet* LiveState() const {
    return static_cast<Effects_ParticleQuadSet*>(slot_.rendererState);
  }

  // The control point records the set's buffer holds, at most
  // kParticleSetCapacity ribbons of them.
  [[nodiscard]] std::span<SVECTOR> ControlPoints() const;
  // Room for `words` words linked at `depth` of the scene table, or nothing.
  [[nodiscard]] std::optional<std::pair<u_long*, u_long*>> Reserve(
      std::int32_t depth, std::size_t words);
  void Link(u_long* entry, u_long* packet, std::size_t words);

  Effects_RendererSlot& slot_;
  const Effects_ParticleDefinition* definition_ = nullptr;
  Effects_EffectInstance* instance_ = nullptr;
};

std::optional<ParticleSetDefinition> LiveParticleHost::Definition(
    std::uint8_t index) {
  const Effects_ParticleDefinition* definition = PackageDefinition(index);
  if (definition == nullptr) {
    VS_LOG_WARN("effects",
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
  const Effects_ParticleDefinition* definition =
      PackageDefinition(state.definition_index);
  if (definition == nullptr) {
    return;
  }
  const bool textured = texturing == ParticleTexturing::kTextured;
  // The untextured sets allocate only the record's prefix.
  const std::size_t bytes =
      textured ? sizeof(Effects_ParticleQuadSet)
               : offsetof(Effects_ParticleQuadSet, textureAnimation);
  void* storage = Core::Memory::AllocateLiveFromHighEnd(bytes);
  if (storage == nullptr) {
    VS_LOG_ERROR("effects", "particle set: heap exhausted for state");
    slot_.rendererState = nullptr;
    return;
  }
  std::memset(storage, 0, bytes);
  auto* live = static_cast<Effects_ParticleQuadSet*>(storage);
  slot_.rendererState = live;
  StoreState(state);
  if (capacity > 0) {
    live->controlPoints =
        static_cast<SVECTOR*>(Core::Memory::AllocateLiveFromHighEnd(
            static_cast<std::size_t>(capacity) * kParticleControlPointBytes));
    if (live->controlPoints == nullptr) {
      VS_LOG_ERROR("effects",
                   "particle set: heap exhausted for {} ribbons; set refused",
                   capacity);
      Core::Memory::FreeLiveFromHighEnd(live);
      slot_.rendererState = nullptr;
      return;
    }
  }
  if (textured) {
    // Clearing the record also cleared the texture animation, as
    // Effects_ClearParticleTextureState did.
    // Cross-slice call: retarget when that row is promoted.
    Effects_InitializeParticleTextureAnimation(&live->textureAnimation,
                                               definition->textureIndex,
                                               definition->animationId);
  }
}

std::optional<ParticleTickInputs> LiveParticleHost::BeginTick(
    std::uint8_t definition_index) {
  instance_ = Effects_currentInstance;
  if (instance_ == nullptr) {
    VS_LOG_WARN("effects",
                "particle set: no current effect instance; set stopped");
    return std::nullopt;
  }
  definition_ = PackageDefinition(definition_index);
  if (definition_ == nullptr) {
    VS_LOG_WARN("effects",
                "particle set: the current package no longer holds "
                "definition {}; set stopped",
                definition_index);
    return std::nullopt;
  }
  if (definition_->transformIndex >= kTransformCount) {
    VS_LOG_WARN("effects",
                "particle set: definition {} names transform {} of {}; set "
                "stopped",
                definition_index, definition_->transformIndex, kTransformCount);
    return std::nullopt;
  }
  const bool targets_transform =
      (definition_->flags & kParticleTargetMask) ==
      static_cast<std::uint32_t>(ParticleTargetAnchor::kTransform);
  if (targets_transform &&
      definition_->attractionTransform >= kTransformCount) {
    VS_LOG_WARN("effects",
                "particle set: definition {} targets transform {} of {}; set "
                "stopped",
                definition_index, definition_->attractionTransform,
                kTransformCount);
    return std::nullopt;
  }
  const math::Transform3 world = math::TransformOf(
      instance_->transforms[definition_->transformIndex].world);
  const Vec3 target_translation =
      targets_transform
          ? math::TransformOf(
                instance_->transforms[definition_->attractionTransform].world)
                .translation
          : Vec3{};
  // Effects_effectOrigin: the action request's position.
  return ParticleTickInputs{
      .definition = TypedDefinition(*definition_),
      .effect_origin = math::VectorOf(Effects_actionRequest.position),
      .world = world,
      .target_translation = target_translation,
      .near_clip = Render_nearClip};
}

std::span<SVECTOR> LiveParticleHost::ControlPoints() const {
  const Effects_ParticleQuadSet* live = LiveState();
  if (live == nullptr || live->controlPoints == nullptr) {
    return {};
  }
  const auto bytes = Core::Memory::LivePayloadCapacity(live->controlPoints);
  if (!bytes) {
    return {};
  }
  const std::size_t ribbons =
      std::min(*bytes / kParticleControlPointBytes, kParticleSetCapacity);
  return {live->controlPoints, ribbons * kParticleControlPoints};
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
  const Effects_ParticleEmitterScratch& scratch = EmitterScratch();
  return {.flags = scratch.flags,
          .position = math::VectorOf(scratch.position),
          .extent = math::VectorOf(scratch.extent),
          .aim_point = math::VectorOf(scratch.aimPoint),
          .spawn_point = math::VectorOf(scratch.spawnPoint),
          .direction_jitter = math::VectorOf(scratch.directionJitter),
          .base_direction = math::VectorOf(scratch.baseDirection),
          .vector = PointOf(scratch.vector),
          .speed_jitter = PointOf(scratch.forceMin),
          .width = PointOf(scratch.accelerationMin),
          .target_offset = PointOf(scratch.attractionOffset),
          .target_jitter = PointOf(scratch.attractionJitter),
          .speed = PairOf(scratch.speedRange),
          .first_radius = PairOf(scratch.massRange),
          .second_radius = PairOf(scratch.gravityScaleRange),
          .aim = math::MatrixOf(scratch.aimMatrix),
          .sine = {scratch.sinAngle0, scratch.sinAngle1},
          .cosine = {scratch.cosAngle0, scratch.cosAngle1},
          .ring_segments = scratch.ringSegments,
          .ring_positions = scratch.ringPositions};
}

void LiveParticleHost::PublishScratch(const ParticleSetScratch& values) {
  Effects_ParticleEmitterScratch& scratch = EmitterScratch();
  scratch.flags = values.flags;
  math::StoreVector(scratch.position, values.position);
  math::StoreVector(scratch.extent, values.extent);
  math::StoreVector(scratch.aimPoint, values.aim_point);
  math::StoreVector(scratch.spawnPoint, values.spawn_point);
  math::StoreVector(scratch.directionJitter, values.direction_jitter);
  math::StoreVector(scratch.baseDirection, values.base_direction);
  StorePoint(scratch.vector, values.vector);
  StorePoint(scratch.forceMin, values.speed_jitter);
  StorePoint(scratch.accelerationMin, values.width);
  StorePoint(scratch.attractionOffset, values.target_offset);
  StorePoint(scratch.attractionJitter, values.target_jitter);
  StorePair(scratch.speedRange, values.speed);
  StorePair(scratch.massRange, values.first_radius);
  StorePair(scratch.gravityScaleRange, values.second_radius);
  math::StoreMatrix(scratch.aimMatrix, values.aim);
  scratch.sinAngle0 = values.sine[0];
  scratch.sinAngle1 = values.sine[1];
  scratch.cosAngle0 = values.cosine[0];
  scratch.cosAngle1 = values.cosine[1];
  scratch.ringSegments = values.ring_segments;
  scratch.ringPositions = values.ring_positions;
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
  if (curve > bound ||
      Effects_package->curveBlock->curveSizes[curve - 1] == 0) {
    if (ShouldReport(refused_curves)) {
      VS_LOG_WARN("effects",
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
  Effects_ParticleTextureAnimation& animation = LiveState()->textureAnimation;
  const std::size_t table = animation.textureIndex;
  const bool bound = Effects_package->textures != nullptr &&
                     table < kTextureTableCount &&
                     std::cmp_less(table, Effects_package->block1TableCount) &&
                     Effects_package->textureInfoTable[table] != nullptr;
  if (bound) {
    // Cross-slice call: retarget when that row is promoted.
    Effects_AdvanceParticleTextureAnimation(&animation);
  }
  if (!bound || animation.textureInfo == nullptr) {
    if (ShouldReport(refused_textures)) {
      VS_LOG_WARN("effects",
                  "particle set: texture table {} of {} has no frame; set "
                  "drawn without quads ({} refusals)",
                  table, Effects_package->block1TableCount, refused_textures);
    }
    return std::nullopt;
  }
  const Effects_PackageTexture& texture =
      Effects_package->textures[animation.textureInfo->dataIndex];
  return ParticleTexture{.clut = static_cast<std::uint16_t>(texture.clut),
                         .tpage = static_cast<std::uint16_t>(texture.tpage),
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
      VS_LOG_WARN("effects",
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
  Render_frameScratch.primitiveCursor = packet + words;
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
  std::ranges::copy(
      EncodeParticleLinePacket(line, static_cast<std::uint32_t>(*entry)),
      packet);
  Link(entry, packet, kParticleLineWords);
  // Cross-slice call: retarget when that row is promoted.
  Effects_StepParticleEndpointTowardStart(
      reinterpret_cast<Effects_ParticleEndpointView*>(packet));
}

using ParticleInstance = bool (*)(ParticleCommand, std::int32_t,
                                  ParticleSetHost&);

int RunParticleSet(Effects_RendererSlot* slot, unsigned int command, int packed,
                   ParticleInstance instance) {
  if (slot == nullptr) {
    VS_LOG_ERROR("effects",
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
