// Hand-maintained (tools/port/owned.csv).
#include "Effects/TrailEmitterLive.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <initializer_list>
#include <span>
#include <string_view>
#include <type_traits>

#include "Core/Log.hpp"
#include "Core/Math/InterpolationLive.hpp"
#include "Core/Math/ScalarMath.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Core/RandomLive.hpp"
#include "Effects/CurvesLive.hpp"
#include "Effects/PackageExtents.hpp"
#include "Effects/Templates/TrailControl.hpp"
#include "Effects/Templates/TrailInit.hpp"
#include "Effects/Templates/TrailRender.hpp"
#include "Effects/Templates/TrailTarget.hpp"
#include "Effects/Templates/TrailUpdate.hpp"
#include "Effects/Templates/TrailsLive.hpp"
#include "Effects/TrailEmitter.hpp"
#include "Legacy/Psx.h"

extern "C" {
#include "Core/BiosRand.h"
#include "Core/Gte.h"
#include "Core/Scratch.h"
#include "Effects/Package.h"
#include "Effects/Particles.h"
#include "Effects/Runtime.h"
#include "Effects/Templates/TemplateState.h"
#include "Effects/Trails.h"
#include "Effects/Transforms.h"
}

namespace VS::Effects {
namespace {
constexpr std::uintptr_t kEmitterScratchOffset = 0x1D0;
using InstanceTransforms = decltype(Effects_EffectInstance::transforms);
using PackageCurves = decltype(Effects_EffectPackage::curves);
constexpr std::size_t kTransformCount = std::extent_v<InstanceTransforms>;
constexpr std::size_t kCurveSlots = std::extent_v<PackageCurves>;
static_assert(kCurveSlots == kEffectCurveCapacity);
static_assert(sizeof(Effects_ParticleDefinition) == kParticleDefinitionBytes);

// The two emitter states: the five-sample quad set (Particles.h) and the
// nine-sample state (TemplateState.h, trails9.h:685). Each carries its
// transparency samples in the bytes allocated past the structure.
template <typename Emitter>
struct EmitterLayout;

template <>
struct EmitterLayout<Effects_TrailQuadSet> {
  using Trail = Effects_Trail;
  using Overlay = LiveTrailOverlay;
  static constexpr std::size_t kCapacity = kMaxTrailsPerEmitter;
  static constexpr std::size_t kSamples = kTrailTransparencySamples;
  static constexpr std::size_t kTailBytes = 8;
  static constexpr auto kLifetime = &Effects_TrailQuadSet::opaque0;
  static constexpr auto kDefinition = &Effects_TrailQuadSet::definitionIndex;
  static constexpr auto kCount = &Effects_TrailQuadSet::trailcount;
  static constexpr auto kTexture = &Effects_TrailQuadSet::textureAnimation;
  static constexpr auto kSample = &SampleTrailTransparency;
  static constexpr auto kRunStage = &Templates::RunLiveTrailStage;
};

template <>
struct EmitterLayout<Effects_Trail9State> {
  using Trail = Effects_Trail9;
  using Overlay = LiveTrailNineOverlay;
  static constexpr std::size_t kCapacity = kMaxNineSampleTrailsPerEmitter;
  static constexpr std::size_t kSamples = kTrailNineTransparencySamples;
  static constexpr std::size_t kTailBytes = 12;
  static constexpr auto kLifetime = &Effects_Trail9State::lifetime;
  static constexpr auto kDefinition = &Effects_Trail9State::effect;
  static constexpr auto kCount = &Effects_Trail9State::trailCount;
  static constexpr auto kTexture = &Effects_Trail9State::texture;
  static constexpr auto kSample = &SampleTrailNineTransparency;
  static constexpr auto kRunStage = &Templates::RunLiveTrailNineStage;
};

template <typename Emitter>
using TrailOf = typename EmitterLayout<Emitter>::Trail;
template <typename Emitter>
using OverlayOf = typename EmitterLayout<Emitter>::Overlay;
template <typename Emitter>
using SamplesOf = std::array<std::uint8_t, EmitterLayout<Emitter>::kSamples>;

template <typename Emitter>
constexpr std::size_t kSamplesAt = offsetof(Emitter, sampledTransparencyCurve);
template <typename Emitter>
constexpr bool TailHoldsSamples() {
  using Layout = EmitterLayout<Emitter>;
  return kSamplesAt<Emitter> + Layout::kSamples <=
         sizeof(Emitter) + Layout::kTailBytes;
}
static_assert(TailHoldsSamples<Effects_TrailQuadSet>());
static_assert(TailHoldsSamples<Effects_Trail9State>());

// PLG074 reuses PLG073's byte-identical stage instances (F01989-F01993 are
// duplicates of F01978-F01982) with its own emission cursor.
const Templates::TrailStages& Plg074Stages() {
  static constexpr Templates::TrailStages kStages{
      &Plg073::TrailTarget, &Plg073::TrailControl, &Plg073::TrailInit,
      &Plg073::TrailUpdate, &Plg073::TrailRender};
  return kStages;
}

[[noreturn]] void InvalidEmitter(std::string_view overlay, const char* reason) {
  VS_LOG_ERROR("effects", "{} trail emitter: {}", overlay, reason);
  std::abort();
}

Effects_ParticleEmitterScratch& EmitterScratch() {
  return *reinterpret_cast<Effects_ParticleEmitterScratch*>(
      reinterpret_cast<std::uintptr_t>(Core_ScratchBase()) +
      kEmitterScratchOffset);
}

TrailVector Wide(const VECTOR& value) {
  return {static_cast<std::int32_t>(value.vx),
          static_cast<std::int32_t>(value.vy),
          static_cast<std::int32_t>(value.vz)};
}

void Store(VECTOR& target, TrailVector value) {
  target.vx = value.x;
  target.vy = value.y;
  target.vz = value.z;
}

template <typename Emitter>
std::span<TrailOf<Emitter>> Trails(std::string_view overlay, Emitter& state) {
  const std::size_t count = state.*EmitterLayout<Emitter>::kCount;
  if (count > EmitterLayout<Emitter>::kCapacity ||
      (count != 0 && state.trails == nullptr)) {
    InvalidEmitter(overlay, "trail buffer does not hold the trail count");
  }
  return {state.trails, count};
}

class LiveOriginServices final : public TrailOriginServices {
 public:
  std::int32_t Rand() override { return Core_Random_Rand(); }
  std::int32_t RandomRangeExclusive(std::int32_t a, std::int32_t b) override {
    return VS::Core::LiveRandomRangeExclusive(a, b);
  }
  std::int32_t Sin(std::int32_t angle) override {
    return VS::Core::Math::Sine(angle);
  }
  std::int32_t Cos(std::int32_t angle) override {
    return VS::Core::Math::Cosine(angle);
  }
  TrailVector Rotate(TrailPoint point) override {
    SVECTOR input{point.x, point.y, point.z, 0};
    VECTOR output{};
    Core_Gte_ApplyRotMatrix(&input, &output);
    return Wide(output);
  }
};

TrailVector SampledRange(
    std::span<const std::int16_t, VS::Core::Math::kVectorRangeHalfwords> range,
    int curve, int distance) {
  VECTOR value{};
  VS::Core::Math::InterpolateLiveSevenBitVector(
      range, VS::Effects::LiveSampleEffectCurve(curve, distance), &value);
  return Wide(value);
}

TrailOriginScratch LoadScratch(const Effects_ParticleEmitterScratch& scratch) {
  return {.position = Wide(scratch.position),
          .extent = Wide(scratch.extent),
          .aim_point = Wide(scratch.aimPoint),
          .sin_angle0 = scratch.sinAngle0,
          .sin_angle1 = scratch.sinAngle1,
          .cos_angle0 = scratch.cosAngle0,
          .cos_angle1 = scratch.cosAngle1,
          .ring_segments = scratch.ringSegments,
          .ring_positions = scratch.ringPositions};
}

void StoreScratch(const TrailOriginScratch& values,
                  Effects_ParticleEmitterScratch& scratch) {
  Store(scratch.position, values.position);
  Store(scratch.extent, values.extent);
  Store(scratch.aimPoint, values.aim_point);
  scratch.sinAngle0 = values.sin_angle0;
  scratch.sinAngle1 = values.sin_angle1;
  scratch.cosAngle0 = values.cos_angle0;
  scratch.cosAngle1 = values.cos_angle1;
  scratch.ringSegments = values.ring_segments;
  scratch.ringPositions = values.ring_positions;
}

// The current package's definition `index`, or null when its recorded
// definition block does not hold it.
Effects_ParticleDefinition* Definition(std::size_t index) {
  if (Effects_package == nullptr || Effects_package->block5Data == nullptr ||
      index >= LiveParticleDefinitionCount()) {
    return nullptr;
  }
  return &Effects_package->block5Data->definitions[index];
}

template <typename Emitter>
SamplesOf<Emitter> Transparency(std::string_view overlay,
                                const Effects_ParticleDefinition& definition) {
  SamplesOf<Emitter> samples{};
  samples.fill(kOpaqueTrailTransparency);
  const std::size_t curve = definition.transparencyCurve;
  if (curve == 0) return samples;
  const std::size_t bound = LiveEffectCurveCount();
  if (curve >= bound) {
    VS_LOG_WARN("effects",
                "{} trail emitter: transparency curve {} is outside the "
                "package's {} bound curves; using opaque samples",
                overlay, curve, bound);
    return samples;
  }
  const auto sampled = EmitterLayout<Emitter>::kSample(
      {Effects_package->curves[curve],
       static_cast<std::size_t>(
           Effects_package->curveBlock->curveSizes[curve])});
  if (sampled.clamped) {
    VS_LOG_DEBUG("effects",
                 "{} trail emitter: one-entry transparency curve {} "
                 "repeats its entry for the last sample",
                 overlay, curve);
  }
  return sampled.samples;
}

// decomp: src/EFFECT/trails5.h:27 and trails9.h:27 VS_TRAIL_ORIGIN_FUNCTION.
template <typename Emitter>
void GenerateOrigins(const Effects_ParticleDefinition& definition,
                     Effects_ParticleEmitterScratch& scratch,
                     Effects_EffectInstance& instance, Emitter& state,
                     const OverlayOf<Emitter>& overlay) {
  if (definition.transformIndex >= kTransformCount) {
    InvalidEmitter(overlay.name, "definition transform outside the instance");
  }
  const std::span<TrailOf<Emitter>> trails = Trails(overlay.name, state);
  const MATRIX& matrix = instance.transforms[definition.transformIndex].world;
  const SVECTOR& origin = *Effects_effectOrigin;
  const TrailOriginRequest request{
      .flags = scratch.flags,
      .position =
          SampledRange(definition.positionRange, definition.positionCurve,
                       state.curveSampleDistance),
      .extent = SampledRange(definition.extentRange, definition.extentCurve,
                             state.curveSampleDistance),
      .effect_origin = {origin.vx, origin.vy, origin.vz},
      .caster_translation = {static_cast<std::int32_t>(matrix.t[0]),
                             static_cast<std::int32_t>(matrix.t[1]),
                             static_cast<std::int32_t>(matrix.t[2])},
      .ring_segments = definition.ringSegments,
      .ring_positions = definition.ringPositions};
  Core_Gte_SetRotMatrix(&matrix);
  Core_Gte_ClearTranslation();

  std::array<TrailPoint, kMaxTrailsPerEmitter> points{};
  for (std::size_t index = 0; index < trails.size(); ++index) {
    const SVECTOR& point = trails[index].splinePoints[0];
    points.at(index) = {point.vx, point.vy, point.vz};
  }
  TrailOriginScratch values = LoadScratch(scratch);
  LiveOriginServices services;
  GenerateTrailOrigins(request, std::span(points).first(trails.size()),
                       *overlay.corner, values, services);
  StoreScratch(values, scratch);
  for (std::size_t index = 0; index < trails.size(); ++index) {
    SVECTOR& point = trails[index].splinePoints[0];
    point.vx = points.at(index).x;
    point.vy = points.at(index).y;
    point.vz = points.at(index).z;
  }
}

template <typename Emitter>
class LiveTrailEmitterHost final : public TrailEmitterHost {
 public:
  LiveTrailEmitterHost(Effects_RendererSlot& slot,
                       const OverlayOf<Emitter>& overlay)
      : slot_(slot), overlay_(overlay) {}

