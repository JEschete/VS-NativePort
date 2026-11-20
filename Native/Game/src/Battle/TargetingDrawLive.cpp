// Hand-maintained (tools/port/owned.csv).
#include "Battle/TargetingDrawLive.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <span>

#include "Battle/BattleTypes.hpp"
#include "Battle/Sphere.hpp"
#include "Battle/SphereDrawLive.hpp"
#include "Battle/SphereGeometryLive.hpp"
#include "Battle/TargetVolume.hpp"
#include "Battle/TargetingDraw.hpp"
#include "Core/Gte.hpp"
#include "Core/Log.hpp"
#include "Core/Math/ScalarMath.hpp"
#include "Legacy/Psx.hpp"
#include "Render/SpritesLive.hpp"

using VS::Core::Gte::LoadV0;
using VS::Core::Gte::Rtps;
using VS::Core::Gte::StoreSxy2;
using VS::Core::Gte::StoreSz3Otz;
using VS::Core::Gte::SVECTOR;

using VS::Battle::ShapeVertex;
using VS::Battle::TargetVolume;

namespace VS::Battle {
namespace {
namespace math = Core::Math;

constexpr unsigned kCoordinateBits = 16;
static_assert(std::size(VS::Battle::TargetSphereRecord{}.wire_vertices) ==
              kTargetWireVertices);

class LiveWireCanvas final : public TargetWireCanvas {
 public:
  math::Mat3 VolumeTransform(const TargetVolumeExtents& extents,
                             std::int16_t angle) override {
    std::array<std::uint8_t, kTargetVolumeExtents> scale{};
    std::copy(extents.begin(), extents.end(), scale.begin());
    return math::MatrixOf(
        *BuildSphereVolumeTransformEntry(scale.data(), angle));
  }

  void BeginProjection() override { VS::Render::LoadLiveFrameViewMatrix(); }

  TargetWirePoint Project(const TargetWorldPoint& world) override {
    SVECTOR point = {world[0], world[1], world[2], 0};
    VS::Core::Gte::Word screen = 0;
    VS::Core::Gte::Word depth = 0;
    LoadV0(&point);
    Rtps();
    StoreSxy2(&screen);
    StoreSz3Otz(&depth);
    const auto packed = static_cast<std::uint32_t>(screen);
    return {.x = math::WrapSigned16(packed),
            .y = math::WrapSigned16(packed >> kCoordinateBits),
            .depth = math::WrapSigned16(static_cast<std::uint32_t>(depth))};
  }

  void DrawLine(const TargetWirePoint& start, const TargetWirePoint& end,
                std::uint32_t command_color, int intensity) override {
    SVECTOR from = {start.x, start.y, start.depth, 0};
    SVECTOR to = {end.x, end.y, end.depth, 0};
    LiveDrawSphereWireLine(&from, &to, command_color, intensity);
  }
};
}  // namespace

// decomp: BATTLE.PRG func_800C0B50 (src/BATTLE/BATTLE.PRG/58350.s:8).
void DrawLiveSphereVolumeWireframe(TargetVolume* shape, int color) {
  const auto* sphere = reinterpret_cast<VS::Battle::TargetSphereRecord*>(
      VS::Battle::targetSphere);
  if (sphere == nullptr || shape == nullptr || sphere->vertex_count < 2) return;
  const auto count = static_cast<std::size_t>(sphere->vertex_count);
  if (count > kTargetWireVertices) {
    VS::Core::LogError("targeting", "Invalid wireframe vertex count {}",
                       sphere->vertex_count);
    std::abort();
  }
  std::array<TargetWireVertex, kTargetWireVertices> vertices{};
  for (std::size_t index = 0; index < count; ++index) {
    const ShapeVertex& source = sphere->wire_vertices[index];
    vertices[index] = {
        .position = {source.xyz[0], source.xyz[1], source.xyz[2]},
        .segment = source.segment};
  }
  const TargetWireState wire{.phase_a = sphere->wire_phase_a,
                             .phase_b = sphere->wire_phase_b,
                             .phase_c = sphere->wire_phase_c,
                             .pulse = sphere->wire_shape,
                             .depth = sphere->depth,
                             .vertices = std::span(vertices).first(count)};
  const TargetWireVolume volume{
      .extents = {shape->scale.extents[0], shape->scale.extents[1],
                  shape->scale.extents[2], shape->scale.extents[3]},
      .angle = shape->angle,
      .position = {shape->position[0], shape->position[1], shape->position[2]}};
  LiveWireCanvas canvas;
  DrawSphereVolumeWireframe(wire, volume, static_cast<std::uint32_t>(color),
                            canvas);
}
}  // namespace VS::Battle
