#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstdint>

#include "Core/Math/Distance.hpp"
#include "Core/Math/ScalarMath.hpp"

// Fixed-point (Q12) vector arithmetic shared by the event camera, battle
// effects and actors. Components wrap like the original 32-bit registers, and
// every function takes and returns plain values: nothing is reachable through
// a module address. VectorLive.hpp binds these to VECTOR/SVECTOR records.
namespace VS::Core::Math {
// Adds two packed screen coordinates (x in the low halfword, y in the high
// halfword). x wraps within its halfword without carrying into y.
[[nodiscard]] std::int32_t AddPackedScreenCoordinates(
    std::int32_t left, std::int32_t right) noexcept;

// Component-wise left + right and left - right, wrapping.
[[nodiscard]] Vec3 AddVectors(Vec3 left, Vec3 right) noexcept;
[[nodiscard]] Vec3 SubtractVectors(Vec3 left, Vec3 right) noexcept;

// The vector itself: the original copies between records, and the binding
// converts between record widths (a halfword store keeps the low 16 bits).
[[nodiscard]] Vec3 CopyVector(Vec3 source) noexcept;

// The component with the greatest magnitude, with its sign. Ties keep the
// earlier component. Magnitudes are 32-bit negations as on the original, so
// INT32_MIN ranks below every other value.
[[nodiscard]] std::int32_t GreatestMagnitudeComponent(Vec3 components) noexcept;

// The argument with the larger magnitude, with its sign; a tie keeps first.
// Magnitudes are compared as for GreatestMagnitudeComponent.
[[nodiscard]] std::int32_t LargerMagnitude(std::int32_t first,
                                           std::int32_t second) noexcept;

// Each component multiplied by a Q12 scale (MultiplyFixed).
[[nodiscard]] Vec3 ScaleFixedVector(Vec3 source, std::int32_t scale) noexcept;

// Length of a Q12 vector in Q12: each component loses its fraction bits
// before squaring, so lengths are whole units. The squares and their sum wrap
// as on the original for components of 2^15 units and more.
[[nodiscard]] std::int32_t FixedVectorMagnitude(Vec3 source) noexcept;

// A short vector in whole units scaled to Q12.
[[nodiscard]] Vec3 ShortVectorToFixed(Position3 source) noexcept;

// A Q12 vector normalized to a Q12 unit direction after dropping its fraction
// bits; a vector shorter than one unit gives a zero direction.
[[nodiscard]] Vec3 NormalizeFixedVector(Vec3 source) noexcept;

// The Q12 cross product left x right of two vectors whose components are
// reduced to signed halfwords (the GTE's OP inputs). Results are full words.
[[nodiscard]] Vec3 CrossProductFixed(Vec3 left, Vec3 right) noexcept;
}  // namespace VS::Core::Math
