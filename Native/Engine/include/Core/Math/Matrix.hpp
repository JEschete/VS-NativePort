#pragma once
// Hand-maintained (tools/port/owned.csv).

#include "Core/Math/Distance.hpp"
#include "Core/Math/ScalarMath.hpp"

// Q12 rotation and transform construction shared by the event camera, battle
// effects and actor models. Every function returns a plain matrix value:
// nothing reads module-loaded tables or GTE registers, and none of these
// routines is reachable through a module address. MatrixLive.hpp binds them
// to MATRIX records.
namespace VS::Core::Math {
// A rotation and a translation, as a MATRIX record holds them.
struct Transform3 {
  Mat3 rotation{};
  Vec3 translation{};

  friend bool operator==(const Transform3&, const Transform3&) = default;
};

// An orientation whose third column points from `from` towards `to`, with the
// second column as the up axis (world up is -y) and the first as right. Each
// axis is a Q12 unit vector from the reciprocal-root table.
[[nodiscard]] Mat3 BuildLookAtOrientation(Vec3 from, Vec3 to) noexcept;

// BuildRotationXyz of the angles a Q12 direction points along: y is the
// Arctangent2 of x over z, x the Arctangent2 of y over the horizontal length,
// z is 0. The direction loses its fraction bits (to signed halfwords) first.
[[nodiscard]] Mat3 DirectionRotationMatrix(Vec3 direction) noexcept;

// Rotation built in the libgte RotMatrix order (x, then y, then z), keeping
// the original's doubled sine/cosine products and their truncation.
[[nodiscard]] Mat3 BuildRotationXyz(Angles3 rotation) noexcept;

// The Q12 identity rotation.
[[nodiscard]] Mat3 IdentityRotation() noexcept;

// An identity rotation with a translation.
[[nodiscard]] Transform3 TranslationTransform(Position3 translation) noexcept;

// The GTE RTV0TR product the MAC registers hold: `point`, reduced to signed
// halfwords as V0 holds it, rotated and translated by `transform` as
// (T * 4096 + R * v) >> 12, each component truncated to a word.
[[nodiscard]] Vec3 RotateTranslate(const Transform3& transform,
                                   Vec3 point) noexcept;
}  // namespace VS::Core::Math
