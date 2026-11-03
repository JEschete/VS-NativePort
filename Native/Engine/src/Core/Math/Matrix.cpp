// Hand-maintained (tools/port/owned.csv).
#include "Core/Math/Matrix.hpp"

#include <cstdint>

#include "Core/Math/Distance.hpp"
#include "Core/Math/ScalarMath.hpp"
#include "Core/Math/Vector.hpp"

namespace VS::Core::Math {
namespace {
constexpr int kFractionBits = 12;
constexpr int kHalvedFractionBits = 13;
constexpr int kIntermediateBits = 2;
constexpr std::int16_t kFixedOne = 4096;

// The GTE GPF product (IR0 * IRn) >> 12 for the in-range sine products the
// rotation builder feeds it.
constexpr std::int32_t Product(std::int32_t left, std::int32_t right) noexcept {
  return (left * right) >> kFractionBits;
}

constexpr std::int16_t Element(std::int32_t value) noexcept {
  return WrapSigned16(static_cast<std::uint32_t>(value));
}
}  // namespace

// decomp: BATTLE.PRG vs_battle_lookAt (src/BATTLE/BATTLE.PRG/146C.c:8209).
Mat3 BuildLookAtOrientation(Vec3 from, Vec3 to) noexcept {
  const Vec3 world_up{0, -kFixedOne, 0};
  const Vec3 forward = NormalizeVector(SubtractVectors(to, from)).direction;
  const Vec3 right =
      NormalizeVector(CrossProductFixed(forward, world_up)).direction;
  const Vec3 up = NormalizeVector(CrossProductFixed(right, forward)).direction;
  Mat3 matrix;
  matrix.m[0] = {Element(right.x), Element(up.x), Element(forward.x)};
  matrix.m[1] = {Element(right.y), Element(up.y), Element(forward.y)};
  matrix.m[2] = {Element(right.z), Element(up.z), Element(forward.z)};
  return matrix;
}

// decomp: BATTLE.PRG _vecToRotMatrix (src/BATTLE/BATTLE.PRG/4A0A8.c:4179).
// The angles were halfword SVECTOR members; Arctangent2 stays within them.
Mat3 DirectionRotationMatrix(Vec3 direction) noexcept {
  const std::int32_t x = Element(direction.x >> kFractionBits);
  const std::int32_t y = Element(direction.y >> kFractionBits);
  const std::int32_t z = Element(direction.z >> kFractionBits);
  const std::int32_t horizontal = SquareRootInteger(WrappingAdd(x * x, z * z));
  return BuildRotationXyz(
      Angles3{Arctangent2(y, horizontal), Arctangent2(x, z), 0});
}

// decomp: SLUS_010.40 RotMatrix_gte (src/SLUS_010.40/31724.s:214). The sines
// and cosines of x and z enter the GPF stages doubled, so the cross products
// keep two extra fraction bits until each element's final shift.
Mat3 BuildRotationXyz(Angles3 rotation) noexcept {
  const std::int32_t sin_x2 = 2 * Sine(rotation.x);
  const std::int32_t cos_x2 = 2 * Cosine(rotation.x);
  const std::int32_t sin_y = Sine(rotation.y);
  const std::int32_t cos_y = Cosine(rotation.y);
  const std::int32_t sin_z2 = 2 * Sine(rotation.z);
  const std::int32_t cos_z2 = 2 * Cosine(rotation.z);
  const std::int32_t sin_z_sin_x = Product(sin_z2, sin_x2);
  const std::int32_t sin_z_cos_x = Product(sin_z2, cos_x2);
  const std::int32_t cos_z_sin_x = Product(cos_z2, sin_x2);
  const std::int32_t cos_z_cos_x = Product(cos_z2, cos_x2);
  Mat3 matrix;
  matrix.m[0] = {Element(Product(cos_z2, cos_y) >> 1),
                 Element(-(Product(sin_z2, cos_y) >> 1)), Element(sin_y)};
  matrix.m[1] = {
      Element((Product(sin_y, cos_z_sin_x) + sin_z_cos_x) >> kIntermediateBits),
      Element((cos_z_cos_x - Product(sin_y, sin_z_sin_x)) >> kIntermediateBits),
      Element(-((cos_y * sin_x2) >> kHalvedFractionBits))};
  matrix.m[2] = {
      Element((sin_z_sin_x - Product(sin_y, cos_z_cos_x)) >> kIntermediateBits),
      Element((Product(sin_y, sin_z_cos_x) + cos_z_sin_x) >> kIntermediateBits),
      Element((cos_y * cos_x2) >> kHalvedFractionBits)};
  return matrix;
}

// decomp: BATTLE.PRG func_800CFE7C (src/BATTLE/BATTLE.PRG/5BF94.c:5922).
Mat3 IdentityRotation() noexcept {
  Mat3 matrix;
  matrix.m[0][0] = kFixedOne;
  matrix.m[1][1] = kFixedOne;
  matrix.m[2][2] = kFixedOne;
  return matrix;
}

// decomp: BATTLE.PRG func_800CFE98 (src/BATTLE/BATTLE.PRG/5BF94.c:5932).
Transform3 TranslationTransform(Position3 translation) noexcept {
  return Transform3{IdentityRotation(),
                    Vec3{translation.x, translation.y, translation.z}};
}
}  // namespace VS::Core::Math
