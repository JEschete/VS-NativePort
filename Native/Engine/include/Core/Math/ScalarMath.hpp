#pragma once

#include <array>
#include <concepts>
#include <cstdint>
#include <optional>

#include "Core/Math/Distance.hpp"

// Fixed-point (Q12) scalar, vector and rotation-matrix math shared by the
// renderer, actors and battle code. Values keep the original precision,
// rounding and saturation, but every function takes and returns plain values:
// nothing reads module-loaded tables, emulated GTE registers or raw pointers,
// and none of these routines is reachable through a module address.
namespace VS::Core::Math {
// Rows and columns in a rotation or scale matrix.
inline constexpr int kMatrixOrder = 3;

// A 3x3 rotation or scale matrix with Q12 elements, indexed [row][column].
struct Mat3 {
  std::array<std::array<std::int16_t, kMatrixOrder>, kMatrixOrder> m{};

  friend bool operator==(const Mat3&, const Mat3&) = default;
};

// A vector with full-word components.
struct Vec3 {
  std::int32_t x{};
  std::int32_t y{};
  std::int32_t z{};

  friend bool operator==(const Vec3&, const Vec3&) = default;
};

// Rotation angles in 4096-per-turn units about the x, y and z axes.
struct Angles3 {
  std::int32_t x{};
  std::int32_t y{};
  std::int32_t z{};
};

// Q12 sine and cosine of one angle.
struct SineCosine {
  std::int32_t sine{};
  std::int32_t cosine{};

  friend bool operator==(const SineCosine&, const SineCosine&) = default;
};

// value^-1/2 as mantissa * 2^-exponent: the mantissa is a Q15 reciprocal root
// of the input normalized to 64..255, the exponent restores its scale.
struct ReciprocalRoot {
  std::int32_t mantissa{};
  std::int32_t exponent{};

  friend bool operator==(const ReciprocalRoot&,
                         const ReciprocalRoot&) = default;
};

// A Q12 unit vector and the squared length it was scaled from.
struct NormalizedVector {
  Vec3 direction{};
  std::uint32_t squared_length{};

