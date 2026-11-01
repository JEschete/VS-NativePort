// Hand-maintained (tools/port/owned.csv).
#include "Battle/TargetingDraw.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

#include "Core/Math/ScalarMath.hpp"

namespace VS::Battle {
namespace {
namespace math = Core::Math;

constexpr std::size_t kSpatialDimensions = 3;
constexpr unsigned kFixedFractionBits = 12;
constexpr std::int64_t kFixedOne = std::int64_t{1} << kFixedFractionBits;
constexpr int kPulseAngleStep = 64;
constexpr int kIntensityMaximum = 32;
constexpr std::uint32_t kWireCommand = 0x44000000;
constexpr unsigned kSegmentFlagShift = 25;

std::array<std::int16_t, kSpatialDimensions> WorldPoint(
    const TargetWireVolume& volume, const math::Mat3& rotation,
    const TargetWireVertex& vertex) {
  std::array<std::int16_t, kSpatialDimensions> world{};
  for (std::size_t axis = 0; axis < kSpatialDimensions; ++axis) {
    std::int64_t sum = std::int64_t{volume.position[axis]} * kFixedOne;
    for (std::size_t component = 0; component < kSpatialDimensions;
         ++component) {
      sum += std::int64_t{rotation.m[axis][component]} *
             vertex.position[component];
    }
    world[axis] = static_cast<std::int16_t>(std::clamp<std::int64_t>(
        sum >> kFixedFractionBits, std::numeric_limits<std::int16_t>::min(),
        std::numeric_limits<std::int16_t>::max()));
  }
  return world;
}
}  // namespace

// decomp: BATTLE.PRG func_800C0B50 (src/BATTLE/BATTLE.PRG/58350.s:8).
void DrawSphereVolumeWireframe(const TargetWireState& wire,
                               const TargetWireVolume& volume,
                               std::uint32_t color, TargetWireCanvas& canvas) {
  if (wire.vertices.size() < 2) return;
  const int extent = std::max(volume.extents[0], volume.extents[2]);
  if (extent == 0) return;
  const math::Mat3 transform =
      canvas.VolumeTransform(volume.extents, volume.angle);
  const std::int32_t pulse = math::Cosine(wire.pulse * kPulseAngleStep);
  const math::Mat3 rotation = math::MultiplyMatrixIntoRight(
      transform,
      math::ScaleMatrixColumns(math::BuildRotationMatrixYXZ(
                                   {wire.phase_a, wire.phase_b, wire.phase_c}),
                               {pulse, pulse, pulse}));
  canvas.BeginProjection();
  TargetWirePoint previous;
  for (std::size_t index = 0; index < wire.vertices.size(); ++index) {
    const TargetWireVertex& vertex = wire.vertices[index];
    const TargetWirePoint projected =
        canvas.Project(WorldPoint(volume, rotation, vertex));
    if (index != 0 && vertex.segment != 0) {
      const int intensity =
          kIntensityMaximum -
          ((projected.depth + previous.depth - wire.depth * 2) / extent);
      const std::uint32_t command_color =
          (kWireCommand | color) -
          (static_cast<std::uint32_t>(vertex.segment) << kSegmentFlagShift);
      canvas.DrawLine(projected, previous, command_color, intensity);
    }
    previous = projected;
  }
}
}  // namespace VS::Battle
