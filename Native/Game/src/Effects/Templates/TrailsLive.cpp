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

#include "Core/Gte.h"
#include "Core/Log.hpp"
#include "Core/Math/InterpolationLive.hpp"
#include "Core/Math/ScalarMath.hpp"
#include "Core/RandomLive.hpp"
#include "Core/Scratch.hpp"
#include "Effects/CurvesLive.hpp"
#include "Effects/Package.hpp"
#include "Effects/PackageExtents.hpp"
#include "Effects/ParticleTextureAnimationLive.hpp"
#include "Effects/Particles.hpp"
#include "Effects/Plugins/Plg049.hpp"
#include "Effects/Plugins/Plg050.hpp"
#include "Effects/Plugins/Plg057.hpp"
#include "Effects/Plugins/Plg069.hpp"
#include "Effects/Plugins/Plg073.hpp"
#include "Effects/Plugins/Plg086.hpp"
#include "Effects/Plugins/Plg087.hpp"
#include "Effects/Plugins/Plg128.hpp"
#include "Effects/Plugins/Plg134.hpp"
#include "Effects/Plugins/Plg135.hpp"
#include "Effects/Plugins/Plg137.hpp"
#include "Effects/Plugins/Plg141.hpp"
#include "Effects/Plugins/Plg146.hpp"
#include "Effects/Plugins/Plg148.hpp"
#include "Effects/Plugins/Plg161.hpp"
#include "Effects/Plugins/Plg165.hpp"
#include "Effects/Plugins/Plg167.hpp"
#include "Effects/Plugins/Plg168.hpp"
#include "Effects/Plugins/Plg169.hpp"
#include "Effects/Plugins/Plg232.hpp"
#include "Effects/Plugins/Plg242.hpp"
#include "Effects/Plugins/Plg255.hpp"
#include "Effects/Plugins/Plg265.hpp"
#include "Effects/Plugins/Plg266.hpp"
#include "Effects/Plugins/Plg291.hpp"
#include "Effects/Runtime.h"
#include "Effects/Templates/RingCore.hpp"
#include "Effects/Templates/TemplateLiveState.hpp"
#include "Effects/Templates/TrailControl.hpp"
#include "Effects/Templates/TrailCore.hpp"
#include "Effects/Templates/TrailInit.hpp"
#include "Effects/Templates/TrailRender.hpp"
#include "Effects/Templates/TrailTarget.hpp"
#include "Effects/Templates/TrailUpdate.hpp"
#include "Effects/TrailEmitterLive.hpp"
#include "Effects/Trails.hpp"
#include "Effects/TransformsLiveState.hpp"
#include "Legacy/Psx.h"
#include "Render/Camera.hpp"
#include "Render/Frame.hpp"
#include "Render/Render.hpp"
#include "Render/SpritesLive.hpp"
#include "Render/UiPacketsLive.hpp"
#include "Render/UiPrimitives.hpp"

