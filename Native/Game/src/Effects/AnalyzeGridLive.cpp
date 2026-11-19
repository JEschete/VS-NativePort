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
#include "Core/Scratch.hpp"
#include "Effects/AnalyzeGrid.hpp"
#include "Effects/CurvesLive.hpp"
#include "Effects/Package.hpp"
#include "Effects/PackageExtents.hpp"
#include "Effects/Particles.hpp"
#include "Effects/Runtime.h"
#include "Effects/TransformsLiveState.hpp"
#include "Render/Camera.hpp"
#include "Render/Render.hpp"
#include "Render/SpritesLive.hpp"
#include "Render/UiPacketsLive.hpp"
#include "Render/UiPrimitives.hpp"

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

static_assert(sizeof(LiveParticleDefinition) == kParticleDefinitionBytes);
static_assert(std::is_trivially_destructible_v<AnalyzeGridState>);

std::uint64_t refusals = 0;

void Refuse(std::string_view detail) {
  if (std::has_single_bit(++refusals)) {
    VS::Core::LogWarn("effects", "Analyze grid: {} ({} refusals)", detail,
                      refusals);
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

grid::SurfaceDefinition DefinitionOf(const LiveParticleDefinition& source) {
  const auto& vectors = source.vector_ranges;
  const auto& pairs = source.pair_ranges;
  return {.flags = source.flags,
          .transparency_curve =
              static_cast<std::uint8_t>(source.transparency_curve),
          .ring_segments = source.ring_segments,
          .ring_positions = source.ring_positions,
          .curves = {.position = source.position_curve,
                     .extent = source.extent_curve,
                     .inner_extent = source.inner_extent_curve,
                     .direction = source.direction_curve,
                     .direction_jitter = source.direction_jitter_curve,
                     .speed = source.speed_curve,
                     .force = source.force_curve,
                     .acceleration = source.acceleration_curve,
                     .mass = source.mass_curve,
                     .gravity = source.gravity_scale_curve,
                     .color = {source.r_curve, source.g_curve, source.b_curve}},
          .position = VectorRange(source.position_range),
          .extent = VectorRange(source.extent_range),
          .inner_extent = math::RangePair(source.inner_extent_range, 0),
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

const LiveParticleDefinition* FindDefinition(AnalyzeGridDefinitionId id) {
  if (Effects_package == nullptr ||
      Effects_package->particle_definitions == nullptr ||
      id.value >= LiveParticleDefinitionCount()) {
    Refuse("definition outside the bound package");
    return nullptr;
  }
  return &Effects_package->particle_definitions->Definitions()[id.value];
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

LiveParticleEmitterScratch& EmitterScratch() {
  return *reinterpret_cast<LiveParticleEmitterScratch*>(
      static_cast<std::byte*>(VS::Core::ScratchBase()) + kEmitterScratchOffset);
}

class LiveAnalyzeGridHost final : public AnalyzeGridHost {
 public:
  explicit LiveAnalyzeGridHost(Effects_RendererSlot& slot) : slot_(slot) {}

  std::optional<grid::SurfaceDefinition> CreateDefinition(
      AnalyzeGridDefinitionId id) override {
    const LiveParticleDefinition* definition = FindDefinition(id);
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
        definition_->transform_index >= kTransformCount) {
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
        instance_->transforms[definition_->transform_index].world;
    const SVECTOR& origin = Effects_actionRequest.position;
    return grid::SurfaceTickInputs{
        .definition = definition,
        .effect_origin = {origin.vx, origin.vy, origin.vz},
        .actor_translation = {static_cast<std::int32_t>(world.t[0]),
                              static_cast<std::int32_t>(world.t[1]),
                              static_cast<std::int32_t>(world.t[2])},
        .near_clip = VS::Render::nearClip};
  }
  std::int32_t SampleCurve(std::uint8_t curve, std::int32_t age) override {
    return curve == 0 ? 0 : LiveSampleEffectCurve(curve, age);
  }
  void PublishScratch(const grid::SurfaceScratch& values) override;
  void BeginProjection(bool effect_view) override {
    if (effect_view) {
      const MATRIX& view =
          instance_->transforms[definition_->transform_index].view;
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
  const LiveParticleDefinition* definition_ = nullptr;
  const Effects_EffectInstance* instance_ = nullptr;
};

void LiveAnalyzeGridHost::PublishScratch(const grid::SurfaceScratch& values) {
  LiveParticleEmitterScratch& scratch = EmitterScratch();
  scratch.flags = values.flags;
  math::StoreVector(scratch.position, values.position);
  math::StoreVector(scratch.extent, values.extent);
  if (values.forces.has_value()) {
    math::StoreVector(scratch.force_min, values.forces->force_min);
    math::StoreVector(scratch.force_max, values.forces->force_max);
    math::StoreVector(scratch.acceleration_min,
                      values.forces->acceleration_min);
    math::StoreVector(scratch.acceleration_max,
                      values.forces->acceleration_max);
  }
  if (values.base_direction.has_value()) {
    math::StoreVector(scratch.base_direction, *values.base_direction);
  }
  if (values.direction_jitter.has_value()) {
    math::StoreVector(scratch.direction_jitter, *values.direction_jitter);
  }
  if (values.speed.has_value()) {
    scratch.speed_range[0] = values.speed->x;
    scratch.speed_range[1] = values.speed->y;
  }
  if (values.mass.has_value()) {
    scratch.mass_range[0] = values.mass->x;
    scratch.mass_range[1] = values.mass->y;
  }
  if (values.gravity.has_value()) {
    scratch.gravity_scale_range[0] = values.gravity->x;
    scratch.gravity_scale_range[1] = values.gravity->y;
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
  VS::Render::FrameScratchpad().primitive_cursor = packet + words.size();
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
