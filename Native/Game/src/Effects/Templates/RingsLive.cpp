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

#include "Core/Gte.h"
#include "Core/Log.hpp"
#include "Core/Math/InterpolationLive.hpp"
#include "Core/Math/ScalarMath.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Core/RandomLive.hpp"
#include "Core/Scratch.hpp"
#include "Effects/CurvesLive.hpp"
#include "Effects/Package.h"
#include "Effects/PackageExtents.hpp"
#include "Effects/ParticleEmitterLive.hpp"
#include "Effects/ParticleTextureAnimationLive.hpp"
#include "Effects/Particles.h"
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
#include "Effects/Runtime.h"
#include "Effects/Templates/ClosedRing.hpp"
#include "Effects/Templates/RepeatingRingsLive.hpp"
#include "Effects/Templates/Ring.hpp"
#include "Effects/Templates/RingCore.hpp"
#include "Effects/Templates/TemplateLiveState.hpp"
#include "Effects/Templates/TiledRing.hpp"
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
using math::Vec3;

constexpr std::uintptr_t kEmitterScratchOffset = 0x1D0;
using InstanceTransforms = decltype(Effects_EffectInstance::transforms);
using TextureTables = decltype(Effects_EffectPackage::textureInfoTable);
constexpr std::size_t kTransformCount = std::extent_v<InstanceTransforms>;
constexpr std::size_t kTextureTableCount = std::extent_v<TextureTables>;
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

static_assert(sizeof(Effects_ParticleDefinition) == kParticleDefinitionBytes);

std::uint64_t refused_curves = 0;
std::uint64_t refused_textures = 0;
std::uint64_t dropped_quads = 0;

// True on the 1st, 2nd, 4th, 8th... occurrence, to keep repeated
// diagnostics readable.
[[nodiscard]] bool ShouldReport(std::uint64_t& count) {
  return std::has_single_bit(++count);
}