namespace VS::Effects::Templates {
namespace {
namespace math = Core::Math;
using math::Vec2;
using math::Vec3;

using InstanceTransforms = decltype(Effects_EffectInstance::transforms);
using TextureTables = decltype(LiveEffectPackage::texture_tables);
constexpr std::size_t kTransformCount = std::extent_v<InstanceTransforms>;
constexpr std::size_t kTextureTableCount = std::tuple_size_v<TextureTables>;
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
// keeps the unused initial speed step in first_step.
static_assert(sizeof(LiveTrail) == kTrailBytes);
static_assert(offsetof(LiveTrail, step_count) == kTrailStepCountAt);
static_assert(offsetof(LiveTrail, current_spline_sample) == kTrailCursorAt);
static_assert(offsetof(LiveTrail, spline_samples) == kTrailSamplesAt);
static_assert(offsetof(LiveTrail, spline_progress) == kTrailProgressAt);
static_assert(offsetof(LiveTrail, first_step) == kTrailFirstStepAt);
static_assert(std::tuple_size_v<decltype(LiveTrail::spline_points)> ==
              kTrailSplinePoints);
static_assert(std::tuple_size_v<decltype(LiveTrail::spline_samples)> ==
              kTrailHistory);
// The nine-sample trail (Trails.hpp) shares that layout up to its longer
// history; it keeps the unused initial speed step in first_step.
constexpr std::size_t kTrailNineBytes = 0x78;
constexpr std::size_t kTrailNineProgressAt = 0x6C;
constexpr std::size_t kTrailNineFirstStepAt = 0x74;
static_assert(sizeof(LiveTrail9) == kTrailNineBytes);
static_assert(offsetof(LiveTrail9, step_count) == kTrailStepCountAt);
static_assert(offsetof(LiveTrail9, current_spline_sample) == kTrailCursorAt);
static_assert(offsetof(LiveTrail9, spline_samples) == kTrailSamplesAt);
static_assert(offsetof(LiveTrail9, spline_progress) == kTrailNineProgressAt);
static_assert(offsetof(LiveTrail9, first_step) == kTrailNineFirstStepAt);
static_assert(std::tuple_size_v<decltype(LiveTrail9::spline_points)> ==
              kTrailSplinePoints);
static_assert(std::tuple_size_v<decltype(LiveTrail9::spline_samples)> ==
              kTrailNineHistory);
using Scratch = LiveParticleEmitterScratch;
static_assert(offsetof(Scratch, aim_matrix) == kScratchAimMatrixAt);
static_assert(offsetof(Scratch, acceleration_min) == kScratchAccelerationAt);
static_assert(offsetof(Scratch, attraction_jitter) == kScratchJitterAt);
static_assert(sizeof(LiveParticleDefinition) == kParticleDefinitionBytes);

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
    const LiveParticleDefinition& source) {
  const auto& vectors = source.vector_ranges;
  const auto& pairs = source.pair_ranges;
  return {.curves = {.attraction_offset = source.attraction_offset_curve,
                     .attraction_jitter = source.attraction_jitter_curve,
                     .mass = source.mass_curve,
                     .gravity_scale = source.gravity_scale_curve,
                     .speed = source.speed_curve,
                     .direction_jitter = source.direction_jitter_curve,
                     .direction = source.direction_curve,
                     .force = source.force_curve,
                     .acceleration = source.acceleration_curve,
                     .color = {source.r_curve, source.g_curve, source.b_curve}},
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
          .inner_extent = math::RangePair(source.inner_extent_range, 0)};
}

[[nodiscard]] Vec2 PairOf(std::span<const int, 2> pair) {
  return {pair[0], pair[1]};
}

void StorePair(std::span<int, 2> pair, Vec2 value) {
  pair[0] = value.x;
  pair[1] = value.y;
}

[[nodiscard]] TrailScratch TrailScratchOf(
    const LiveParticleEmitterScratch& source) {
  return {.flags = source.flags,
          .attraction_offset = math::VectorOf(source.attraction_offset),
          .attraction_jitter = math::VectorOf(source.attraction_jitter),
          .mass = PairOf(source.mass_range),
          .gravity_scale = PairOf(source.gravity_scale_range),
          .speed = PairOf(source.speed_range),
          .lifetime = PairOf(source.lifetime_range),
          .direction_jitter = math::VectorOf(source.direction_jitter),
          .base_direction = math::VectorOf(source.base_direction),
          .force_min = math::VectorOf(source.force_min),
          .aim_point = math::VectorOf(source.aim_point),
          .spawn_point = math::VectorOf(source.spawn_point),
          .aim_matrix = math::MatrixOf(source.aim_matrix),
          .acceleration_min = math::VectorOf(source.acceleration_min)};
}

// Every field the stages may have written goes back; the rest of the scratch
// block, and the aim matrix's translation, are left as they were.
void StoreTrailScratch(const TrailScratch& values,
                       LiveParticleEmitterScratch& target) {
  target.flags = values.flags;
  math::StoreVector(target.attraction_offset, values.attraction_offset);
  math::StoreVector(target.attraction_jitter, values.attraction_jitter);
  StorePair(target.mass_range, values.mass);
  StorePair(target.gravity_scale_range, values.gravity_scale);
  StorePair(target.speed_range, values.speed);
  StorePair(target.lifetime_range, values.lifetime);
  math::StoreVector(target.direction_jitter, values.direction_jitter);
  math::StoreVector(target.base_direction, values.base_direction);
  math::StoreVector(target.force_min, values.force_min);
  math::StoreVector(target.aim_point, values.aim_point);
  math::StoreVector(target.spawn_point, values.spawn_point);
  math::StoreMatrix(target.aim_matrix, values.aim_matrix);
  math::StoreVector(target.acceleration_min, values.acceleration_min);
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
struct LegacyTrail<LiveTrail> {
  static constexpr std::size_t kHistory = kTrailHistory;
  static constexpr std::size_t kCapacity = kMaxTrailsPerEmitter;
  static constexpr auto kFirstStep = &LiveTrail::first_step;
};

template <>
struct LegacyTrail<LiveTrail9> {
  static constexpr std::size_t kHistory = kTrailNineHistory;
  static constexpr std::size_t kCapacity = kMaxNineSampleTrailsPerEmitter;
  static constexpr auto kFirstStep = &LiveTrail9::first_step;
};

template <typename Trail>
using LegacyRecord = BasicTrailRecord<LegacyTrail<Trail>::kHistory>;

template <typename Trail>
[[nodiscard]] LegacyRecord<Trail> TrailRecordOf(const Trail& source) {
  LegacyRecord<Trail> record{
      .step_count = source.step_count,
      .sample_count = source.spline_sample_count,
      .collapse = source.trail_collapse_counter,
      .current = source.current_spline_sample,
      .progress = source.spline_progress,
      .speed = source.speed,
      .first_step = source.*LegacyTrail<Trail>::kFirstStep};
  std::ranges::transform(source.spline_points, record.points.begin(), PointOf);
  std::ranges::transform(source.spline_samples, record.samples.begin(),
                         PointOf);
  return record;
}

template <typename Trail>
void StoreTrailRecord(const LegacyRecord<Trail>& record, Trail& target) {
  for (std::size_t index = 0; index < kTrailSplinePoints; ++index) {
    StorePoint(target.spline_points[index], record.points.at(index));
  }
  for (std::size_t index = 0; index < LegacyTrail<Trail>::kHistory; ++index) {
    StorePoint(target.spline_samples[index], record.samples.at(index));
  }
  target.step_count = record.step_count;
  target.spline_sample_count = record.sample_count;
  target.trail_collapse_counter = record.collapse;
  target.current_spline_sample = record.current;
  target.spline_progress = record.progress;
  target.speed = record.speed;
  target.*LegacyTrail<Trail>::kFirstStep = record.first_step;
}

// The emitter state members the host reads: its texture animation, its
// transparency samples and its definition, for diagnostics.
struct EmitterContext {
  LiveParticleTextureAnimation* texture = nullptr;
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

template <std::size_t Samples>
[[nodiscard]] EmitterView<LiveTrailRecord<Samples>> ViewOf(
    BasicLiveTrailState<Samples>& state) {
  return {.trails = state.trails,
          .count = state.trail_count,
          .distance = state.curve_sample_distance,
          .context = {.texture = &state.texture,
                      .transparency = state.transparency.data(),
                      .definition_index = state.effect}};
}

// BasicTrailHost over one emitter: its definition, the effect instance's
// transforms, the emitter state's texture animation and `History`
// transparency samples, the GTE, the BIOS generator and the current frame.
template <std::size_t History>
class LiveTrailHost final : public BasicTrailHost<History> {
 public:
  LiveTrailHost(const LiveParticleDefinition& definition,
                Effects_EffectInstance& instance, const EmitterContext& context)
      : definition_(definition), instance_(instance), context_(context) {}

  [[nodiscard]] std::int32_t SampleCurve(std::uint8_t curve,
                                         std::int32_t distance) override;
  [[nodiscard]] std::int32_t Rand() override {
    return VS::Core::NextBiosRand();
  }
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
  [[nodiscard]] std::int32_t NearClip() override {
    return VS::Render::nearClip;
  }
  void Submit(const RingQuad& quad, std::int32_t depth) override;
  void ReportSkippedTrail(std::size_t index, TrailFault fault) override;

 private:
  // The instance transform `index`, or null with a diagnostic naming `role`.
  [[nodiscard]] const LiveTransformSlot* Transform(std::size_t index,
                                                   std::string_view role);
  [[nodiscard]] bool TextureTableBound(std::size_t table) const;

  const LiveParticleDefinition& definition_;
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
  const bool held =
      curve <= curves && Effects_package->curve_block->Size(curve - 1) != 0;
  if (held) {
    // The promoted curve sampler (#276 row 38) through its Live entry.
    return VS::Effects::LiveSampleEffectCurve(curve, distance);
  }
  if (Reportable(refused_curves)) {
    VS::Core::LogWarn(
        "effects",
        "trail: curve {} lies past the package's {} bound curves or "
        "has no entries; sampled as 0 ({} refusals)",
        curve, curves, refused_curves);
  }
  return 0;
}

template <std::size_t History>
const LiveTransformSlot* LiveTrailHost<History>::Transform(
    std::size_t index, std::string_view role) {
  if (index < kTransformCount) {
    return &instance_.transforms[index];
  }
  if (Reportable(refused_transforms)) {
    VS::Core::LogWarn("effects",
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
  const LiveTransformSlot* transform =
      Transform(definition_.attraction_transform, "attraction");
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
  const LiveTransformSlot* transform =
      Transform(definition_.transform_index, "view");
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
  const LiveEffectPackage& package = *Effects_package;
  if (package.textures == nullptr || table >= kTextureTableCount) {
    return false;
  }
  return std::cmp_less(table, package.texture_table_count) &&
         package.texture_tables[table] != nullptr;
}

// decomp: trails5.h:507-509 and trails9.h:514-516. The original stepped the
// animation and read the frame's texture unchecked; a table the package
// lacks, or a frame without texture info, leaves the trails undrawn this tick.
template <std::size_t History>
std::optional<RingTexture> LiveTrailHost<History>::AdvanceTexture() {
  LiveParticleTextureAnimation& animation = *context_.texture;
  const std::size_t table = animation.texture_index;
  const LivePackageTextureFrame* frame = nullptr;
  if (TextureTableBound(table)) {
    LiveAdvanceParticleTextureAnimation(&animation);
    frame = animation.texture_info;
  }
  if (frame == nullptr) {
    if (Reportable(undrawn_textures)) {
      VS::Core::LogWarn(
          "effects",
          "trail: texture table {} of {} gives no frame; trails left "
          "undrawn ({} ticks)",
          table, Effects_package->texture_table_count, undrawn_textures);
    }
    return std::nullopt;
  }
  const LivePackageTexture& source =
      Effects_package->textures[frame->first_texture];
  RingTexture texture;
  texture.clut = source.clut;
  texture.tpage = source.tpage;
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
      VS::Core::LogWarn(
          "effects",
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
  VS::Render::FrameScratchpad().primitive_cursor = words + packet.size();
}

template <std::size_t History>
void LiveTrailHost<History>::ReportSkippedTrail(std::size_t index,
                                                TrailFault fault) {
  if (Reportable(skipped_trails)) {
    VS::Core::LogWarn(
        "effects",
        "trail: trail {} of definition {} has {}; left undrawn ({} "
        "skips)",
        index, context_.definition_index, FaultName(fault), skipped_trails);
  }
}

// The emitter's trail buffer, or nothing when it cannot hold its count.
template <typename Trail>
[[nodiscard]] std::optional<std::span<Trail>> HeldTrails(
    const EmitterView<Trail>& view) {
  constexpr std::size_t kCapacity = LegacyTrail<Trail>::kCapacity;
  if (view.count > kCapacity || (view.count != 0 && view.trails == nullptr)) {
    VS::Core::LogWarn(
        "effects",
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
              const LiveParticleDefinition& definition,
              LiveParticleEmitterScratch& scratch,
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
                       const LiveParticleDefinition& definition,
                       LiveParticleEmitterScratch& scratch,
                       Effects_EffectInstance& instance,
                       LiveTrailState& state) {
  RunStage(stage, definition, scratch, instance, ViewOf(state));
}

void RunLiveTrailNineStage(TrailNineStage stage,
                           const LiveParticleDefinition& definition,
                           LiveParticleEmitterScratch& scratch,
                           Effects_EffectInstance& instance,
                           LiveTrailNineState& state) {
  RunStage(stage, definition, scratch, instance, ViewOf(state));
}

// Each entry is its overlay's VS_TRAIL_DISPATCH_FUNCTION instance
// (trails5.h:677): the emitter lifecycle (TrailEmitter.cpp) with the
// overlay's own emission cursor (VS_TRAIL_CORNER_STATE) and its five stage
// instances (TrailTarget.cpp ... TrailRender.cpp, which carry the decomp
// provenance). PLG057 shares its first cursor with the nine-sample trails, so
// its five-sample trails use the second.
int UpdateLivePlg057Trail(LiveRendererSlot* slot, unsigned int command,
                          int packed) {
  static constexpr TrailStages kStages{
      &Plg057::TrailTarget, &Plg057::TrailControl, &Plg057::TrailInit,
      &Plg057::TrailUpdate, &Plg057::TrailRender};
  return UpdateLiveTrailEmitter(
      slot, command, packed,
      {"PLG057", &Effects::Plg057::trailEmissionCounter2, &kStages});
}

int UpdateLivePlg073Trail(LiveRendererSlot* slot, unsigned int command,
                          int packed) {
  static constexpr TrailStages kStages{
      &Plg073::TrailTarget, &Plg073::TrailControl, &Plg073::TrailInit,
      &Plg073::TrailUpdate, &Plg073::TrailRender};
  return UpdateLiveTrailEmitter(
      slot, command, packed,
      {"PLG073", &Effects::Plg073::trailEmissionCounter, &kStages});
}

int UpdateLivePlg086Trail(LiveRendererSlot* slot, unsigned int command,
                          int packed) {
  static constexpr TrailStages kStages{
      &Plg086::TrailTarget, &Plg086::TrailControl, &Plg086::TrailInit,
      &Plg086::TrailUpdate, &Plg086::TrailRender};
  return UpdateLiveTrailEmitter(
      slot, command, packed,
      {"PLG086", &Effects::Plg086::trailEmissionCounter, &kStages});
}

int UpdateLivePlg087Trail(LiveRendererSlot* slot, unsigned int command,
                          int packed) {
  static constexpr TrailStages kStages{
      &Plg087::TrailTarget, &Plg087::TrailControl, &Plg087::TrailInit,
      &Plg087::TrailUpdate, &Plg087::TrailRender};
  return UpdateLiveTrailEmitter(
      slot, command, packed,
      {"PLG087", &Effects::Plg087::trailEmissionCounter, &kStages});
}

int UpdateLivePlg128Trail(LiveRendererSlot* slot, unsigned int command,
                          int packed) {
  static constexpr TrailStages kStages{
      &Plg128::TrailTarget, &Plg128::TrailControl, &Plg128::TrailInit,
      &Plg128::TrailUpdate, &Plg128::TrailRender};
  return UpdateLiveTrailEmitter(
      slot, command, packed,
      {"PLG128", &Effects::Plg128::trailEmissionCounter, &kStages});
}

int UpdateLivePlg134Trail(LiveRendererSlot* slot, unsigned int command,
                          int packed) {
  static constexpr TrailStages kStages{
      &Plg134::TrailTarget, &Plg134::TrailControl, &Plg134::TrailInit,
      &Plg134::TrailUpdate, &Plg134::TrailRender};
  return UpdateLiveTrailEmitter(
      slot, command, packed,
      {"PLG134", &Effects::Plg134::trailEmissionCounter, &kStages});
}

int UpdateLivePlg137Trail(LiveRendererSlot* slot, unsigned int command,
                          int packed) {
  static constexpr TrailStages kStages{
      &Plg137::TrailTarget, &Plg137::TrailControl, &Plg137::TrailInit,
      &Plg137::TrailUpdate, &Plg137::TrailRender};
  return UpdateLiveTrailEmitter(
      slot, command, packed,
      {"PLG137", &Effects::Plg137::trailEmissionCounter, &kStages});
}

int UpdateLivePlg141Trail(LiveRendererSlot* slot, unsigned int command,
                          int packed) {
  static constexpr TrailStages kStages{
      &Plg141::TrailTarget, &Plg141::TrailControl, &Plg141::TrailInit,
      &Plg141::TrailUpdate, &Plg141::TrailRender};
  return UpdateLiveTrailEmitter(
      slot, command, packed,
      {"PLG141", &Effects::Plg141::trailEmissionCounter, &kStages});
}

int UpdateLivePlg146Trail(LiveRendererSlot* slot, unsigned int command,
                          int packed) {
  static constexpr TrailStages kStages{
      &Plg146::TrailTarget, &Plg146::TrailControl, &Plg146::TrailInit,
      &Plg146::TrailUpdate, &Plg146::TrailRender};
  return UpdateLiveTrailEmitter(
      slot, command, packed,
      {"PLG146", &Effects::Plg146::trailEmissionCounter, &kStages});
}

int UpdateLivePlg148Trail(LiveRendererSlot* slot, unsigned int command,
                          int packed) {
  static constexpr TrailStages kStages{
      &Plg148::TrailTarget, &Plg148::TrailControl, &Plg148::TrailInit,
      &Plg148::TrailUpdate, &Plg148::TrailRender};
  return UpdateLiveTrailEmitter(
      slot, command, packed,
      {"PLG148", &Effects::Plg148::trailEmissionCounter, &kStages});
}

int UpdateLivePlg242Trail(LiveRendererSlot* slot, unsigned int command,
                          int packed) {
  static constexpr TrailStages kStages{
      &Plg242::TrailTarget, &Plg242::TrailControl, &Plg242::TrailInit,
      &Plg242::TrailUpdate, &Plg242::TrailRender};
  return UpdateLiveTrailEmitter(
      slot, command, packed,
      {"PLG242", &Effects::Plg242::trailEmissionCounter, &kStages});
}

int UpdateLivePlg255Trail(LiveRendererSlot* slot, unsigned int command,
                          int packed) {
  static constexpr TrailStages kStages{
      &Plg255::TrailTarget, &Plg255::TrailControl, &Plg255::TrailInit,
      &Plg255::TrailUpdate, &Plg255::TrailRender};
  return UpdateLiveTrailEmitter(
      slot, command, packed,
      {"PLG255", &Effects::Plg255::trailEmissionCounter, &kStages});
}

int UpdateLivePlg265Trail(LiveRendererSlot* slot, unsigned int command,
                          int packed) {
  static constexpr TrailStages kStages{
      &Plg265::TrailTarget, &Plg265::TrailControl, &Plg265::TrailInit,
      &Plg265::TrailUpdate, &Plg265::TrailRender};
  return UpdateLiveTrailEmitter(
      slot, command, packed,
      {"PLG265", &Effects::Plg265::trailEmissionCounter, &kStages});
}

int UpdateLivePlg266Trail(LiveRendererSlot* slot, unsigned int command,
                          int packed) {
  static constexpr TrailStages kStages{
      &Plg266::TrailTarget, &Plg266::TrailControl, &Plg266::TrailInit,
      &Plg266::TrailUpdate, &Plg266::TrailRender};
  return UpdateLiveTrailEmitter(
      slot, command, packed,
      {"PLG266", &Effects::Plg266::trailEmissionCounter, &kStages});
}

int UpdateLivePlg291Trail(LiveRendererSlot* slot, unsigned int command,
                          int packed) {
  static constexpr TrailStages kStages{
      &Plg291::TrailTarget, &Plg291::TrailControl, &Plg291::TrailInit,
      &Plg291::TrailUpdate, &Plg291::TrailRender};
  return UpdateLiveTrailEmitter(
      slot, command, packed,
      {"PLG291", &Effects::Plg291::trailEmissionCounter, &kStages});
}

// Each nine-sample entry is its overlay's VS_TRAIL_DISPATCH_FUNCTION instance
// (trails9.h:685): the emitter lifecycle (TrailEmitter.cpp) with the overlay's
// own emission cursor (VS_TRAIL_CORNER_STATE; PLG057's first) and the shared
// nine-sample stages.
int UpdateLivePlg049TrailNine(LiveRendererSlot* slot, unsigned int command,
                              int packed) {
  return UpdateLiveTrailNineEmitter(
      slot, command, packed,
      {"PLG049", &Effects::Plg049::trailEmissionCounter, &NineStages()});
}

int UpdateLivePlg050TrailNine(LiveRendererSlot* slot, unsigned int command,
                              int packed) {
  return UpdateLiveTrailNineEmitter(
      slot, command, packed,
      {"PLG050", &Effects::Plg050::trailEmissionCounter, &NineStages()});
}

int UpdateLivePlg057TrailNine(LiveRendererSlot* slot, unsigned int command,
                              int packed) {
  return UpdateLiveTrailNineEmitter(
      slot, command, packed,
      {"PLG057", &Effects::Plg057::trailEmissionCounter, &NineStages()});
}

int UpdateLivePlg069TrailNine(LiveRendererSlot* slot, unsigned int command,
                              int packed) {
  return UpdateLiveTrailNineEmitter(
      slot, command, packed,
      {"PLG069", &Effects::Plg069::trailEmissionCounter, &NineStages()});
}

int UpdateLivePlg135TrailNine(LiveRendererSlot* slot, unsigned int command,
                              int packed) {
  return UpdateLiveTrailNineEmitter(
      slot, command, packed,
      {"PLG135", &Effects::Plg135::trailEmissionCounter, &NineStages()});
}

int UpdateLivePlg161TrailNine(LiveRendererSlot* slot, unsigned int command,
                              int packed) {
  return UpdateLiveTrailNineEmitter(
      slot, command, packed,
      {"PLG161", &Effects::Plg161::trailEmissionCounter, &NineStages()});
}

int UpdateLivePlg165TrailNine(LiveRendererSlot* slot, unsigned int command,
                              int packed) {
  return UpdateLiveTrailNineEmitter(
      slot, command, packed,
      {"PLG165", &Effects::Plg165::trailEmissionCounter, &NineStages()});
}

int UpdateLivePlg167TrailNine(LiveRendererSlot* slot, unsigned int command,
                              int packed) {
  return UpdateLiveTrailNineEmitter(
      slot, command, packed,
      {"PLG167", &Effects::Plg167::trailEmissionCounter, &NineStages()});
}

int UpdateLivePlg168TrailNine(LiveRendererSlot* slot, unsigned int command,
                              int packed) {
  return UpdateLiveTrailNineEmitter(
      slot, command, packed,
      {"PLG168", &Effects::Plg168::trailEmissionCounter, &NineStages()});
}

int UpdateLivePlg169TrailNine(LiveRendererSlot* slot, unsigned int command,
                              int packed) {
  return UpdateLiveTrailNineEmitter(
      slot, command, packed,
      {"PLG169", &Effects::Plg169::trailEmissionCounter, &NineStages()});
}

int UpdateLivePlg232TrailNine(LiveRendererSlot* slot, unsigned int command,
                              int packed) {
  return UpdateLiveTrailNineEmitter(
      slot, command, packed,
      {"PLG232", &Effects::Plg232::trailEmissionCounter, &NineStages()});
}
}  // namespace VS::Effects::Templates
