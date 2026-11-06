// Hand-maintained (tools/port/owned.csv).
#include "Effects/Templates/TrailsLive.hpp"

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
#include "Core/Math/ScalarMath.hpp"
#include "Core/RandomLive.hpp"
#include "Effects/CurvesLive.hpp"
#include "Effects/PackageExtents.hpp"
#include "Effects/Templates/RingCore.hpp"
#include "Effects/Templates/TrailControl.hpp"
#include "Effects/Templates/TrailCore.hpp"
#include "Effects/Templates/TrailInit.hpp"
#include "Effects/Templates/TrailRender.hpp"
#include "Effects/Templates/TrailTarget.hpp"
#include "Effects/Templates/TrailUpdate.hpp"
#include "Effects/TrailEmitterLive.hpp"
#include "Legacy/Psx.h"
#include "Render/SpritesLive.hpp"
#include "Render/UiPacketsLive.hpp"

extern "C" {
#include "Core/BiosRand.h"
#include "Core/Gte.h"
#include "Core/Scratch.h"
#include "Effects/Package.h"
#include "Effects/Particles.h"
#include "Effects/Plugins/Plg049.h"
#include "Effects/Plugins/Plg050.h"
#include "Effects/Plugins/Plg057.h"
#include "Effects/Plugins/Plg069.h"
#include "Effects/Plugins/Plg073.h"
#include "Effects/Plugins/Plg086.h"
#include "Effects/Plugins/Plg087.h"
#include "Effects/Plugins/Plg128.h"
#include "Effects/Plugins/Plg134.h"
#include "Effects/Plugins/Plg135.h"
#include "Effects/Plugins/Plg137.h"
#include "Effects/Plugins/Plg141.h"
#include "Effects/Plugins/Plg146.h"
#include "Effects/Plugins/Plg148.h"
#include "Effects/Plugins/Plg161.h"
#include "Effects/Plugins/Plg165.h"
#include "Effects/Plugins/Plg167.h"
#include "Effects/Plugins/Plg168.h"
#include "Effects/Plugins/Plg169.h"
#include "Effects/Plugins/Plg232.h"
#include "Effects/Plugins/Plg242.h"
#include "Effects/Plugins/Plg255.h"
#include "Effects/Plugins/Plg265.h"
#include "Effects/Plugins/Plg266.h"
#include "Effects/Plugins/Plg291.h"
#include "Effects/Runtime.h"
#include "Effects/Templates/TemplateState.h"
#include "Effects/Trails.h"
#include "Effects/Transforms.h"
#include "Render/Camera.h"
#include "Render/Frame.h"
#include "Render/Render.h"
#include "Render/UiPrimitives.h"
}

