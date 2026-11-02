// Hand-maintained (tools/port/owned.csv).
#include "Effects/TrailEmitterLive.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <span>
#include <type_traits>

#include "Core/Log.hpp"
#include "Effects/PackageExtents.hpp"
#include "Effects/TrailEmitter.hpp"
#include "Legacy/Psx.h"

extern "C" {
#include "Core/BiosRand.h"
#include "Core/Gte.h"
#include "Core/Math/Interpolation.h"
#include "Core/Memory/Heap.h"
#include "Core/Random.h"
#include "Core/Scratch.h"
#include "Effects/Curves.h"
#include "Effects/Package.h"
#include "Effects/Particles.h"
#include "Effects/Runtime.h"
#include "Effects/Templates/TrailControl.h"
#include "Effects/Templates/TrailInit.h"
#include "Effects/Templates/TrailRender.h"
#include "Effects/Templates/TrailTarget.h"
#include "Effects/Templates/TrailUpdate.h"
#include "Effects/Trails.h"
#include "Effects/Transforms.h"
}

namespace VS::Effects {
namespace {
constexpr std::uintptr_t kEmitterScratchOffset = 0x1D0;
using InstanceTransforms = decltype(Effects_EffectInstance::transforms);
using TrailSplineSamples = decltype(Effects_Trail::splineSamples);
using PackageCurves = decltype(Effects_EffectPackage::curves);
constexpr std::size_t kTransformCount = std::extent_v<InstanceTransforms>;
constexpr std::size_t kSplineSampleCount = std::extent_v<TrailSplineSamples>;
constexpr std::size_t kCurveSlots = std::extent_v<PackageCurves>;
// The emitter state carries its five transparency samples in the eight bytes
// allocated past the structure.
constexpr std::size_t kStateTailBytes = 8;
using QuadSet = Effects_TrailQuadSet;
constexpr std::size_t kSamplesAt = offsetof(QuadSet, sampledTransparencyCurve);
constexpr std::size_t kSamplesEnd = kSamplesAt + kTrailTransparencySamples;
constexpr std::size_t kStateBytes = sizeof(QuadSet) + kStateTailBytes;
static_assert(kCurveSlots == kEffectCurveCapacity);
static_assert(kSamplesEnd <= kStateBytes);
static_assert(sizeof(Effects_ParticleDefinition) == kParticleDefinitionBytes);

[[noreturn]] void InvalidEmitter(const char* reason) {
  VS_LOG_ERROR("effects", "PLG074 trail emitter: {}", reason);
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

std::span<Effects_Trail> Trails(Effects_TrailQuadSet& state) {
  if (state.trailcount > kMaxTrailsPerEmitter ||
      (state.trailcount != 0 && state.trails == nullptr)) {
    InvalidEmitter("trail buffer does not hold the trail count");
  }
  return {state.trails, state.trailcount};
}

class LiveOriginServices final : public TrailOriginServices {
 public:
  std::int32_t Rand() override { return Core_Random_Rand(); }
  std::int32_t RandomRangeExclusive(std::int32_t a, std::int32_t b) override {
    return Core_Random_RandomRangeExclusive(a, b);
  }
  std::int32_t Sin(std::int32_t angle) override { return Core_Gte_Sin(angle); }
  std::int32_t Cos(std::int32_t angle) override { return Core_Gte_Cos(angle); }
  TrailVector Rotate(TrailPoint point) override {
    SVECTOR input{point.x, point.y, point.z, 0};
    VECTOR output{};
    Core_Gte_ApplyRotMatrix(&input, &output);
    return Wide(output);
  }
};

TrailVector SampledRange(const std::int16_t* range, int curve, int distance) {
  VECTOR value{};
  Core_Math_InterpolatePairsToWideVector(
      const_cast<std::int16_t*>(range),
      Effects_SampleEffectCurve(curve, distance), &value);
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

TrailTransparency Transparency(const Effects_ParticleDefinition& definition) {
  TrailTransparency samples{};
  samples.fill(kOpaqueTrailTransparency);
  const std::size_t curve = definition.transparencyCurve;
  if (curve == 0) return samples;
  const std::size_t bound = LiveEffectCurveCount();
  if (curve >= bound) {
    VS_LOG_WARN("effects",
                "PLG074 trail emitter: transparency curve {} is outside the "
                "package's {} bound curves; using opaque samples",
                curve, bound);
    return samples;
  }
  const SampledTrailTransparency sampled = SampleTrailTransparency(
      {Effects_package->curves[curve],
       static_cast<std::size_t>(
           Effects_package->curveBlock->curveSizes[curve])});
  if (sampled.clamped) {
    VS_LOG_DEBUG("effects",
                 "PLG074 trail emitter: one-entry transparency curve {} "
                 "repeats its entry for the fifth sample",
                 curve);
  }
  return sampled.samples;
}

class LiveTrailEmitterHost final : public TrailEmitterHost {
 public:
  explicit LiveTrailEmitterHost(Effects_RendererSlot& slot) : slot_(slot) {}

  void Create(TrailEmitterPlan plan) override;

  bool HasState() const override { return State() != nullptr; }

  TrailEmitterCounters Counters() const override {
    const Effects_TrailQuadSet& state = *State();
    return {.lifetime = state.opaque0,
            .initialized = state.opaque3 != 0,
            .curve_sample_distance = state.curveSampleDistance};
  }

  void StoreCounters(const TrailEmitterCounters& counters) override {
    Effects_TrailQuadSet& state = *State();
    state.opaque0 = counters.lifetime;
    state.opaque3 = counters.initialized ? 1 : 0;
    state.curveSampleDistance = counters.curve_sample_distance;
  }

  bool BeginTick() override;

  void InitializeTrails() override {
    Effects_TrailQuadSet* state = State();
    Effects_ParticleEmitterScratch* scratch = &EmitterScratch();
    GenerateLivePlg074TrailOrigins(*definition_, *scratch, *instance_, *state);
    Effects_Plg073_TrailTarget(definition_, scratch, instance_, state);
    Effects_Plg073_TrailControl(definition_, scratch, instance_, state);
    Effects_Plg073_TrailInit(definition_, scratch, instance_, state);
  }

  void DrawTrails() override;

  void ReleaseTrails() override {
    Effects_TrailQuadSet* state = State();
    if (state->trails != nullptr) {
      Core_Memory_FreeFromHighEnd(state->trails);
      state->trails = nullptr;
    }
  }

 private:
  [[nodiscard]] Effects_TrailQuadSet* State() const {
    return static_cast<Effects_TrailQuadSet*>(slot_.rendererState);
  }

  void EmitTickEvents(std::size_t event);

  Effects_RendererSlot& slot_;
  Effects_ParticleDefinition* definition_ = nullptr;
  Effects_EffectInstance* instance_ = nullptr;
};

void LiveTrailEmitterHost::Create(TrailEmitterPlan plan) {
  const Effects_ParticleDefinition* definition =
      Definition(plan.definition_index);
  if (definition == nullptr) {
    VS_LOG_WARN("effects",
                "PLG074 trail emitter: definition {} is outside the package's "
                "{} definitions; emitter refused",
                plan.definition_index, LiveParticleDefinitionCount());
    return;
  }
  const auto count = TrailCountFor(definition->intervalRange[0]);
  if (!count) {
    VS_LOG_WARN("effects",
                "PLG074 trail emitter: definition {} has a negative trail "
                "count; emitter refused",
                plan.definition_index);
    return;
  }
  auto* state =
      static_cast<Effects_TrailQuadSet*>(Core_Memory_AllocateFromHighEnd(
          sizeof(Effects_TrailQuadSet) + kStateTailBytes));
  if (state == nullptr) {
    VS_LOG_ERROR("effects", "PLG074 trail emitter: heap exhausted for state");
    return;
  }
  auto* trails = static_cast<Effects_Trail*>(
      Core_Memory_AllocateFromHighEnd(*count * sizeof(Effects_Trail)));
  if (trails == nullptr) {
    Core_Memory_FreeFromHighEnd(state);
    VS_LOG_ERROR("effects",
                 "PLG074 trail emitter: heap exhausted for {} trails", *count);
    return;
  }
  // The original left the trail buffer uninitialized; the "no placement" box
  // mode then offset whatever the heap held.
  std::fill_n(trails, *count, Effects_Trail{});
  slot_.rendererState = state;
  state->definitionIndex = plan.definition_index;
  state->opaque0 = plan.lifetime;
  state->curveSampleDistance = 0;
  state->trails = trails;
  state->trailcount = *count;
  state->opaque3 = 0;
  const TrailTransparency samples = Transparency(*definition);
  std::ranges::copy(samples, reinterpret_cast<u_char*>(state) + kSamplesAt);
  Effects_ClearParticleTextureState(
      reinterpret_cast<int*>(&state->textureAnimation));
  Effects_InitializeParticleTextureAnimation(&state->textureAnimation,
                                             definition->textureIndex,
                                             definition->animationId);
}

bool LiveTrailEmitterHost::BeginTick() {
  Effects_TrailQuadSet& state = *State();
  static_cast<void>(Trails(state));
  instance_ = Effects_currentInstance;
  if (instance_ == nullptr) InvalidEmitter("no current effect instance");
  definition_ = Definition(state.definitionIndex);
  if (definition_ == nullptr) {
    VS_LOG_WARN("effects",
                "PLG074 trail emitter: the current package no longer holds "
                "definition {}; emitter stopped",
                state.definitionIndex);
    return false;
  }
  if (definition_->transformIndex >= kTransformCount) {
    VS_LOG_WARN("effects",
                "PLG074 trail emitter: definition {} names transform {} of "
                "{}; emitter stopped",
                state.definitionIndex, definition_->transformIndex,
                kTransformCount);
    return false;
  }
  EmitterScratch().flags = definition_->flags;
  return true;
}

void LiveTrailEmitterHost::DrawTrails() {
  Effects_TrailQuadSet* state = State();
  Effects_ParticleEmitterScratch* scratch = &EmitterScratch();
  Effects_Plg073_TrailUpdate(definition_, scratch, instance_, state);
  Effects_Plg073_TrailRender(definition_, scratch, instance_, state);
  if (definition_->tickEvent == 0) return;
  const std::size_t event = definition_->tickEvent - 1U;
  const std::size_t bound = LiveParticleDefinitionCount();
  if (event >= bound) {
    VS_LOG_WARN("effects",
                "PLG074 trail emitter: tick event {} is outside the package's "
                "{} definitions; events skipped",
                event, bound);
    return;
  }
  EmitTickEvents(event);
}

// decomp: trails5.h:752-800 (tick events at each trail's newest sample).
void LiveTrailEmitterHost::EmitTickEvents(std::size_t event) {
  Effects_ParticleEmitterScratch& scratch = EmitterScratch();
  const MATRIX& world =
      instance_->transforms[definition_->transformIndex].world;
  const bool world_events = (definition_->flags & kTrailWorldEvents) != 0;
  for (Effects_Trail& trail : Trails(*State())) {
    if (trail.currentSplineSample >= kSplineSampleCount) {
      InvalidEmitter("spline sample cursor outside the trail history");
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
}  // namespace

// decomp: PLG074 func_800FB414 (src/EFFECT/trails5.h:27).
void GenerateLivePlg074TrailOrigins(
    const Effects_ParticleDefinition& definition,
    Effects_ParticleEmitterScratch& scratch, Effects_EffectInstance& instance,
    Effects_TrailQuadSet& state) {
  if (definition.transformIndex >= kTransformCount) {
    InvalidEmitter("definition transform outside the instance");
  }
  const std::span<Effects_Trail> trails = Trails(state);
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
                       Content_plg074TrailEmissionCounter[0], values, services);
  StoreScratch(values, scratch);
  for (std::size_t index = 0; index < trails.size(); ++index) {
    SVECTOR& point = trails[index].splinePoints[0];
    point.vx = points.at(index).x;
    point.vy = points.at(index).y;
    point.vz = points.at(index).z;
  }
}

// decomp: PLG074 func_800FCFEC (src/EFFECT/trails5.h:677).
int UpdateLivePlg074TrailEmitter(Effects_RendererSlot* slot,
                                 unsigned int command, int packed) {
  if (slot == nullptr) InvalidEmitter("missing renderer slot");
  LiveTrailEmitterHost host(*slot);
  return UpdateTrailEmitter(static_cast<TrailEmitterCommand>(command), packed,
                            host)
             ? 1
             : 0;
}
}  // namespace VS::Effects
