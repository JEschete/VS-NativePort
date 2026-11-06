// Hand-maintained (tools/port/owned.csv).
#include "Effects/Templates/RotatingSurface.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>

#include "Core/Math/Interpolation.hpp"
#include "Core/Math/ScalarMath.hpp"
#include "Effects/Templates/RingCore.hpp"
#include "Effects/Templates/SurfaceCore.hpp"

namespace VS::Effects::Templates {
namespace {
namespace math = Core::Math;

// A quarter turn in the 4096-step angle unit, the quarters of a row and the
// columns per quarter.
constexpr std::int32_t kQuarterTurn = 1024;
constexpr std::size_t kQuarters = 4;
constexpr std::size_t kQuarterColumns = kSurfaceColumns / kQuarters;
// The angle between neighbouring columns of a quarter.
constexpr auto kColumnTurn = kQuarterTurn / std::int32_t{kQuarterColumns};

// The angle's motion from row to row.
struct RowTwist {
  std::int32_t angle{};
  std::int32_t speed{};
  std::int32_t acceleration{};
};

// Where one row lies: its radius and angle around the origin and its height
// above it.
struct RowPlacement {
  math::Vec3 origin{};
  std::int32_t radius{};
  std::int32_t angle{};
  std::int32_t lift{};
};

// decomp: rotatingSurfaces.h:163-179: the four columns of the first quarter
// at the row's angle, each repeated a quarter turn further three times; x
// and z of each copy are the previous copy's (-z, x).
[[nodiscard]] std::array<RingPoint, kSurfaceColumns> TurnRow(
    const RowPlacement& at) {
  const math::Vec3& origin = at.origin;
  std::array<RingPoint, kSurfaceColumns> row{};
  for (std::size_t column = 0; column < kQuarterColumns; ++column) {
    const std::int32_t turn = math::WrappingAdd(
        at.angle, static_cast<std::int32_t>(column) * kColumnTurn);
    std::int32_t x = RingFixed(at.radius, math::Cosine(turn));
    std::int32_t z = RingFixed(at.radius, math::Sine(turn));
    for (std::size_t quarter = 0; quarter < kQuarters; ++quarter) {
      row.at((quarter * kQuarterColumns) + column) = RingPointAt(
          {math::WrappingAdd(origin.x, x), math::WrappingAdd(origin.y, at.lift),
           math::WrappingAdd(origin.z, z)});
      const std::int32_t turned_x = -z;
      z = x;
      x = turned_x;
    }
  }
  return row;
}
}  // namespace

// decomp: rotatingSurfaces.h:144-155 (the turn of one tick).
std::int32_t RotatingSurfaceStep(math::Vec2 speed, SurfaceHost& host) {
  if (speed.x == speed.y) {
    return speed.x;
  }
  const std::int32_t low = std::min(speed.x, speed.y);
  const std::int32_t high = std::max(speed.x, speed.y);
  const std::int32_t width =
      math::WrappingAdd(math::WrappingSubtract(high, low), 1);
  return math::WrappingAdd(SurfaceRemainder(host.Rand(), width), low);
}

// decomp: rotatingSurfaces.h:156-187 (the points of the five rows).
SheetPoints RotatingSurfacePoints(SheetFrame& frame, std::int32_t step) {
  frame.state.angle = math::WrappingAdd(frame.state.angle, step);
  const math::Vec3 origin = frame.scratch.position;
  const SurfaceForces forces = frame.scratch.forces.value_or(SurfaceForces{});
  const math::Vec3 twist =
      frame.scratch.direction_jitter.value_or(math::Vec3{});
  SheetRowGrowth growth{.radius = frame.radius, .height = frame.height};
  RowTwist motion{.angle = frame.state.angle};
  std::int32_t lift = 0;
  SheetPoints points{};
  std::size_t next = 0;
  for (std::size_t row = 0; row < kSurfaceRows; ++row) {
    for (const RingPoint& point : TurnRow({.origin = origin,
                                           .radius = growth.radius,
                                           .angle = motion.angle,
                                           .lift = lift})) {
      points.at(next++) = point;
    }
    GrowSheetRow(growth, forces);
    lift = math::WrappingAdd(lift, growth.height);
    motion.acceleration = math::WrappingAdd(motion.acceleration, twist.z);
    motion.speed = math::WrappingAdd(
        motion.speed, math::WrappingAdd(motion.acceleration, twist.y));
    motion.angle = math::WrappingAdd(motion.angle,
                                     math::WrappingAdd(motion.speed, twist.x));
  }
  return points;
}

// decomp: EFFECT VS_ROTATING_SURFACE_FUNCTION
// (src/EFFECT/rotatingSurfaces.h:34).
bool RotatingSurface(SurfaceTextureSpan span, SurfaceCommand command,
                     std::int32_t packed, SheetHost& host) {
  return RunSheet(command, packed, kSurfaceRows, span, host,
                  [&host](SheetFrame& frame) {
                    const std::int32_t step = RotatingSurfaceStep(
                        frame.scratch.speed.value_or(math::Vec2{}), host);
                    return RotatingSurfacePoints(frame, step);
                  });
}
}  // namespace VS::Effects::Templates