namespace VS::Effects::Templates {
namespace {
namespace math = Core::Math;
using math::Vec2;
using math::Vec3;

using InstanceTransforms = decltype(Effects_EffectInstance::transforms);
using TextureTables = decltype(Effects_EffectPackage::textureInfoTable);
constexpr std::size_t kTransformCount = std::extent_v<InstanceTransforms>;
constexpr std::size_t kTextureTableCount = std::extent_v<TextureTables>;
// vectorRanges[] and pairRanges[] rows of a particle definition the trails
// read (trails5.h:243-449, trails9.h:243-456).
constexpr std::size_t kDirectionRow = 0;
constexpr std::size_t kDirectionJitterRow = 1;
constexpr std::size_t kForceRow = 2;
constexpr std::size_t kAccelerationRow = 4;
constexpr std::size_t kAttractionOffsetRow = 6;
constexpr std::size_t kAttractionJitterRow = 7;
constexpr std::size_t kMassPair = 0;
constexpr std::size_t kGravityScalePair = 1;
constexpr std::size_t kSpeedPair = 2;
constexpr std::size_t kLifetimePair = 4;

// The PS1 layouts the records are copied through: a trail (Trails.h) and the
// emitter-scratch members the stages read and write (Particles.h).
constexpr std::size_t kTrailBytes = 0x58;
constexpr std::size_t kTrailStepCountAt = 0x20;
constexpr std::size_t kTrailCursorAt = 0x23;
constexpr std::size_t kTrailSamplesAt = 0x24;
constexpr std::size_t kTrailProgressAt = 0x4C;
constexpr std::size_t kTrailFirstStepAt = 0x54;
constexpr std::size_t kScratchAimMatrixAt = 0x64;
constexpr std::size_t kScratchAccelerationAt = 0x108;
constexpr std::size_t kScratchJitterAt = 0x144;

// The trail and its typed record share the PS1 layout's members; the record
// keeps the unused initial speed step in opaque54.
static_assert(sizeof(Effects_Trail) == kTrailBytes);
static_assert(offsetof(Effects_Trail, stepCount) == kTrailStepCountAt);
static_assert(offsetof(Effects_Trail, currentSplineSample) == kTrailCursorAt);
static_assert(offsetof(Effects_Trail, splineSamples) == kTrailSamplesAt);
static_assert(offsetof(Effects_Trail, splineProgress) == kTrailProgressAt);
static_assert(offsetof(Effects_Trail, opaque54) == kTrailFirstStepAt);
static_assert(std::extent_v<decltype(Effects_Trail::splinePoints)> ==
              kTrailSplinePoints);
static_assert(std::extent_v<decltype(Effects_Trail::splineSamples)> ==
              kTrailHistory);
// The nine-sample trail (TemplateState.h) shares that layout up to its longer
// history; it keeps the unused initial speed step in opaque74.
constexpr std::size_t kTrailNineBytes = 0x78;
constexpr std::size_t kTrailNineProgressAt = 0x6C;
constexpr std::size_t kTrailNineFirstStepAt = 0x74;
static_assert(sizeof(Effects_Trail9) == kTrailNineBytes);
static_assert(offsetof(Effects_Trail9, stepCount) == kTrailStepCountAt);
static_assert(offsetof(Effects_Trail9, currentSplineSample) == kTrailCursorAt);
static_assert(offsetof(Effects_Trail9, splineSamples) == kTrailSamplesAt);
static_assert(offsetof(Effects_Trail9, splineProgress) == kTrailNineProgressAt);
static_assert(offsetof(Effects_Trail9, opaque74) == kTrailNineFirstStepAt);
static_assert(std::extent_v<decltype(Effects_Trail9::splinePoints)> ==
              kTrailSplinePoints);
static_assert(std::extent_v<decltype(Effects_Trail9::splineSamples)> ==
              kTrailNineHistory);
using Scratch = Effects_ParticleEmitterScratch;
static_assert(offsetof(Scratch, aimMatrix) == kScratchAimMatrixAt);
static_assert(offsetof(Scratch, accelerationMin) == kScratchAccelerationAt);
static_assert(offsetof(Scratch, attractionJitter) == kScratchJitterAt);
static_assert(sizeof(Effects_ParticleDefinition) == kParticleDefinitionBytes);

std::uint64_t refused_curves = 0;
std::uint64_t refused_transforms = 0;
std::uint64_t undrawn_textures = 0;
std::uint64_t dropped_quads = 0;
std::uint64_t skipped_trails = 0;

// Repeated diagnostics are kept at the 1st, 2nd, 4th, 8th... occurrence.
[[nodiscard]] bool Reportable(std::uint64_t& occurrences) {
  ++occurrences;
  return std::has_single_bit(occurrences);
}

[[nodiscard]] std::string_view FaultName(TrailFault fault) {
  switch (fault) {
    case TrailFault::kHistoryOverflow:
      return "more samples than its history holds";
    case TrailFault::kCursorOutside:
      return "its newest sample outside the history";
    case TrailFault::kCollapseOutside:
      return "a collapse beyond the transparency samples";
    case TrailFault::kFadeUnderflow:
      return "a collapse shorter than its samples";
  }
  return "an unknown fault";
}

[[nodiscard]] TrailVectorRange VectorRows(
    std::span<const std::int16_t, math::kVectorRangeHalfwords> row) {
  return {math::RangePair(row, 0), math::RangePair(row, 1),
          math::RangePair(row, 2)};
}

[[nodiscard]] TrailPlaneRange PlaneRows(
    std::span<const std::int16_t, math::kPlaneRangeHalfwords> row) {
  return {math::RangePair(row, 0), math::RangePair(row, 1)};
}

[[nodiscard]] TrailDefinition TrailDefinitionOf(
    const Effects_ParticleDefinition& source) {
  const auto& vectors = source.vectorRanges;
  const auto& pairs = source.pairRanges;
  return {.curves = {.attraction_offset = source.attractionOffsetCurve,
                     .attraction_jitter = source.attractionJitterCurve,
                     .mass = source.massCurve,
                     .gravity_scale = source.gravityScaleCurve,
                     .speed = source.speedCurve,
                     .direction_jitter = source.directionJitterCurve,
                     .direction = source.directionCurve,
                     .force = source.forceCurve,
                     .acceleration = source.accelerationCurve,
                     .color = {source.rCurve, source.gCurve, source.bCurve}},
          .attraction_offset = VectorRows(vectors[kAttractionOffsetRow]),
          .attraction_jitter = VectorRows(vectors[kAttractionJitterRow]),
          .direction = VectorRows(vectors[kDirectionRow]),
          .direction_jitter = VectorRows(vectors[kDirectionJitterRow]),
          .force = VectorRows(vectors[kForceRow]),
          .acceleration = VectorRows(vectors[kAccelerationRow]),
          .mass = PlaneRows(pairs[kMassPair]),
          .gravity_scale = PlaneRows(pairs[kGravityScalePair]),
          .speed = PlaneRows(pairs[kSpeedPair]),
          .lifetime = PlaneRows(pairs[kLifetimePair]),
          .inner_extent = math::RangePair(source.innerExtentRange, 0)};
}

[[nodiscard]] Vec2 PairOf(std::span<const int, 2> pair) {
  return {pair[0], pair[1]};
}

void StorePair(std::span<int, 2> pair, Vec2 value) {
  pair[0] = value.x;
  pair[1] = value.y;
}

[[nodiscard]] TrailScratch TrailScratchOf(
    const Effects_ParticleEmitterScratch& source) {
  return {.flags = source.flags,
          .attraction_offset = math::VectorOf(source.attractionOffset),
          .attraction_jitter = math::VectorOf(source.attractionJitter),
          .mass = PairOf(source.massRange),
          .gravity_scale = PairOf(source.gravityScaleRange),
          .speed = PairOf(source.speedRange),
          .lifetime = PairOf(source.lifetimeRange),
          .direction_jitter = math::VectorOf(source.directionJitter),
          .base_direction = math::VectorOf(source.baseDirection),
          .force_min = math::VectorOf(source.forceMin),
          .aim_point = math::VectorOf(source.aimPoint),
          .spawn_point = math::VectorOf(source.spawnPoint),
          .aim_matrix = math::MatrixOf(source.aimMatrix),
          .acceleration_min = math::VectorOf(source.accelerationMin)};
}

// Every field the stages may have written goes back; the rest of the scratch
// block, and the aim matrix's translation, are left as they were.
void StoreTrailScratch(const TrailScratch& values,
                       Effects_ParticleEmitterScratch& target) {
  target.flags = values.flags;
  math::StoreVector(target.attractionOffset, values.attraction_offset);
  math::StoreVector(target.attractionJitter, values.attraction_jitter);
  StorePair(target.massRange, values.mass);
  StorePair(target.gravityScaleRange, values.gravity_scale);
  StorePair(target.speedRange, values.speed);
  StorePair(target.lifetimeRange, values.lifetime);
  math::StoreVector(target.directionJitter, values.direction_jitter);
  math::StoreVector(target.baseDirection, values.base_direction);
  math::StoreVector(target.forceMin, values.force_min);
  math::StoreVector(target.aimPoint, values.aim_point);
  math::StoreVector(target.spawnPoint, values.spawn_point);
  math::StoreMatrix(target.aimMatrix, values.aim_matrix);
  math::StoreVector(target.accelerationMin, values.acceleration_min);
}

[[nodiscard]] TrailPoint PointOf(const SVECTOR& vector) {
  return {vector.vx, vector.vy, vector.vz};
}

void StorePoint(SVECTOR& vector, TrailPoint point) {
  vector.vx = point.x;
  vector.vy = point.y;
  vector.vz = point.z;
}

// A PS1 trail layout's history length, the most trails its emitter holds and
// the member keeping the record's unused initial speed step.
template <typename Trail>
struct LegacyTrail;

template <>
struct LegacyTrail<Effects_Trail> {
  static constexpr std::size_t kHistory = kTrailHistory;
  static constexpr std::size_t kCapacity = kMaxTrailsPerEmitter;
  static constexpr auto kFirstStep = &Effects_Trail::opaque54;
};

template <>
struct LegacyTrail<Effects_Trail9> {
  static constexpr std::size_t kHistory = kTrailNineHistory;
  static constexpr std::size_t kCapacity = kMaxNineSampleTrailsPerEmitter;
  static constexpr auto kFirstStep = &Effects_Trail9::opaque74;
};

template <typename Trail>
using LegacyRecord = BasicTrailRecord<LegacyTrail<Trail>::kHistory>;

template <typename Trail>
[[nodiscard]] LegacyRecord<Trail> TrailRecordOf(const Trail& source) {
  LegacyRecord<Trail> record{
      .step_count = source.stepCount,
      .sample_count = source.splineSampleCount,
      .collapse = source.trailCollapseCounter,
      .current = source.currentSplineSample,
      .progress = source.splineProgress,
      .speed = source.speed,
      .first_step = source.*LegacyTrail<Trail>::kFirstStep};
  std::ranges::transform(source.splinePoints, record.points.begin(), PointOf);
  std::ranges::transform(source.splineSamples, record.samples.begin(), PointOf);
  return record;
}

template <typename Trail>
void StoreTrailRecord(const LegacyRecord<Trail>& record, Trail& target) {
  for (std::size_t index = 0; index < kTrailSplinePoints; ++index) {
    StorePoint(target.splinePoints[index], record.points.at(index));
  }
  for (std::size_t index = 0; index < LegacyTrail<Trail>::kHistory; ++index) {
    StorePoint(target.splineSamples[index], record.samples.at(index));
  }
  target.stepCount = record.step_count;
  target.splineSampleCount = record.sample_count;
  target.trailCollapseCounter = record.collapse;
  target.currentSplineSample = record.current;
  target.splineProgress = record.progress;
  target.speed = record.speed;
  target.*LegacyTrail<Trail>::kFirstStep = record.first_step;
}

// The emitter state members the host reads: its texture animation, the
// transparency samples its allocation carries past the structure
// (TrailEmitterLive.cpp) and its definition, for diagnostics.
struct EmitterContext {
  Effects_ParticleTextureAnimation* texture = nullptr;
  const std::uint8_t* transparency = nullptr;
  std::size_t definition_index = 0;
};

// One emitter state's trail buffer and curve distance with its context.
template <typename Trail>
struct EmitterView {
  Trail* trails = nullptr;
  std::size_t count = 0;
  std::int32_t distance = 0;
  EmitterContext context;
};

template <typename State>
[[nodiscard]] const std::uint8_t* TransparencyTail(const State& state) {
  return reinterpret_cast<const std::uint8_t*>(&state) +
         offsetof(State, sampledTransparencyCurve);
}

[[nodiscard]] EmitterView<Effects_Trail> ViewOf(Effects_TrailQuadSet& state) {
  return {.trails = state.trails,
          .count = state.trailcount,
          .distance = state.curveSampleDistance,
          .context = {.texture = &state.textureAnimation,
                      .transparency = TransparencyTail(state),
                      .definition_index = state.definitionIndex}};
}

[[nodiscard]] EmitterView<Effects_Trail9> ViewOf(Effects_Trail9State& state) {
  return {.trails = state.trails,
          .count = state.trailCount,
          .distance = state.curveSampleDistance,
          .context = {.texture = &state.texture,
                      .transparency = TransparencyTail(state),
                      .definition_index = state.effect}};
}

// BasicTrailHost over one emitter: its definition, the effect instance's
// transforms, the emitter state's texture animation and `History`
// transparency samples, the GTE, the BIOS generator and the current frame.
template <std::size_t History>
class LiveTrailHost final : public BasicTrailHost<History> {
 public:
  LiveTrailHost(const Effects_ParticleDefinition& definition,
                Effects_EffectInstance& instance, const EmitterContext& context)
      : definition_(definition), instance_(instance), context_(context) {}

