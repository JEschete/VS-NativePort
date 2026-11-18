#include "Effects/AnalyzeGridLive.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <type_traits>

#include "Core/Gte.h"
#include "Core/Log.hpp"
#include "Core/Math/InterpolationLive.hpp"
#include "Core/Math/ScalarMath.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Core/Scratch.h"
#include "Effects/AnalyzeGrid.hpp"
#include "Effects/CurvesLive.hpp"
#include "Effects/Package.h"
#include "Effects/PackageExtents.hpp"
#include "Effects/Particles.h"
#include "Effects/Runtime.h"
#include "Effects/Transforms.h"
#include "Render/Camera.h"
#include "Render/Render.h"
#include "Render/SpritesLive.hpp"
#include "Render/UiPacketsLive.hpp"
#include "Render/UiPrimitives.h"

namespace VS::Effects {
namespace {
namespace math = Core::Math;
namespace grid = Templates;

constexpr std::size_t kEmitterScratchOffset = 0x1D0;
using InstanceTransforms = decltype(Effects_EffectInstance::transforms);
constexpr std::size_t kTransformCount = std::extent_v<InstanceTransforms>;
constexpr unsigned kHalfwordBits = 16;
constexpr std::size_t kDirectionRange = 0;
constexpr std::size_t kJitterRange = 1;
constexpr std::size_t kForceMinRange = 2;
constexpr std::size_t kForceMaxRange = 3;
constexpr std::size_t kAccelerationMinRange = 4;
constexpr std::size_t kAccelerationMaxRange = 5;
constexpr std::size_t kMassRange = 0;
constexpr std::size_t kGravityRange = 1;
constexpr std::size_t kSpeedRange = 2;
constexpr std::size_t kTextureOriginRange = 3;
constexpr std::size_t kSecondPlaneComponent = 2;
constexpr std::size_t kUsedCurves = 13;

static_assert(sizeof(Effects_ParticleDefinition) == kParticleDefinitionBytes);
static_assert(std::is_trivially_destructible_v<AnalyzeGridState>);

std::uint64_t refusals = 0;

void Refuse(std::string_view detail) {
  if (std::has_single_bit(++refusals)) {
    VS_LOG_WARN("effects", "Analyze grid: {} ({} refusals)", detail, refusals);
  }
}

grid::RingVectorRange VectorRange(
    std::span<const std::int16_t, math::kVectorRangeHalfwords> values) {
  return {math::RangePair(values, 0), math::RangePair(values, 1),
          math::RangePair(values, 2)};
}

std::array<math::ScalarSpan, 2> PlaneRange(
    std::span<const std::int16_t, math::kPlaneRangeHalfwords> values) {
  return {math::RangePair(values, 0), math::RangePair(values, 1)};
}

grid::SurfaceDefinition DefinitionOf(const Effects_ParticleDefinition& source) {
  const auto& vectors = source.vectorRanges;
  const auto& pairs = source.pairRanges;
  return {
      .flags = source.flags,
      .transparency_curve = static_cast<std::uint8_t>(source.transparencyCurve),
      .ring_segments = source.ringSegments,
      .ring_positions = source.ringPositions,
      .curves = {.position = source.positionCurve,
                 .extent = source.extentCurve,
                 .inner_extent = source.innerExtentCurve,
                 .direction = source.directionCurve,
                 .direction_jitter = source.directionJitterCurve,
                 .speed = source.speedCurve,
                 .force = source.forceCurve,
                 .acceleration = source.accelerationCurve,
                 .mass = source.massCurve,
                 .gravity = source.gravityScaleCurve,
                 .color = {source.rCurve, source.gCurve, source.bCurve}},
      .position = VectorRange(source.positionRange),
      .extent = VectorRange(source.extentRange),
      .inner_extent = math::RangePair(source.innerExtentRange, 0),
      .direction = VectorRange(vectors[kDirectionRange]),
      .direction_jitter = VectorRange(vectors[kJitterRange]),
      .force_min = VectorRange(vectors[kForceMinRange]),
      .force_max = VectorRange(vectors[kForceMaxRange]),
      .acceleration_min = VectorRange(vectors[kAccelerationMinRange]),
      .acceleration_max = VectorRange(vectors[kAccelerationMaxRange]),
      .speed = PlaneRange(pairs[kSpeedRange]),
      .mass = PlaneRange(pairs[kMassRange]),
      .gravity = PlaneRange(pairs[kGravityRange]),
      .grid_u = static_cast<std::uint8_t>(pairs[kTextureOriginRange][0]),
      .grid_v = static_cast<std::uint8_t>(
          pairs[kTextureOriginRange][kSecondPlaneComponent]),
      .grid_width = pairs[kMassRange][0],
      .grid_height = pairs[kMassRange][kSecondPlaneComponent]};
}

const Effects_ParticleDefinition* FindDefinition(AnalyzeGridDefinitionId id) {
  if (Effects_package == nullptr || Effects_package->block5Data == nullptr ||
      id.value >= LiveParticleDefinitionCount()) {
    Refuse("definition outside the bound package");
    return nullptr;
  }
  return &Effects_package->block5Data->definitions[id.value];
}

bool CurvesAvailable(const grid::SurfaceCurves& curves, std::int32_t age) {
  const std::array<std::uint8_t, kUsedCurves> ids{
      curves.position,  curves.extent,           curves.inner_extent,
      curves.direction, curves.direction_jitter, curves.speed,
      curves.force,     curves.acceleration,     curves.mass,
      curves.gravity,   curves.color[0],         curves.color[1],
      curves.color[2]};
  return std::ranges::all_of(ids, [age](std::uint8_t id) {
    return id == 0 || !IsCurveRefusal(SampleLiveEffectCurve(id, age).status);
  });
}

Effects_ParticleEmitterScratch& EmitterScratch() {
  return *reinterpret_cast<Effects_ParticleEmitterScratch*>(
      static_cast<std::byte*>(Core_ScratchBase()) + kEmitterScratchOffset);
}

class LiveAnalyzeGridHost final : public AnalyzeGridHost {
 public:
  explicit LiveAnalyzeGridHost(Effects_RendererSlot& slot) : slot_(slot) {}

