#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include "Core/Math/ScalarMath.hpp"

// The pulsing wireframe drawn over each volume of a spell's target sphere.
// The sphere's wire state is passed as values and projection and drawing go
// through TargetWireCanvas, so the wireframe reads no target sphere global,
// GTE register or PS1 address.
namespace VS::Battle {
// The wire vertices the target sphere can hold.
inline constexpr std::size_t kTargetWireVertices = 297;
inline constexpr std::size_t kTargetWireAxes = 3;
inline constexpr std::size_t kTargetVolumeExtents = 4;

using TargetWorldPoint = std::array<std::int16_t, kTargetWireAxes>;
// Extents 0 to 2 scale the volume's axes and extent 3 tilts it.
using TargetVolumeExtents = std::array<std::uint8_t, kTargetVolumeExtents>;

struct TargetWireVertex {
  TargetWorldPoint position{};
  // Nonzero when a segment joins the previous vertex; also lowers the line
  // command's code by that many steps of 0x02000000.
  std::int16_t segment = 0;
};

// The target sphere's wireframe animation and its centre's ordering depth.
struct TargetWireState {
  std::int16_t phase_a = 0;
  std::int16_t phase_b = 0;
  std::int16_t phase_c = 0;
  std::int16_t pulse = 0;
  std::int16_t depth = 0;
  std::span<const TargetWireVertex> vertices;
};

// One targeting volume: extents 0 to 2 scale its axes, extent 3 tilts it.
struct TargetWireVolume {
  TargetVolumeExtents extents{};
  std::int16_t angle = 0;
  TargetWorldPoint position{};
};

struct TargetWirePoint {
  std::int16_t x = 0;
  std::int16_t y = 0;
  std::int16_t depth = 0;
};

class TargetWireCanvas {
 public:
  TargetWireCanvas() = default;
  TargetWireCanvas(const TargetWireCanvas&) = delete;
  TargetWireCanvas& operator=(const TargetWireCanvas&) = delete;
  TargetWireCanvas(TargetWireCanvas&&) = delete;
  TargetWireCanvas& operator=(TargetWireCanvas&&) = delete;
  virtual ~TargetWireCanvas() = default;

  // The volume's scale-and-tilt matrix (func_800C085C).
  [[nodiscard]] virtual Core::Math::Mat3 VolumeTransform(
      const TargetVolumeExtents& extents, std::int16_t angle) = 0;
  // Loads the frame view for the projections that follow.
  virtual void BeginProjection() = 0;
  // Projects a world point to the screen and its ordering depth.
  [[nodiscard]] virtual TargetWirePoint Project(
      const TargetWorldPoint& world) = 0;
  // Draws one faded segment (func_800C0990).
  virtual void DrawLine(const TargetWirePoint& start,
                        const TargetWirePoint& end, std::uint32_t command_color,
                        int intensity) = 0;
};

// Draws the sphere's wireframe over `volume` in `color` (func_800C0B50).
// Nothing is drawn for fewer than two vertices or a volume with no horizontal
// extent.
void DrawSphereVolumeWireframe(const TargetWireState& wire,
                               const TargetWireVolume& volume,
                               std::uint32_t color, TargetWireCanvas& canvas);
}  // namespace VS::Battle