  void Create(TrailEmitterPlan plan) override;

  bool HasState() const override { return State() != nullptr; }

  TrailEmitterCounters Counters() const override {
    const Emitter& state = *State();
    return {.lifetime = state.*Layout::kLifetime,
            .initialized = state.opaque3 != 0,
            .curve_sample_distance = state.curveSampleDistance};
  }

  void StoreCounters(const TrailEmitterCounters& counters) override {
    Emitter& state = *State();
    state.*Layout::kLifetime = counters.lifetime;
    state.opaque3 = counters.initialized ? 1 : 0;
    state.curveSampleDistance = counters.curve_sample_distance;
  }

  bool BeginTick() override;

  // decomp: trails5.h:736-742 and trails9.h:745-751 (the first tick runs
  // origin, target, control and init).
  void InitializeTrails() override {
    Emitter& state = *State();
    Effects_ParticleEmitterScratch& scratch = EmitterScratch();
    GenerateOrigins(*definition_, scratch, *instance_, state, overlay_);
    const auto& stages = *overlay_.stages;
    for (const auto stage : {stages.target, stages.control, stages.init}) {
      Layout::kRunStage(stage, *definition_, scratch, *instance_, state);
    }
  }

  void DrawTrails() override;

  void ReleaseTrails() override {
    Emitter* state = State();
    if (state->trails != nullptr) {
      VS::Core::Memory::FreeLiveFromHighEnd(state->trails);
      state->trails = nullptr;
    }
  }