  std::optional<grid::SurfaceDefinition> CreateDefinition(
      AnalyzeGridDefinitionId id) override {
    const Effects_ParticleDefinition* definition = FindDefinition(id);
    return definition != nullptr ? std::optional(DefinitionOf(*definition))
                                 : std::nullopt;
  }
  bool Create(const AnalyzeGridState& state) override {
    if (slot_.rendererState != nullptr) {
      AnalyzeGridState* live = LiveState();
      if (live == nullptr) {
        return false;
      }
      *live = state;
      return true;
    }
    void* storage = Core::Memory::AllocateLiveFromHighEnd(sizeof(state));
    if (storage == nullptr) {
      Refuse("state allocation failed");
      return false;
    }
    slot_.rendererState =
        std::construct_at(static_cast<AnalyzeGridState*>(storage), state);
    return true;
  }
  std::optional<AnalyzeGridState> State() const override {
    const AnalyzeGridState* state = LiveState();
    return state != nullptr ? std::optional(*state) : std::nullopt;
  }
  void StoreState(const AnalyzeGridState& state) override {
    AnalyzeGridState* live = LiveState();
    if (live != nullptr) {
      *live = state;
    }
  }
  std::optional<grid::SurfaceTickInputs> BeginTick(
      AnalyzeGridDefinitionId id) override {
    definition_ = FindDefinition(id);
    instance_ = Effects_currentInstance;
    if (definition_ == nullptr || instance_ == nullptr ||
        definition_->transformIndex >= kTransformCount) {
      Refuse("missing instance or transform outside the instance");
      return std::nullopt;
    }
    const AnalyzeGridState* state = LiveState();
    const grid::SurfaceDefinition definition = DefinitionOf(*definition_);
    if (state == nullptr || !CurvesAvailable(definition.curves, state->age)) {
      Refuse("missing state or unsampleable curve");
      return std::nullopt;
    }
    const MATRIX& world =
        instance_->transforms[definition_->transformIndex].world;
    const SVECTOR& origin = Effects_actionRequest.position;
    return grid::SurfaceTickInputs{
        .definition = definition,
        .effect_origin = {origin.vx, origin.vy, origin.vz},
        .actor_translation = {static_cast<std::int32_t>(world.t[0]),
                              static_cast<std::int32_t>(world.t[1]),
                              static_cast<std::int32_t>(world.t[2])},
        .near_clip = Render_nearClip};
  }
  std::int32_t SampleCurve(std::uint8_t curve, std::int32_t age) override {
    return curve == 0 ? 0 : LiveSampleEffectCurve(curve, age);
  }
  void PublishScratch(const grid::SurfaceScratch& values) override;
  void BeginProjection(bool effect_view) override {
    if (effect_view) {
      const MATRIX& view =
          instance_->transforms[definition_->transformIndex].view;
      Core_Gte_SetRotMatrix(&view);
      Core_Gte_SetTransMatrix(&view);
    } else {
      Render::LoadLiveFrameViewMatrix();
    }
  }
  AnalyzeGridProjection Project(grid::RingPoint point) override {
    const SVECTOR vertex{point.x, point.y, point.z, 0};
    Core_GteWord screen = 0;
    Core_GteWord depth = 0;
    Core_Gte_LoadV0(&vertex);
    Core_Gte_Rtps();
    Core_Gte_StoreSxy2(&screen);
    Core_Gte_StoreSz3Otz(&depth);
    const auto xy = static_cast<std::uint32_t>(screen);
    return {{math::WrapSigned16(xy), math::WrapSigned16(xy >> kHalfwordBits)},
            static_cast<std::int32_t>(depth)};
  }
  void Draw(const AnalyzeGridQuad& quad) override;