  friend bool operator==(const NormalizedVector&,
                         const NormalizedVector&) = default;
};

// The low 16 bits of a word read as a signed halfword.
[[nodiscard]] constexpr std::int16_t WrapSigned16(
    std::uint32_t value) noexcept {
  return static_cast<std::int16_t>(static_cast<std::uint16_t>(value));
}

// Q12 sine and cosine of an angle in 4096-per-turn units; only the low 12
// bits of the angle count.
[[nodiscard]] std::int32_t Sine(std::int32_t angle) noexcept;
[[nodiscard]] std::int32_t Cosine(std::int32_t angle) noexcept;

// (left * right) >> 12 computed in 64 bits and truncated to a word.
[[nodiscard]] std::int32_t MultiplyFixed(std::int32_t left,
                                         std::int32_t right) noexcept;

// Squares each component after reducing it to a signed halfword.
[[nodiscard]] Vec3 SquareVectorComponents(Vec3 input) noexcept;

// left * right in Q12, each element saturated to a signed halfword. The four
// names are the original entry points; all compute the same product.
[[nodiscard]] Mat3 MultiplyMatrixIntoRight(const Mat3& left,
                                           const Mat3& right) noexcept;
[[nodiscard]] Mat3 MultiplyMatrixIntoLeft(const Mat3& left,
                                          const Mat3& right) noexcept;
[[nodiscard]] Mat3 MultiplyRotationMatrixInPlace(const Mat3& left,
                                                 const Mat3& right) noexcept;
[[nodiscard]] Mat3 PremultiplyRotationMatrix(const Mat3& left,
                                             const Mat3& right) noexcept;

// floor(sqrt(value)) to the precision of the original 8-bit-normalized table;
// non-positive inputs give 0.
[[nodiscard]] std::int32_t SquareRootInteger(std::int32_t value) noexcept;

// Sine and cosine of one angle as a pair.
[[nodiscard]] SineCosine LookupRotationSineCosine(std::int32_t angle) noexcept;

// Rotation built in the battle Euler order (keeps two extra fraction bits in
// the intermediate products until each final sum).
[[nodiscard]] Mat3 BuildEulerRotationMatrix(Angles3 rotation) noexcept;

// Rotation built in the actor-model Euler order.
[[nodiscard]] Mat3 BuildEulerRotationMatrixQuadrant(Angles3 rotation) noexcept;

// Multiplies each column by the matching scale component, truncated to a
// signed halfword, with each element saturated to a signed halfword.
[[nodiscard]] Mat3 ScaleMatrixColumns(const Mat3& matrix, Vec3 scale) noexcept;

// Splits value^-1/2 into table mantissa and exponent. Zero and -1 have no
// leading-bit position and give no result.
[[nodiscard]] std::optional<ReciprocalRoot> InverseSquareRootParts(
    std::int32_t value) noexcept;

// Scales a vector (components reduced to signed halfwords) to Q12 unit length
// with the reciprocal-root table. A zero vector gives a zero direction.
[[nodiscard]] NormalizedVector NormalizeVectorRegisters(Vec3 input) noexcept;

// A record with a 3x3 halfword rotation member m, such as a MATRIX. Adapters
// read and write only m, so any translation or padding is left untouched.
template <typename Matrix>
concept MatrixRecord = requires(Matrix& matrix) {
  { matrix.m[0][0] } -> std::convertible_to<std::int16_t>;
};

template <MatrixRecord Matrix>
[[nodiscard]] constexpr Mat3 MatrixOf(const Matrix& record) noexcept {
  Mat3 matrix;
  for (int row = 0; row < kMatrixOrder; ++row) {
    for (int column = 0; column < kMatrixOrder; ++column) {
      matrix.m[row][column] = record.m[row][column];
    }
  }
  return matrix;
}

template <MatrixRecord Matrix>
constexpr void StoreMatrix(Matrix& record, const Mat3& matrix) noexcept {
  for (int row = 0; row < kMatrixOrder; ++row) {
    for (int column = 0; column < kMatrixOrder; ++column) {
      record.m[row][column] = matrix.m[row][column];
    }
  }
}

template <VectorRecord Vector>
[[nodiscard]] constexpr Vec3 VectorOf(const Vector& vector) noexcept {
  return Vec3{static_cast<std::int32_t>(vector.vx),
              static_cast<std::int32_t>(vector.vy),
              static_cast<std::int32_t>(vector.vz)};
}

template <VectorRecord Vector>
[[nodiscard]] constexpr Angles3 AnglesOf(const Vector& vector) noexcept {
  return Angles3{static_cast<std::int32_t>(vector.vx),
                 static_cast<std::int32_t>(vector.vy),
                 static_cast<std::int32_t>(vector.vz)};
}

// Record forms of the matrix operations for callers that keep MATRIX/SVECTOR
// records. Each reads its inputs before writing, so an argument may alias the
// destination.
template <MatrixRecord Left, MatrixRecord Right>
void ApplyMultiplyMatrixIntoRight(const Left& left, Right& right) noexcept {
  StoreMatrix(right, MultiplyMatrixIntoRight(MatrixOf(left), MatrixOf(right)));
}

template <MatrixRecord Left, MatrixRecord Right>
void ApplyMultiplyMatrixIntoLeft(Left& left, const Right& right) noexcept {
  StoreMatrix(left, MultiplyMatrixIntoLeft(MatrixOf(left), MatrixOf(right)));
}

template <MatrixRecord Left, MatrixRecord Right>
void ApplyMultiplyRotationMatrixInPlace(const Left& left,
                                        Right& right) noexcept {
  StoreMatrix(right,
              MultiplyRotationMatrixInPlace(MatrixOf(left), MatrixOf(right)));
}

template <MatrixRecord Left, MatrixRecord Right>
void ApplyPremultiplyRotationMatrix(const Left& left, Right& right) noexcept {
  StoreMatrix(right,
              PremultiplyRotationMatrix(MatrixOf(left), MatrixOf(right)));
}

template <VectorRecord Rotation, MatrixRecord Matrix>
void ApplyBuildEulerRotationMatrix(const Rotation& rotation,
                                   Matrix& output) noexcept {
  StoreMatrix(output, BuildEulerRotationMatrix(AnglesOf(rotation)));
}

template <VectorRecord Rotation, MatrixRecord Matrix>
void ApplyBuildEulerRotationMatrixQuadrant(const Rotation& rotation,
                                           Matrix& output) noexcept {
  StoreMatrix(output, BuildEulerRotationMatrixQuadrant(AnglesOf(rotation)));
}

template <MatrixRecord Matrix, VectorRecord Scale>
void ApplyScaleMatrixColumns(Matrix& matrix, const Scale& scale) noexcept {
  StoreMatrix(matrix, ScaleMatrixColumns(MatrixOf(matrix), VectorOf(scale)));
}
}  // namespace VS::Core::Math