  [[nodiscard]] std::int32_t SampleCurve(std::uint8_t curve,
                                         std::int32_t distance) override;
  [[nodiscard]] std::int32_t Rand() override { return Core_Random_Rand(); }
  [[nodiscard]] std::int32_t RandomRangeExclusive(std::int32_t a,
                                                  std::int32_t b) override {
    return Core::LiveRandomRangeExclusive(a, b);
  }
  [[nodiscard]] Vec3 RequestPosition() override {
    return math::VectorOf(Effects_actionRequest.position);
  }
  [[nodiscard]] std::optional<Vec3> AttractionTranslation() override;
  [[nodiscard]] bool BeginProjection(bool effect_view) override;
  [[nodiscard]] RingProjection Project(TrailPoint point) override;
  [[nodiscard]] std::optional<RingTexture> AdvanceTexture() override;
  [[nodiscard]] std::array<std::uint8_t, History> Transparency() override;
  [[nodiscard]] std::int32_t NearClip() override { return Render_nearClip; }
  void Submit(const RingQuad& quad, std::int32_t depth) override;
  void ReportSkippedTrail(std::size_t index, TrailFault fault) override;

 private:
  // The instance transform `index`, or null with a diagnostic naming `role`.
  [[nodiscard]] const Effects_TransformSlot* Transform(std::size_t index,
                                                       std::string_view role);
  [[nodiscard]] bool TextureTableBound(std::size_t table) const;