 private:
  AnalyzeGridState* LiveState() const {
    const auto capacity =
        Core::Memory::LivePayloadCapacity(slot_.rendererState);
    if (!capacity.has_value() || *capacity < sizeof(AnalyzeGridState)) {
      Refuse("state is absent or outside a complete heap payload");
      return nullptr;
    }
    return static_cast<AnalyzeGridState*>(slot_.rendererState);
  }

  Effects_RendererSlot& slot_;
  const Effects_ParticleDefinition* definition_ = nullptr;
  const Effects_EffectInstance* instance_ = nullptr;
};

void LiveAnalyzeGridHost::PublishScratch(const grid::SurfaceScratch& values) {
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
}

void LiveAnalyzeGridHost::Draw(const AnalyzeGridQuad& quad) {
  u_long* entry = Render::LiveFrameOrderingEntry(
      Render::LiveOrderingBase::kScene, quad.depth);
  u_long* packet =
      entry != nullptr
          ? Render::ReserveLivePrimitiveWords(grid::kGridPacketWords)
          : nullptr;
  if (packet == nullptr) {
    Refuse("quad outside the ordering table or primitive capacity");
    return;
  }
  grid::GridQuad encoded{.color = quad.color,
                         .uvs = quad.texture_coordinates,
                         .clut = quad.palette,
                         .tpage = quad.texture_page};
  for (std::size_t corner = 0; corner < quad.corners.size(); ++corner) {
    const math::Vec2 at = quad.corners.at(corner);
    encoded.screens.at(corner) =
        static_cast<std::uint16_t>(at.x) |
        (static_cast<std::uint32_t>(static_cast<std::uint16_t>(at.y))
         << kHalfwordBits);
  }
  const auto words = grid::EncodeGridPacket(encoded);
  std::ranges::copy(words, packet);
  packet[0] |= *entry & grid::kRingLinkMask;
  *entry = (*entry & ~grid::kRingLinkMask) |
           (LegacyGpu_Handle(packet) & grid::kRingLinkMask);
  Render_frameScratch.primitiveCursor = packet + words.size();
}

using AnalyzeGridEntry = bool (*)(AnalyzeGridCommand, AnalyzeGridPlan,
                                  AnalyzeGridHost&);

int RunLiveAnalyzeGrid(Effects_RendererSlot* slot, unsigned int command,
                       int packed, AnalyzeGridEntry entry) {
  if (slot == nullptr) {
    Refuse("renderer called without a slot");
    return 0;
  }
  if (command < static_cast<unsigned int>(AnalyzeGridCommand::kCreate) ||
      command > static_cast<unsigned int>(AnalyzeGridCommand::kStop)) {
    return 1;
  }
  const grid::RingPlan decoded = grid::PlanSurface(packed);
  LiveAnalyzeGridHost host(*slot);
  return entry(static_cast<AnalyzeGridCommand>(command),
               {decoded.lifetime, {decoded.definition_index}}, host)
             ? 1
             : 0;
}
}  // namespace

// decomp: vs_effect_analyze (src/EFFECT/PLG005.BIN/18.c:35).
int UpdateLivePlg005AnalyzeGrid(Effects_RendererSlot* slot,
                                unsigned int command, int packed) {
  return RunLiveAnalyzeGrid(slot, command, packed, Plg005::UpdateAnalyzeGrid);
}

// decomp: func_800F9818 (src/EFFECT/PLG054.BIN/18.c:3).
int UpdateLivePlg054AnalyzeGrid(Effects_RendererSlot* slot,
                                unsigned int command, int packed) {
  return RunLiveAnalyzeGrid(slot, command, packed, Plg054::UpdateAnalyzeGrid);
}

// decomp: func_800F9818 (src/EFFECT/PLG055.BIN/18.c:3).
int UpdateLivePlg055AnalyzeGrid(Effects_RendererSlot* slot,
                                unsigned int command, int packed) {
  return RunLiveAnalyzeGrid(slot, command, packed, Plg055::UpdateAnalyzeGrid);
}

// decomp: func_800FE3EC (src/EFFECT/PLG074.BIN/F8.c:23).
int UpdateLivePlg074AnalyzeGrid(Effects_RendererSlot* slot,
                                unsigned int command, int packed) {
  return RunLiveAnalyzeGrid(slot, command, packed, Plg074::UpdateAnalyzeGrid);
}
}  // namespace VS::Effects