namespace VS::Effects {
// decomp: PLG049.BIN func_800FD51C (src/EFFECT/PLG049.BIN/E0.c:24).
bool Plg049::RotatingSurface(Templates::SurfaceCommand command,
                             std::int32_t packed, Templates::SheetHost& host) {
  return Templates::RotatingSurface(
      Templates::SurfaceTextureSpan::kSixteenColumns, command, packed, host);
}

// decomp: PLG065.BIN func_800F98F8 (src/EFFECT/PLG065.BIN/F8.c:8).
bool Plg065::RotatingSurface(Templates::SurfaceCommand command,
                             std::int32_t packed, Templates::SheetHost& host) {
  return Templates::RotatingSurface(
      Templates::SurfaceTextureSpan::kSixteenColumns, command, packed, host);
}

// decomp: PLG073.BIN func_800FD534 (src/EFFECT/PLG073.BIN/F8.c:20).
// decomp: PLG074.BIN func_800FD534 (src/EFFECT/PLG074.BIN/F8.c:21).
bool Plg073::RotatingSurface(Templates::SurfaceCommand command,
                             std::int32_t packed, Templates::SheetHost& host) {
  return Templates::RotatingSurface(
      Templates::SurfaceTextureSpan::kSixteenColumns, command, packed, host);
}

// decomp: PLG081.BIN func_800F9800 (src/EFFECT/PLG081.BIN/0.c:8).
bool Plg081::RotatingSurface(Templates::SurfaceCommand command,
                             std::int32_t packed, Templates::SheetHost& host) {
  return Templates::RotatingSurface(
      Templates::SurfaceTextureSpan::kSixteenColumns, command, packed, host);
}

// decomp: PLG086.BIN func_800FA868 (src/EFFECT/PLG086.BIN/F8.c:13).
bool Plg086::RotatingSurface(Templates::SurfaceCommand command,
                             std::int32_t packed, Templates::SheetHost& host) {
  return Templates::RotatingSurface(
      Templates::SurfaceTextureSpan::kSixteenColumns, command, packed, host);
}

// decomp: PLG128.BIN func_800FB400 (src/EFFECT/PLG128.BIN/F8.c:56).
bool Plg128::RotatingSurface(Templates::SurfaceCommand command,
                             std::int32_t packed, Templates::SheetHost& host) {
  return Templates::RotatingSurface(Templates::SurfaceTextureSpan::kFourColumns,
                                    command, packed, host);
}

// decomp: PLG129.BIN func_800FC6EC (src/EFFECT/PLG129.BIN/0.c:23).
bool Plg129::RotatingSurface(Templates::SurfaceCommand command,
                             std::int32_t packed, Templates::SheetHost& host) {
  return Templates::RotatingSurface(Templates::SurfaceTextureSpan::kFourColumns,
                                    command, packed, host);
}

// decomp: PLG138.BIN func_800F9818 (src/EFFECT/PLG138.BIN/18.c:2).
bool Plg138::RotatingSurface(Templates::SurfaceCommand command,
                             std::int32_t packed, Templates::SheetHost& host) {
  return Templates::RotatingSurface(
      Templates::SurfaceTextureSpan::kSixteenColumns, command, packed, host);
}

// decomp: PLG142.BIN func_800FBEFC (src/EFFECT/PLG142.BIN/18.c:22).
bool Plg142::RotatingSurface(Templates::SurfaceCommand command,
                             std::int32_t packed, Templates::SheetHost& host) {
  return Templates::RotatingSurface(Templates::SurfaceTextureSpan::kFourColumns,
                                    command, packed, host);
}

// decomp: PLG158.BIN func_800FBF7C (src/EFFECT/PLG158.BIN/0.c:24).
bool Plg158::RotatingSurface(Templates::SurfaceCommand command,
                             std::int32_t packed, Templates::SheetHost& host) {
  return Templates::RotatingSurface(Templates::SurfaceTextureSpan::kFourColumns,
                                    command, packed, host);
}

// decomp: PLG161.BIN func_800F98F8 (src/EFFECT/PLG161.BIN/F8.c:2).
bool Plg161::RotatingSurface(Templates::SurfaceCommand command,
                             std::int32_t packed, Templates::SheetHost& host) {
  return Templates::RotatingSurface(
      Templates::SurfaceTextureSpan::kSixteenColumns, command, packed, host);
}

// decomp: PLG162.BIN func_800FA740 (src/EFFECT/PLG162.BIN/0.c:336).
bool Plg162::RotatingSurface(Templates::SurfaceCommand command,
                             std::int32_t packed, Templates::SheetHost& host) {
  return Templates::RotatingSurface(
      Templates::SurfaceTextureSpan::kSixteenColumns, command, packed, host);
}

// decomp: PLG165.BIN func_800FBA18 (src/EFFECT/PLG165.BIN/F8.c:18).
bool Plg165::RotatingSurface(Templates::SurfaceCommand command,
                             std::int32_t packed, Templates::SheetHost& host) {
  return Templates::RotatingSurface(
      Templates::SurfaceTextureSpan::kSixteenColumns, command, packed, host);
}

// decomp: PLG166.BIN func_800F9800 (src/EFFECT/PLG166.BIN/0.c:8).
bool Plg166::RotatingSurface(Templates::SurfaceCommand command,
                             std::int32_t packed, Templates::SheetHost& host) {
  return Templates::RotatingSurface(
      Templates::SurfaceTextureSpan::kSixteenColumns, command, packed, host);
}

// decomp: PLG167.BIN func_800FA84C (src/EFFECT/PLG167.BIN/E0.c:12).
bool Plg167::RotatingSurface(Templates::SurfaceCommand command,
                             std::int32_t packed, Templates::SheetHost& host) {
  return Templates::RotatingSurface(
      Templates::SurfaceTextureSpan::kSixteenColumns, command, packed, host);
}

// decomp: PLG170.BIN func_800FA48C (src/EFFECT/PLG170.BIN/18.c:14).
bool Plg170::RotatingSurface(Templates::SurfaceCommand command,
                             std::int32_t packed, Templates::SheetHost& host) {
  return Templates::RotatingSurface(Templates::SurfaceTextureSpan::kFourColumns,
                                    command, packed, host);
}

// decomp: PLG236.BIN func_800FA770 (src/EFFECT/PLG236.BIN/0.c:15).
bool Plg236::RotatingSurface(Templates::SurfaceCommand command,
                             std::int32_t packed, Templates::SheetHost& host) {
  return Templates::RotatingSurface(Templates::SurfaceTextureSpan::kFourColumns,
                                    command, packed, host);
}

// decomp: PLG246.BIN func_800F9800 (src/EFFECT/PLG246.BIN/0.c:2).
bool Plg246::RotatingSurface(Templates::SurfaceCommand command,
                             std::int32_t packed, Templates::SheetHost& host) {
  return Templates::RotatingSurface(
      Templates::SurfaceTextureSpan::kSixteenColumns, command, packed, host);
}

// decomp: PLG250.BIN func_800F9800 (src/EFFECT/PLG250.BIN/0.c:10).
bool Plg250::RotatingSurface(Templates::SurfaceCommand command,
                             std::int32_t packed, Templates::SheetHost& host) {
  return Templates::RotatingSurface(Templates::SurfaceTextureSpan::kFourColumns,
                                    command, packed, host);
}
}  // namespace VS::Effects
