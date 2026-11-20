// Hand-maintained (tools/port/owned.csv).
#include "Effects/Templates/RingsLive.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <type_traits>
#include <utility>

#include "Core/Gte.hpp"
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
#include "Effects/Plugins/Plg044.hpp"
#include "Effects/Plugins/Plg047.hpp"
#include "Effects/Plugins/Plg049.hpp"
#include "Effects/Plugins/Plg050.hpp"
#include "Effects/Plugins/Plg051.hpp"
#include "Effects/Plugins/Plg053.hpp"
#include "Effects/Plugins/Plg061.hpp"
#include "Effects/Plugins/Plg065.hpp"
#include "Effects/Plugins/Plg069.hpp"
#include "Effects/Plugins/Plg073.hpp"
#include "Effects/Plugins/Plg081.hpp"
#include "Effects/Plugins/Plg082.hpp"
#include "Effects/Plugins/Plg085.hpp"
#include "Effects/Plugins/Plg086.hpp"
#include "Effects/Plugins/Plg087.hpp"
#include "Effects/Plugins/Plg128.hpp"
#include "Effects/Plugins/Plg129.hpp"
#include "Effects/Plugins/Plg134.hpp"
#include "Effects/Plugins/Plg135.hpp"
#include "Effects/Plugins/Plg137.hpp"
#include "Effects/Plugins/Plg138.hpp"
#include "Effects/Plugins/Plg142.hpp"
#include "Effects/Plugins/Plg143.hpp"
#include "Effects/Plugins/Plg146.hpp"
#include "Effects/Plugins/Plg147.hpp"
#include "Effects/Plugins/Plg148.hpp"
#include "Effects/Plugins/Plg149.hpp"
#include "Effects/Plugins/Plg150.hpp"
#include "Effects/Plugins/Plg153.hpp"
#include "Effects/Plugins/Plg154.hpp"
#include "Effects/Plugins/Plg158.hpp"
#include "Effects/Plugins/Plg160.hpp"
#include "Effects/Plugins/Plg161.hpp"
#include "Effects/Plugins/Plg162.hpp"
#include "Effects/Plugins/Plg166.hpp"
#include "Effects/Plugins/Plg167.hpp"
#include "Effects/Plugins/Plg169.hpp"
#include "Effects/Plugins/Plg170.hpp"
#include "Effects/Plugins/Plg236.hpp"
#include "Effects/Plugins/Plg242.hpp"
#include "Effects/Plugins/Plg250.hpp"
#include "Effects/Plugins/Plg263.hpp"
#include "Effects/Plugins/Plg265.hpp"
#include "Effects/Plugins/Plg272.hpp"
#include "Effects/Plugins/Plg291.hpp"
#include "Effects/Plugins/Plg297.hpp"
#include "Effects/Plugins/Plg301.hpp"
#include "Effects/Plugins/PluginData.hpp"
#include "Effects/Runtime.hpp"
#include "Effects/Templates/ClosedRing.hpp"
#include "Effects/Templates/RepeatingRingsLive.hpp"
#include "Effects/Templates/Ring.hpp"
#include "Effects/Templates/RingCore.hpp"
#include "Effects/Templates/TemplateLiveState.hpp"
#include "Effects/Templates/TiledRing.hpp"
#include "Effects/TransformsLiveState.hpp"
#include "Legacy/Psx.hpp"
#include "Render/Camera.hpp"
#include "Render/Frame.hpp"
#include "Render/Render.hpp"
#include "Render/SpritesLive.hpp"
#include "Render/UiPacketsLive.hpp"
#include "Render/UiPrimitives.hpp"

using VS::Core::Gte::CVECTOR;
using VS::Core::Gte::LoadV0;
using VS::Core::Gte::MATRIX;
using VS::Core::Gte::Rtps;
using VS::Core::Gte::Rtv0Tr;
using VS::Core::Gte::SetRotMatrix;
using VS::Core::Gte::SetTransMatrix;
using VS::Core::Gte::StoreLvnl;
using VS::Core::Gte::StoreSxy2;
using VS::Core::Gte::StoreSz3Otz;
using VS::Core::Gte::SVECTOR;
using VS::Core::Gte::VECTOR;

using VS::Legacy::GpuHandle;

