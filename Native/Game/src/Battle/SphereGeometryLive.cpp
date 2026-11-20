// Hand-maintained (tools/port/owned.csv).
#include "Battle/SphereGeometryLive.hpp"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <span>

#include "Battle/Sphere.hpp"
#include "Core/Log.hpp"
#include "Core/Scratch.hpp"

using VS::Battle::ShapeVertex;
using VS::Battle::TargetVolume;

namespace VS::Battle {
namespace {
constexpr std::size_t kSphereArcBaseCount = 72;
constexpr std::size_t kSphereMirroredVertexCount = 186;
constexpr std::size_t kSphereShapeStartCount = 52;
constexpr std::size_t kSphereCommandCount = 218;
constexpr std::size_t kSphereSineSampleCount = 32;
constexpr std::size_t kArcBasisStride = 4;
constexpr std::size_t kWireBasisStride = kTargetWireAxes;
constexpr std::size_t kTransformScratchOffset = 0x350;
constexpr std::size_t kBasisScratchOffset = 0x398;
constexpr std::size_t kSineScratchOffset = 0x3B0;
constexpr std::uint32_t kWireVertexCountMask = 0x0000FFFF;
constexpr std::int32_t kWireVertexCountShift = 16;
constexpr std::size_t kWirePhaseAOffset = 0x24;
constexpr std::size_t kWirePhaseBOffset = 0x26;
constexpr std::size_t kWirePhaseCOffset = 0x28;
constexpr std::size_t kWireShapeOffset = 0x2A;
constexpr std::size_t kPulseModeAndVertexCountOffset = 0x30;
constexpr std::size_t kWireVerticesOffset = 0x50;
constexpr std::size_t kTargetVolumeScaleOffset = 0x4;
constexpr std::size_t kShapeVertexOpaqueOffset = 6;

static_assert(offsetof(VS::Battle::TargetSphereSetupView, wire_phase_a) ==
              kWirePhaseAOffset);
static_assert(offsetof(VS::Battle::TargetSphereSetupView, wire_phase_b) ==
              kWirePhaseBOffset);
static_assert(offsetof(VS::Battle::TargetSphereSetupView, wire_phase_c) ==
              kWirePhaseCOffset);
static_assert(offsetof(VS::Battle::TargetSphereSetupView, wire_shape) ==
              kWireShapeOffset);
static_assert(offsetof(VS::Battle::TargetSphereSetupView,
                       pulse_mode_and_vertex_count) ==
              kPulseModeAndVertexCountOffset);
static_assert(offsetof(VS::Battle::TargetSphereSetupView, wire_vertices) ==
              kWireVerticesOffset);
static_assert(
    sizeof(VS::Battle::TargetSphereSetupView{}.wire_vertices) /
        sizeof(VS::Battle::TargetSphereSetupView{}.wire_vertices[0]) ==
    kTargetWireVertices);
static_assert(offsetof(TargetVolume, scale) == kTargetVolumeScaleOffset);
static_assert(sizeof(TargetVolume{}.scale.extents) == kTargetVolumeExtents);
static_assert(offsetof(ShapeVertex, xyz) == 0);
static_assert(offsetof(ShapeVertex, opaque6) == kShapeVertexOpaqueOffset);

[[noreturn]] void Refuse(const char* operation, const char* reason) {
  VS::Core::LogError("battle.sphere", "{} refused: {}", operation, reason);
  std::abort();
}

std::byte* ScratchBytes() {
  auto* scratch = static_cast<std::byte*>(VS::Core::ScratchBase());
  if (scratch == nullptr) {
    Refuse("sphere geometry", "missing scratch workspace");
  }
  return scratch;
}

std::int16_t* ScratchShorts(std::size_t offset) {
  return reinterpret_cast<std::int16_t*>(ScratchBytes() + offset);
}

SphereArcBasis ReadBasis(const std::int16_t* values, std::size_t stride) {
  SphereArcBasis basis{};
  for (std::size_t axis = 0; axis < basis.size(); ++axis) {
    for (std::size_t component = 0; component < basis[axis].size();
         ++component) {
      basis[axis][component] = values[axis * stride + component];
    }
  }
  return basis;
}

VS::Battle::TargetSphereSetupView& RequiredSphere(const char* operation) {
  if (VS::Battle::targetSphere == nullptr) {
    Refuse(operation, "missing sphere workspace");
  }
  return *VS::Battle::targetSphere;
}

void WriteVertices(VS::Battle::TargetSphereSetupView& sphere,
                   std::span<const TargetWireVertex> vertices,
                   std::size_t first, std::size_t end) {
  if (first > end || end > vertices.size() || end > kTargetWireVertices) {
    Refuse("sphere geometry", "vertex range exceeds workspace");
  }
  for (std::size_t index = first; index < end; ++index) {
    const auto& source = vertices[index];
    auto& destination = sphere.wire_vertices[index];
    for (std::size_t component = 0; component < source.position.size();
         ++component) {
      destination.xyz[component] = source.position[component];
    }
    destination.opaque6 = source.segment;
  }
}
}  // namespace

// decomp: BATTLE.PRG func_800C085C (src/BATTLE/BATTLE.PRG/573B8.c:523).
std::optional<Core::Math::Mat3> BuildLiveSphereVolumeTransform(
    const TargetVolume* volume) noexcept {
  if (volume == nullptr) return std::nullopt;
  const TargetVolumeExtents extents{
      volume->scale.extents[0], volume->scale.extents[1],
      volume->scale.extents[2], volume->scale.extents[3]};
  return BuildSphereVolumeTransform(extents, volume->angle);
}

// decomp: BATTLE.PRG func_800C253C (src/BATTLE/BATTLE.PRG/58578.c:887).
std::optional<SphereWireframeBuildResult> BuildLiveSphereWireframe(
    VS::Battle::TargetSphereSetupView* sphere, std::int32_t type,
    const SphereGeometryTables& tables) noexcept {
  if (sphere == nullptr) return std::nullopt;

  std::array<TargetWireVertex, kTargetWireVertices> generated{};
  const SphereWireframeState state{.phase_a = sphere->wire_phase_a,
                                   .phase_b = sphere->wire_phase_b,
                                   .phase_c = sphere->wire_phase_c,
                                   .shape = sphere->wire_shape};
  const auto result = BuildSphereWireframe(type, state, tables, generated);
  if (!result) return std::nullopt;

  for (std::size_t index = 0; index < result->vertex_count; ++index) {
    const auto& source = generated[index];
    auto& destination = sphere->wire_vertices[index];
    destination.xyz[0] = source.position[0];
    destination.xyz[1] = source.position[1];
    destination.xyz[2] = source.position[2];
    destination.opaque6 = source.segment;
  }
  sphere->wire_phase_a = result->state.phase_a;
  sphere->wire_phase_b = result->state.phase_b;
  sphere->wire_phase_c = result->state.phase_c;
  sphere->wire_shape = result->state.shape;
  const auto vertex_count = static_cast<std::uint16_t>(result->vertex_count);
  static_assert(sizeof(sphere->pulse_mode_and_vertex_count) ==
                sizeof(std::uint32_t));
  const auto packed_fields =
      (std::bit_cast<std::uint32_t>(sphere->pulse_mode_and_vertex_count) &
       kWireVertexCountMask) |
      (static_cast<std::uint32_t>(vertex_count) << kWireVertexCountShift);
  sphere->pulse_mode_and_vertex_count = std::bit_cast<int>(packed_fields);
  return result;
}

// decomp: BATTLE.PRG func_800C0758 (src/BATTLE/BATTLE.PRG/573B8.c:495).
int AppendSphereArcVerticesEntry(int phase, int segments, int index) {
  auto& sphere = RequiredSphere("sphere arc append");
  std::array<TargetWireVertex, kTargetWireVertices> generated{};
  const auto next = AppendSphereArcVertices(
      ReadBasis(ScratchShorts(kBasisScratchOffset), kArcBasisStride),
      std::span<const std::int16_t>(ScratchShorts(kSineScratchOffset),
                                    kSphereSineSampleCount),
      phase, segments, generated, static_cast<std::size_t>(index));
  if (!next) Refuse("sphere arc append", "invalid geometry inputs");
  WriteVertices(sphere, generated, static_cast<std::size_t>(index), *next);
  return static_cast<int>(*next);
}

// decomp: BATTLE.PRG func_800C085C (src/BATTLE/BATTLE.PRG/573B8.c:523).
MATRIX* BuildSphereVolumeTransformEntry(u_char* scale, int angle) {
  if (scale == nullptr) {
    Refuse("sphere volume transform", "missing scale extents");
  }
  TargetVolumeExtents extents{};
  for (std::size_t index = 0; index < extents.size(); ++index) {
    extents[index] = scale[index];
  }
  const auto transform = BuildSphereVolumeTransform(extents, angle);
  auto* matrix = ScratchShorts(kTransformScratchOffset);
  for (std::size_t axis = 0; axis < transform.m.size(); ++axis) {
    for (std::size_t component = 0; component < transform.m[axis].size();
         ++component) {
      matrix[axis * kTargetWireAxes + component] = transform.m[axis][component];
    }
  }
  return reinterpret_cast<MATRIX*>(matrix);
}

// decomp: BATTLE.PRG func_800C2254 (src/BATTLE/BATTLE.PRG/58578.c:821).
int AppendSphereCircleVerticesEntry(int angle, int index) {
  auto& sphere = RequiredSphere("sphere circle append");
  std::array<TargetWireVertex, kTargetWireVertices> generated{};
  const auto next = AppendSphereCircleVertices(
      ReadBasis(ScratchShorts(kBasisScratchOffset), kWireBasisStride), angle,
      generated, static_cast<std::size_t>(index));
  if (!next) Refuse("sphere circle append", "invalid geometry inputs");
  WriteVertices(sphere, generated, static_cast<std::size_t>(index), *next);
  return static_cast<int>(*next);
}

// decomp: BATTLE.PRG func_800C2368 (src/BATTLE/BATTLE.PRG/58578.c:845).
int AppendSphereConeVerticesEntry(int step, int radius, int index) {
  auto& sphere = RequiredSphere("sphere cone append");
  std::array<TargetWireVertex, kTargetWireVertices> generated{};
  const auto appended = AppendSphereConeVertices(
      step, radius, generated, static_cast<std::size_t>(index));
  if (!appended) Refuse("sphere cone append", "invalid geometry inputs");
  auto* basis = ScratchShorts(kBasisScratchOffset);
  for (std::size_t axis = 0; axis < appended->basis.size(); ++axis) {
    for (std::size_t component = 0; component < appended->basis[axis].size();
         ++component) {
      basis[axis * kTargetWireAxes + component] =
          appended->basis[axis][component];
    }
  }
  WriteVertices(sphere, generated, static_cast<std::size_t>(index),
                appended->next_index);
  return static_cast<int>(appended->next_index);
}

// decomp: BATTLE.PRG func_800C253C (src/BATTLE/BATTLE.PRG/58578.c:887).
void BuildSphereWireframeEntry(int type) {
  auto* sine = ScratchShorts(kSineScratchOffset);
  const SphereGeometryTables tables{
      .arc_bases = {VS::Battle::sphereArcBases.data(), kSphereArcBaseCount},
      .mirrored_vertices = {VS::Battle::sphereMirroredVertices.data(),
                            kSphereMirroredVertexCount},
      .shape_command_starts = {VS::Battle::sphereShapeCommandStarts.data(),
                               kSphereShapeStartCount},
      .commands = {VS::Battle::sphereWireframeCommands.data(),
                   kSphereCommandCount},
      .sine_samples = {sine, kSphereSineSampleCount},
  };
  if (!BuildLiveSphereWireframe(VS::Battle::targetSphere, type, tables)) {
    Refuse("sphere wireframe build", "invalid geometry state or tables");
  }
}
}  // namespace VS::Battle
