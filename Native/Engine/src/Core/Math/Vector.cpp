// Hand-maintained (tools/port/owned.csv).
#include "Core/Math/Vector.hpp"

#include <cstdint>

#include "Core/Math/Distance.hpp"
#include "Core/Math/ScalarMath.hpp"

namespace VS::Core::Math {
namespace {
constexpr int kFractionBits = 12;
constexpr std::int32_t kFixedOne = 4096;
constexpr std::uint32_t kHalfwordMask = 0xFFFF;
constexpr int kHalfwordBits = 16;

// The original's `v >= 0 ? v : -v`, which leaves INT32_MIN negative.
constexpr std::int32_t Magnitude(std::int32_t value) noexcept {
  return value >= 0 ? value : WrappingNegate(value);
}

constexpr std::int64_t Halfword(std::int32_t value) noexcept {
  return WrapSigned16(static_cast<std::uint32_t>(value));
}

// (first * second - third * fourth) >> 12 of signed halfwords, as one MAC lane
// of the GTE OP instruction with sf = 1.
constexpr std::int32_t CrossLane(std::int64_t first, std::int64_t second,
                                 std::int64_t third,
                                 std::int64_t fourth) noexcept {
  return static_cast<std::int32_t>(((first * second) - (third * fourth)) >>
                                   kFractionBits);
}
}  // namespace

// decomp: BATTLE.PRG func_80096254 (src/BATTLE/BATTLE.PRG/2842C.c:2564).
std::int32_t AddPackedScreenCoordinates(std::int32_t left,
                                        std::int32_t right) noexcept {
  const std::uint32_t low =
      (static_cast<std::uint32_t>(left) + static_cast<std::uint32_t>(right)) &
      kHalfwordMask;
  const std::uint32_t high =
      static_cast<std::uint32_t>((left >> kHalfwordBits) +
                                 (right >> kHalfwordBits))
      << kHalfwordBits;
  return static_cast<std::int32_t>(low | high);
}

// decomp: BATTLE.PRG vectorAdd (src/BATTLE/BATTLE.PRG/4DEA8.c:4755; was
// _vectorAdd), addSVectorToVector (src/BATTLE/BATTLE.PRG/5BF94.c:5477; was
// _addSVectorToVector), addVectors (src/BATTLE/BATTLE.PRG/5BF94.c:6455; was
// vs_battle_addVector) and vs_battle_addVecToSvec
// (src/BATTLE/BATTLE.PRG/5BF94.c:6462).
Vec3 AddVectors(Vec3 left, Vec3 right) noexcept {
  return Vec3{WrappingAdd(left.x, right.x), WrappingAdd(left.y, right.y),
              WrappingAdd(left.z, right.z)};
}

// decomp: BATTLE.PRG vectorSubtract (src/BATTLE/BATTLE.PRG/4DEA8.c:4761; was
// _vectorSubtract).
Vec3 SubtractVectors(Vec3 left, Vec3 right) noexcept {
  return Vec3{WrappingSubtract(left.x, right.x),
              WrappingSubtract(left.y, right.y),
              WrappingSubtract(left.z, right.z)};
}

// decomp: BATTLE.PRG vectorCopy (src/BATTLE/BATTLE.PRG/4DEA8.c:4774; was
// _copyVector), vs_battle_svecToVec (src/BATTLE/BATTLE.PRG/5BF94.c:6469) and
// vs_battle_vecToSvec (src/BATTLE/BATTLE.PRG/5BF94.c:6476).
Vec3 CopyVector(Vec3 source) noexcept { return source; }

// decomp: BATTLE.PRG absMax3 (src/BATTLE/BATTLE.PRG/5BF94.c:5393; was
// _absMax3).
std::int32_t GreatestMagnitudeComponent(Vec3 components) noexcept {
  const std::int32_t first = Magnitude(components.x);
  const std::int32_t second = Magnitude(components.y);
  const std::int32_t third = Magnitude(components.z);
  if (first < second) {
    return second < third ? components.z : components.y;
  }
  return first < third ? components.z : components.x;
}

// decomp: BATTLE.PRG absMax2 (src/BATTLE/BATTLE.PRG/5BF94.c:5414; was
// _absMax2).
std::int32_t LargerMagnitude(std::int32_t first, std::int32_t second) noexcept {
  return Magnitude(first) < Magnitude(second) ? second : first;
}

// decomp: BATTLE.PRG vectorFixedPointMult
// (src/BATTLE/BATTLE.PRG/4DEA8.c:4767; was _vectorFixedPointMult).
Vec3 ScaleFixedVector(Vec3 source, std::int32_t scale) noexcept {
  return Vec3{MultiplyFixed(source.x, scale), MultiplyFixed(source.y, scale),
              MultiplyFixed(source.z, scale)};
}

// decomp: BATTLE.PRG vectorMagnitude (src/BATTLE/BATTLE.PRG/4DEA8.c:4780; was
// _vectorMagnitude).
std::int32_t FixedVectorMagnitude(Vec3 source) noexcept {
  const std::int32_t x = source.x >> kFractionBits;
  const std::int32_t y = source.y >> kFractionBits;
  const std::int32_t z = source.z >> kFractionBits;
  const std::int32_t squared =
      WrappingAdd(WrappingAdd(WrappingMultiply(x, x), WrappingMultiply(y, y)),
                  WrappingMultiply(z, z));
  return WrappingMultiply(SquareRootInteger(squared), kFixedOne);
}

// decomp: BATTLE.PRG sVectorToFixedPointVector
// (src/BATTLE/BATTLE.PRG/4DEA8.c:4804; was _sVectorToFixedPointVector).
Vec3 ShortVectorToFixed(Position3 source) noexcept {
  return Vec3{source.x * kFixedOne, source.y * kFixedOne, source.z * kFixedOne};
}

// decomp: BATTLE.PRG toNormalIntegerVector
// (src/BATTLE/BATTLE.PRG/4DEA8.c:4842; was _toNormalIntegerVector).
Vec3 NormalizeFixedVector(Vec3 source) noexcept {
  return NormalizeVector(Vec3{source.x >> kFractionBits,
                              source.y >> kFractionBits,
                              source.z >> kFractionBits})
      .direction;
}

// decomp: SLUS_010.40 OuterProduct0 (src/SLUS_010.40/32154.s:39). `left` goes
// through the rotation diagonal (R11, R22, R33) and `right` through IR1-IR3;
// MAC1-MAC3 are stored as words without saturation.
Vec3 CrossProductFixed(Vec3 left, Vec3 right) noexcept {
  const std::int64_t left_x = Halfword(left.x);
  const std::int64_t left_y = Halfword(left.y);
  const std::int64_t left_z = Halfword(left.z);
  const std::int64_t right_x = Halfword(right.x);
  const std::int64_t right_y = Halfword(right.y);
  const std::int64_t right_z = Halfword(right.z);
  return Vec3{CrossLane(left_y, right_z, left_z, right_y),
              CrossLane(left_z, right_x, left_x, right_z),
              CrossLane(left_x, right_y, left_y, right_x)};
}
}  // namespace VS::Core::Math