namespace VS::Effects::Templates {
namespace {
namespace math = Core::Math;
using math::Vec3;

constexpr std::uintptr_t kEmitterScratchOffset = 0x1D0;
using InstanceTransforms = decltype(LiveEffectInstance::transforms);
using TextureTables = decltype(LiveEffectPackage::texture_tables);
constexpr std::size_t kTransformCount = std::tuple_size_v<InstanceTransforms>;
constexpr std::size_t kTextureTableCount = std::tuple_size_v<TextureTables>;
constexpr std::uint32_t kOrderingHighMask = ~kRingLinkMask;
// vectorRanges[] and pairRanges[] rows of a particle definition the rings
// read.
constexpr std::size_t kDirectionRange = 0;
constexpr std::size_t kDirectionJitterRange = 1;
constexpr std::size_t kForceMinRange = 2;
constexpr std::size_t kForceMaxRange = 3;
constexpr std::size_t kAccelerationMinRange = 4;
constexpr std::size_t kAccelerationMaxRange = 5;
constexpr std::size_t kSpeedRange = 2;

static_assert(sizeof(LiveParticleDefinition) == kParticleDefinitionBytes);

std::uint64_t refused_curves = 0;
std::uint64_t refused_textures = 0;
std::uint64_t dropped_quads = 0;

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
const LiveParticleDefinition* Definition(std::size_t index) {
  if (VS::Effects::package == nullptr ||
      VS::Effects::package->particle_definitions == nullptr ||
      index >= LiveParticleDefinitionCount()) {
    return nullptr;
  }
  return &VS::Effects::package->particle_definitions->Definitions()[index];
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

[[nodiscard]] RingDefinition TypedDefinition(
    const LiveParticleDefinition& definition) {
  const auto& vectors = definition.vector_ranges;
  const std::span<const std::int16_t> speed =
      definition.pair_ranges[kSpeedRange];
  return {
      .flags = definition.flags,
      // The palette byte offset is (byte 7 & 0x1C): the transparency
      // field's low three bits.
      .palette_index = static_cast<std::uint8_t>(definition.transparency_curve),
      .tick_event = definition.tick_event,
      .curves = {.position = definition.position_curve,
                 .extent = definition.extent_curve,
                 .inner_extent = definition.inner_extent_curve,
                 .direction = definition.direction_curve,
                 .direction_jitter = definition.direction_jitter_curve,
                 .speed = definition.speed_curve,
                 .force = definition.force_curve,
                 .acceleration = definition.acceleration_curve,
                 .color = {definition.r_curve, definition.g_curve,
                           definition.b_curve}},
      .position = VectorRange(definition.position_range),
      .extent = VectorRange(definition.extent_range),
      .inner_extent = math::RangePair(definition.inner_extent_range, 0),
      .direction = VectorRange(vectors[kDirectionRange]),
      .direction_jitter = VectorRange(vectors[kDirectionJitterRange]),
      .force_min = VectorRange(vectors[kForceMinRange]),
      .force_max = VectorRange(vectors[kForceMaxRange]),
      .acceleration_min = VectorRange(vectors[kAccelerationMinRange]),
      .acceleration_max = VectorRange(vectors[kAccelerationMaxRange]),
      .speed = {math::RangePair(speed, 0), math::RangePair(speed, 1)}};
}

using RingPaletteEntries = std::array<RingPaletteEntry, kRingPaletteEntries>;

// Row intensities from the first three bytes of each palette entry.
[[nodiscard]] RingPaletteEntries PaletteOf(
    const plugins::RingPaletteBytes& entries) {
  RingPaletteEntries palette{};
  for (std::size_t index = 0; index < palette.size(); ++index) {
    const std::span<const std::uint8_t, plugins::kRingPaletteEntryBytes> entry =
        entries[index];
    palette.at(index).rows = {entry[0], entry[1], entry[2]};
  }
  return palette;
}

[[nodiscard]] RingPaletteEntries PaletteOf(
    const plugins::RingPalette& entries) {
  RingPaletteEntries palette{};
  for (std::size_t index = 0; index < palette.size(); ++index) {
    const CVECTOR& entry = entries[index];
    palette.at(index).rows = {entry.r, entry.g, entry.b};
  }
  return palette;
}

class LiveRingHost final : public RingHost {
 public:
  explicit LiveRingHost(LiveRendererSlot& slot) : slot_(slot) {}

  void Create(const RingState& state, RingTextureSource source) override;

  [[nodiscard]] std::optional<RingState> State() const override {
    const LiveRingState* live = LiveState();
    if (live == nullptr) {
      return std::nullopt;
    }
    return RingState{.lifetime = live->lifetime,
                     .definition_index = live->effect,
                     .radius = live->radius,
                     .radius_speed = live->radius_speed,
                     .height = live->height,
                     .height_speed = live->height_speed,
                     .angle = live->angle,
                     .age = live->age};
  }

  void StoreState(const RingState& state) override {
    LiveRingState* live = LiveState();
    if (live == nullptr) {
      return;
    }
    live->lifetime = state.lifetime;
    live->effect = state.definition_index;
    live->radius = state.radius;
    live->radius_speed = state.radius_speed;
    live->height = state.height;
    live->height_speed = state.height_speed;
    live->angle = state.angle;
    live->age = state.age;
  }

  [[nodiscard]] std::optional<RingTickInputs> BeginTick(
      std::uint8_t definition_index) override;
  [[nodiscard]] std::int32_t SampleCurve(std::uint8_t curve,
                                         std::int32_t distance) override;

  [[nodiscard]] std::int32_t Rand() override {
    return VS::Core::NextBiosRand();
  }

  void PublishScratch(const RingScratch& values) override;

  void BeginProjection(bool effect_view) override {
    if (!effect_view) {
      Render::LoadLiveFrameViewMatrix();
      return;
    }
    const MATRIX& view =
        instance_->transforms[definition_->transform_index].view;
    SetRotMatrix(&view);
    SetTransMatrix(&view);
  }

  [[nodiscard]] RingProjection Project(RingPoint point) override {
    const SVECTOR vertex{point.x, point.y, point.z, 0};
    LoadV0(&vertex);
    Rtps();
    VS::Core::Gte::Word screen = 0;
    VS::Core::Gte::Word depth = 0;
    StoreSxy2(&screen);
    StoreSz3Otz(&depth);
    return {.screen = static_cast<std::uint32_t>(screen),
            .depth = static_cast<std::int32_t>(depth)};
  }

  [[nodiscard]] std::optional<RingTexture> AdvanceTexture() override;
  void Submit(const RingQuad& quad, std::int32_t depth) override;
  [[nodiscard]] Vec3 TransformByActor(RingPoint point) override;
  void EmitParticleEvent(std::uint8_t event, Vec3 position) override;

 private:
  [[nodiscard]] LiveRingState* LiveState() const {
    return static_cast<LiveRingState*>(slot_.renderer_state);
  }

  LiveRendererSlot& slot_;
  const LiveParticleDefinition* definition_ = nullptr;
  LiveEffectInstance* instance_ = nullptr;
};

// decomp: rings.h:41-55, closedRings.h:63-75 and tiledRings.h:57-69 (the
// create command). The original indexed the definition and allocated the
// state unchecked and left reserved_2 as the heap held it.
void LiveRingHost::Create(const RingState& state, RingTextureSource source) {
  const LiveParticleDefinition* definition = Definition(state.definition_index);
  if (definition == nullptr) {
    VS::Core::LogWarn("effects",
                      "ring: definition {} is outside the package's {} "
                      "definitions; ring refused",
                      state.definition_index, LiveParticleDefinitionCount());
    return;
  }
  auto* live = static_cast<LiveRingState*>(
      VS::Core::Memory::AllocateLiveFromHighEnd(sizeof(LiveRingState)));
  if (live == nullptr) {
    VS::Core::LogError("effects", "ring: heap exhausted for state");
    return;
  }
  *live = LiveRingState{};
  slot_.renderer_state = live;
  StoreState(state);
  const bool segments = source == RingTextureSource::kSegmentsAndPositions;
  const int table =
      segments ? definition->ring_segments : definition->texture_index;
  const int program =
      segments ? definition->ring_positions : definition->animation_id;
  // The native closed ring's "effects.texture" diagnostics, for every ring.
  const bool primary = VS::Effects::package == &actionPackage;
  const unsigned request_effect =
      primary ? actionRequest.effect_id : secondaryRequest.effect_id;
  const int tables = VS::Effects::package->texture_table_count;
  VS::Core::LogDebug("effects",
                     "ring: init definition={} lifetime={} table={} program={} "
                     "context={} request_effect={} tables={}",
                     state.definition_index, state.lifetime, table, program,
                     primary ? "primary" : "secondary", request_effect, tables);
  if (table >= tables || std::cmp_greater_equal(table, kTextureTableCount) ||
      VS::Effects::package->texture_tables[table] == nullptr) {
    VS::Core::LogDebug(
        "effects", "ring: definition {} names unbound texture table {} of {}",
        state.definition_index, table, tables);
  }
  // Clearing the record also cleared the texture animation, as
  // func_800D6CCC did.
  LiveInitializeParticleTextureAnimation(&live->texture, table, program);
}

std::optional<RingTickInputs> LiveRingHost::BeginTick(
    std::uint8_t definition_index) {
  instance_ = currentInstance;
  if (instance_ == nullptr) {
    VS::Core::LogWarn("effects",
                      "ring: no current effect instance; ring stopped");
    return std::nullopt;
  }
  definition_ = Definition(definition_index);
  if (definition_ == nullptr) {
    VS::Core::LogWarn(
        "effects",
        "ring: the current package no longer holds definition {}; "
        "ring stopped",
        definition_index);
    return std::nullopt;
  }
  if (definition_->transform_index >= kTransformCount) {
    VS::Core::LogWarn(
        "effects", "ring: definition {} names transform {} of {}; ring stopped",
        definition_index, definition_->transform_index, kTransformCount);
    return std::nullopt;
  }
  // The original stored the flags before sampling anything.
  EmitterScratch().flags = definition_->flags;
  const MATRIX& world =
      instance_->transforms[definition_->transform_index].world;
  // The effect origin (D_800F5310): the action request's focus point.
  const SVECTOR& origin = actionRequest.position;
  return RingTickInputs{
      .definition = TypedDefinition(*definition_),
      .effect_origin = {origin.vx, origin.vy, origin.vz},
      .actor_translation = {static_cast<std::int32_t>(world.t[0]),
                            static_cast<std::int32_t>(world.t[1]),
                            static_cast<std::int32_t>(world.t[2])},
      .near_clip = VS::Render::nearClip};
}

// Curve 0 samples 0 without reading the package, as the original's sampler
// does. A curve the package's recorded curve block lacks, or one without
// entries (the sampler's modulo divisor), samples 0 with a diagnostic.
std::int32_t LiveRingHost::SampleCurve(std::uint8_t curve,
                                       std::int32_t distance) {
  if (curve == 0) {
    return 0;
  }
  const std::size_t bound = LiveEffectCurveCount();
  if (curve > bound ||
      VS::Effects::package->curve_block->Size(curve - 1) == 0) {
    if (ShouldReport(refused_curves)) {
      VS::Core::LogWarn(
          "effects",
          "ring: curve {} is outside the package's {} bound curves "
          "or empty; sampled as 0 ({} refusals)",
          curve, bound, refused_curves);
    }
    return 0;
  }
  // The promoted curve sampler (#276 row 38) through its Live entry.
  return VS::Effects::LiveSampleEffectCurve(curve, distance);
}

void LiveRingHost::PublishScratch(const RingScratch& values) {
  LiveParticleEmitterScratch& scratch = EmitterScratch();
  scratch.flags = values.flags;
  math::StoreVector(scratch.position, values.position);
  math::StoreVector(scratch.extent, values.extent);
  math::StoreVector(scratch.force_min, values.force_min);
  math::StoreVector(scratch.force_max, values.force_max);
  math::StoreVector(scratch.acceleration_min, values.acceleration_min);
  math::StoreVector(scratch.acceleration_max, values.acceleration_max);
  math::StoreVector(scratch.base_direction, values.base_direction);
  scratch.speed_range[0] = values.speed.x;
  scratch.speed_range[1] = values.speed.y;
  if (values.direction_jitter) {
    math::StoreVector(scratch.direction_jitter, *values.direction_jitter);
  }
}

// The original stepped the animation and indexed the frame's texture
// unchecked; a texture table the package lacks, or a frame without texture
// info, leaves this tick undrawn here.
std::optional<RingTexture> LiveRingHost::AdvanceTexture() {
  LiveParticleTextureAnimation& animation = LiveState()->texture;
  const std::size_t table = animation.texture_index;
  const bool bound =
      VS::Effects::package->textures != nullptr && table < kTextureTableCount &&
      std::cmp_less(table, VS::Effects::package->texture_table_count) &&
      VS::Effects::package->texture_tables[table] != nullptr;
  if (bound) {
    LiveAdvanceParticleTextureAnimation(&animation);
  }
  if (!bound || animation.texture_info == nullptr) {
    if (ShouldReport(refused_textures)) {
      VS::Core::LogWarn("effects",
                        "ring: texture table {} of {} has no frame; ring drawn "
                        "without quads ({} refusals)",
                        table, VS::Effects::package->texture_table_count,
                        refused_textures);
    }
    return std::nullopt;
  }
  const LivePackageTexture& texture =
      VS::Effects::package->textures[animation.texture_info->first_texture];
  return RingTexture{.clut = texture.clut,
                     .tpage = texture.tpage,
                     .u = texture.u,
                     .v = texture.v,
                     .w = texture.w,
                     .h = texture.h};
}

// decomp: rings.h:274-280 (addPrim at the quad's depth). The original wrote
// past the primitive buffer when it was full; a quad without room or outside
// the scene table is dropped here.
void LiveRingHost::Submit(const RingQuad& quad, std::int32_t depth) {
  std::uint32_t* const entry =
      Render::LiveFrameOrderingEntry(Render::LiveOrderingBase::kScene, depth);
  std::uint32_t* const packet =
      entry != nullptr ? Render::ReserveLivePrimitiveWords(kRingPacketWords)
                       : nullptr;
  if (packet == nullptr) {
    if (ShouldReport(dropped_quads)) {
      VS::Core::LogWarn(
          "effects",
          "ring: quad at depth {} has no ordering entry or buffer "
          "room; dropped ({} drops)",
          depth, dropped_quads);
    }
    return;
  }
  const std::array<std::uint32_t, kRingPacketWords> words =
      EncodeRingPacket(quad, static_cast<std::uint32_t>(*entry));
  std::ranges::copy(words, packet);
  *entry = (*entry & kOrderingHighMask) | (GpuHandle(packet) & kRingLinkMask);
  VS::Render::FrameScratchpad().primitive_cursor = packet + kRingPacketWords;
}

// decomp: rings.h:283-293 (the tick event through the transform's world
// matrix, by way of the scratch vector and aim point).
Vec3 LiveRingHost::TransformByActor(RingPoint point) {
  const MATRIX& world =
      instance_->transforms[definition_->transform_index].world;
  SetRotMatrix(&world);
  SetTransMatrix(&world);
  LiveParticleEmitterScratch& scratch = EmitterScratch();
  scratch.vector.vx = point.x;
  scratch.vector.vy = point.y;
  scratch.vector.vz = point.z;
  LoadV0(&scratch.vector);
  Rtv0Tr();
  StoreLvnl(&scratch.aim_point);
  return Wide(scratch.aim_point);
}

void LiveRingHost::EmitParticleEvent(std::uint8_t event, Vec3 position) {
  const std::size_t bound = LiveParticleDefinitionCount();
  if (event >= bound) {
    VS::Core::LogWarn("effects",
                      "ring: tick event {} is outside the package's {} "
                      "definitions; event skipped",
                      event, bound);
    return;
  }
  // The original left the context's other fields as stack contents; they
  // start cleared here.
  LiveParticle context{};
  context.transparency = 0;
  math::StoreVector(context.motion.position, position);
  EmitLiveParticleEvent(instance_, event, 0, 0, &context);
}

using RingInstance = bool (*)(RingCommand, std::int32_t,
                              std::span<const RingPaletteEntry>, RingHost&);

int RunRing(LiveRendererSlot* slot, unsigned int command, int packed,
            std::span<const RingPaletteEntry> palette, RingInstance instance) {
  if (slot == nullptr) {
    VS::Core::LogError("effects",
                       "ring: renderer called without a slot; ignored");
    return 0;
  }
  LiveRingHost host(*slot);
  return instance(static_cast<RingCommand>(command), packed, palette, host) ? 1
                                                                            : 0;
}
}  // namespace

int RunLiveRing(LiveRendererSlot* slot, unsigned int command, int packed,
                std::span<const RingPaletteEntry> palette,
                LiveRingInstance instance) {
  return RunRing(slot, command, packed, palette, instance);
}

// Each entry passes its overlay's palette to the instance in Ring.cpp,
// ClosedRing.cpp or TiledRing.cpp, which carries the decomp provenance.
int UpdateLivePlg049Ring(LiveRendererSlot* slot, unsigned int command,
                         int packed) {
  return RunRing(slot, command, packed,
                 PaletteOf(Effects::Plg049::ringColorFactors), &Plg049::Ring);
}

int UpdateLivePlg050Ring(LiveRendererSlot* slot, unsigned int command,
                         int packed) {
  return RunRing(slot, command, packed,
                 PaletteOf(Effects::Plg050::ringColorFactors), &Plg050::Ring);
}

int UpdateLivePlg061Ring(LiveRendererSlot* slot, unsigned int command,
                         int packed) {
  return RunRing(slot, command, packed,
                 PaletteOf(Effects::Plg061::ringColorFactors), &Plg061::Ring);
}

int UpdateLivePlg065Ring(LiveRendererSlot* slot, unsigned int command,
                         int packed) {
  return RunRing(slot, command, packed,
                 PaletteOf(Effects::Plg065::ringColorFactors), &Plg065::Ring);
}

int UpdateLivePlg073Ring(LiveRendererSlot* slot, unsigned int command,
                         int packed) {
  return RunRing(slot, command, packed,
                 PaletteOf(Effects::Plg073::ringColorFactors), &Plg073::Ring);
}

int UpdateLivePlg081Ring(LiveRendererSlot* slot, unsigned int command,
                         int packed) {
  return RunRing(slot, command, packed,
                 PaletteOf(Effects::Plg081::ringColorFactors), &Plg081::Ring);
}

int UpdateLivePlg129Ring(LiveRendererSlot* slot, unsigned int command,
                         int packed) {
  return RunRing(slot, command, packed,
                 PaletteOf(Effects::Plg129::ringColorFactors), &Plg129::Ring);
}

int UpdateLivePlg129Ring2(LiveRendererSlot* slot, unsigned int command,
                          int packed) {
  return RunRing(slot, command, packed,
                 PaletteOf(Effects::Plg129::ringColorFactors2), &Plg129::Ring2);
}

int UpdateLivePlg138Ring(LiveRendererSlot* slot, unsigned int command,
                         int packed) {
  return RunRing(slot, command, packed,
                 PaletteOf(Effects::Plg138::ringColorFactors), &Plg138::Ring);
}

int UpdateLivePlg142Ring(LiveRendererSlot* slot, unsigned int command,
                         int packed) {
  return RunRing(slot, command, packed,
                 PaletteOf(Effects::Plg142::ringColorFactors), &Plg142::Ring);
}

int UpdateLivePlg146Ring(LiveRendererSlot* slot, unsigned int command,
                         int packed) {
  return RunRing(slot, command, packed,
                 PaletteOf(Effects::Plg146::ringColorFactors), &Plg146::Ring);
}

int UpdateLivePlg147Ring(LiveRendererSlot* slot, unsigned int command,
                         int packed) {
  return RunRing(slot, command, packed,
                 PaletteOf(Effects::Plg147::ringColorFactors), &Plg147::Ring);
}

int UpdateLivePlg149Ring(LiveRendererSlot* slot, unsigned int command,
                         int packed) {
  return RunRing(slot, command, packed,
                 PaletteOf(Effects::Plg149::ringColorFactors), &Plg149::Ring);
}

int UpdateLivePlg150Ring(LiveRendererSlot* slot, unsigned int command,
                         int packed) {
  return RunRing(slot, command, packed,
                 PaletteOf(Effects::Plg150::ringColorFactors), &Plg150::Ring);
}

int UpdateLivePlg153Ring(LiveRendererSlot* slot, unsigned int command,
                         int packed) {
  return RunRing(slot, command, packed,
                 PaletteOf(Effects::Plg153::ringColorFactors), &Plg153::Ring);
}

int UpdateLivePlg160Ring(LiveRendererSlot* slot, unsigned int command,
                         int packed) {
  return RunRing(slot, command, packed,
                 PaletteOf(Effects::Plg160::ringColorFactors), &Plg160::Ring);
}

int UpdateLivePlg161Ring(LiveRendererSlot* slot, unsigned int command,
                         int packed) {
  return RunRing(slot, command, packed,
                 PaletteOf(Effects::Plg161::ringColorFactors), &Plg161::Ring);
}

int UpdateLivePlg161Ring2(LiveRendererSlot* slot, unsigned int command,
                          int packed) {
  return RunRing(slot, command, packed,
                 PaletteOf(Effects::Plg161::ringColorFactors2), &Plg161::Ring2);
}

int UpdateLivePlg162Ring(LiveRendererSlot* slot, unsigned int command,
                         int packed) {
  return RunRing(slot, command, packed,
                 PaletteOf(Effects::Plg162::ringColorFactors2), &Plg162::Ring);
}

int UpdateLivePlg166Ring(LiveRendererSlot* slot, unsigned int command,
                         int packed) {
  return RunRing(slot, command, packed,
                 PaletteOf(Effects::Plg166::ringColorFactors), &Plg166::Ring);
}

int UpdateLivePlg169Ring(LiveRendererSlot* slot, unsigned int command,
                         int packed) {
  return RunRing(slot, command, packed,
                 PaletteOf(Effects::Plg169::ringColorFactors), &Plg169::Ring);
}

int UpdateLivePlg170Ring(LiveRendererSlot* slot, unsigned int command,
                         int packed) {
  return RunRing(slot, command, packed,
                 PaletteOf(Effects::Plg170::ringColorFactors), &Plg170::Ring);
}

int UpdateLivePlg198Ring(LiveRendererSlot* slot, unsigned int command,
                         int packed) {
  return RunRing(slot, command, packed,
                 PaletteOf(Effects::Plg149::ringColorFactors), &Plg198::Ring);
}

int UpdateLivePlg236Ring(LiveRendererSlot* slot, unsigned int command,
                         int packed) {
  return RunRing(slot, command, packed,
                 PaletteOf(Effects::Plg236::ringColorFactors2), &Plg236::Ring);
}

int UpdateLivePlg263Ring(LiveRendererSlot* slot, unsigned int command,
                         int packed) {
  return RunRing(slot, command, packed,
                 PaletteOf(Effects::Plg263::ringColorFactors), &Plg263::Ring);
}

int UpdateLivePlg297Ring(LiveRendererSlot* slot, unsigned int command,
                         int packed) {
  return RunRing(slot, command, packed,
                 PaletteOf(Effects::Plg297::ringColorFactors), &Plg297::Ring);
}

int UpdateLivePlg047ClosedRing(LiveRendererSlot* slot, unsigned int command,
                               int packed) {
  return RunRing(slot, command, packed,
                 PaletteOf(Effects::Plg047::closedRingColorFactors),
                 &Plg047::ClosedRing);
}

int UpdateLivePlg051ClosedRing(LiveRendererSlot* slot, unsigned int command,
                               int packed) {
  return RunRing(slot, command, packed,
                 PaletteOf(Effects::Plg051::closedRingColorFactors),
                 &Plg051::ClosedRing);
}

int UpdateLivePlg053ClosedRing(LiveRendererSlot* slot, unsigned int command,
                               int packed) {
  return RunRing(slot, command, packed,
                 PaletteOf(Effects::Plg053::closedRingColorFactors),
                 &Plg053::ClosedRing);
}

int UpdateLivePlg061ClosedRing(LiveRendererSlot* slot, unsigned int command,
                               int packed) {
  return RunRing(slot, command, packed,
                 PaletteOf(Effects::Plg061::closedRingColorFactors),
                 &Plg061::ClosedRing);
}

int UpdateLivePlg065ClosedRing(LiveRendererSlot* slot, unsigned int command,
                               int packed) {
  return RunRing(slot, command, packed,
                 PaletteOf(Effects::Plg065::closedRingColorFactors),
                 &Plg065::ClosedRing);
}

int UpdateLivePlg081ClosedRing(LiveRendererSlot* slot, unsigned int command,
                               int packed) {
  return RunRing(slot, command, packed,
                 PaletteOf(Effects::Plg081::closedRingColorFactors),
                 &Plg081::ClosedRing);
}

int UpdateLivePlg082ClosedRing(LiveRendererSlot* slot, unsigned int command,
                               int packed) {
  return RunRing(slot, command, packed,
                 PaletteOf(Effects::Plg082::closedRingColorFactors),
                 &Plg082::ClosedRing);
}

int UpdateLivePlg086ClosedRing(LiveRendererSlot* slot, unsigned int command,
                               int packed) {
  return RunRing(slot, command, packed,
                 PaletteOf(Effects::Plg086::closedRingColorFactors),
                 &Plg086::ClosedRing);
}

int UpdateLivePlg087ClosedRing(LiveRendererSlot* slot, unsigned int command,
                               int packed) {
  return RunRing(slot, command, packed,
                 PaletteOf(Effects::Plg087::closedRingColorFactors),
                 &Plg087::ClosedRing);
}

int UpdateLivePlg128ClosedRing(LiveRendererSlot* slot, unsigned int command,
                               int packed) {
  return RunRing(slot, command, packed,
                 PaletteOf(Effects::Plg128::closedRingColorFactors),
                 &Plg128::ClosedRing);
}

int UpdateLivePlg134ClosedRing(LiveRendererSlot* slot, unsigned int command,
                               int packed) {
  return RunRing(slot, command, packed,
                 PaletteOf(Effects::Plg134::closedRingColorFactors),
                 &Plg134::ClosedRing);
}

int UpdateLivePlg135ClosedRing(LiveRendererSlot* slot, unsigned int command,
                               int packed) {
  return RunRing(slot, command, packed,
                 PaletteOf(Effects::Plg135::closedRingColorFactors),
                 &Plg135::ClosedRing);
}

int UpdateLivePlg142ClosedRing(LiveRendererSlot* slot, unsigned int command,
                               int packed) {
  return RunRing(slot, command, packed,
                 PaletteOf(Effects::Plg142::closedRingColorFactors),
                 &Plg142::ClosedRing);
}

int UpdateLivePlg143ClosedRing(LiveRendererSlot* slot, unsigned int command,
                               int packed) {
  return RunRing(slot, command, packed,
                 PaletteOf(Effects::Plg143::closedRingColorFactors),
                 &Plg143::ClosedRing);
}

int UpdateLivePlg152ClosedRing(LiveRendererSlot* slot, unsigned int command,
                               int packed) {
  return RunRing(slot, command, packed,
                 PaletteOf(Effects::Plg082::closedRingColorFactors),
                 &Plg152::ClosedRing);
}

int UpdateLivePlg154ClosedRing(LiveRendererSlot* slot, unsigned int command,
                               int packed) {
  return RunRing(slot, command, packed,
                 PaletteOf(Effects::Plg154::closedRingColorFactors),
                 &Plg154::ClosedRing);
}

int UpdateLivePlg158ClosedRing(LiveRendererSlot* slot, unsigned int command,
                               int packed) {
  return RunRing(slot, command, packed,
                 PaletteOf(Effects::Plg158::closedRingColorFactors),
                 &Plg158::ClosedRing);
}

int UpdateLivePlg162ClosedRing(LiveRendererSlot* slot, unsigned int command,
                               int packed) {
  return RunRing(slot, command, packed,
                 PaletteOf(Effects::Plg162::closedRingColorFactors),
                 &Plg162::ClosedRing);
}

int UpdateLivePlg165ClosedRing(LiveRendererSlot* slot, unsigned int command,
                               int packed) {
  return RunRing(slot, command, packed,
                 PaletteOf(Effects::Plg086::closedRingColorFactors),
                 &Plg165::ClosedRing);
}

int UpdateLivePlg166ClosedRing(LiveRendererSlot* slot, unsigned int command,
                               int packed) {
  return RunRing(slot, command, packed,
                 PaletteOf(Effects::Plg166::closedRingColorFactors),
                 &Plg166::ClosedRing);
}

int UpdateLivePlg167ClosedRing(LiveRendererSlot* slot, unsigned int command,
                               int packed) {
  return RunRing(slot, command, packed,
                 PaletteOf(Effects::Plg167::closedRingColorFactors),
                 &Plg167::ClosedRing);
}

int UpdateLivePlg169ClosedRing(LiveRendererSlot* slot, unsigned int command,
                               int packed) {
  return RunRing(slot, command, packed,
                 PaletteOf(Effects::Plg169::closedRingColorFactors),
                 &Plg169::ClosedRing);
}

int UpdateLivePlg170ClosedRing(LiveRendererSlot* slot, unsigned int command,
                               int packed) {
  return RunRing(slot, command, packed,
                 PaletteOf(Effects::Plg170::closedRingColorFactors),
                 &Plg170::ClosedRing);
}

int UpdateLivePlg236ClosedRing(LiveRendererSlot* slot, unsigned int command,
                               int packed) {
  return RunRing(slot, command, packed,
                 PaletteOf(Effects::Plg236::closedRingColorFactors),
                 &Plg236::ClosedRing);
}

int UpdateLivePlg250ClosedRing(LiveRendererSlot* slot, unsigned int command,
                               int packed) {
  return RunRing(slot, command, packed,
                 PaletteOf(Effects::Plg250::closedRingColorFactors),
                 &Plg250::ClosedRing);
}

int UpdateLivePlg272ClosedRing(LiveRendererSlot* slot, unsigned int command,
                               int packed) {
  return RunRing(slot, command, packed,
                 PaletteOf(Effects::Plg272::closedRingColorFactors),
                 &Plg272::ClosedRing);
}

int UpdateLivePlg291ClosedRing(LiveRendererSlot* slot, unsigned int command,
                               int packed) {
  return RunRing(slot, command, packed,
                 PaletteOf(Effects::Plg291::closedRingColorFactors),
                 &Plg291::ClosedRing);
}

int UpdateLivePlg297ClosedRing(LiveRendererSlot* slot, unsigned int command,
                               int packed) {
  return RunRing(slot, command, packed,
                 PaletteOf(Effects::Plg297::closedRingColorFactors),
                 &Plg297::ClosedRing);
}

int UpdateLivePlg301ClosedRing(LiveRendererSlot* slot, unsigned int command,
                               int packed) {
  return RunRing(slot, command, packed,
                 PaletteOf(Effects::Plg301::closedRingColorFactors),
                 &Plg301::ClosedRing);
}

int UpdateLivePlg044TiledRing(LiveRendererSlot* slot, unsigned int command,
                              int packed) {
  return RunRing(slot, command, packed,
                 PaletteOf(Effects::Plg044::tiledRingColorFactors),
                 &Plg044::TiledRing);
}

int UpdateLivePlg049TiledRing(LiveRendererSlot* slot, unsigned int command,
                              int packed) {
  return RunRing(slot, command, packed,
                 PaletteOf(Effects::Plg049::tiledRingColorFactors),
                 &Plg049::TiledRing);
}

int UpdateLivePlg069TiledRing(LiveRendererSlot* slot, unsigned int command,
                              int packed) {
  return RunRing(slot, command, packed,
                 PaletteOf(Effects::Plg069::tiledRingColorFactors),
                 &Plg069::TiledRing);
}

int UpdateLivePlg073TiledRing(LiveRendererSlot* slot, unsigned int command,
                              int packed) {
  return RunRing(slot, command, packed,
                 PaletteOf(Effects::Plg073::tiledRingColorFactors),
                 &Plg073::TiledRing);
}

int UpdateLivePlg085TiledRing(LiveRendererSlot* slot, unsigned int command,
                              int packed) {
  return RunRing(slot, command, packed,
                 PaletteOf(Effects::Plg085::tiledRingColorFactors),
                 &Plg085::TiledRing);
}

int UpdateLivePlg137TiledRing(LiveRendererSlot* slot, unsigned int command,
                              int packed) {
  return RunRing(slot, command, packed,
                 PaletteOf(Effects::Plg137::tiledRingColorFactors),
                 &Plg137::TiledRing);
}

int UpdateLivePlg138TiledRing(LiveRendererSlot* slot, unsigned int command,
                              int packed) {
  return RunRing(slot, command, packed,
                 PaletteOf(Effects::Plg138::tiledRingColorFactors),
                 &Plg138::TiledRing);
}

int UpdateLivePlg139TiledRing(LiveRendererSlot* slot, unsigned int command,
                              int packed) {
  return RunRing(slot, command, packed,
                 PaletteOf(Effects::Plg085::tiledRingColorFactors),
                 &Plg139::TiledRing);
}

int UpdateLivePlg143TiledRing(LiveRendererSlot* slot, unsigned int command,
                              int packed) {
  return RunRing(slot, command, packed,
                 PaletteOf(Effects::Plg143::tiledRingColorFactors),
                 &Plg143::TiledRing);
}

int UpdateLivePlg146TiledRing(LiveRendererSlot* slot, unsigned int command,
                              int packed) {
  return RunRing(slot, command, packed,
                 PaletteOf(Effects::Plg146::tiledRingColorFactors),
                 &Plg146::TiledRing);
}

int UpdateLivePlg148TiledRing(LiveRendererSlot* slot, unsigned int command,
                              int packed) {
  return RunRing(slot, command, packed,
                 PaletteOf(Effects::Plg148::tiledRingColorFactors),
                 &Plg148::TiledRing);
}

int UpdateLivePlg150TiledRing(LiveRendererSlot* slot, unsigned int command,
                              int packed) {
  return RunRing(slot, command, packed,
                 PaletteOf(Effects::Plg150::tiledRingColorFactors),
                 &Plg150::TiledRing);
}

int UpdateLivePlg160TiledRing(LiveRendererSlot* slot, unsigned int command,
                              int packed) {
  return RunRing(slot, command, packed,
                 PaletteOf(Effects::Plg160::tiledRingColorFactors),
                 &Plg160::TiledRing);
}

int UpdateLivePlg242TiledRing(LiveRendererSlot* slot, unsigned int command,
                              int packed) {
  return RunRing(slot, command, packed,
                 PaletteOf(Effects::Plg242::tiledRingColorFactors),
                 &Plg242::TiledRing);
}

int UpdateLivePlg265TiledRing(LiveRendererSlot* slot, unsigned int command,
                              int packed) {
  return RunRing(slot, command, packed,
                 PaletteOf(Effects::Plg265::tiledRingColorFactors),
                 &Plg265::TiledRing);
}
}  // namespace VS::Effects::Templates
