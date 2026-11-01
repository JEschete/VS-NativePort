// Hand-maintained (tools/port/owned.csv).
#include "Battle/TargetingDrawLive.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <span>

#include "Battle/TargetingDraw.hpp"
#include "Core/Log.hpp"
#include "Core/Math/ScalarMath.hpp"
#include "Legacy/Psx.h"
extern "C" {
#include "Battle/BattleTypes.h"
#include "Battle/Sphere.h"
#include "Battle/TargetVolume.h"
#include "Core/Gte.h"
#include "Render/Sprites.h"
}

namespace VS::Battle {
namespace {
namespace math = Core::Math;

constexpr unsigned kCoordinateBits = 16;
static_assert(std::size(Battle_TargetSphere{}.wireVertices) ==
              kTargetWireVertices);

class LiveWireCanvas final : public TargetWireCanvas {
 public:
  math::Mat3 VolumeTransform(const TargetVolumeExtents& extents,
                             std::int16_t angle) override {
    std::array<u_char, kTargetVolumeExtents> scale{};
    std::copy(extents.begin(), extents.end(), scale.begin());
    return math::MatrixOf(
        *Battle_BuildSphereVolumeTransform(scale.data(), angle));
  }

  void BeginProjection() override { Render_LoadFrameViewMatrix(); }

  TargetWirePoint Project(const TargetWorldPoint& world) override {
    SVECTOR point = {world[0], world[1], world[2], 0};
    Core_GteWord screen = 0;
    Core_GteWord depth = 0;
    Core_Gte_LoadV0(&point);
    Core_Gte_Rtps();
    Core_Gte_StoreSxy2(&screen);
    Core_Gte_StoreSz3Otz(&depth);
    const auto packed = static_cast<std::uint32_t>(screen);
    return {.x = math::WrapSigned16(packed),
            .y = math::WrapSigned16(packed >> kCoordinateBits),
            .depth = math::WrapSigned16(static_cast<std::uint32_t>(depth))};
  }

  void DrawLine(const TargetWirePoint& start, const TargetWirePoint& end,
                std::uint32_t command_color, int intensity) override {
    SVECTOR from = {start.x, start.y, start.depth, 0};
    SVECTOR to = {end.x, end.y, end.depth, 0};
    Battle_DrawSphereWireLine(&from, &to, command_color, intensity);
  }
};
}  // namespace

// decomp: BATTLE.PRG func_800C0B50 (src/BATTLE/BATTLE.PRG/58350.s:8).
void DrawLiveSphereVolumeWireframe(Battle_TargetVolume* shape, int color) {
  const auto* sphere =
      reinterpret_cast<Battle_TargetSphere*>(Battle_targetSphere);
  if (sphere == nullptr || shape == nullptr || sphere->vertexCount < 2) return;
  const auto count = static_cast<std::size_t>(sphere->vertexCount);
  if (count > kTargetWireVertices) {
    VS_LOG_ERROR("targeting", "Invalid wireframe vertex count {}",
                 sphere->vertexCount);
    std::abort();
  }
  std::array<TargetWireVertex, kTargetWireVertices> vertices{};
  for (std::size_t index = 0; index < count; ++index) {
    const Battle_ShapeVertex& source = sphere->wireVertices[index];
    vertices[index] = {
        .position = {source.xyz[0], source.xyz[1], source.xyz[2]},
        .segment = source.opaque6};
  }
  const TargetWireState wire{.phase_a = sphere->wirePhaseA,
                             .phase_b = sphere->wirePhaseB,
                             .phase_c = sphere->wirePhaseC,
                             .pulse = sphere->wireShape,
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
