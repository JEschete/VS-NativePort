#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstdint>

#include "Core/Math/Distance.hpp"
#include "Core/Math/ScalarMath.hpp"
#include "Core/Math/Vector.hpp"

// Record bindings of Vector.hpp and NormalizeVector for generated callers
// that still pass VECTOR/SVECTOR records in the original argument order
// (tools/port/copy_fixups.py OWNED_CALL_BINDINGS). Each reads every input
// before writing, so an argument may alias the destination. Destinations are
// written through StoreVector, which keeps the member width and leaves any
// padding untouched.
namespace VS::Core::Math {
// _vectorAdd(destination, left, right): returns the destination.
template <VectorRecord Destination, VectorRecord Left, VectorRecord Right>
Destination* AddLiveVectors(Destination* destination, const Left* left,
                            const Right* right) noexcept {
  StoreVector(*destination, AddVectors(VectorOf(*left), VectorOf(*right)));
  return destination;
}

// _addSVectorToVector, vs_battle_addVector and vs_battle_addVecToSvec:
// (left, right, destination).
template <VectorRecord Left, VectorRecord Right, VectorRecord Destination>
void AddLiveVectorsTo(const Left* left, const Right* right,
                      Destination* destination) noexcept {
  StoreVector(*destination, AddVectors(VectorOf(*left), VectorOf(*right)));
}

// _vectorSubtract(destination, left, right): returns the destination.
template <VectorRecord Destination, VectorRecord Left, VectorRecord Right>
Destination* SubtractLiveVectors(Destination* destination, const Left* left,
                                 const Right* right) noexcept {
  StoreVector(*destination, SubtractVectors(VectorOf(*left), VectorOf(*right)));
  return destination;
}

// _copyVector(destination, source): returns the destination.
template <VectorRecord Destination, VectorRecord Source>
Destination* CopyLiveVector(Destination* destination,
                            const Source* source) noexcept {
  StoreVector(*destination, CopyVector(VectorOf(*source)));
  return destination;
}

// vs_battle_svecToVec and vs_battle_vecToSvec: (source, destination). A
// halfword destination keeps the low 16 bits of each component.
template <VectorRecord Source, VectorRecord Destination>
void CopyLiveVectorTo(const Source* source, Destination* destination) noexcept {
  StoreVector(*destination, CopyVector(VectorOf(*source)));
}

// _absMax3(first, second, third).
[[nodiscard]] inline std::int32_t SelectLiveGreatestMagnitude(
    // NOLINTNEXTLINE(bugprone-easily-swappable-parameters): call shape.
    std::int32_t first, std::int32_t second, std::int32_t third) noexcept {
  return GreatestMagnitudeComponent(Vec3{first, second, third});
}

// _vectorFixedPointMult(destination, source, scale): returns the destination.
template <VectorRecord Destination, VectorRecord Source>
Destination* ScaleLiveFixedVector(Destination* destination,
                                  const Source* source,
                                  std::int32_t scale) noexcept {
  StoreVector(*destination, ScaleFixedVector(VectorOf(*source), scale));
  return destination;
}

// _vectorMagnitude(source).
template <VectorRecord Source>
[[nodiscard]] std::int32_t MeasureLiveFixedVector(
    const Source* source) noexcept {
  return FixedVectorMagnitude(VectorOf(*source));
}

// _sVectorToFixedPointVector(destination, source): returns the destination.
template <VectorRecord Destination, VectorRecord Source>
Destination* WidenLiveShortVectorToFixed(Destination* destination,
                                         const Source* source) noexcept {
  StoreVector(*destination, ShortVectorToFixed(PositionOf(*source)));
  return destination;
}

// _toNormalIntegerVector(source, destination).
template <VectorRecord Source, VectorRecord Destination>
void NormalizeLiveFixedVector(const Source* source,
                              Destination* destination) noexcept {
  StoreVector(*destination, NormalizeFixedVector(VectorOf(*source)));
}

// OuterProduct0(left, right, destination).
template <VectorRecord Left, VectorRecord Right, VectorRecord Destination>
void StoreLiveCrossProduct(const Left* left, const Right* right,
                           Destination* destination) noexcept {
  StoreVector(*destination,
              CrossProductFixed(VectorOf(*left), VectorOf(*right)));
}

// VectorNormal, VectorNormalS and VectorNormalSS(input, output): returns the
// squared length as the original's 32-bit result word.
template <VectorRecord Input, VectorRecord Output>
std::int32_t NormalizeLiveVector(const Input* input, Output* output) noexcept {
  const NormalizedVector normalized = NormalizeVector(VectorOf(*input));
  StoreVector(*output, normalized.direction);
  return static_cast<std::int32_t>(normalized.squared_length);
}
}  // namespace VS::Core::Math
