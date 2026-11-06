// Hand-maintained (tools/port/owned.csv).
#include "Effects/Templates/SurfacesLive.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <type_traits>
#include <utility>

#include "Core/Log.hpp"
#include "Core/Math/InterpolationLive.hpp"
#include "Core/Math/MatrixLive.hpp"
#include "Core/Math/ScalarMath.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Effects/CurvesLive.hpp"
#include "Effects/PackageExtents.hpp"
#include "Effects/Templates/Grid.hpp"
#include "Effects/Templates/Mesh10.hpp"
#include "Effects/Templates/MirrorSurface.hpp"
#include "Effects/Templates/RingCore.hpp"
#include "Effects/Templates/RotatingSurface.hpp"
#include "Effects/Templates/Surface.hpp"
#include "Effects/Templates/SurfaceCore.hpp"
#include "Legacy/Psx.h"
#include "Render/SpritesLive.hpp"
#include "Render/UiPacketsLive.hpp"

extern "C" {
#include "Core/BiosRand.h"
#include "Core/Gte.h"
#include "Core/Scratch.h"
#include "Effects/Package.h"
#include "Effects/Particles.h"
#include "Effects/Plugins/Plg061.h"
#include "Effects/Plugins/Plg069.h"
#include "Effects/Plugins/Plg077.h"
#include "Effects/Plugins/Plg087.h"
#include "Effects/Plugins/Plg128.h"
#include "Effects/Plugins/Plg129.h"
#include "Effects/Plugins/Plg137.h"
#include "Effects/Plugins/Plg146.h"
#include "Effects/Plugins/Plg150.h"
#include "Effects/Plugins/Plg153.h"
#include "Effects/Plugins/Plg154.h"
#include "Effects/Plugins/Plg158.h"
#include "Effects/Plugins/Plg166.h"
#include "Effects/Plugins/Plg170.h"
#include "Effects/Plugins/Plg260.h"
#include "Effects/Plugins/Plg263.h"
#include "Effects/Plugins/Plg272.h"
#include "Effects/Plugins/Plg273.h"
#include "Effects/Plugins/Plg297.h"
#include "Effects/Plugins/Plg300.h"
#include "Effects/Runtime.h"
#include "Effects/Templates/TemplateState.h"
#include "Effects/Transforms.h"
#include "Render/Camera.h"
#include "Render/Frame.h"
#include "Render/LegacyPackets.h"
#include "Render/Render.h"
#include "Render/UiPrimitives.h"
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
constexpr std::uint32_t kOrderingHighMask = ~kRingLinkMask;
constexpr std::size_t kWordBytes = 4;
// vectorRanges[] and pairRanges[] rows of a particle definition the
// templates read.
constexpr std::size_t kDirectionRange = 0;
constexpr std::size_t kDirectionJitterRange = 1;
constexpr std::size_t kForceMinRange = 2;
constexpr std::size_t kForceMaxRange = 3;
constexpr std::size_t kAccelerationMinRange = 4;
constexpr std::size_t kAccelerationMaxRange = 5;
constexpr std::size_t kAttractionJitterRange = 7;
constexpr std::size_t kMassRange = 0;
constexpr std::size_t kGravityRange = 1;
constexpr std::size_t kSpeedRange = 2;
constexpr std::size_t kGridOriginRange = 3;
// The halfwords of a pair range holding the first and the second value's
// start.
constexpr std::size_t kFirstStart = 0;
constexpr std::size_t kSecondStart = 2;
// The second colour's curve ids: the minimum force range's start halfwords.
constexpr std::size_t kSecondColorBlue = 4;
// A face's three vertex indices, then its three texture offsets.
constexpr std::size_t kFaceBytes = kMesh10Corners * 2;

using LegacyProfile = SVECTOR[kSurfaceRows][kSurfaceColumns];
using LegacyMesh10Vertices = SVECTOR[kMesh10Vertices];
using LegacyMesh10Faces = u_char[kMesh10Faces][kFaceBytes];

static_assert(sizeof(Effects_ParticleDefinition) == kParticleDefinitionBytes);
static_assert(sizeof(Render_GridPacket) == kGridPacketWords * kWordBytes);
static_assert(sizeof(Render_Mesh10Packet) == kMesh10PacketWords * kWordBytes);
static_assert(sizeof(Render_SurfacePacket) == kRingPacketWords * kWordBytes);
static_assert(sizeof(Render_MirrorSurfacePacket) ==
              kRingPacketWords * kWordBytes);
static_assert(sizeof(Render_RotatingSurfacePacket) ==
              kRingPacketWords * kWordBytes);
static_assert(sizeof(LegacyProfile) == kSurfaceProfilePoints * sizeof(SVECTOR));
static_assert(std::extent_v<decltype(Effects_SurfaceState::factors)> ==
              kSurfaceRows);
static_assert(std::extent_v<decltype(Effects_RotatingSurfaceState::factors)> ==
              kSurfaceRows);
static_assert(std::extent_v<decltype(Effects_MirrorSurfaceState::factors)> ==
              kMirrorSurfaceRows);

std::uint64_t refused_curves = 0;
std::uint64_t refused_textures = 0;
std::uint64_t dropped_primitives = 0;

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
const Effects_ParticleDefinition* Definition(std::size_t index) {
  if (Effects_package == nullptr || Effects_package->block5Data == nullptr ||
      index >= LiveParticleDefinitionCount()) {
    return nullptr;
  }
  return &Effects_package->block5Data->definitions[index];
}

[[nodiscard]] Vec3 Wide(const VECTOR& value) {
  return {static_cast<std::int32_t>(value.vx),
          static_cast<std::int32_t>(value.vy),
          static_cast<std::int32_t>(value.vz)};
}

[[nodiscard]] RingVectorRange VectorRange(
    std::span<const std::int16_t, math::kVectorRangeHalfwords> ranges) {
  return {math::RangePair(ranges, 0), math::RangePair(ranges, 1),
          math::RangePair(ranges, 2)};
}

[[nodiscard]] std::array<math::ScalarSpan, 2> PlaneRange(
    std::span<const std::int16_t, math::kPlaneRangeHalfwords> ranges) {
  return {math::RangePair(ranges, 0), math::RangePair(ranges, 1)};
}

[[nodiscard]] SurfaceCurves TypedCurves(
    const Effects_ParticleDefinition& definition) {
  const std::span<const std::int16_t, math::kVectorRangeHalfwords> second =
      definition.vectorRanges[kForceMinRange];
  return {.position = definition.positionCurve,
          .extent = definition.extentCurve,
          .inner_extent = definition.innerExtentCurve,
          .direction = definition.directionCurve,
          .direction_jitter = definition.directionJitterCurve,
          .speed = definition.speedCurve,
          .force = definition.forceCurve,
          .acceleration = definition.accelerationCurve,
          .attraction_jitter = definition.attractionJitterCurve,
          .mass = definition.massCurve,
          .gravity = definition.gravityScaleCurve,
          .color = {definition.rCurve, definition.gCurve, definition.bCurve},
          .second_color = {second[kFirstStart], second[kSecondStart],
                           second[kSecondColorBlue]}};
}

[[nodiscard]] SurfaceDefinition TypedDefinition(
    const Effects_ParticleDefinition& definition) {
  const auto& vectors = definition.vectorRanges;
  const auto& pairs = definition.pairRanges;
  const std::span<const std::int16_t, math::kPlaneRangeHalfwords> mass =
      pairs[kMassRange];
  const std::span<const std::int16_t, math::kPlaneRangeHalfwords> origin =
      pairs[kGridOriginRange];
  return {.flags = definition.flags,
          .tick_event = definition.tickEvent,
          .texture_index = definition.textureIndex,
          .animation_id = definition.animationId,
          .transparency_curve =
              static_cast<std::uint8_t>(definition.transparencyCurve),
          .ring_segments = definition.ringSegments,
          .ring_positions = definition.ringPositions,
          .curves = TypedCurves(definition),
          .position = VectorRange(definition.positionRange),
          .extent = VectorRange(definition.extentRange),
          .inner_extent = math::RangePair(definition.innerExtentRange, 0),
          .direction = VectorRange(vectors[kDirectionRange]),
          .direction_jitter = VectorRange(vectors[kDirectionJitterRange]),
          .force_min = VectorRange(vectors[kForceMinRange]),
          .force_max = VectorRange(vectors[kForceMaxRange]),
          .acceleration_min = VectorRange(vectors[kAccelerationMinRange]),
          .acceleration_max = VectorRange(vectors[kAccelerationMaxRange]),
          .attraction_jitter = VectorRange(vectors[kAttractionJitterRange]),
          .speed = PlaneRange(pairs[kSpeedRange]),
          .mass = PlaneRange(mass),
          .gravity = PlaneRange(pairs[kGravityRange]),
          // The texture origin is the low byte of each halfword.
          .grid_u = static_cast<std::uint8_t>(origin[kFirstStart]),
          .grid_v = static_cast<std::uint8_t>(origin[kSecondStart]),
          .grid_width = mass[kFirstStart],
          .grid_height = mass[kSecondStart]};
}

// The services every template's host shares: the package, the scratch, the
// GTE, the texture animation and the frame. `name` labels diagnostics.
class LiveSurfaceServices {
 public:
  explicit LiveSurfaceServices(std::string_view name) : name_(name) {}

  [[nodiscard]] std::string_view Name() const { return name_; }
  [[nodiscard]] std::optional<SurfaceDefinition> CreateDefinition(
      std::uint8_t definition_index) const;
  [[nodiscard]] std::span<const std::uint8_t> FactorCurve(
      std::uint8_t curve) const;
  [[nodiscard]] std::optional<SurfaceTickInputs> BeginTick(
      std::uint8_t definition_index);
  [[nodiscard]] std::int32_t SampleCurve(std::int32_t curve,
                                         std::int32_t distance) const;
  static void PublishScratch(const SurfaceScratch& values);
  void BeginProjection(bool effect_view) const;
  [[nodiscard]] static RingProjection Project(RingPoint point);
  [[nodiscard]] std::optional<RingTexture> AdvanceTexture(
      Effects_ParticleTextureAnimation& animation) const;
  void Submit(std::span<const std::uint32_t> words, std::int32_t depth) const;
  [[nodiscard]] Vec3 TransformByActor(RingPoint point) const;
  void EmitParticleEvent(std::uint8_t event, Vec3 position) const;
  // A cleared state record of `bytes`, or null with a diagnostic.
  [[nodiscard]] void* AllocateState(std::size_t bytes) const;
  void StartAnimation(Effects_ParticleTextureAnimation& animation,
                      SurfaceAnimation source) const;

 private:
  std::string_view name_;
  const Effects_ParticleDefinition* definition_ = nullptr;
  Effects_EffectInstance* instance_ = nullptr;
};

std::optional<SurfaceDefinition> LiveSurfaceServices::CreateDefinition(
    std::uint8_t definition_index) const {
  const Effects_ParticleDefinition* definition = Definition(definition_index);
  if (definition == nullptr) {
    VS_LOG_WARN("effects",
                "{}: definition {} is outside the package's {} definitions; "
                "{} refused",
                name_, definition_index, LiveParticleDefinitionCount(), name_);
    return std::nullopt;
  }
  return TypedDefinition(*definition);
}

// decomp: surfaces.h:69-90, mirroredSurfaces.h:69-90 and
// rotatingSurfaces.h:78-99 (the row intensities). The original read the
// transparency curve unchecked, from index 0 rather than the sampler's 1;
// a curve the package lacks, or one without entries, leaves every row at
// full intensity here.
std::span<const std::uint8_t> LiveSurfaceServices::FactorCurve(
    std::uint8_t curve) const {
  const std::size_t bound = LiveEffectCurveCount();
  if (curve >= bound || Effects_package->curves[curve] == nullptr ||
      Effects_package->curveBlock->curveSizes[curve] == 0) {
    VS_LOG_WARN("effects",
                "{}: transparency curve {} is outside the package's {} bound "
                "curves or empty; rows drawn at full intensity",
                name_, curve, bound);
    return {};
  }
  return {
      Effects_package->curves[curve],
      static_cast<std::size_t>(Effects_package->curveBlock->curveSizes[curve])};
}

std::optional<SurfaceTickInputs> LiveSurfaceServices::BeginTick(
    std::uint8_t definition_index) {
  instance_ = Effects_currentInstance;
  if (instance_ == nullptr) {
    VS_LOG_WARN("effects", "{}: no current effect instance; {} stopped", name_,
                name_);
    return std::nullopt;
  }
  definition_ = Definition(definition_index);
  if (definition_ == nullptr) {
    VS_LOG_WARN("effects",
                "{}: the current package no longer holds definition {}; {} "
                "stopped",
                name_, definition_index, name_);
    return std::nullopt;
  }
  if (definition_->transformIndex >= kTransformCount) {
    VS_LOG_WARN("effects",
                "{}: definition {} names transform {} of {}; {} stopped", name_,
                definition_index, definition_->transformIndex, kTransformCount,
                name_);
    return std::nullopt;
  }
  // The original stored the flags before sampling anything.
  EmitterScratch().flags = definition_->flags;
  const MATRIX& world =
      instance_->transforms[definition_->transformIndex].world;
  // Effects_effectOrigin: the action request's position.
  const SVECTOR& origin = Effects_actionRequest.position;
  return SurfaceTickInputs{
      .definition = TypedDefinition(*definition_),
      .effect_origin = {origin.vx, origin.vy, origin.vz},
      .actor_translation = {static_cast<std::int32_t>(world.t[0]),
                            static_cast<std::int32_t>(world.t[1]),
                            static_cast<std::int32_t>(world.t[2])},
      .near_clip = Render_nearClip};
}

// Curve 0 samples 0 without reading the package, as the original's sampler
// does. A curve the package's recorded curve block lacks, one without
// entries (the sampler's modulo divisor) or a negative id (the mesh's second
// colour reads its ids from halfwords) samples 0 with a diagnostic.
std::int32_t LiveSurfaceServices::SampleCurve(std::int32_t curve,
                                              std::int32_t distance) const {
  if (curve == 0) {
    return 0;
  }
  const std::size_t bound = LiveEffectCurveCount();
  if (curve < 0 || std::cmp_greater(curve, bound) ||
      Effects_package->curveBlock->curveSizes[curve - 1] == 0) {
    if (ShouldReport(refused_curves)) {
      VS_LOG_WARN("effects",
                  "{}: curve {} is outside the package's {} bound curves or "
                  "empty; sampled as 0 ({} refusals)",
                  name_, curve, bound, refused_curves);
    }
    return 0;
  }
  // The promoted curve sampler (#276 row 38) through its Live entry.
  return VS::Effects::LiveSampleEffectCurve(curve, distance);
}

void LiveSurfaceServices::PublishScratch(const SurfaceScratch& values) {
  Effects_ParticleEmitterScratch& scratch = EmitterScratch();
  scratch.flags = values.flags;
  math::StoreVector(scratch.position, values.position);
  math::StoreVector(scratch.extent, values.extent);
  if (values.forces.has_value()) {
    math::StoreVector(scratch.forceMin, values.forces->force_min);
    math::StoreVector(scratch.forceMax, values.forces->force_max);
    math::StoreVector(scratch.accelerationMin, values.forces->acceleration_min);
    math::StoreVector(scratch.accelerationMax, values.forces->acceleration_max);
  }
  if (values.base_direction.has_value()) {
    math::StoreVector(scratch.baseDirection, *values.base_direction);
  }
  if (values.direction_jitter.has_value()) {
    math::StoreVector(scratch.directionJitter, *values.direction_jitter);
  }
  if (values.speed.has_value()) {
    scratch.speedRange[0] = values.speed->x;
    scratch.speedRange[1] = values.speed->y;
  }
  if (values.mass.has_value()) {
    scratch.massRange[0] = values.mass->x;
    scratch.massRange[1] = values.mass->y;
  }
  if (values.gravity.has_value()) {
    scratch.gravityScaleRange[0] = values.gravity->x;
    scratch.gravityScaleRange[1] = values.gravity->y;
  }
  if (values.attraction_jitter.has_value()) {
    math::StoreVector(scratch.attractionJitter, *values.attraction_jitter);
  }
  if (values.aim_angles.has_value()) {
    scratch.vector.vx = values.aim_angles->x;
    scratch.vector.vy = values.aim_angles->y;
    scratch.vector.vz = values.aim_angles->z;
  }
  if (values.aim.has_value()) {
    math::StoreTransform(scratch.aimMatrix, *values.aim);
  }
}

void LiveSurfaceServices::BeginProjection(bool effect_view) const {
  if (!effect_view) {
    Render::LoadLiveFrameViewMatrix();
    return;
  }
  const MATRIX& view = instance_->transforms[definition_->transformIndex].view;
  Core_Gte_SetRotMatrix(&view);
  Core_Gte_SetTransMatrix(&view);
}

RingProjection LiveSurfaceServices::Project(RingPoint point) {
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

// The original stepped the animation and indexed the frame's texture
// unchecked; a texture table the package lacks, or a frame without texture
// info, leaves this tick undrawn here.
std::optional<RingTexture> LiveSurfaceServices::AdvanceTexture(
    Effects_ParticleTextureAnimation& animation) const {
  const std::size_t table = animation.textureIndex;
  const bool bound = Effects_package->textures != nullptr &&
                     table < kTextureTableCount &&
                     std::cmp_less(table, Effects_package->block1TableCount) &&
                     Effects_package->textureInfoTable[table] != nullptr;
  if (bound) {
    // Cross-slice calls: retarget when those rows are promoted.
    Effects_AdvanceParticleTextureAnimation(&animation);
  }
  if (!bound || animation.textureInfo == nullptr) {
    if (ShouldReport(refused_textures)) {
      VS_LOG_WARN("effects",
                  "{}: texture table {} of {} has no frame; drawn without "
                  "primitives ({} refusals)",
                  name_, table, Effects_package->block1TableCount,
                  refused_textures);
    }
    return std::nullopt;
  }
  const Effects_PackageTexture& texture =
      Effects_package->textures[animation.textureInfo->dataIndex];
  return RingTexture{.clut = static_cast<std::uint16_t>(texture.clut),
                     .tpage = static_cast<std::uint16_t>(texture.tpage),
                     .u = texture.u,
                     .v = texture.v,
                     .w = texture.w,
                     .h = texture.h};
}

// decomp: grids.h:237-257, meshes10.h:208-233, surfaces.h:200-247,
// mirroredSurfaces.h:220-268 and rotatingSurfaces.h:236-286 (addPrim at the
// primitive's depth). The original wrote past the primitive buffer when it
// was full; a primitive without room or outside the scene table is dropped
// here.
void LiveSurfaceServices::Submit(std::span<const std::uint32_t> words,
                                 std::int32_t depth) const {
  u_long* const entry =
      Render::LiveFrameOrderingEntry(Render::LiveOrderingBase::kScene, depth);
  u_long* const packet = entry != nullptr
                             ? Render::ReserveLivePrimitiveWords(words.size())
                             : nullptr;
  if (packet == nullptr) {
    if (ShouldReport(dropped_primitives)) {
      VS_LOG_WARN("effects",
                  "{}: primitive at depth {} has no ordering entry or buffer "
                  "room; dropped ({} drops)",
                  name_, depth, dropped_primitives);
    }
    return;
  }
  std::ranges::copy(words, packet);
  packet[0] |= *entry & kRingLinkMask;
  *entry =
      (*entry & kOrderingHighMask) | (LegacyGpu_Handle(packet) & kRingLinkMask);
  Render_frameScratch.primitiveCursor = packet + words.size();
}

// decomp: meshes10.h:236-248 (the tick event through the transform's world
// matrix, by way of the scratch vector and aim point).
Vec3 LiveSurfaceServices::TransformByActor(RingPoint point) const {
  const MATRIX& world =
      instance_->transforms[definition_->transformIndex].world;
  Core_Gte_SetRotMatrix(&world);
  Core_Gte_SetTransMatrix(&world);
  Effects_ParticleEmitterScratch& scratch = EmitterScratch();
  scratch.vector.vx = point.x;
  scratch.vector.vy = point.y;
  scratch.vector.vz = point.z;
  Core_Gte_LoadV0(&scratch.vector);
  Core_Gte_Rtv0Tr();
  Core_Gte_StoreLvnl(&scratch.aimPoint);
  return Wide(scratch.aimPoint);
}

void LiveSurfaceServices::EmitParticleEvent(std::uint8_t event,
                                            Vec3 position) const {
  const std::size_t bound = LiveParticleDefinitionCount();
  if (event >= bound) {
    VS_LOG_WARN("effects",
                "{}: tick event {} is outside the package's {} definitions; "
                "event skipped",
                name_, event, bound);
    return;
  }
  // The original left the context's other fields as stack contents; they
  // start cleared here.
  Effects_PluginEffectState context{};
  math::StoreVector(context.position, position);
  // Cross-slice calls: retarget when those rows are promoted.
  Effects_EmitParticleEvent(instance_, event, 0, 0, &context);
}

void* LiveSurfaceServices::AllocateState(std::size_t bytes) const {
  void* state = Core::Memory::AllocateLiveFromHighEnd(bytes);
  if (state == nullptr) {
    VS_LOG_ERROR("effects", "{}: heap exhausted for state", name_);
  }
  return state;
}

// decomp: meshes10.h:44-50, surfaces.h:91-93, mirroredSurfaces.h:91-93 and
// rotatingSurfaces.h:100-102 (the animation a create starts).
void LiveSurfaceServices::StartAnimation(
    Effects_ParticleTextureAnimation& animation,
    SurfaceAnimation source) const {
  const int tables = Effects_package->block1TableCount;
  if (source.table >= tables ||
      std::cmp_greater_equal(source.table, kTextureTableCount) ||
      Effects_package->textureInfoTable[source.table] == nullptr) {
    VS_LOG_DEBUG("effects", "{}: create names unbound texture table {} of {}",
                 name_, source.table, tables);
  }
  // Clearing the record also cleared the texture animation, as
  // Effects_ClearParticleTextureState did.
  // Cross-slice calls: retarget when those rows are promoted.
  Effects_InitializeParticleTextureAnimation(&animation, source.table,
                                             source.program);
}

template <typename Legacy>
concept AnimatedRecord = requires(Legacy& live) { live.texture; };

template <typename Legacy>
concept TurningRecord = requires(Legacy& live) { live.angle; };

[[nodiscard]] GridState RecordOf(const Effects_GridState& live) {
  return {.lifetime = live.lifetime,
          .definition_index = live.effect,
          .u = live.u,
          .v = live.v,
          .u_scroll = live.uScroll,
          .v_scroll = live.vScroll,
          .tpage = static_cast<std::uint16_t>(live.tpage),
          .clut = static_cast<std::uint16_t>(live.clut),
          .x_spacing = live.xSpacing,
          .x_speed = live.xSpeed,
          .z_spacing = live.zSpacing,
          .z_speed = live.zSpeed,
          .x_phase = live.xPhase,
          .z_phase = live.zPhase,
          .age = live.age};
}

void StoreRecord(Effects_GridState& live, const GridState& state) {
  live.lifetime = state.lifetime;
  live.effect = state.definition_index;
  live.u = state.u;
  live.v = state.v;
  live.uScroll = state.u_scroll;
  live.vScroll = state.v_scroll;
  live.tpage = static_cast<std::int16_t>(state.tpage);
  live.clut = static_cast<std::int16_t>(state.clut);
  live.xSpacing = state.x_spacing;
  live.xSpeed = state.x_speed;
  live.zSpacing = state.z_spacing;
  live.zSpeed = state.z_speed;
  live.xPhase = state.x_phase;
  live.zPhase = state.z_phase;
  live.age = state.age;
}

[[nodiscard]] Mesh10State RecordOf(const Effects_Mesh10State& live) {
  return {.lifetime = live.lifetime,
          .definition_index = live.effect,
          .age = live.age,
          .rotation = {live.rotation.vx, live.rotation.vy, live.rotation.vz}};
}

void StoreRecord(Effects_Mesh10State& live, const Mesh10State& state) {
  live.lifetime = state.lifetime;
  live.effect = state.definition_index;
  live.age = state.age;
  live.rotation.vx = state.rotation.x;
  live.rotation.vy = state.rotation.y;
  live.rotation.vz = state.rotation.z;
}

// The three sheet records: the surface and the mirrored surface keep no
// angle, and each keeps as many row intensities as it has rows.
template <typename Legacy>
[[nodiscard]] SheetState RecordOf(const Legacy& live) {
  SheetState state{.lifetime = live.lifetime,
                   .definition_index = live.effect,
                   .radius = live.radius,
                   .radius_speed = live.radiusSpeed,
                   .height = live.height,
                   .height_speed = live.heightSpeed,
                   .age = live.age};
  if constexpr (TurningRecord<Legacy>) {
    state.angle = live.angle;
  }
  std::ranges::copy(live.factors, state.factors.begin());
  return state;
}

template <typename Legacy>
void StoreRecord(Legacy& live, const SheetState& state) {
  constexpr std::size_t kRows = std::extent_v<decltype(Legacy::factors)>;
  live.lifetime = state.lifetime;
  live.effect = state.definition_index;
  live.radius = state.radius;
  live.radiusSpeed = state.radius_speed;
  live.height = state.height;
  live.heightSpeed = state.height_speed;
  live.age = state.age;
  if constexpr (TurningRecord<Legacy>) {
    live.angle = state.angle;
  }
  std::ranges::copy(std::span(state.factors).template first<kRows>(),
                    std::span(live.factors).begin());
}

// One template's host over its C record (`Legacy`) in the slot; every
// service is LiveSurfaceServices'.
template <typename Record, typename Legacy>
class LiveSurfaceHost final : public SurfaceRecordHost<Record> {
 public:
  LiveSurfaceHost(Effects_RendererSlot& slot, std::string_view name)
      : slot_(slot), services_(name) {}

  // decomp: grids.h:40-62, meshes10.h:40-51, surfaces.h:56-94,
  // mirroredSurfaces.h:56-94 and rotatingSurfaces.h:65-103 (the create
  // command). The original allocated the state unchecked and left the
  // reserved fields as the heap held them.
  void Create(const Record& state,
              std::optional<SurfaceAnimation> animation) override {
    auto* live = static_cast<Legacy*>(services_.AllocateState(sizeof(Legacy)));
    if (live == nullptr) {
      return;
    }
    *live = Legacy{};
    slot_.rendererState = live;
    StoreState(state);
    if constexpr (AnimatedRecord<Legacy>) {
      if (animation.has_value()) {
        services_.StartAnimation(live->texture, *animation);
      }
    }
  }

  [[nodiscard]] std::optional<Record> State() const override {
    const Legacy* live = LiveState();
    if (live == nullptr) {
      return std::nullopt;
    }
    return RecordOf(*live);
  }

  void StoreState(const Record& state) override {
    Legacy* live = LiveState();
    if (live != nullptr) {
      StoreRecord(*live, state);
    }
  }

  [[nodiscard]] std::optional<SurfaceDefinition> CreateDefinition(
      std::uint8_t definition_index) override {
    return services_.CreateDefinition(definition_index);
  }
  [[nodiscard]] std::span<const std::uint8_t> FactorCurve(
      std::uint8_t curve) override {
    return services_.FactorCurve(curve);
  }
  [[nodiscard]] std::optional<SurfaceTickInputs> BeginTick(
      std::uint8_t definition_index) override {
    return services_.BeginTick(definition_index);
  }
  [[nodiscard]] std::int32_t SampleCurve(std::int32_t curve,
                                         std::int32_t distance) override {
    return services_.SampleCurve(curve, distance);
  }
  [[nodiscard]] std::int32_t Rand() override { return Core_Random_Rand(); }
  void PublishScratch(const SurfaceScratch& scratch) override {
    LiveSurfaceServices::PublishScratch(scratch);
  }
  void BeginProjection(bool effect_view) override {
    services_.BeginProjection(effect_view);
  }
  [[nodiscard]] RingProjection Project(RingPoint point) override {
    return LiveSurfaceServices::Project(point);
  }
  [[nodiscard]] std::optional<RingTexture> AdvanceTexture() override {
    if constexpr (AnimatedRecord<Legacy>) {
      Legacy* live = LiveState();
      if (live != nullptr) {
        return services_.AdvanceTexture(live->texture);
      }
    }
    return std::nullopt;
  }
  void Submit(std::span<const std::uint32_t> packet,
              std::int32_t depth) override {
    services_.Submit(packet, depth);
  }
  [[nodiscard]] Vec3 TransformByActor(RingPoint point) override {
    return services_.TransformByActor(point);
  }
  void EmitParticleEvent(std::uint8_t event, Vec3 position) override {
    services_.EmitParticleEvent(event, position);
  }

 private:
  [[nodiscard]] Legacy* LiveState() const {
    return static_cast<Legacy*>(slot_.rendererState);
  }

  Effects_RendererSlot& slot_;
  LiveSurfaceServices services_;
};

// Runs one renderer command of `instance` over the slot's `Legacy` record;
// `inputs` (a profile or a model) go between the packed parameter and the
// host.
template <typename Record, typename Legacy, typename Instance, typename... Args>
int RunSurfaceEntry(Effects_RendererSlot* slot, unsigned int command,
                    int packed, std::string_view name, Instance instance,
                    const Args&... inputs) {
  if (slot == nullptr) {
    VS_LOG_ERROR("effects", "{}: renderer called without a slot; ignored",
                 name);
    return 0;
  }
  LiveSurfaceHost<Record, Legacy> host(*slot, name);
  return instance(static_cast<SurfaceCommand>(command), packed, inputs..., host)
             ? 1
             : 0;
}

using SurfaceProfilePoints = std::array<RingPoint, kSurfaceProfilePoints>;

// An overlay's profile table as points, row by row.
[[nodiscard]] SurfaceProfilePoints ProfileOf(const LegacyProfile& rows) {
  SurfaceProfilePoints profile{};
  std::size_t next = 0;
  for (const auto& row : rows) {
    for (const SVECTOR& point : row) {
      profile.at(next++) = {point.vx, point.vy, point.vz};
    }
  }
  return profile;
}

// An overlay's mesh tables: each vertex's pad holds its colour, each face
// its three vertices and then its three texture offsets.
struct LiveMesh10Model {
  std::array<Mesh10Vertex, kMesh10Vertices> vertices{};
  std::array<Mesh10Face, kMesh10Faces> faces{};
};

[[nodiscard]] LiveMesh10Model ModelOf(const LegacyMesh10Vertices& vertices,
                                      const LegacyMesh10Faces& faces) {
  LiveMesh10Model model{};
  std::size_t next = 0;
  for (const SVECTOR& vertex : vertices) {
    model.vertices.at(next++) = {
        .point = {vertex.vx, vertex.vy, vertex.vz},
        .color = static_cast<std::uint16_t>(vertex.pad)};
  }
  next = 0;
  for (const auto& face : faces) {
    const std::span<const u_char, kFaceBytes> bytes = face;
    model.faces.at(next++) = {
        .vertices = {bytes[0], bytes[1], bytes[2]},
        .uvs = {bytes[kMesh10Corners], bytes[kMesh10Corners + 1],
                bytes[kMesh10Corners + 2]}};
  }
  return model;
}

using GridInstance = bool (*)(SurfaceCommand, std::int32_t, GridHost&);
using SheetInstance = bool (*)(SurfaceCommand, std::int32_t, SurfaceProfile,
                               SheetHost&);
using RotatingSurfaceInstance = bool (*)(SurfaceCommand, std::int32_t,
                                         SheetHost&);
using Mesh10Instance = bool (*)(SurfaceCommand, std::int32_t, Mesh10Model,
                                Mesh10Host&);

int RunGrid(Effects_RendererSlot* slot, unsigned int command, int packed,
            GridInstance instance) {
  return RunSurfaceEntry<GridState, Effects_GridState>(slot, command, packed,
                                                       "grid", instance);
}

int RunSurface(Effects_RendererSlot* slot, unsigned int command, int packed,
               const SurfaceProfilePoints& profile, SheetInstance instance) {
  return RunSurfaceEntry<SheetState, Effects_SurfaceState>(
      slot, command, packed, "surface", instance, SurfaceProfile(profile));
}

int RunMirrorSurface(Effects_RendererSlot* slot, unsigned int command,
                     int packed, const SurfaceProfilePoints& profile,
                     SheetInstance instance) {
  return RunSurfaceEntry<SheetState, Effects_MirrorSurfaceState>(
      slot, command, packed, "mirrored surface", instance,
      SurfaceProfile(profile));
}

int RunRotatingSurface(Effects_RendererSlot* slot, unsigned int command,
                       int packed, RotatingSurfaceInstance instance) {
  return RunSurfaceEntry<SheetState, Effects_RotatingSurfaceState>(
      slot, command, packed, "rotating surface", instance);
}

int RunMesh10(Effects_RendererSlot* slot, unsigned int command, int packed,
              const LiveMesh10Model& model, Mesh10Instance instance) {
  return RunSurfaceEntry<Mesh10State, Effects_Mesh10State>(
      slot, command, packed, "mesh", instance,
      Mesh10Model{.vertices = model.vertices, .faces = model.faces});
}
}  // namespace

// Each entry passes its overlay's profile or model to the instance in
// Grid.cpp, Mesh10.cpp, Surface.cpp, MirrorSurface.cpp or
// RotatingSurface.cpp, which carries the decomp provenance.
int UpdateLivePlg049RotatingSurface(Effects_RendererSlot* slot,
                                    unsigned int command, int packed) {
  return RunRotatingSurface(slot, command, packed, &Plg049::RotatingSurface);
}

int UpdateLivePlg053Grid(Effects_RendererSlot* slot, unsigned int command,
                         int packed) {
  return RunGrid(slot, command, packed, &Plg053::Grid);
}

int UpdateLivePlg061Grid(Effects_RendererSlot* slot, unsigned int command,
                         int packed) {
  return RunGrid(slot, command, packed, &Plg061::Grid);
}

int UpdateLivePlg061Surface(Effects_RendererSlot* slot, unsigned int command,
                            int packed) {
  return RunSurface(slot, command, packed,
                    ProfileOf(Effects_Plg061_surfaceVertices),
                    &Plg061::Surface);
}

int UpdateLivePlg065RotatingSurface(Effects_RendererSlot* slot,
                                    unsigned int command, int packed) {
  return RunRotatingSurface(slot, command, packed, &Plg065::RotatingSurface);
}

int UpdateLivePlg069Mesh10(Effects_RendererSlot* slot, unsigned int command,
                           int packed) {
  return RunMesh10(
      slot, command, packed,
      ModelOf(Effects_Plg069_mesh10Vertices, Effects_Plg069_mesh10Faces),
      &Plg069::Mesh10);
}

int UpdateLivePlg073Grid(Effects_RendererSlot* slot, unsigned int command,
                         int packed) {
  return RunGrid(slot, command, packed, &Plg073::Grid);
}

int UpdateLivePlg073RotatingSurface(Effects_RendererSlot* slot,
                                    unsigned int command, int packed) {
  return RunRotatingSurface(slot, command, packed, &Plg073::RotatingSurface);
}

int UpdateLivePlg077Grid(Effects_RendererSlot* slot, unsigned int command,
                         int packed) {
  return RunGrid(slot, command, packed, &Plg077::Grid);
}

int UpdateLivePlg077Surface(Effects_RendererSlot* slot, unsigned int command,
                            int packed) {
  return RunSurface(slot, command, packed,
                    ProfileOf(Effects_Plg077_surfaceVertices),
                    &Plg077::Surface);
}

int UpdateLivePlg077Mesh10(Effects_RendererSlot* slot, unsigned int command,
                           int packed) {
  return RunMesh10(
      slot, command, packed,
      ModelOf(Effects_Plg077_mesh10Vertices, Effects_Plg077_mesh10Faces),
      &Plg077::Mesh10);
}

int UpdateLivePlg081RotatingSurface(Effects_RendererSlot* slot,
                                    unsigned int command, int packed) {
  return RunRotatingSurface(slot, command, packed, &Plg081::RotatingSurface);
}

int UpdateLivePlg082Grid(Effects_RendererSlot* slot, unsigned int command,
                         int packed) {
  return RunGrid(slot, command, packed, &Plg082::Grid);
}

int UpdateLivePlg085Grid(Effects_RendererSlot* slot, unsigned int command,
                         int packed) {
  return RunGrid(slot, command, packed, &Plg085::Grid);
}

int UpdateLivePlg086Grid(Effects_RendererSlot* slot, unsigned int command,
                         int packed) {
  return RunGrid(slot, command, packed, &Plg086::Grid);
}

int UpdateLivePlg086RotatingSurface(Effects_RendererSlot* slot,
                                    unsigned int command, int packed) {
  return RunRotatingSurface(slot, command, packed, &Plg086::RotatingSurface);
}

int UpdateLivePlg087Grid(Effects_RendererSlot* slot, unsigned int command,
                         int packed) {
  return RunGrid(slot, command, packed, &Plg087::Grid);
}

int UpdateLivePlg087Surface(Effects_RendererSlot* slot, unsigned int command,
                            int packed) {
  return RunSurface(slot, command, packed,
                    ProfileOf(Effects_Plg087_surfaceVertices),
                    &Plg087::Surface);
}

int UpdateLivePlg128Grid(Effects_RendererSlot* slot, unsigned int command,
                         int packed) {
  return RunGrid(slot, command, packed, &Plg128::Grid);
}

int UpdateLivePlg128Surface(Effects_RendererSlot* slot, unsigned int command,
                            int packed) {
  return RunSurface(slot, command, packed,
                    ProfileOf(Effects_Plg128_surfaceVertices),
                    &Plg128::Surface);
}

int UpdateLivePlg128Surface2(Effects_RendererSlot* slot, unsigned int command,
                             int packed) {
  return RunSurface(slot, command, packed,
                    ProfileOf(Effects_Plg128_surfaceVertices2),
                    &Plg128::Surface2);
}

int UpdateLivePlg128RotatingSurface(Effects_RendererSlot* slot,
                                    unsigned int command, int packed) {
  return RunRotatingSurface(slot, command, packed, &Plg128::RotatingSurface);
}

int UpdateLivePlg129Surface(Effects_RendererSlot* slot, unsigned int command,
                            int packed) {
  return RunSurface(slot, command, packed,
                    ProfileOf(Effects_Plg129_surfaceVertices),
                    &Plg129::Surface);
}

int UpdateLivePlg129RotatingSurface(Effects_RendererSlot* slot,
                                    unsigned int command, int packed) {
  return RunRotatingSurface(slot, command, packed, &Plg129::RotatingSurface);
}

int UpdateLivePlg129Mesh10(Effects_RendererSlot* slot, unsigned int command,
                           int packed) {
  return RunMesh10(
      slot, command, packed,
      ModelOf(Effects_Plg129_mesh10Vertices, Effects_Plg129_mesh10Faces),
      &Plg129::Mesh10);
}

int UpdateLivePlg137Grid(Effects_RendererSlot* slot, unsigned int command,
                         int packed) {
  return RunGrid(slot, command, packed, &Plg137::Grid);
}

int UpdateLivePlg137Mesh10(Effects_RendererSlot* slot, unsigned int command,
                           int packed) {
  return RunMesh10(
      slot, command, packed,
      ModelOf(Effects_Plg137_mesh10Vertices, Effects_Plg137_mesh10Faces),
      &Plg137::Mesh10);
}

int UpdateLivePlg138Grid(Effects_RendererSlot* slot, unsigned int command,
                         int packed) {
  return RunGrid(slot, command, packed, &Plg138::Grid);
}

int UpdateLivePlg138RotatingSurface(Effects_RendererSlot* slot,
                                    unsigned int command, int packed) {
  return RunRotatingSurface(slot, command, packed, &Plg138::RotatingSurface);
}

int UpdateLivePlg139Grid(Effects_RendererSlot* slot, unsigned int command,
                         int packed) {
  return RunGrid(slot, command, packed, &Plg139::Grid);
}

int UpdateLivePlg142Grid(Effects_RendererSlot* slot, unsigned int command,
                         int packed) {
  return RunGrid(slot, command, packed, &Plg142::Grid);
}

int UpdateLivePlg142RotatingSurface(Effects_RendererSlot* slot,
                                    unsigned int command, int packed) {
  return RunRotatingSurface(slot, command, packed, &Plg142::RotatingSurface);
}

int UpdateLivePlg146MirrorSurface(Effects_RendererSlot* slot,
                                  unsigned int command, int packed) {
  return RunMirrorSurface(slot, command, packed,
                          ProfileOf(Effects_Plg146_mirrorSurfaceVertices),
                          &Plg146::MirrorSurface);
}

int UpdateLivePlg150Grid(Effects_RendererSlot* slot, unsigned int command,
                         int packed) {
  return RunGrid(slot, command, packed, &Plg150::Grid);
}

int UpdateLivePlg150Surface(Effects_RendererSlot* slot, unsigned int command,
                            int packed) {
  return RunSurface(slot, command, packed,
                    ProfileOf(Effects_Plg150_surfaceVertices),
                    &Plg150::Surface);
}

int UpdateLivePlg152Grid(Effects_RendererSlot* slot, unsigned int command,
                         int packed) {
  return RunGrid(slot, command, packed, &Plg152::Grid);
}

int UpdateLivePlg153Mesh10(Effects_RendererSlot* slot, unsigned int command,
                           int packed) {
  return RunMesh10(
      slot, command, packed,
      ModelOf(Effects_Plg153_mesh10Vertices, Effects_Plg153_mesh10Faces),
      &Plg153::Mesh10);
}

int UpdateLivePlg154Mesh10(Effects_RendererSlot* slot, unsigned int command,
                           int packed) {
  return RunMesh10(
      slot, command, packed,
      ModelOf(Effects_Plg154_mesh10Vertices, Effects_Plg154_mesh10Faces),
      &Plg154::Mesh10);
}

int UpdateLivePlg156Grid(Effects_RendererSlot* slot, unsigned int command,
                         int packed) {
  return RunGrid(slot, command, packed, &Plg156::Grid);
}

int UpdateLivePlg158Surface(Effects_RendererSlot* slot, unsigned int command,
                            int packed) {
  return RunSurface(slot, command, packed,
                    ProfileOf(Effects_Plg158_surfaceVertices),
                    &Plg158::Surface);
}

int UpdateLivePlg158MirrorSurface(Effects_RendererSlot* slot,
                                  unsigned int command, int packed) {
  return RunMirrorSurface(slot, command, packed,
                          ProfileOf(Effects_Plg158_mirrorSurfaceVertices),
                          &Plg158::MirrorSurface);
}

int UpdateLivePlg158RotatingSurface(Effects_RendererSlot* slot,
                                    unsigned int command, int packed) {
  return RunRotatingSurface(slot, command, packed, &Plg158::RotatingSurface);
}

int UpdateLivePlg161Grid(Effects_RendererSlot* slot, unsigned int command,
                         int packed) {
  return RunGrid(slot, command, packed, &Plg161::Grid);
}

int UpdateLivePlg161RotatingSurface(Effects_RendererSlot* slot,
                                    unsigned int command, int packed) {
  return RunRotatingSurface(slot, command, packed, &Plg161::RotatingSurface);
}

int UpdateLivePlg162RotatingSurface(Effects_RendererSlot* slot,
                                    unsigned int command, int packed) {
  return RunRotatingSurface(slot, command, packed, &Plg162::RotatingSurface);
}

int UpdateLivePlg165Grid(Effects_RendererSlot* slot, unsigned int command,
                         int packed) {
  return RunGrid(slot, command, packed, &Plg165::Grid);
}

int UpdateLivePlg165RotatingSurface(Effects_RendererSlot* slot,
                                    unsigned int command, int packed) {
  return RunRotatingSurface(slot, command, packed, &Plg165::RotatingSurface);
}

int UpdateLivePlg166MirrorSurface(Effects_RendererSlot* slot,
                                  unsigned int command, int packed) {
  return RunMirrorSurface(slot, command, packed,
                          ProfileOf(Effects_Plg166_mirrorSurfaceVertices),
                          &Plg166::MirrorSurface);
}

int UpdateLivePlg166RotatingSurface(Effects_RendererSlot* slot,
                                    unsigned int command, int packed) {
  return RunRotatingSurface(slot, command, packed, &Plg166::RotatingSurface);
}

int UpdateLivePlg167RotatingSurface(Effects_RendererSlot* slot,
                                    unsigned int command, int packed) {
  return RunRotatingSurface(slot, command, packed, &Plg167::RotatingSurface);
}

int UpdateLivePlg168Grid(Effects_RendererSlot* slot, unsigned int command,
                         int packed) {
  return RunGrid(slot, command, packed, &Plg168::Grid);
}

int UpdateLivePlg170Grid(Effects_RendererSlot* slot, unsigned int command,
                         int packed) {
  return RunGrid(slot, command, packed, &Plg170::Grid);
}

int UpdateLivePlg170Surface(Effects_RendererSlot* slot, unsigned int command,
                            int packed) {
  return RunSurface(slot, command, packed,
                    ProfileOf(Effects_Plg170_surfaceVertices),
                    &Plg170::Surface);
}

int UpdateLivePlg170MirrorSurface(Effects_RendererSlot* slot,
                                  unsigned int command, int packed) {
  return RunMirrorSurface(slot, command, packed,
                          ProfileOf(Effects_Plg170_mirrorSurfaceVertices),
                          &Plg170::MirrorSurface);
}

int UpdateLivePlg170RotatingSurface(Effects_RendererSlot* slot,
                                    unsigned int command, int packed) {
  return RunRotatingSurface(slot, command, packed, &Plg170::RotatingSurface);
}

int UpdateLivePlg236RotatingSurface(Effects_RendererSlot* slot,
                                    unsigned int command, int packed) {
  return RunRotatingSurface(slot, command, packed, &Plg236::RotatingSurface);
}

int UpdateLivePlg242Grid(Effects_RendererSlot* slot, unsigned int command,
                         int packed) {
  return RunGrid(slot, command, packed, &Plg242::Grid);
}

int UpdateLivePlg246RotatingSurface(Effects_RendererSlot* slot,
                                    unsigned int command, int packed) {
  return RunRotatingSurface(slot, command, packed, &Plg246::RotatingSurface);
}

int UpdateLivePlg250RotatingSurface(Effects_RendererSlot* slot,
                                    unsigned int command, int packed) {
  return RunRotatingSurface(slot, command, packed, &Plg250::RotatingSurface);
}

int UpdateLivePlg260Mesh10(Effects_RendererSlot* slot, unsigned int command,
                           int packed) {
  return RunMesh10(
      slot, command, packed,
      ModelOf(Effects_Plg260_mesh10Vertices, Effects_Plg260_mesh10Faces),
      &Plg260::Mesh10);
}

int UpdateLivePlg263Surface(Effects_RendererSlot* slot, unsigned int command,
                            int packed) {
  return RunSurface(slot, command, packed,
                    ProfileOf(Effects_Plg263_surfaceVertices),
                    &Plg263::Surface);
}

int UpdateLivePlg272Grid(Effects_RendererSlot* slot, unsigned int command,
                         int packed) {
  return RunGrid(slot, command, packed, &Plg272::Grid);
}

int UpdateLivePlg272Surface(Effects_RendererSlot* slot, unsigned int command,
                            int packed) {
  return RunSurface(slot, command, packed,
                    ProfileOf(Effects_Plg272_surfaceVertices),
                    &Plg272::Surface);
}

int UpdateLivePlg273MirrorSurface(Effects_RendererSlot* slot,
                                  unsigned int command, int packed) {
  return RunMirrorSurface(slot, command, packed,
                          ProfileOf(Effects_Plg273_mirrorSurfaceVertices),
                          &Plg273::MirrorSurface);
}

int UpdateLivePlg291Grid(Effects_RendererSlot* slot, unsigned int command,
                         int packed) {
  return RunGrid(slot, command, packed, &Plg291::Grid);
}

int UpdateLivePlg297Grid(Effects_RendererSlot* slot, unsigned int command,
                         int packed) {
  return RunGrid(slot, command, packed, &Plg297::Grid);
}

int UpdateLivePlg297MirrorSurface(Effects_RendererSlot* slot,
                                  unsigned int command, int packed) {
  return RunMirrorSurface(slot, command, packed,
                          ProfileOf(Effects_Plg297_mirrorSurfaceVertices),
                          &Plg297::MirrorSurface);
}

int UpdateLivePlg300MirrorSurface(Effects_RendererSlot* slot,
                                  unsigned int command, int packed) {
  return RunMirrorSurface(slot, command, packed,
                          ProfileOf(Effects_Plg300_mirrorSurfaceVertices),
                          &Plg300::MirrorSurface);
}
}  // namespace VS::Effects::Templates
