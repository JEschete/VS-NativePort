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

#include "Core/Gte.h"
#include "Core/Log.hpp"
#include "Core/Math/InterpolationLive.hpp"
#include "Core/Math/ScalarMath.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Core/RandomLive.hpp"
#include "Core/Scratch.hpp"
#include "Effects/CurvesLive.hpp"
#include "Effects/Package.hpp"
#include "Effects/PackageExtents.hpp"
#include "Effects/ParticleEmitterLive.hpp"
#include "Effects/ParticleTextureAnimationLive.hpp"
#include "Effects/Particles.hpp"
#include "Effects/Runtime.h"
#include "Effects/Templates/TemplateLiveState.hpp"
#include "Effects/Templates/TrailControl.hpp"
#include "Effects/Templates/TrailInit.hpp"
#include "Effects/Templates/TrailRender.hpp"
#include "Effects/Templates/TrailTarget.hpp"
#include "Effects/Templates/TrailUpdate.hpp"
#include "Effects/Templates/TrailsLive.hpp"
#include "Effects/TrailEmitter.hpp"
#include "Effects/Trails.hpp"
#include "Effects/TransformsLiveState.hpp"
#include "Legacy/Psx.h"

namespace VS::Effects {
namespace {
constexpr std::uintptr_t kEmitterScratchOffset = 0x1D0;
using InstanceTransforms = decltype(Effects_EffectInstance::transforms);
using PackageCurves = decltype(LiveEffectPackage::curves);
constexpr std::size_t kTransformCount = std::extent_v<InstanceTransforms>;
constexpr std::size_t kCurveSlots = std::tuple_size_v<PackageCurves>;
static_assert(kCurveSlots == kEffectCurveCapacity);
static_assert(sizeof(LiveParticleDefinition) == kParticleDefinitionBytes);

// The two emitter states (TemplateLiveState.hpp): the five-sample state of
// trails5.h and the nine-sample state of trails9.h, each with its
// transparency samples.
template <typename Emitter>
struct EmitterLayout;

template <>
struct EmitterLayout<LiveTrailState> {
  using Trail = LiveTrail;
  using Overlay = LiveTrailOverlay;
  static constexpr std::size_t kCapacity = kMaxTrailsPerEmitter;
  static constexpr auto kSample = &SampleTrailTransparency;
  static constexpr auto kRunStage = &Templates::RunLiveTrailStage;
};

template <>
struct EmitterLayout<LiveTrailNineState> {
  using Trail = LiveTrail9;
  using Overlay = LiveTrailNineOverlay;
  static constexpr std::size_t kCapacity = kMaxNineSampleTrailsPerEmitter;
  static constexpr auto kSample = &SampleTrailNineTransparency;
  static constexpr auto kRunStage = &Templates::RunLiveTrailNineStage;
};

template <typename Emitter>
using TrailOf = typename EmitterLayout<Emitter>::Trail;
template <typename Emitter>
using OverlayOf = typename EmitterLayout<Emitter>::Overlay;
template <typename Emitter>
using SamplesOf = decltype(Emitter::transparency);

// PLG074 reuses PLG073's byte-identical stage instances (F01989-F01993 are
// duplicates of F01978-F01982) with its own emission cursor.
const Templates::TrailStages& Plg074Stages() {
  static constexpr Templates::TrailStages kStages{
      &Plg073::TrailTarget, &Plg073::TrailControl, &Plg073::TrailInit,
      &Plg073::TrailUpdate, &Plg073::TrailRender};
  return kStages;
}

[[noreturn]] void InvalidEmitter(std::string_view overlay, const char* reason) {
  VS::Core::LogError("effects", "{} trail emitter: {}", overlay, reason);
  std::abort();
}

LiveParticleEmitterScratch& EmitterScratch() {
  return *reinterpret_cast<LiveParticleEmitterScratch*>(
      reinterpret_cast<std::uintptr_t>(VS::Core::ScratchBase()) +
      kEmitterScratchOffset);
}

TrailVector Wide(const VECTOR& value) {
  return {static_cast<std::int32_t>(value.vx),
          static_cast<std::int32_t>(value.vy),
          static_cast<std::int32_t>(value.vz)};
}

template <typename Vector>
void Store(Vector& target, TrailVector value) {
  target.vx = value.x;
  target.vy = value.y;
  target.vz = value.z;
}

template <typename Emitter>
std::span<TrailOf<Emitter>> Trails(std::string_view overlay, Emitter& state) {
  const std::size_t count = state.trail_count;
  if (count > EmitterLayout<Emitter>::kCapacity ||
      (count != 0 && state.trails == nullptr)) {
    InvalidEmitter(overlay, "trail buffer does not hold the trail count");
  }
  return {state.trails, count};
}

class LiveOriginServices final : public TrailOriginServices {
 public:
  std::int32_t Rand() override { return VS::Core::NextBiosRand(); }
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

TrailOriginScratch LoadScratch(const LiveParticleEmitterScratch& scratch) {
  return {.position = Wide(scratch.position),
          .extent = Wide(scratch.extent),
          .aim_point = Wide(scratch.aim_point),
          .sin_angle0 = scratch.sin_angle0,
          .sin_angle1 = scratch.sin_angle1,
          .cos_angle0 = scratch.cos_angle0,
          .cos_angle1 = scratch.cos_angle1,
          .ring_segments = scratch.ring_segments,
          .ring_positions = scratch.ring_positions};
}

void StoreScratch(const TrailOriginScratch& values,
                  LiveParticleEmitterScratch& scratch) {
  Store(scratch.position, values.position);
  Store(scratch.extent, values.extent);
  Store(scratch.aim_point, values.aim_point);
  scratch.sin_angle0 = values.sin_angle0;
  scratch.sin_angle1 = values.sin_angle1;
  scratch.cos_angle0 = values.cos_angle0;
  scratch.cos_angle1 = values.cos_angle1;
  scratch.ring_segments = values.ring_segments;
  scratch.ring_positions = values.ring_positions;
}

// The current package's definition `index`, or null when its recorded
// definition block does not hold it.
LiveParticleDefinition* Definition(std::size_t index) {
  if (Effects_package == nullptr ||
      Effects_package->particle_definitions == nullptr ||
      index >= LiveParticleDefinitionCount()) {
    return nullptr;
  }
  return &Effects_package->particle_definitions->Definitions()[index];
}

template <typename Emitter>
SamplesOf<Emitter> Transparency(std::string_view overlay,
                                const LiveParticleDefinition& definition) {
  SamplesOf<Emitter> samples{};
  samples.fill(kOpaqueTrailTransparency);
  const std::size_t curve = definition.transparency_curve;
  if (curve == 0) return samples;
  const std::size_t bound = LiveEffectCurveCount();
  if (curve >= bound) {
    VS::Core::LogWarn("effects",
                      "{} trail emitter: transparency curve {} is outside the "
                      "package's {} bound curves; using opaque samples",
                      overlay, curve, bound);
    return samples;
  }
  const auto sampled = EmitterLayout<Emitter>::kSample(
      {Effects_package->curves[curve],
       Effects_package->curve_block->Size(curve)});
  if (sampled.clamped) {
    VS::Core::LogDebug("effects",
                       "{} trail emitter: one-entry transparency curve {} "
                       "repeats its entry for the last sample",
                       overlay, curve);
  }
  return sampled.samples;
}

// decomp: src/EFFECT/trails5.h:27 and trails9.h:27 VS_TRAIL_ORIGIN_FUNCTION.
template <typename Emitter>
void GenerateOrigins(const LiveParticleDefinition& definition,
                     LiveParticleEmitterScratch& scratch,
                     Effects_EffectInstance& instance, Emitter& state,
                     const OverlayOf<Emitter>& overlay) {
  if (definition.transform_index >= kTransformCount) {
    InvalidEmitter(overlay.name, "definition transform outside the instance");
  }
  const std::span<TrailOf<Emitter>> trails = Trails(overlay.name, state);
  const MATRIX& matrix = instance.transforms[definition.transform_index].world;
  const SVECTOR& origin = Effects_actionRequest.position;
  const TrailOriginRequest request{
      .flags = scratch.flags,
      .position =
          SampledRange(definition.position_range, definition.position_curve,
                       state.curve_sample_distance),
      .extent = SampledRange(definition.extent_range, definition.extent_curve,
                             state.curve_sample_distance),
      .effect_origin = {origin.vx, origin.vy, origin.vz},
      .caster_translation = {static_cast<std::int32_t>(matrix.t[0]),
                             static_cast<std::int32_t>(matrix.t[1]),
                             static_cast<std::int32_t>(matrix.t[2])},
      .ring_segments = definition.ring_segments,
      .ring_positions = definition.ring_positions};
  Core_Gte_SetRotMatrix(&matrix);
  Core_Gte_ClearTranslation();

  std::array<TrailPoint, kMaxTrailsPerEmitter> points{};
  for (std::size_t index = 0; index < trails.size(); ++index) {
    const SVECTOR& point = trails[index].spline_points[0];
    points.at(index) = {point.vx, point.vy, point.vz};
  }
  TrailOriginScratch values = LoadScratch(scratch);
  LiveOriginServices services;
  GenerateTrailOrigins(request, std::span(points).first(trails.size()),
                       *overlay.corner, values, services);
  StoreScratch(values, scratch);
  for (std::size_t index = 0; index < trails.size(); ++index) {
    SVECTOR& point = trails[index].spline_points[0];
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
    return {.lifetime = state.lifetime,
            .initialized = state.initialized != 0,
            .curve_sample_distance = state.curve_sample_distance};
  }

  void StoreCounters(const TrailEmitterCounters& counters) override {
    Emitter& state = *State();
    state.lifetime = counters.lifetime;
    state.initialized = counters.initialized ? 1 : 0;
    state.curve_sample_distance = counters.curve_sample_distance;
  }

  bool BeginTick() override;

  // decomp: trails5.h:736-742 and trails9.h:745-751 (the first tick runs
  // origin, target, control and init).
  void InitializeTrails() override {
    Emitter& state = *State();
    LiveParticleEmitterScratch& scratch = EmitterScratch();
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
  LiveParticleDefinition* definition_ = nullptr;
  Effects_EffectInstance* instance_ = nullptr;
};

template <typename Emitter>
void LiveTrailEmitterHost<Emitter>::Create(TrailEmitterPlan plan) {
  const LiveParticleDefinition* definition = Definition(plan.definition_index);
  if (definition == nullptr) {
    VS::Core::LogWarn(
        "effects",
        "{} trail emitter: definition {} is outside the package's "
        "{} definitions; emitter refused",
        overlay_.name, plan.definition_index, LiveParticleDefinitionCount());
    return;
  }
  const auto count =
      TrailCountFor(definition->interval_range[0], Layout::kCapacity);
  if (!count) {
    VS::Core::LogWarn("effects",
                      "{} trail emitter: definition {} has a negative trail "
                      "count; emitter refused",
                      overlay_.name, plan.definition_index);
    return;
  }
  auto* state = static_cast<Emitter*>(
      VS::Core::Memory::AllocateLiveFromHighEnd(sizeof(Emitter)));
  if (state == nullptr) {
    VS::Core::LogError("effects", "{} trail emitter: heap exhausted for state",
                       overlay_.name);
    return;
  }
  using Trail = TrailOf<Emitter>;
  auto* trails = static_cast<Trail*>(
      VS::Core::Memory::AllocateLiveFromHighEnd(*count * sizeof(Trail)));
  if (trails == nullptr) {
    VS::Core::Memory::FreeLiveFromHighEnd(state);
    VS::Core::LogError("effects",
                       "{} trail emitter: heap exhausted for {} trails",
                       overlay_.name, *count);
    return;
  }
  // The original left the trail buffer uninitialized; the "no placement" box
  // mode then offset whatever the heap held.
  std::fill_n(trails, *count, Trail{});
  slot_.rendererState = state;
  state->effect = plan.definition_index;
  state->lifetime = plan.lifetime;
  state->curve_sample_distance = 0;
  state->trails = trails;
  state->trail_count = *count;
  state->initialized = 0;
  state->transparency = Transparency<Emitter>(overlay_.name, *definition);
  LiveParticleTextureAnimation& texture = state->texture;
  LiveClearParticleTextureState(&texture);
  LiveInitializeParticleTextureAnimation(&texture, definition->texture_index,
                                         definition->animation_id);
}

template <typename Emitter>
bool LiveTrailEmitterHost<Emitter>::BeginTick() {
  Emitter& state = *State();
  static_cast<void>(Trails(overlay_.name, state));
  instance_ = Effects_currentInstance;
  if (instance_ == nullptr) {
    VS::Core::LogWarn("effects",
                      "{} trail emitter: no current effect instance; emitter "
                      "stopped",
                      overlay_.name);
    return false;
  }
  const std::size_t index = state.effect;
  definition_ = Definition(index);
  if (definition_ == nullptr) {
    VS::Core::LogWarn("effects",
                      "{} trail emitter: the current package no longer holds "
                      "definition {}; emitter stopped",
                      overlay_.name, index);
    return false;
  }
  if (definition_->transform_index >= kTransformCount) {
    VS::Core::LogWarn("effects",
                      "{} trail emitter: definition {} names transform {} of "
                      "{}; emitter stopped",
                      overlay_.name, index, definition_->transform_index,
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
  LiveParticleEmitterScratch& scratch = EmitterScratch();
  Layout::kRunStage(overlay_.stages->update, *definition_, scratch, *instance_,
                    state);
  Layout::kRunStage(overlay_.stages->render, *definition_, scratch, *instance_,
                    state);
  if (definition_->tick_event == 0) return;
  const std::size_t event = definition_->tick_event - 1U;
  const std::size_t bound = LiveParticleDefinitionCount();
  if (event >= bound) {
    VS::Core::LogWarn(
        "effects",
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
      std::tuple_size_v<decltype(TrailOf<Emitter>::spline_samples)>;
  LiveParticleEmitterScratch& scratch = EmitterScratch();
  const MATRIX& world =
      instance_->transforms[definition_->transform_index].world;
  const bool world_events = (definition_->flags & kTrailWorldEvents) != 0;
  for (TrailOf<Emitter>& trail : Trails(overlay_.name, *State())) {
    if (trail.current_spline_sample >= kSplineSampleCount) {
      InvalidEmitter(overlay_.name,
                     "spline sample cursor outside the trail history");
    }
    const SVECTOR& sample = trail.spline_samples[trail.current_spline_sample];
    TrailVector point{sample.vx, sample.vy, sample.vz};
    if (world_events) {
      Core_Gte_SetRotMatrix(&world);
      Core_Gte_SetTransMatrix(&world);
      scratch.vector.vx = sample.vx;
      scratch.vector.vy = sample.vy;
      scratch.vector.vz = sample.vz;
      Core_Gte_LoadV0(&scratch.vector);
      Core_Gte_Rtv0Tr();
      Core_Gte_StoreLvnl(&scratch.aim_point);
      point = Wide(scratch.aim_point);
    }
    // The original left the context's other fields, including its variant,
    // as stack contents; they start cleared here.
    LiveParticle context{};
    context.transparency = 0;
    Store(context.motion.position, TrailEventPosition(point));
    EmitLiveParticleEvent(instance_, static_cast<int>(event), 0, 0, &context);
  }
}

template <typename Emitter>
int UpdateEmitter(Effects_RendererSlot* slot, unsigned int command, int packed,
                  const OverlayOf<Emitter>& overlay) {
  if (overlay.corner == nullptr || overlay.stages == nullptr) {
    InvalidEmitter(overlay.name, "overlay binding is incomplete");
  }
  // EFFPURGE's update sweep (F01889) calls every entry without a slot.
  if (slot == nullptr) {
    VS::Core::LogError(
        "effects", "{} trail emitter: renderer called without a slot; ignored",
        overlay.name);
    return 0;
  }
  LiveTrailEmitterHost<Emitter> host(*slot, overlay);
  return UpdateTrailEmitter(static_cast<TrailEmitterCommand>(command), packed,
                            host)
             ? 1
             : 0;
}
}  // namespace

// decomp: src/EFFECT/trails5.h:27 VS_TRAIL_ORIGIN_FUNCTION.
void GenerateLiveTrailOrigins(const LiveParticleDefinition& definition,
                              LiveParticleEmitterScratch& scratch,
                              Effects_EffectInstance& instance,
                              LiveTrailState& state,
                              const LiveTrailOverlay& overlay) {
  GenerateOrigins(definition, scratch, instance, state, overlay);
}

// decomp: src/EFFECT/trails5.h:677 VS_TRAIL_DISPATCH_FUNCTION.
int UpdateLiveTrailEmitter(Effects_RendererSlot* slot, unsigned int command,
                           int packed, const LiveTrailOverlay& overlay) {
  return UpdateEmitter<LiveTrailState>(slot, command, packed, overlay);
}

// decomp: src/EFFECT/trails9.h:27 VS_TRAIL_ORIGIN_FUNCTION.
void GenerateLiveTrailNineOrigins(const LiveParticleDefinition& definition,
                                  LiveParticleEmitterScratch& scratch,
                                  Effects_EffectInstance& instance,
                                  LiveTrailNineState& state,
                                  const LiveTrailNineOverlay& overlay) {
  GenerateOrigins(definition, scratch, instance, state, overlay);
}

// decomp: src/EFFECT/trails9.h:685 VS_TRAIL_DISPATCH_FUNCTION.
int UpdateLiveTrailNineEmitter(Effects_RendererSlot* slot, unsigned int command,
                               int packed,
                               const LiveTrailNineOverlay& overlay) {
  return UpdateEmitter<LiveTrailNineState>(slot, command, packed, overlay);
}

// decomp: PLG074 func_800FB414 (src/EFFECT/trails5.h:27).
void GenerateLivePlg074TrailOrigins(const LiveParticleDefinition& definition,
                                    LiveParticleEmitterScratch& scratch,
                                    Effects_EffectInstance& instance,
                                    LiveTrailState& state) {
  GenerateLiveTrailOrigins(
      definition, scratch, instance, state,
      {"PLG074", &VS::Content::plg074TrailEmissionCounter[0], &Plg074Stages()});
}

// decomp: PLG074 func_800FCFEC (src/EFFECT/trails5.h:677).
int UpdateLivePlg074TrailEmitter(Effects_RendererSlot* slot,
                                 unsigned int command, int packed) {
  return UpdateLiveTrailEmitter(
      slot, command, packed,
      {"PLG074", &VS::Content::plg074TrailEmissionCounter[0], &Plg074Stages()});
}
}  // namespace VS::Effects
