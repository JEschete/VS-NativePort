#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstdint>
#include <type_traits>

#include "Core/Math/Distance.hpp"
#include "Core/Math/Matrix.hpp"
#include "Core/Math/ScalarMath.hpp"

// Record bindings of Matrix.hpp and the Euler rotation builder for generated
// callers that still pass SVECTOR/VECTOR/MATRIX records in the original
// argument order (tools/port/copy_fixups.py OWNED_CALL_BINDINGS). Each reads
// every input before writing, so an argument may alias the destination, and
// writes only the members the original stored.
namespace VS::Core::Math {
// A MATRIX-like record with a rotation m and a translation t.
template <typename Matrix>
concept TransformRecord = MatrixRecord<Matrix> && requires(Matrix& matrix) {
  matrix.t[0];
  matrix.t[1];
  matrix.t[2];
};

template <TransformRecord Matrix>
[[nodiscard]] constexpr Transform3 TransformOf(const Matrix& record) noexcept {
  return Transform3{MatrixOf(record),
                    Vec3{static_cast<std::int32_t>(record.t[0]),
                         static_cast<std::int32_t>(record.t[1]),
                         static_cast<std::int32_t>(record.t[2])}};
}

template <TransformRecord Matrix>
constexpr void StoreTransform(Matrix& record,
                              const Transform3& transform) noexcept {
  using Component = std::remove_cvref_t<decltype(record.t[0])>;
  StoreMatrix(record, transform.rotation);
  record.t[0] = static_cast<Component>(transform.translation.x);
  record.t[1] = static_cast<Component>(transform.translation.y);
  record.t[2] = static_cast<Component>(transform.translation.z);
}

// RotMatrix(rotation, matrix): returns the matrix.
template <VectorRecord Rotation, MatrixRecord Matrix>
Matrix* BuildLiveRotationXyz(const Rotation* rotation,
                             Matrix* matrix) noexcept {
  StoreMatrix(*matrix, BuildRotationXyz(AnglesOf(*rotation)));
  return matrix;
}

// RotMatrixYXZ(rotation, matrix): returns the matrix.
template <VectorRecord Rotation, MatrixRecord Matrix>
Matrix* BuildLiveEulerRotationMatrix(const Rotation* rotation,
                                     Matrix* matrix) noexcept {
  StoreMatrix(*matrix, BuildEulerRotationMatrix(AnglesOf(*rotation)));
  return matrix;
}

// vs_battle_lookAt(from, to, matrix).
template <VectorRecord From, VectorRecord To, MatrixRecord Matrix>
void BuildLiveLookAtOrientation(const From* from, const To* to,
                                Matrix* matrix) noexcept {
  StoreMatrix(*matrix, BuildLookAtOrientation(VectorOf(*from), VectorOf(*to)));
}

// _vecToRotMatrix(direction, matrix).
template <VectorRecord Direction, MatrixRecord Matrix>
void BuildLiveDirectionRotationMatrix(const Direction* direction,
                                      Matrix* matrix) noexcept {
  StoreMatrix(*matrix, DirectionRotationMatrix(VectorOf(*direction)));
}

// func_800CFE7C(matrix): the rotation only. The original's word stores also
// cleared the padding after m, which nothing reads.
template <MatrixRecord Matrix>
void SetLiveIdentityRotation(Matrix* matrix) noexcept {
  StoreMatrix(*matrix, IdentityRotation());
}

// func_800CFE98(translation, matrix): identity rotation and the translation.
template <VectorRecord Translation, TransformRecord Matrix>
void SetLiveTranslationTransform(const Translation* translation,
                                 Matrix* matrix) noexcept {
  StoreTransform(*matrix, TranslationTransform(PositionOf(*translation)));
}
}  // namespace VS::Core::Math