Effects_ParticleEmitterScratch& EmitterScratch() {
  return *reinterpret_cast<Effects_ParticleEmitterScratch*>(
      reinterpret_cast<std::uintptr_t>(VS::Core::ScratchBase()) +
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

[[nodiscard]] RingDefinition TypedDefinition(
    const Effects_ParticleDefinition& definition) {
  const auto& vectors = definition.vectorRanges;
  const std::span<const std::int16_t> speed =
      definition.pairRanges[kSpeedRange];
  return {
      .flags = definition.flags,
      // The palette byte offset is (byte 7 & 0x1C): the transparency
      // field's low three bits.
      .palette_index = static_cast<std::uint8_t>(definition.transparencyCurve),
      .tick_event = definition.tickEvent,
      .curves = {.position = definition.positionCurve,
                 .extent = definition.extentCurve,
                 .inner_extent = definition.innerExtentCurve,
                 .direction = definition.directionCurve,
                 .direction_jitter = definition.directionJitterCurve,
                 .speed = definition.speedCurve,
                 .force = definition.forceCurve,
                 .acceleration = definition.accelerationCurve,
                 .color = {definition.rCurve, definition.gCurve,
                           definition.bCurve}},
      .position = VectorRange(definition.positionRange),
      .extent = VectorRange(definition.extentRange),
      .inner_extent = math::RangePair(definition.innerExtentRange, 0),
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
  explicit LiveRingHost(Effects_RendererSlot& slot) : slot_(slot) {}

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
        instance_->transforms[definition_->transformIndex].view;
    Core_Gte_SetRotMatrix(&view);
    Core_Gte_SetTransMatrix(&view);
  }

  [[nodiscard]] RingProjection Project(RingPoint point) override {
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

  [[nodiscard]] std::optional<RingTexture> AdvanceTexture() override;
  void Submit(const RingQuad& quad, std::int32_t depth) override;
  [[nodiscard]] Vec3 TransformByActor(RingPoint point) override;
  void EmitParticleEvent(std::uint8_t event, Vec3 position) override;

 private:
  [[nodiscard]] LiveRingState* LiveState() const {
    return static_cast<LiveRingState*>(slot_.rendererState);
  }

  Effects_RendererSlot& slot_;
  const Effects_ParticleDefinition* definition_ = nullptr;
  Effects_EffectInstance* instance_ = nullptr;
};

// decomp: rings.h:41-55, closedRings.h:63-75 and tiledRings.h:57-69 (the
// create command). The original indexed the definition and allocated the
// state unchecked and left reserved_2 as the heap held it.
void LiveRingHost::Create(const RingState& state, RingTextureSource source) {
  const Effects_ParticleDefinition* definition =
      Definition(state.definition_index);
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
  slot_.rendererState = live;
  StoreState(state);
  const bool segments = source == RingTextureSource::kSegmentsAndPositions;
  const int table =
      segments ? definition->ringSegments : definition->textureIndex;
  const int program =
      segments ? definition->ringPositions : definition->animationId;
  // The native closed ring's "effects.texture" diagnostics, for every ring.
  const bool primary = Effects_package == &Effects_actionPackage;
  const unsigned request_effect = primary ? Effects_actionRequest.effectId
                                          : Effects_secondaryRequest.effectId;
  const int tables = Effects_package->block1TableCount;
  VS::Core::LogDebug("effects",
                     "ring: init definition={} lifetime={} table={} program={} "
                     "context={} request_effect={} tables={}",
                     state.definition_index, state.lifetime, table, program,
                     primary ? "primary" : "secondary", request_effect, tables);
  if (table >= tables || std::cmp_greater_equal(table, kTextureTableCount) ||
      Effects_package->textureInfoTable[table] == nullptr) {
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
  instance_ = Effects_currentInstance;
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
  if (definition_->transformIndex >= kTransformCount) {
    VS::Core::LogWarn(
        "effects", "ring: definition {} names transform {} of {}; ring stopped",
        definition_index, definition_->transformIndex, kTransformCount);
    return std::nullopt;
  }
  // The original stored the flags before sampling anything.
  EmitterScratch().flags = definition_->flags;
  const MATRIX& world =
      instance_->transforms[definition_->transformIndex].world;
  // Effects_effectOrigin: the action request's position.
  const SVECTOR& origin = Effects_actionRequest.position;
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
      Effects_package->curveBlock->curveSizes[curve - 1] == 0) {
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
  Effects_ParticleEmitterScratch& scratch = EmitterScratch();
  scratch.flags = values.flags;
  math::StoreVector(scratch.position, values.position);
  math::StoreVector(scratch.extent, values.extent);
  math::StoreVector(scratch.forceMin, values.force_min);
  math::StoreVector(scratch.forceMax, values.force_max);
  math::StoreVector(scratch.accelerationMin, values.acceleration_min);
  math::StoreVector(scratch.accelerationMax, values.acceleration_max);
  math::StoreVector(scratch.baseDirection, values.base_direction);
  scratch.speedRange[0] = values.speed.x;
  scratch.speedRange[1] = values.speed.y;
  if (values.direction_jitter) {
    math::StoreVector(scratch.directionJitter, *values.direction_jitter);
  }
}

// The original stepped the animation and indexed the frame's texture
// unchecked; a texture table the package lacks, or a frame without texture
// info, leaves this tick undrawn here.
std::optional<RingTexture> LiveRingHost::AdvanceTexture() {
  Effects_ParticleTextureAnimation& animation = LiveState()->texture;
  const std::size_t table = animation.textureIndex;
  const bool bound = Effects_package->textures != nullptr &&
                     table < kTextureTableCount &&
                     std::cmp_less(table, Effects_package->block1TableCount) &&
                     Effects_package->textureInfoTable[table] != nullptr;
  if (bound) {
    LiveAdvanceParticleTextureAnimation(&animation);
  }
  if (!bound || animation.textureInfo == nullptr) {
    if (ShouldReport(refused_textures)) {
      VS::Core::LogWarn("effects",
                        "ring: texture table {} of {} has no frame; ring drawn "
                        "without quads ({} refusals)",
                        table, Effects_package->block1TableCount,
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

// decomp: rings.h:274-280 (addPrim at the quad's depth). The original wrote
// past the primitive buffer when it was full; a quad without room or outside
// the scene table is dropped here.
void LiveRingHost::Submit(const RingQuad& quad, std::int32_t depth) {
  u_long* const entry =
      Render::LiveFrameOrderingEntry(Render::LiveOrderingBase::kScene, depth);
  u_long* const packet =
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
  *entry =
      (*entry & kOrderingHighMask) | (LegacyGpu_Handle(packet) & kRingLinkMask);
  VS::Render::FrameScratchpad().primitive_cursor = packet + kRingPacketWords;
}

// decomp: rings.h:283-293 (the tick event through the transform's world
// matrix, by way of the scratch vector and aim point).
Vec3 LiveRingHost::TransformByActor(RingPoint point) {
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
  Effects_PluginEffectState context{};
  context.transparency = 0;
  math::StoreVector(context.position, position);
  EmitLiveParticleEvent(instance_, event, 0, 0, &context);
}

using RingInstance = bool (*)(RingCommand, std::int32_t,
                              std::span<const RingPaletteEntry>, RingHost&);

int RunRing(Effects_RendererSlot* slot, unsigned int command, int packed,
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

int RunLiveRing(Effects_RendererSlot* slot, unsigned int command, int packed,
                std::span<const RingPaletteEntry> palette,
                LiveRingInstance instance) {
  return RunRing(slot, command, packed, palette, instance);
}

// Each entry passes its overlay's palette to the instance in Ring.cpp,
// ClosedRing.cpp or TiledRing.cpp, which carries the decomp provenance.
int UpdateLivePlg049Ring(Effects_RendererSlot* slot, unsigned int command,
                         int packed) {
  return RunRing(slot, command, packed,
                 PaletteOf(Effects::Plg049::ringColorFactors), &Plg049::Ring);
}

int UpdateLivePlg050Ring(Effects_RendererSlot* slot, unsigned int command,
                         int packed) {
  return RunRing(slot, command, packed,
                 PaletteOf(Effects::Plg050::ringColorFactors), &Plg050::Ring);
}

int UpdateLivePlg061Ring(Effects_RendererSlot* slot, unsigned int command,
                         int packed) {
  return RunRing(slot, command, packed,
                 PaletteOf(Effects::Plg061::ringColorFactors), &Plg061::Ring);
}

int UpdateLivePlg065Ring(Effects_RendererSlot* slot, unsigned int command,
                         int packed) {
  return RunRing(slot, command, packed,
                 PaletteOf(Effects::Plg065::ringColorFactors), &Plg065::Ring);
}

int UpdateLivePlg073Ring(Effects_RendererSlot* slot, unsigned int command,
                         int packed) {
  return RunRing(slot, command, packed,
                 PaletteOf(Effects::Plg073::ringColorFactors), &Plg073::Ring);
}

int UpdateLivePlg081Ring(Effects_RendererSlot* slot, unsigned int command,
                         int packed) {
  return RunRing(slot, command, packed,
                 PaletteOf(Effects::Plg081::ringColorFactors), &Plg081::Ring);
}

int UpdateLivePlg129Ring(Effects_RendererSlot* slot, unsigned int command,
                         int packed) {
  return RunRing(slot, command, packed,
                 PaletteOf(Effects::Plg129::ringColorFactors), &Plg129::Ring);
}

int UpdateLivePlg129Ring2(Effects_RendererSlot* slot, unsigned int command,
                          int packed) {
  return RunRing(slot, command, packed,
                 PaletteOf(Effects::Plg129::ringColorFactors2), &Plg129::Ring2);
}

int UpdateLivePlg138Ring(Effects_RendererSlot* slot, unsigned int command,
                         int packed) {
  return RunRing(slot, command, packed,
                 PaletteOf(Effects::Plg138::ringColorFactors), &Plg138::Ring);
}

int UpdateLivePlg142Ring(Effects_RendererSlot* slot, unsigned int command,
                         int packed) {
  return RunRing(slot, command, packed,
                 PaletteOf(Effects::Plg142::ringColorFactors), &Plg142::Ring);
}

int UpdateLivePlg146Ring(Effects_RendererSlot* slot, unsigned int command,
                         int packed) {
  return RunRing(slot, command, packed,
                 PaletteOf(Effects::Plg146::ringColorFactors), &Plg146::Ring);
}

int UpdateLivePlg147Ring(Effects_RendererSlot* slot, unsigned int command,
                         int packed) {
  return RunRing(slot, command, packed,
                 PaletteOf(Effects::Plg147::ringColorFactors), &Plg147::Ring);
}

int UpdateLivePlg149Ring(Effects_RendererSlot* slot, unsigned int command,
                         int packed) {
  return RunRing(slot, command, packed,
                 PaletteOf(Effects::Plg149::ringColorFactors), &Plg149::Ring);
}

int UpdateLivePlg150Ring(Effects_RendererSlot* slot, unsigned int command,
                         int packed) {
  return RunRing(slot, command, packed,
                 PaletteOf(Effects::Plg150::ringColorFactors), &Plg150::Ring);
}

int UpdateLivePlg153Ring(Effects_RendererSlot* slot, unsigned int command,
                         int packed) {
  return RunRing(slot, command, packed,
                 PaletteOf(Effects::Plg153::ringColorFactors), &Plg153::Ring);
}

int UpdateLivePlg160Ring(Effects_RendererSlot* slot, unsigned int command,
                         int packed) {
  return RunRing(slot, command, packed,
                 PaletteOf(Effects::Plg160::ringColorFactors), &Plg160::Ring);
}

int UpdateLivePlg161Ring(Effects_RendererSlot* slot, unsigned int command,
                         int packed) {
  return RunRing(slot, command, packed,
                 PaletteOf(Effects::Plg161::ringColorFactors), &Plg161::Ring);
}

int UpdateLivePlg161Ring2(Effects_RendererSlot* slot, unsigned int command,
                          int packed) {
  return RunRing(slot, command, packed,
                 PaletteOf(Effects::Plg161::ringColorFactors2), &Plg161::Ring2);
}

int UpdateLivePlg162Ring(Effects_RendererSlot* slot, unsigned int command,
                         int packed) {
  return RunRing(slot, command, packed,
                 PaletteOf(Effects::Plg162::ringColorFactors2), &Plg162::Ring);
}

int UpdateLivePlg166Ring(Effects_RendererSlot* slot, unsigned int command,
                         int packed) {
  return RunRing(slot, command, packed,
                 PaletteOf(Effects::Plg166::ringColorFactors), &Plg166::Ring);
}

int UpdateLivePlg169Ring(Effects_RendererSlot* slot, unsigned int command,
                         int packed) {
  return RunRing(slot, command, packed,
                 PaletteOf(Effects::Plg169::ringColorFactors), &Plg169::Ring);
}

int UpdateLivePlg170Ring(Effects_RendererSlot* slot, unsigned int command,
                         int packed) {
  return RunRing(slot, command, packed,
                 PaletteOf(Effects::Plg170::ringColorFactors), &Plg170::Ring);
}

int UpdateLivePlg198Ring(Effects_RendererSlot* slot, unsigned int command,
                         int packed) {
  return RunRing(slot, command, packed,
                 PaletteOf(Effects::Plg149::ringColorFactors), &Plg198::Ring);
}

int UpdateLivePlg236Ring(Effects_RendererSlot* slot, unsigned int command,
                         int packed) {
  return RunRing(slot, command, packed,
                 PaletteOf(Effects::Plg236::ringColorFactors2), &Plg236::Ring);
}

int UpdateLivePlg263Ring(Effects_RendererSlot* slot, unsigned int command,
                         int packed) {
  return RunRing(slot, command, packed,
                 PaletteOf(Effects::Plg263::ringColorFactors), &Plg263::Ring);
}

int UpdateLivePlg297Ring(Effects_RendererSlot* slot, unsigned int command,
                         int packed) {
  return RunRing(slot, command, packed,
                 PaletteOf(Effects::Plg297::ringColorFactors), &Plg297::Ring);
}

int UpdateLivePlg047ClosedRing(Effects_RendererSlot* slot, unsigned int command,
                               int packed) {
  return RunRing(slot, command, packed,
                 PaletteOf(Effects::Plg047::closedRingColorFactors),
                 &Plg047::ClosedRing);
}

int UpdateLivePlg051ClosedRing(Effects_RendererSlot* slot, unsigned int command,
                               int packed) {
  return RunRing(slot, command, packed,
                 PaletteOf(Effects::Plg051::closedRingColorFactors),
                 &Plg051::ClosedRing);
}

int UpdateLivePlg053ClosedRing(Effects_RendererSlot* slot, unsigned int command,
                               int packed) {
  return RunRing(slot, command, packed,
                 PaletteOf(Effects::Plg053::closedRingColorFactors),
                 &Plg053::ClosedRing);
}

int UpdateLivePlg061ClosedRing(Effects_RendererSlot* slot, unsigned int command,
                               int packed) {
  return RunRing(slot, command, packed,
                 PaletteOf(Effects::Plg061::closedRingColorFactors),
                 &Plg061::ClosedRing);
}

int UpdateLivePlg065ClosedRing(Effects_RendererSlot* slot, unsigned int command,
                               int packed) {
  return RunRing(slot, command, packed,
                 PaletteOf(Effects::Plg065::closedRingColorFactors),
                 &Plg065::ClosedRing);
}

int UpdateLivePlg081ClosedRing(Effects_RendererSlot* slot, unsigned int command,
                               int packed) {
  return RunRing(slot, command, packed,
                 PaletteOf(Effects::Plg081::closedRingColorFactors),
                 &Plg081::ClosedRing);
}

int UpdateLivePlg082ClosedRing(Effects_RendererSlot* slot, unsigned int command,
                               int packed) {
  return RunRing(slot, command, packed,
                 PaletteOf(Effects::Plg082::closedRingColorFactors),
                 &Plg082::ClosedRing);
}

int UpdateLivePlg086ClosedRing(Effects_RendererSlot* slot, unsigned int command,
                               int packed) {
  return RunRing(slot, command, packed,
                 PaletteOf(Effects::Plg086::closedRingColorFactors),
                 &Plg086::ClosedRing);
}

int UpdateLivePlg087ClosedRing(Effects_RendererSlot* slot, unsigned int command,
                               int packed) {
  return RunRing(slot, command, packed,
                 PaletteOf(Effects::Plg087::closedRingColorFactors),
                 &Plg087::ClosedRing);
}

int UpdateLivePlg128ClosedRing(Effects_RendererSlot* slot, unsigned int command,
                               int packed) {
  return RunRing(slot, command, packed,
                 PaletteOf(Effects::Plg128::closedRingColorFactors),
                 &Plg128::ClosedRing);
}

int UpdateLivePlg134ClosedRing(Effects_RendererSlot* slot, unsigned int command,
                               int packed) {
  return RunRing(slot, command, packed,
                 PaletteOf(Effects::Plg134::closedRingColorFactors),
                 &Plg134::ClosedRing);
}

int UpdateLivePlg135ClosedRing(Effects_RendererSlot* slot, unsigned int command,
                               int packed) {
  return RunRing(slot, command, packed,
                 PaletteOf(Effects::Plg135::closedRingColorFactors),
                 &Plg135::ClosedRing);
}

int UpdateLivePlg142ClosedRing(Effects_RendererSlot* slot, unsigned int command,
                               int packed) {
  return RunRing(slot, command, packed,
                 PaletteOf(Effects::Plg142::closedRingColorFactors),
                 &Plg142::ClosedRing);
}

int UpdateLivePlg143ClosedRing(Effects_RendererSlot* slot, unsigned int command,
                               int packed) {
  return RunRing(slot, command, packed,
                 PaletteOf(Effects::Plg143::closedRingColorFactors),
                 &Plg143::ClosedRing);
}

int UpdateLivePlg152ClosedRing(Effects_RendererSlot* slot, unsigned int command,
                               int packed) {
  return RunRing(slot, command, packed,
                 PaletteOf(Effects::Plg082::closedRingColorFactors),
                 &Plg152::ClosedRing);
}

int UpdateLivePlg154ClosedRing(Effects_RendererSlot* slot, unsigned int command,
                               int packed) {
  return RunRing(slot, command, packed,
                 PaletteOf(Effects::Plg154::closedRingColorFactors),
                 &Plg154::ClosedRing);
}

int UpdateLivePlg158ClosedRing(Effects_RendererSlot* slot, unsigned int command,
                               int packed) {
  return RunRing(slot, command, packed,
                 PaletteOf(Effects::Plg158::closedRingColorFactors),
                 &Plg158::ClosedRing);
}

int UpdateLivePlg162ClosedRing(Effects_RendererSlot* slot, unsigned int command,
                               int packed) {
  return RunRing(slot, command, packed,
                 PaletteOf(Effects::Plg162::closedRingColorFactors),
                 &Plg162::ClosedRing);
}

int UpdateLivePlg165ClosedRing(Effects_RendererSlot* slot, unsigned int command,
                               int packed) {
  return RunRing(slot, command, packed,
                 PaletteOf(Effects::Plg086::closedRingColorFactors),
                 &Plg165::ClosedRing);
}

int UpdateLivePlg166ClosedRing(Effects_RendererSlot* slot, unsigned int command,
                               int packed) {
  return RunRing(slot, command, packed,
                 PaletteOf(Effects::Plg166::closedRingColorFactors),
                 &Plg166::ClosedRing);
}

int UpdateLivePlg167ClosedRing(Effects_RendererSlot* slot, unsigned int command,
                               int packed) {
  return RunRing(slot, command, packed,
                 PaletteOf(Effects::Plg167::closedRingColorFactors),
                 &Plg167::ClosedRing);
}

int UpdateLivePlg169ClosedRing(Effects_RendererSlot* slot, unsigned int command,
                               int packed) {
  return RunRing(slot, command, packed,
                 PaletteOf(Effects::Plg169::closedRingColorFactors),
                 &Plg169::ClosedRing);
}

int UpdateLivePlg170ClosedRing(Effects_RendererSlot* slot, unsigned int command,
                               int packed) {
  return RunRing(slot, command, packed,
                 PaletteOf(Effects::Plg170::closedRingColorFactors),
                 &Plg170::ClosedRing);
}

int UpdateLivePlg236ClosedRing(Effects_RendererSlot* slot, unsigned int command,
                               int packed) {
  return RunRing(slot, command, packed,
                 PaletteOf(Effects::Plg236::closedRingColorFactors),
                 &Plg236::ClosedRing);
}

int UpdateLivePlg250ClosedRing(Effects_RendererSlot* slot, unsigned int command,
                               int packed) {
  return RunRing(slot, command, packed,
                 PaletteOf(Effects::Plg250::closedRingColorFactors),
                 &Plg250::ClosedRing);
}

int UpdateLivePlg272ClosedRing(Effects_RendererSlot* slot, unsigned int command,
                               int packed) {
  return RunRing(slot, command, packed,
                 PaletteOf(Effects::Plg272::closedRingColorFactors),
                 &Plg272::ClosedRing);
}

int UpdateLivePlg291ClosedRing(Effects_RendererSlot* slot, unsigned int command,
                               int packed) {
  return RunRing(slot, command, packed,
                 PaletteOf(Effects::Plg291::closedRingColorFactors),
                 &Plg291::ClosedRing);
}

int UpdateLivePlg297ClosedRing(Effects_RendererSlot* slot, unsigned int command,
                               int packed) {
  return RunRing(slot, command, packed,
                 PaletteOf(Effects::Plg297::closedRingColorFactors),
                 &Plg297::ClosedRing);
}

int UpdateLivePlg301ClosedRing(Effects_RendererSlot* slot, unsigned int command,
                               int packed) {
  return RunRing(slot, command, packed,
                 PaletteOf(Effects::Plg301::closedRingColorFactors),
                 &Plg301::ClosedRing);
}

int UpdateLivePlg044TiledRing(Effects_RendererSlot* slot, unsigned int command,
                              int packed) {
  return RunRing(slot, command, packed,
                 PaletteOf(Effects::Plg044::tiledRingColorFactors),
                 &Plg044::TiledRing);
}

int UpdateLivePlg049TiledRing(Effects_RendererSlot* slot, unsigned int command,
                              int packed) {
  return RunRing(slot, command, packed,
                 PaletteOf(Effects::Plg049::tiledRingColorFactors),
                 &Plg049::TiledRing);
}

int UpdateLivePlg069TiledRing(Effects_RendererSlot* slot, unsigned int command,
                              int packed) {
  return RunRing(slot, command, packed,
                 PaletteOf(Effects::Plg069::tiledRingColorFactors),
                 &Plg069::TiledRing);
}

int UpdateLivePlg073TiledRing(Effects_RendererSlot* slot, unsigned int command,
                              int packed) {
  return RunRing(slot, command, packed,
                 PaletteOf(Effects::Plg073::tiledRingColorFactors),
                 &Plg073::TiledRing);
}

int UpdateLivePlg085TiledRing(Effects_RendererSlot* slot, unsigned int command,
                              int packed) {
  return RunRing(slot, command, packed,
                 PaletteOf(Effects::Plg085::tiledRingColorFactors),
                 &Plg085::TiledRing);
}

int UpdateLivePlg137TiledRing(Effects_RendererSlot* slot, unsigned int command,
                              int packed) {
  return RunRing(slot, command, packed,
                 PaletteOf(Effects::Plg137::tiledRingColorFactors),
                 &Plg137::TiledRing);
}

int UpdateLivePlg138TiledRing(Effects_RendererSlot* slot, unsigned int command,
                              int packed) {
  return RunRing(slot, command, packed,
                 PaletteOf(Effects::Plg138::tiledRingColorFactors),
                 &Plg138::TiledRing);
}

int UpdateLivePlg139TiledRing(Effects_RendererSlot* slot, unsigned int command,
                              int packed) {
  return RunRing(slot, command, packed,
                 PaletteOf(Effects::Plg085::tiledRingColorFactors),
                 &Plg139::TiledRing);
}

int UpdateLivePlg143TiledRing(Effects_RendererSlot* slot, unsigned int command,
                              int packed) {
  return RunRing(slot, command, packed,
                 PaletteOf(Effects::Plg143::tiledRingColorFactors),
                 &Plg143::TiledRing);
}

int UpdateLivePlg146TiledRing(Effects_RendererSlot* slot, unsigned int command,
                              int packed) {
  return RunRing(slot, command, packed,
                 PaletteOf(Effects::Plg146::tiledRingColorFactors),
                 &Plg146::TiledRing);
}

int UpdateLivePlg148TiledRing(Effects_RendererSlot* slot, unsigned int command,
                              int packed) {
  return RunRing(slot, command, packed,
                 PaletteOf(Effects::Plg148::tiledRingColorFactors),
                 &Plg148::TiledRing);
}

int UpdateLivePlg150TiledRing(Effects_RendererSlot* slot, unsigned int command,
                              int packed) {
  return RunRing(slot, command, packed,
                 PaletteOf(Effects::Plg150::tiledRingColorFactors),
                 &Plg150::TiledRing);
}

int UpdateLivePlg160TiledRing(Effects_RendererSlot* slot, unsigned int command,
                              int packed) {
  return RunRing(slot, command, packed,
                 PaletteOf(Effects::Plg160::tiledRingColorFactors),
                 &Plg160::TiledRing);
}

int UpdateLivePlg242TiledRing(Effects_RendererSlot* slot, unsigned int command,
                              int packed) {
  return RunRing(slot, command, packed,
                 PaletteOf(Effects::Plg242::tiledRingColorFactors),
                 &Plg242::TiledRing);
}

int UpdateLivePlg265TiledRing(Effects_RendererSlot* slot, unsigned int command,
                              int packed) {
  return RunRing(slot, command, packed,
                 PaletteOf(Effects::Plg265::tiledRingColorFactors),
                 &Plg265::TiledRing);
}
}  // namespace VS::Effects::Templates