 private:
  using Layout = EmitterLayout<Emitter>;

  [[nodiscard]] Emitter* State() const {
    return static_cast<Emitter*>(slot_.rendererState);
  }

  void EmitTickEvents(std::size_t event);

  Effects_RendererSlot& slot_;
  const OverlayOf<Emitter>& overlay_;
  Effects_ParticleDefinition* definition_ = nullptr;
  Effects_EffectInstance* instance_ = nullptr;
};

template <typename Emitter>
void LiveTrailEmitterHost<Emitter>::Create(TrailEmitterPlan plan) {
  const Effects_ParticleDefinition* definition =
      Definition(plan.definition_index);
  if (definition == nullptr) {
    VS_LOG_WARN("effects",
                "{} trail emitter: definition {} is outside the package's "
                "{} definitions; emitter refused",
                overlay_.name, plan.definition_index,
                LiveParticleDefinitionCount());
    return;
  }
  const auto count =
      TrailCountFor(definition->intervalRange[0], Layout::kCapacity);
  if (!count) {
    VS_LOG_WARN("effects",
                "{} trail emitter: definition {} has a negative trail "
                "count; emitter refused",
                overlay_.name, plan.definition_index);
    return;
  }
  auto* state = static_cast<Emitter*>(VS::Core::Memory::AllocateLiveFromHighEnd(
      sizeof(Emitter) + Layout::kTailBytes));
  if (state == nullptr) {
    VS_LOG_ERROR("effects", "{} trail emitter: heap exhausted for state",
                 overlay_.name);
    return;
  }
  using Trail = TrailOf<Emitter>;
  auto* trails = static_cast<Trail*>(
      VS::Core::Memory::AllocateLiveFromHighEnd(*count * sizeof(Trail)));
  if (trails == nullptr) {
    VS::Core::Memory::FreeLiveFromHighEnd(state);
    VS_LOG_ERROR("effects", "{} trail emitter: heap exhausted for {} trails",
                 overlay_.name, *count);
    return;
  }
  // The original left the trail buffer uninitialized; the "no placement" box
  // mode then offset whatever the heap held.
  std::fill_n(trails, *count, Trail{});
  slot_.rendererState = state;
  state->*Layout::kDefinition = plan.definition_index;
  state->*Layout::kLifetime = plan.lifetime;
  state->curveSampleDistance = 0;
  state->trails = trails;
  state->*Layout::kCount = *count;
  state->opaque3 = 0;
  const SamplesOf<Emitter> samples =
      Transparency<Emitter>(overlay_.name, *definition);
  std::ranges::copy(samples,
                    reinterpret_cast<u_char*>(state) + kSamplesAt<Emitter>);
  Effects_ParticleTextureAnimation& texture = state->*Layout::kTexture;
  Effects_ClearParticleTextureState(reinterpret_cast<int*>(&texture));
  Effects_InitializeParticleTextureAnimation(&texture, definition->textureIndex,
                                             definition->animationId);
}

template <typename Emitter>
bool LiveTrailEmitterHost<Emitter>::BeginTick() {
  Emitter& state = *State();
  static_cast<void>(Trails(overlay_.name, state));
  instance_ = Effects_currentInstance;
  if (instance_ == nullptr) {
    InvalidEmitter(overlay_.name, "no current effect instance");
  }
  const std::size_t index = state.*Layout::kDefinition;
  definition_ = Definition(index);
  if (definition_ == nullptr) {
    VS_LOG_WARN("effects",
                "{} trail emitter: the current package no longer holds "
                "definition {}; emitter stopped",
                overlay_.name, index);
    return false;
  }
  if (definition_->transformIndex >= kTransformCount) {
    VS_LOG_WARN("effects",
                "{} trail emitter: definition {} names transform {} of "
                "{}; emitter stopped",
                overlay_.name, index, definition_->transformIndex,
                kTransformCount);
    return false;
  }
  EmitterScratch().flags = definition_->flags;
  return true;
}

// decomp: trails5.h:744-745 and trails9.h:753-754 (every tick runs update and
// render).
template <typename Emitter>
void LiveTrailEmitterHost<Emitter>::DrawTrails() {
  Emitter& state = *State();
  Effects_ParticleEmitterScratch& scratch = EmitterScratch();
  Layout::kRunStage(overlay_.stages->update, *definition_, scratch, *instance_,
                    state);
  Layout::kRunStage(overlay_.stages->render, *definition_, scratch, *instance_,
                    state);
  if (definition_->tickEvent == 0) return;
  const std::size_t event = definition_->tickEvent - 1U;
  const std::size_t bound = LiveParticleDefinitionCount();
  if (event >= bound) {
    VS_LOG_WARN("effects",
                "{} trail emitter: tick event {} is outside the package's "
                "{} definitions; events skipped",
                overlay_.name, event, bound);
    return;
  }
  EmitTickEvents(event);
}

// decomp: trails5.h:752-800 and trails9.h:761-798 (tick events at each
// trail's newest sample).
template <typename Emitter>
void LiveTrailEmitterHost<Emitter>::EmitTickEvents(std::size_t event) {
  constexpr std::size_t kSplineSampleCount =
      std::extent_v<decltype(TrailOf<Emitter>::splineSamples)>;
  Effects_ParticleEmitterScratch& scratch = EmitterScratch();
  const MATRIX& world =
      instance_->transforms[definition_->transformIndex].world;
  const bool world_events = (definition_->flags & kTrailWorldEvents) != 0;
  for (TrailOf<Emitter>& trail : Trails(overlay_.name, *State())) {
    if (trail.currentSplineSample >= kSplineSampleCount) {
      InvalidEmitter(overlay_.name,
                     "spline sample cursor outside the trail history");
    }
    const SVECTOR& sample = trail.splineSamples[trail.currentSplineSample];
    TrailVector point{sample.vx, sample.vy, sample.vz};
    if (world_events) {
      Core_Gte_SetRotMatrix(&world);
      Core_Gte_SetTransMatrix(&world);
      scratch.vector.vx = sample.vx;
      scratch.vector.vy = sample.vy;
      scratch.vector.vz = sample.vz;
      Core_Gte_LoadV0(&scratch.vector);
      Core_Gte_Rtv0Tr();
      Core_Gte_StoreLvnl(&scratch.aimPoint);
      point = Wide(scratch.aimPoint);
    }
    // The original left the context's other fields, including its variant,
    // as stack contents; they start cleared here.
    Effects_PluginEffectState context{};
    context.transparency = 0;
    Store(context.position, TrailEventPosition(point));
    Effects_EmitParticleEvent(instance_, static_cast<int>(event), 0, 0,
                              &context);
  }
}

template <typename Emitter>
int UpdateEmitter(Effects_RendererSlot* slot, unsigned int command, int packed,
                  const OverlayOf<Emitter>& overlay) {
  if (overlay.corner == nullptr || overlay.stages == nullptr) {
    InvalidEmitter(overlay.name, "overlay binding is incomplete");
  }
  if (slot == nullptr) InvalidEmitter(overlay.name, "missing renderer slot");
  LiveTrailEmitterHost<Emitter> host(*slot, overlay);
  return UpdateTrailEmitter(static_cast<TrailEmitterCommand>(command), packed,
                            host)
             ? 1
             : 0;
}
}  // namespace

// decomp: src/EFFECT/trails5.h:27 VS_TRAIL_ORIGIN_FUNCTION.
void GenerateLiveTrailOrigins(const Effects_ParticleDefinition& definition,
                              Effects_ParticleEmitterScratch& scratch,
                              Effects_EffectInstance& instance,
                              Effects_TrailQuadSet& state,
                              const LiveTrailOverlay& overlay) {
  GenerateOrigins(definition, scratch, instance, state, overlay);
}

// decomp: src/EFFECT/trails5.h:677 VS_TRAIL_DISPATCH_FUNCTION.
int UpdateLiveTrailEmitter(Effects_RendererSlot* slot, unsigned int command,
                           int packed, const LiveTrailOverlay& overlay) {
  return UpdateEmitter<Effects_TrailQuadSet>(slot, command, packed, overlay);
}

// decomp: src/EFFECT/trails9.h:27 VS_TRAIL_ORIGIN_FUNCTION.
void GenerateLiveTrailNineOrigins(const Effects_ParticleDefinition& definition,
                                  Effects_ParticleEmitterScratch& scratch,
                                  Effects_EffectInstance& instance,
                                  Effects_Trail9State& state,
                                  const LiveTrailNineOverlay& overlay) {
  GenerateOrigins(definition, scratch, instance, state, overlay);
}

// decomp: src/EFFECT/trails9.h:685 VS_TRAIL_DISPATCH_FUNCTION.
int UpdateLiveTrailNineEmitter(Effects_RendererSlot* slot, unsigned int command,
                               int packed,
                               const LiveTrailNineOverlay& overlay) {
  return UpdateEmitter<Effects_Trail9State>(slot, command, packed, overlay);
}

// decomp: PLG074 func_800FB414 (src/EFFECT/trails5.h:27).
void GenerateLivePlg074TrailOrigins(
    const Effects_ParticleDefinition& definition,
    Effects_ParticleEmitterScratch& scratch, Effects_EffectInstance& instance,
    Effects_TrailQuadSet& state) {
  GenerateLiveTrailOrigins(
      definition, scratch, instance, state,
      {"PLG074", &Content_plg074TrailEmissionCounter[0], &Plg074Stages()});
}

// decomp: PLG074 func_800FCFEC (src/EFFECT/trails5.h:677).
int UpdateLivePlg074TrailEmitter(Effects_RendererSlot* slot,
                                 unsigned int command, int packed) {
  return UpdateLiveTrailEmitter(
      slot, command, packed,
      {"PLG074", &Content_plg074TrailEmissionCounter[0], &Plg074Stages()});
}
}  // namespace VS::Effects