  const Effects_ParticleDefinition& definition_;
  Effects_EffectInstance& instance_;
  EmitterContext context_;
};

// Curve 0 is the original sampler's constant 0 and reads no package. A curve
// the package's recorded curve block lacks, or one without entries (the
// sampler's modulo divisor), also samples 0 here, with a diagnostic.
template <std::size_t History>
std::int32_t LiveTrailHost<History>::SampleCurve(std::uint8_t curve,
                                                 std::int32_t distance) {
  if (curve == 0) {
    return 0;
  }
  const std::size_t curves = LiveEffectCurveCount();
  const bool held = curve <= curves &&
                    Effects_package->curveBlock->curveSizes[curve - 1] != 0;
  if (held) {
    // The promoted curve sampler (#276 row 38) through its Live entry.
    return VS::Effects::LiveSampleEffectCurve(curve, distance);
  }
  if (Reportable(refused_curves)) {
    VS_LOG_WARN("effects",
                "trail: curve {} lies past the package's {} bound curves or "
                "has no entries; sampled as 0 ({} refusals)",
                curve, curves, refused_curves);
  }
  return 0;
}

template <std::size_t History>
const Effects_TransformSlot* LiveTrailHost<History>::Transform(
    std::size_t index, std::string_view role) {
  if (index < kTransformCount) {
    return &instance_.transforms[index];
  }
  if (Reportable(refused_transforms)) {
    VS_LOG_WARN("effects",
                "trail: {} transform {} lies past the instance's {} "
                "transforms; refused ({} refusals)",
                role, index, kTransformCount, refused_transforms);
  }
  return nullptr;
}

// decomp: trails5.h:257-260 and trails9.h:257-260. The original indexed the
// transform unchecked.
template <std::size_t History>
std::optional<Vec3> LiveTrailHost<History>::AttractionTranslation() {
  const Effects_TransformSlot* transform =
      Transform(definition_.attractionTransform, "attraction");
  if (transform == nullptr) {
    return std::nullopt;
  }
  const MATRIX& world = transform->world;
  return Vec3{static_cast<std::int32_t>(world.t[0]),
              static_cast<std::int32_t>(world.t[1]),
              static_cast<std::int32_t>(world.t[2])};
}

// decomp: trails5.h:499-505 and trails9.h:506-512 (the effect transform's
// view, or the frame's view matrix).
template <std::size_t History>
bool LiveTrailHost<History>::BeginProjection(bool effect_view) {
  if (!effect_view) {
    Render::LoadLiveFrameViewMatrix();
    return true;
  }
  const Effects_TransformSlot* transform =
      Transform(definition_.transformIndex, "view");
  if (transform == nullptr) {
    return false;
  }
  Core_Gte_SetRotMatrix(&transform->view);
  Core_Gte_SetTransMatrix(&transform->view);
  return true;
}

template <std::size_t History>
RingProjection LiveTrailHost<History>::Project(TrailPoint point) {
  SVECTOR sample{};
  StorePoint(sample, point);
  Core_Gte_LoadV0(&sample);
  Core_Gte_Rtps();
  std::array<Core_GteWord, 2> stored{};
  Core_Gte_StoreSxy2(&stored[0]);
  Core_Gte_StoreSz3Otz(&stored[1]);
  RingProjection projection;
  projection.screen = static_cast<std::uint32_t>(stored[0]);
  projection.depth = static_cast<std::int32_t>(stored[1]);
  return projection;
}

template <std::size_t History>
bool LiveTrailHost<History>::TextureTableBound(std::size_t table) const {
  const Effects_EffectPackage& package = *Effects_package;
  if (package.textures == nullptr || table >= kTextureTableCount) {
    return false;
  }
  return std::cmp_less(table, package.block1TableCount) &&
         package.textureInfoTable[table] != nullptr;
}

// decomp: trails5.h:507-509 and trails9.h:514-516. The original stepped the
// animation and read the frame's texture unchecked; a table the package
// lacks, or a frame without texture info, leaves the trails undrawn this tick.
template <std::size_t History>
std::optional<RingTexture> LiveTrailHost<History>::AdvanceTexture() {
  Effects_ParticleTextureAnimation& animation = *context_.texture;
  const std::size_t table = animation.textureIndex;
  const Effects_PackageTextureInfo* frame = nullptr;
  if (TextureTableBound(table)) {
    // Cross-slice calls: retarget when those rows are promoted.
    Effects_AdvanceParticleTextureAnimation(&animation);
    frame = animation.textureInfo;
  }
  if (frame == nullptr) {
    if (Reportable(undrawn_textures)) {
      VS_LOG_WARN("effects",
                  "trail: texture table {} of {} gives no frame; trails left "
                  "undrawn ({} ticks)",
                  table, Effects_package->block1TableCount, undrawn_textures);
    }
    return std::nullopt;
  }
  const Effects_PackageTexture& source =
      Effects_package->textures[frame->dataIndex];
  RingTexture texture;
  texture.clut = static_cast<std::uint16_t>(source.clut);
  texture.tpage = static_cast<std::uint16_t>(source.tpage);
  texture.u = source.u;
  texture.v = source.v;
  texture.w = source.w;
  texture.h = source.h;
  return texture;
}

template <std::size_t History>
std::array<std::uint8_t, History> LiveTrailHost<History>::Transparency() {
  std::array<std::uint8_t, History> samples{};
  const std::span<const std::uint8_t> tail(context_.transparency,
                                           samples.size());
  std::ranges::copy(tail, samples.begin());
  return samples;
}

// decomp: trails5.h:597-610 and 646-659, trails9.h:604-617 and 653-666
// (addPrim at the quad's depth). The original wrote past the primitive buffer
// when it was full and indexed the ordering table unchecked; such a quad is
// dropped here.
template <std::size_t History>
void LiveTrailHost<History>::Submit(const RingQuad& quad, std::int32_t depth) {
  u_long* const bucket =
      Render::LiveFrameOrderingEntry(Render::LiveOrderingBase::kScene, depth);
  u_long* const words =
      bucket == nullptr ? nullptr
                        : Render::ReserveLivePrimitiveWords(kRingPacketWords);
  if (words == nullptr) {
    if (Reportable(dropped_quads)) {
      VS_LOG_WARN("effects",
                  "trail: no ordering entry or primitive room for a quad at "
                  "depth {}; dropped ({} drops)",
                  depth, dropped_quads);
    }
    return;
  }
  const std::uint32_t previous = static_cast<std::uint32_t>(*bucket);
  const auto packet = EncodeRingPacket(quad, previous);
  std::ranges::copy(packet, words);
  const std::uint32_t handle = LegacyGpu_Handle(words);
  *bucket = (previous & ~kRingLinkMask) | (handle & kRingLinkMask);
  Render_frameScratch.primitiveCursor = words + packet.size();
}

template <std::size_t History>
void LiveTrailHost<History>::ReportSkippedTrail(std::size_t index,
                                                TrailFault fault) {
  if (Reportable(skipped_trails)) {
    VS_LOG_WARN("effects",
                "trail: trail {} of definition {} has {}; left undrawn ({} "
                "skips)",
                index, context_.definition_index, FaultName(fault),
                skipped_trails);
  }
}

// The emitter's trail buffer, or nothing when it cannot hold its count.
template <typename Trail>
[[nodiscard]] std::optional<std::span<Trail>> HeldTrails(
    const EmitterView<Trail>& view) {
  constexpr std::size_t kCapacity = LegacyTrail<Trail>::kCapacity;
  if (view.count > kCapacity || (view.count != 0 && view.trails == nullptr)) {
    VS_LOG_WARN("effects",
                "trail: definition {} has {} trails in a buffer of {}; stage "
                "skipped",
                view.context.definition_index, view.count,
                view.trails == nullptr ? 0 : kCapacity);
    return std::nullopt;
  }
  if (view.count == 0) {
    return std::span<Trail>{};
  }
  return std::span(view.trails, view.count);
}

template <typename Trail>
void RunStage(BasicTrailStage<LegacyTrail<Trail>::kHistory> stage,
              const Effects_ParticleDefinition& definition,
              Effects_ParticleEmitterScratch& scratch,
              Effects_EffectInstance& instance,
              const EmitterView<Trail>& view) {
  const std::optional<std::span<Trail>> trails = HeldTrails(view);
  if (stage == nullptr || !trails) {
    return;
  }
  std::array<LegacyRecord<Trail>, LegacyTrail<Trail>::kCapacity> records{};
  std::ranges::transform(*trails, records.begin(), TrailRecordOf<Trail>);
  TrailScratch values = TrailScratchOf(scratch);
  LiveTrailHost<LegacyTrail<Trail>::kHistory> host(definition, instance,
                                                   view.context);
  stage(TrailDefinitionOf(definition), values,
        std::span(records).first(trails->size()), view.distance, host);
  StoreTrailScratch(values, scratch);
  for (std::size_t index = 0; index < trails->size(); ++index) {
    StoreTrailRecord(records.at(index), (*trails)[index]);
  }
}

// The trails9.h overlays' stage instances are byte-identical (TrailTarget.cpp
// ... TrailRender.cpp carry each one's decomp provenance).
const TrailNineStages& NineStages() {
  static constexpr TrailNineStages kStages{&TrailNineTarget, &TrailNineControl,
                                           &TrailNineInit, &TrailNineUpdate,
                                           &TrailNineRender};
  return kStages;
}
}  // namespace

void RunLiveTrailStage(TrailStage stage,
                       const Effects_ParticleDefinition& definition,
                       Effects_ParticleEmitterScratch& scratch,
                       Effects_EffectInstance& instance,
                       Effects_TrailQuadSet& state) {
  RunStage(stage, definition, scratch, instance, ViewOf(state));
}

void RunLiveTrailNineStage(TrailNineStage stage,
                           const Effects_ParticleDefinition& definition,
                           Effects_ParticleEmitterScratch& scratch,
                           Effects_EffectInstance& instance,
                           Effects_Trail9State& state) {
  RunStage(stage, definition, scratch, instance, ViewOf(state));
}

// Each entry is its overlay's VS_TRAIL_DISPATCH_FUNCTION instance
// (trails5.h:677): the emitter lifecycle (TrailEmitter.cpp) with the
// overlay's own emission cursor (VS_TRAIL_CORNER_STATE) and its five stage
// instances (TrailTarget.cpp ... TrailRender.cpp, which carry the decomp
// provenance). PLG057 shares its first cursor with the nine-sample trails, so
// its five-sample trails use the second.
int UpdateLivePlg057Trail(Effects_RendererSlot* slot, unsigned int command,
                          int packed) {
  static constexpr TrailStages kStages{
      &Plg057::TrailTarget, &Plg057::TrailControl, &Plg057::TrailInit,
      &Plg057::TrailUpdate, &Plg057::TrailRender};
  return UpdateLiveTrailEmitter(
      slot, command, packed,
      {"PLG057", &Effects_Plg057_trailEmissionCounter2, &kStages});
}

int UpdateLivePlg073Trail(Effects_RendererSlot* slot, unsigned int command,
                          int packed) {
  static constexpr TrailStages kStages{
      &Plg073::TrailTarget, &Plg073::TrailControl, &Plg073::TrailInit,
      &Plg073::TrailUpdate, &Plg073::TrailRender};
  return UpdateLiveTrailEmitter(
      slot, command, packed,
      {"PLG073", &Effects_Plg073_trailEmissionCounter, &kStages});
}

int UpdateLivePlg086Trail(Effects_RendererSlot* slot, unsigned int command,
                          int packed) {
  static constexpr TrailStages kStages{
      &Plg086::TrailTarget, &Plg086::TrailControl, &Plg086::TrailInit,
      &Plg086::TrailUpdate, &Plg086::TrailRender};
  return UpdateLiveTrailEmitter(
      slot, command, packed,
      {"PLG086", &Effects_Plg086_trailEmissionCounter, &kStages});
}

int UpdateLivePlg087Trail(Effects_RendererSlot* slot, unsigned int command,
                          int packed) {
  static constexpr TrailStages kStages{
      &Plg087::TrailTarget, &Plg087::TrailControl, &Plg087::TrailInit,
      &Plg087::TrailUpdate, &Plg087::TrailRender};
  return UpdateLiveTrailEmitter(
      slot, command, packed,
      {"PLG087", &Effects_Plg087_trailEmissionCounter, &kStages});
}

int UpdateLivePlg128Trail(Effects_RendererSlot* slot, unsigned int command,
                          int packed) {
  static constexpr TrailStages kStages{
      &Plg128::TrailTarget, &Plg128::TrailControl, &Plg128::TrailInit,
      &Plg128::TrailUpdate, &Plg128::TrailRender};
  return UpdateLiveTrailEmitter(
      slot, command, packed,
      {"PLG128", &Effects_Plg128_trailEmissionCounter, &kStages});
}

int UpdateLivePlg134Trail(Effects_RendererSlot* slot, unsigned int command,
                          int packed) {
  static constexpr TrailStages kStages{
      &Plg134::TrailTarget, &Plg134::TrailControl, &Plg134::TrailInit,
      &Plg134::TrailUpdate, &Plg134::TrailRender};
  return UpdateLiveTrailEmitter(
      slot, command, packed,
      {"PLG134", &Effects_Plg134_trailEmissionCounter, &kStages});
}

int UpdateLivePlg137Trail(Effects_RendererSlot* slot, unsigned int command,
                          int packed) {
  static constexpr TrailStages kStages{
      &Plg137::TrailTarget, &Plg137::TrailControl, &Plg137::TrailInit,
      &Plg137::TrailUpdate, &Plg137::TrailRender};
  return UpdateLiveTrailEmitter(
      slot, command, packed,
      {"PLG137", &Effects_Plg137_trailEmissionCounter, &kStages});
}

int UpdateLivePlg141Trail(Effects_RendererSlot* slot, unsigned int command,
                          int packed) {
  static constexpr TrailStages kStages{
      &Plg141::TrailTarget, &Plg141::TrailControl, &Plg141::TrailInit,
      &Plg141::TrailUpdate, &Plg141::TrailRender};
  return UpdateLiveTrailEmitter(
      slot, command, packed,
      {"PLG141", &Effects_Plg141_trailEmissionCounter, &kStages});
}

int UpdateLivePlg146Trail(Effects_RendererSlot* slot, unsigned int command,
                          int packed) {
  static constexpr TrailStages kStages{
      &Plg146::TrailTarget, &Plg146::TrailControl, &Plg146::TrailInit,
      &Plg146::TrailUpdate, &Plg146::TrailRender};
  return UpdateLiveTrailEmitter(
      slot, command, packed,
      {"PLG146", &Effects_Plg146_trailEmissionCounter, &kStages});
}

int UpdateLivePlg148Trail(Effects_RendererSlot* slot, unsigned int command,
                          int packed) {
  static constexpr TrailStages kStages{
      &Plg148::TrailTarget, &Plg148::TrailControl, &Plg148::TrailInit,
      &Plg148::TrailUpdate, &Plg148::TrailRender};
  return UpdateLiveTrailEmitter(
      slot, command, packed,
      {"PLG148", &Effects_Plg148_trailEmissionCounter, &kStages});
}

int UpdateLivePlg242Trail(Effects_RendererSlot* slot, unsigned int command,
                          int packed) {
  static constexpr TrailStages kStages{
      &Plg242::TrailTarget, &Plg242::TrailControl, &Plg242::TrailInit,
      &Plg242::TrailUpdate, &Plg242::TrailRender};
  return UpdateLiveTrailEmitter(
      slot, command, packed,
      {"PLG242", &Effects_Plg242_trailEmissionCounter, &kStages});
}

int UpdateLivePlg255Trail(Effects_RendererSlot* slot, unsigned int command,
                          int packed) {
  static constexpr TrailStages kStages{
      &Plg255::TrailTarget, &Plg255::TrailControl, &Plg255::TrailInit,
      &Plg255::TrailUpdate, &Plg255::TrailRender};
  return UpdateLiveTrailEmitter(
      slot, command, packed,
      {"PLG255", &Effects_Plg255_trailEmissionCounter, &kStages});
}

int UpdateLivePlg265Trail(Effects_RendererSlot* slot, unsigned int command,
                          int packed) {
  static constexpr TrailStages kStages{
      &Plg265::TrailTarget, &Plg265::TrailControl, &Plg265::TrailInit,
      &Plg265::TrailUpdate, &Plg265::TrailRender};
  return UpdateLiveTrailEmitter(
      slot, command, packed,
      {"PLG265", &Effects_Plg265_trailEmissionCounter, &kStages});
}

int UpdateLivePlg266Trail(Effects_RendererSlot* slot, unsigned int command,
                          int packed) {
  static constexpr TrailStages kStages{
      &Plg266::TrailTarget, &Plg266::TrailControl, &Plg266::TrailInit,
      &Plg266::TrailUpdate, &Plg266::TrailRender};
  return UpdateLiveTrailEmitter(
      slot, command, packed,
      {"PLG266", &Effects_Plg266_trailEmissionCounter, &kStages});
}

int UpdateLivePlg291Trail(Effects_RendererSlot* slot, unsigned int command,
                          int packed) {
  static constexpr TrailStages kStages{
      &Plg291::TrailTarget, &Plg291::TrailControl, &Plg291::TrailInit,
      &Plg291::TrailUpdate, &Plg291::TrailRender};
  return UpdateLiveTrailEmitter(
      slot, command, packed,
      {"PLG291", &Effects_Plg291_trailEmissionCounter, &kStages});
}

// Each nine-sample entry is its overlay's VS_TRAIL_DISPATCH_FUNCTION instance
// (trails9.h:685): the emitter lifecycle (TrailEmitter.cpp) with the overlay's
// own emission cursor (VS_TRAIL_CORNER_STATE; PLG057's first) and the shared
// nine-sample stages.
int UpdateLivePlg049TrailNine(Effects_RendererSlot* slot, unsigned int command,
                              int packed) {
  return UpdateLiveTrailNineEmitter(
      slot, command, packed,
      {"PLG049", &Effects_Plg049_trailEmissionCounter, &NineStages()});
}

int UpdateLivePlg050TrailNine(Effects_RendererSlot* slot, unsigned int command,
                              int packed) {
  return UpdateLiveTrailNineEmitter(
      slot, command, packed,
      {"PLG050", &Effects_Plg050_trailEmissionCounter, &NineStages()});
}

int UpdateLivePlg057TrailNine(Effects_RendererSlot* slot, unsigned int command,
                              int packed) {
  return UpdateLiveTrailNineEmitter(
      slot, command, packed,
      {"PLG057", &Effects_Plg057_trailEmissionCounter, &NineStages()});
}

int UpdateLivePlg069TrailNine(Effects_RendererSlot* slot, unsigned int command,
                              int packed) {
  return UpdateLiveTrailNineEmitter(
      slot, command, packed,
      {"PLG069", &Effects_Plg069_trailEmissionCounter, &NineStages()});
}

int UpdateLivePlg135TrailNine(Effects_RendererSlot* slot, unsigned int command,
                              int packed) {
  return UpdateLiveTrailNineEmitter(
      slot, command, packed,
      {"PLG135", &Effects_Plg135_trailEmissionCounter, &NineStages()});
}

int UpdateLivePlg161TrailNine(Effects_RendererSlot* slot, unsigned int command,
                              int packed) {
  return UpdateLiveTrailNineEmitter(
      slot, command, packed,
      {"PLG161", &Effects_Plg161_trailEmissionCounter, &NineStages()});
}

int UpdateLivePlg165TrailNine(Effects_RendererSlot* slot, unsigned int command,
                              int packed) {
  return UpdateLiveTrailNineEmitter(
      slot, command, packed,
      {"PLG165", &Effects_Plg165_trailEmissionCounter, &NineStages()});
}

int UpdateLivePlg167TrailNine(Effects_RendererSlot* slot, unsigned int command,
                              int packed) {
  return UpdateLiveTrailNineEmitter(
      slot, command, packed,
      {"PLG167", &Effects_Plg167_trailEmissionCounter, &NineStages()});
}

int UpdateLivePlg168TrailNine(Effects_RendererSlot* slot, unsigned int command,
                              int packed) {
  return UpdateLiveTrailNineEmitter(
      slot, command, packed,
      {"PLG168", &Effects_Plg168_trailEmissionCounter, &NineStages()});
}

int UpdateLivePlg169TrailNine(Effects_RendererSlot* slot, unsigned int command,
                              int packed) {
  return UpdateLiveTrailNineEmitter(
      slot, command, packed,
      {"PLG169", &Effects_Plg169_trailEmissionCounter, &NineStages()});
}

int UpdateLivePlg232TrailNine(Effects_RendererSlot* slot, unsigned int command,
                              int packed) {
  return UpdateLiveTrailNineEmitter(
      slot, command, packed,
      {"PLG232", &Effects_Plg232_trailEmissionCounter, &NineStages()});
}
}  // namespace VS::Effects::Templates
