// Hand-maintained (tools/port/owned.csv).
#include "Core/Math/ScalarMath.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numbers>
#include <optional>

#include "Core/Math/Distance.hpp"

namespace VS::Core::Math {
namespace {
constexpr int kDimensions = 3;
constexpr int kFixedFractionBits = 12;
constexpr std::int32_t kIntermediateScale = 4;
constexpr unsigned kWordBits = 32;
constexpr unsigned kNormalizedIndexShift = 24;
constexpr unsigned kRootLookupFractionBits = 18;
constexpr unsigned kAngleMask = 0xFFF;
constexpr double kRadiansPerAngleUnit = 2.0 * std::numbers::pi / 4096.0;
constexpr double kFixedOne = 4096.0;

// Normalized reciprocal-root inputs are 64..255.
constexpr std::uint32_t kFirstRootIndex = 64;
constexpr std::uint32_t kRootIndexCount = 192;

constexpr std::uint32_t IntegerSquareRoot(std::uint32_t value) noexcept {
  std::uint32_t result = 0;
  std::uint32_t bit = std::uint32_t{1} << (kWordBits - 2);
  while (bit > value) {
    bit >>= 2;
  }
  while (bit != 0) {
    if (value >= result + bit) {
      value -= result + bit;
      result = (result >> 1) + bit;
    } else {
      result >>= 1;
    }
    bit >>= 2;
  }
  return result;
}

// D_80040D98 (SLUS_010.40 0x80040D98): floor(32768 / sqrt(index)) for
// index 64..255, equivalently the largest v with v * v * index <= 2^30. Built
// here rather than read from the loaded executable.
constexpr std::uint32_t kReciprocalRootNumerator = std::uint32_t{1} << 30;
constexpr std::array<std::int16_t, kRootIndexCount> kReciprocalRoots = [] {
  std::array<std::int16_t, kRootIndexCount> table{};
  for (std::uint32_t index = 0; index < kRootIndexCount; ++index) {
    table[index] = static_cast<std::int16_t>(IntegerSquareRoot(
        kReciprocalRootNumerator / (index + kFirstRootIndex)));
  }
  return table;
}();
// First and last D_80040D98 entries on the disc.
constexpr std::int16_t kDiscFirstRoot = 4096;
constexpr std::int16_t kDiscLastRoot = 2052;
static_assert(kReciprocalRoots.front() == kDiscFirstRoot);
static_assert(kReciprocalRoots.back() == kDiscLastRoot);

// Shared by the reciprocal-root entries: positive (or unsigned) value in,
// table mantissa and the exponent restoring its scale out.
ReciprocalRoot ReciprocalRootOf(std::uint32_t value) noexcept {
  const unsigned even_leading =
      static_cast<unsigned>(std::countl_zero(value)) & ~1u;
  const std::uint32_t normalized =
      even_leading >= kNormalizedIndexShift
          ? value << (even_leading - kNormalizedIndexShift)
          : value >> (kNormalizedIndexShift - even_leading);
  return ReciprocalRoot{
      kReciprocalRoots[normalized - kFirstRootIndex],
      static_cast<std::int32_t>((kWordBits - 1 - even_leading) >> 1)};
}

// The 8-bit-normalized input shared by the square-root tables: value shifted
// so its top even-aligned set bit pair lands at bits 6..7, and the even
// leading-zero count that restores its scale. Requires a positive value.
struct NormalizedRoot {
  std::uint32_t sample{};
  int leading{};
};

NormalizedRoot NormalizedRootSample(std::uint32_t value) noexcept {
  const unsigned leading =
      static_cast<unsigned>(std::countl_zero(value)) & (kWordBits - 2);
  const std::uint32_t normalized =
      leading >= kNormalizedIndexShift
          ? value << (leading - kNormalizedIndexShift)
          : value >> (kNormalizedIndexShift - leading);
  // D_80040C18 (SLUS_010.40 0x80040C18) holds floor(sqrt(index << 18)) for
  // indices 64..255.
  return NormalizedRoot{
      IntegerSquareRoot(normalized << kRootLookupFractionBits),
      static_cast<int>(leading)};
}

// D_80040A14 (SLUS_010.40 0x80040A14): atan(i / 512) for i = 0..512 in
// 2048-per-turn units, capped at 255. The disc rounding is reproduced exactly
// by sampling at i + 0.7 and adding 0.245 before the floor; every entry sits
// at least 0.0009 from a rounding boundary, so the build is deterministic.
constexpr std::size_t kArctangentSteps = 512;
constexpr unsigned kArctangentIndexBits = 9;
constexpr double kArctangentSampleOffset = 0.7;
constexpr double kArctangentRoundingBias = 0.245;
constexpr double kArctangentOctantLimit = 255.0;
constexpr double kArctangentUnitsPerRadian = 1024.0 / std::numbers::pi;
using ArctangentTable = std::array<std::uint8_t, kArctangentSteps + 1>;

const ArctangentTable& ArctangentOctant() noexcept {
  static const ArctangentTable table = [] {
    ArctangentTable entries{};
    for (std::size_t index = 0; index < entries.size(); ++index) {
      const double ratio =
          (static_cast<double>(index) + kArctangentSampleOffset) /
          static_cast<double>(kArctangentSteps);
      const double angle = std::atan(ratio) * kArctangentUnitsPerRadian +
                           kArctangentRoundingBias;
      entries[index] = static_cast<std::uint8_t>(
          std::min(std::floor(angle), kArctangentOctantLimit));
    }
    return entries;
  }();
  return table;
}

// Quarter and half turns in the table's 2048-per-turn units.
constexpr std::int32_t kArctangentQuarterTurn = 512;
constexpr std::int32_t kArctangentHalfTurn = 1024;
// _atan2FixedPoint drops six fraction bits from each Q12 component.
constexpr unsigned kFixedAtan2InputShift = 6;
// SquareRoot12's result shift is (19 - leading) / 2, rounded down.
constexpr int kSquareRootFixedScale = 19;

std::int16_t SaturateToHalfword(std::int64_t value) noexcept {
  return static_cast<std::int16_t>(
      std::clamp<std::int64_t>(value, std::numeric_limits<std::int16_t>::min(),
                               std::numeric_limits<std::int16_t>::max()));
}

// Q12 matrix product; `narrow` reduces each shifted 64-bit sum to a halfword.
template <typename Narrow>
Mat3 MultiplyQ12(const Mat3& left, const Mat3& right, Narrow narrow) noexcept {
  Mat3 product;
  for (int row = 0; row < kDimensions; ++row) {
    for (int column = 0; column < kDimensions; ++column) {
      std::int64_t sum = 0;
      for (int component = 0; component < kDimensions; ++component) {
        sum +=
            std::int64_t{left.m[row][component]} * right.m[component][column];
      }
      product.m[row][column] = narrow(sum >> kFixedFractionBits);
    }
  }
  return product;
}

double AngleRadians(std::int32_t angle) noexcept {
  return static_cast<double>(static_cast<std::uint32_t>(angle) & kAngleMask) *
         kRadiansPerAngleUnit;
}
}  // namespace

// decomp: SLUS_010.40 rsin (src/SLUS_010.40/31724.s:13); BATTLE.PRG
// func_80098AB8 (src/BATTLE/BATTLE.PRG/2EA3C.s) and ENDING.PRG func_800688D4
// (src/ENDING/ENDING.PRG/D4.c:187) are copies. Rounding sin to Q12 reproduces
// every _trig_table (0x80040210) entry the octant folding reads.
std::int32_t Sine(std::int32_t angle) noexcept {
  return static_cast<std::int32_t>(
      std::lround(std::sin(AngleRadians(angle)) * kFixedOne));
}

// decomp: SLUS_010.40 rcos (src/SLUS_010.40/31724.s:9) and BATTLE.PRG
// func_80098AB4 (src/BATTLE/BATTLE.PRG/2EA3C.s): rsin of angle + 1024.
std::int32_t Cosine(std::int32_t angle) noexcept {
  return static_cast<std::int32_t>(
      std::lround(std::cos(AngleRadians(angle)) * kFixedOne));
}

// decomp: SLUS_010.40 ratan2 (src/SLUS_010.40/31B7C.c:6).
// Magnitudes and the shifted ratio are 64-bit: the original's 32-bit
// `ax << 9` wrapped for |x| or |y| of 2^22 and above and then indexed outside
// D_80040A14, and -INT32_MIN stayed negative. Those inputs now read the entry
// for their true ratio.
// Both branches index by smaller / larger magnitude, so the zero-zero input
// is the only one with no ratio.
std::int32_t Arctangent2(std::int32_t y, std::int32_t x) noexcept {
  const std::int64_t abs_x = x < 0 ? -std::int64_t{x} : std::int64_t{x};
  const std::int64_t abs_y = y < 0 ? -std::int64_t{y} : std::int64_t{y};
  const std::int64_t larger = std::max(abs_x, abs_y);
  if (larger == 0) {
    return 0;
  }
  const std::int64_t smaller = std::min(abs_x, abs_y);
  const std::int32_t entry = ArctangentOctant()[static_cast<std::size_t>(
      (smaller << kArctangentIndexBits) / larger)];
  std::int32_t angle = abs_x < abs_y ? kArctangentQuarterTurn - entry : entry;
  if (x < 0) {
    angle = kArctangentHalfTurn - angle;
  }
  angle *= 2;
  return y < 0 ? -angle : angle;
}

// decomp: BATTLE.PRG _atan2FixedPoint (src/BATTLE/BATTLE.PRG/4A0A8.c:4177).
std::int32_t FixedAtan2(std::int32_t y, std::int32_t x) noexcept {
  return Arctangent2(y >> kFixedAtan2InputShift, x >> kFixedAtan2InputShift);
}

// decomp: SLUS_010.40 SquareRoot12 (src/SLUS_010.40/32154.s:240).
// Zero and -1 returned 0; other negative inputs indexed outside D_80040C18
// and have no square root, so every non-positive input gives 0 here.
std::int32_t SquareRootFixed(std::int32_t value) noexcept {
  if (value <= 0) {
    return 0;
  }
  const NormalizedRoot root =
      NormalizedRootSample(static_cast<std::uint32_t>(value));
  const int shift = (kSquareRootFixedScale - root.leading) >> 1;
  return static_cast<std::int32_t>(shift > 0 ? root.sample << shift
                                             : root.sample >> -shift);
}

// decomp: BATTLE.PRG vs_battle_clamp (src/BATTLE/BATTLE.PRG/2842C.c:390).
std::int16_t ClampSignedShort(std::int16_t value, std::int32_t first_bound,
                              std::int32_t second_bound) noexcept {
  const std::int16_t first =
      WrapSigned16(static_cast<std::uint32_t>(first_bound));
  const std::int16_t second =
      WrapSigned16(static_cast<std::uint32_t>(second_bound));
  return std::clamp(value, std::min(first, second), std::max(first, second));
}

// decomp: BATTLE.PRG vs_battle_toBCD (src/BATTLE/BATTLE.PRG/5BF94.c:4494).
std::int32_t SplitDecimalDigit(std::int32_t value) noexcept {
  constexpr std::int32_t kDecimalBase = 10;
  constexpr std::int32_t kNibbleScale = 16;
  return (value % kDecimalBase) |
         WrappingMultiply(value / kDecimalBase, kNibbleScale);
}

// decomp: BATTLE.PRG _fixedPointMult (src/BATTLE/BATTLE.PRG/4A0A8.c:4163).
std::int32_t MultiplyFixed(std::int32_t left, std::int32_t right) noexcept {
  return static_cast<std::int32_t>(static_cast<std::uint32_t>(
      static_cast<std::uint64_t>(std::int64_t{left} * right) >>
      kFixedFractionBits));
}

// decomp: SLUS_010.40 Square0 (src/SLUS_010.40/32154.s:24).
Vec3 SquareVectorComponents(Vec3 input) noexcept {
  const std::int32_t x = WrapSigned16(static_cast<std::uint32_t>(input.x));
  const std::int32_t y = WrapSigned16(static_cast<std::uint32_t>(input.y));
  const std::int32_t z = WrapSigned16(static_cast<std::uint32_t>(input.z));
  return Vec3{x * x, y * y, z * z};
}

// decomp: SLUS_010.40 func_80041C68 (src/SLUS_010.40/32154.s:280).
Mat3 MultiplyMatrixIntoRight(const Mat3& left, const Mat3& right) noexcept {
  return MultiplyQ12(left, right, SaturateToHalfword);
}

// decomp: SLUS_010.40 MulMatrix (src/SLUS_010.40/32154.s:275).
Mat3 MultiplyMatrixIntoLeft(const Mat3& left, const Mat3& right) noexcept {
  return MultiplyMatrixIntoRight(left, right);
}

// decomp: BATTLE.PRG func_800989B8 (src/BATTLE/BATTLE.PRG/2EA3C.s:1664).
Mat3 MultiplyRotationMatrixInPlace(const Mat3& left,
                                   const Mat3& right) noexcept {
  return MultiplyMatrixIntoRight(left, right);
}

// decomp: BATTLE.PRG func_800B196C (src/BATTLE/BATTLE.PRG/48C7C.s:365).
Mat3 PremultiplyRotationMatrix(const Mat3& left, const Mat3& right) noexcept {
  return MultiplyMatrixIntoRight(left, right);
}

// decomp: SLUS_010.40 vs_gte_rsqrt (src/SLUS_010.40/32154.s:207).
std::int32_t SquareRootInteger(std::int32_t value) noexcept {
  if (value <= 0) {
    return 0;
  }
  const NormalizedRoot root =
      NormalizedRootSample(static_cast<std::uint32_t>(value));
  const int shift = (static_cast<int>(kWordBits) - 1 - root.leading) >> 1;
  return static_cast<std::int32_t>((root.sample << shift) >>
                                   kFixedFractionBits);
}

// decomp: BATTLE.PRG func_800988BC (src/BATTLE/BATTLE.PRG/2EA3C.s:1597).
// func_800B16F4 (src/BATTLE/BATTLE.PRG/48C7C.s:188) and SLUS_010.40
// func_80041104 (src/SLUS_010.40/31724.s:147) have identical octant blocks.
SineCosine LookupRotationSineCosine(std::int32_t angle) noexcept {
  return SineCosine{Sine(angle), Cosine(angle)};
}

// decomp: BATTLE.PRG func_8009874C (src/BATTLE/BATTLE.PRG/2EA3C.s:1499) and
// SLUS_010.40 RotMatrixYXZ_gte (src/SLUS_010.40/31724.s:47), which computes
// the same elements from the same products.
// Keep the GPF stages' two extra fractional bits until each final sum.
Mat3 BuildEulerRotationMatrix(Angles3 rotation) noexcept {
  const SineCosine x = LookupRotationSineCosine(rotation.x);
  const SineCosine y = LookupRotationSineCosine(rotation.y);
  const SineCosine z = LookupRotationSineCosine(rotation.z);
  const std::int32_t sin_z_sin_y =
      MultiplyFixed(z.sine * kIntermediateScale, y.sine);
  const std::int32_t sin_z_cos_y =
      MultiplyFixed(z.sine * kIntermediateScale, y.cosine);
  const std::int32_t cos_z_sin_y =
      MultiplyFixed(z.cosine * kIntermediateScale, y.sine);
  const std::int32_t cos_z_cos_y =
      MultiplyFixed(z.cosine * kIntermediateScale, y.cosine);
  Mat3 output;
  output.m[0][0] = static_cast<std::int16_t>(
      (MultiplyFixed(x.sine, sin_z_sin_y) + cos_z_cos_y) >> 2);
  output.m[0][1] = static_cast<std::int16_t>(
      (MultiplyFixed(x.sine, cos_z_sin_y) - sin_z_cos_y) >> 2);
  output.m[0][2] = static_cast<std::int16_t>(MultiplyFixed(x.cosine, y.sine));
  output.m[1][0] = static_cast<std::int16_t>(MultiplyFixed(z.sine, x.cosine));
  output.m[1][1] = static_cast<std::int16_t>(MultiplyFixed(z.cosine, x.cosine));
  output.m[1][2] = static_cast<std::int16_t>(-x.sine);
  output.m[2][0] = static_cast<std::int16_t>(
      (MultiplyFixed(x.sine, sin_z_cos_y) - cos_z_sin_y) >> 2);
  output.m[2][1] = static_cast<std::int16_t>(
      (MultiplyFixed(x.sine, cos_z_cos_y) + sin_z_sin_y) >> 2);
  output.m[2][2] = static_cast<std::int16_t>(MultiplyFixed(x.cosine, y.cosine));
  return output;
}

// decomp: BATTLE.PRG func_800B17F0 (src/BATTLE/BATTLE.PRG/48C7C.s:254).
Mat3 BuildEulerRotationMatrixQuadrant(Angles3 rotation) noexcept {
  const std::int32_t sin_x = Sine(rotation.x);
  const std::int32_t cos_x = Cosine(rotation.x);
  const std::int32_t sin_y = Sine(rotation.y);
  const std::int32_t cos_y = Cosine(rotation.y);
  const std::int32_t sin_z = Sine(rotation.z);
  const std::int32_t cos_z = Cosine(rotation.z);
  const std::int32_t sin_z_sin_x =
      (sin_z * sin_x * kIntermediateScale) >> kFixedFractionBits;
  const std::int32_t sin_z_cos_x =
      (sin_z * cos_x * kIntermediateScale) >> kFixedFractionBits;
  const std::int32_t cos_z_sin_x =
      (cos_z * sin_x * kIntermediateScale) >> kFixedFractionBits;
  const std::int32_t cos_z_cos_x =
      (cos_z * cos_x * kIntermediateScale) >> kFixedFractionBits;
  Mat3 output;
  output.m[0][0] =
      static_cast<std::int16_t>((cos_z * cos_y) >> kFixedFractionBits);
  output.m[0][1] = static_cast<std::int16_t>(
      (((sin_y * cos_z_sin_x) >> kFixedFractionBits) - sin_z_cos_x) >> 2);
  output.m[0][2] = static_cast<std::int16_t>(
      (((sin_y * cos_z_cos_x) >> kFixedFractionBits) + sin_z_sin_x) >> 2);
  output.m[1][0] =
      static_cast<std::int16_t>((sin_z * cos_y) >> kFixedFractionBits);
  output.m[1][1] = static_cast<std::int16_t>(
      (((sin_y * sin_z_sin_x) >> kFixedFractionBits) + cos_z_cos_x) >> 2);
  output.m[1][2] = static_cast<std::int16_t>(
      (((sin_y * sin_z_cos_x) >> kFixedFractionBits) - cos_z_sin_x) >> 2);
  output.m[2][0] = static_cast<std::int16_t>(-sin_y);
  output.m[2][1] =
      static_cast<std::int16_t>((cos_y * sin_x) >> kFixedFractionBits);
  output.m[2][2] =
      static_cast<std::int16_t>((cos_y * cos_x) >> kFixedFractionBits);
  return output;
}

// decomp: SLUS_010.40 ScaleMatrix (src/SLUS_010.40/31C0C.s); BATTLE.PRG
// func_800B396C (src/BATTLE/BATTLE.PRG/4B16C.s:8) is identical.
Mat3 ScaleMatrixColumns(const Mat3& matrix, Vec3 scale) noexcept {
  const std::array<std::int16_t, kDimensions> factors{
      static_cast<std::int16_t>(scale.x), static_cast<std::int16_t>(scale.y),
      static_cast<std::int16_t>(scale.z)};
  Mat3 output;
  for (int row = 0; row < kDimensions; ++row) {
    for (int column = 0; column < kDimensions; ++column) {
      output.m[row][column] = SaturateToHalfword(
          (std::int32_t{matrix.m[row][column]} * factors[column]) >>
          kFixedFractionBits);
    }
  }
  return output;
}

// decomp: InvSquareRoot (src/SLUS_010.40/32154.s:79-105).
// Zero and -1 branched to the original return without writing either output.
// Other negative inputs indexed outside D_80040D98 into neighbouring resident
// tables; they have no reciprocal root and give no result here.
std::optional<ReciprocalRoot> InverseSquareRootParts(
    std::int32_t value) noexcept {
  if (value <= 0) {
    return std::nullopt;
  }
  return ReciprocalRootOf(static_cast<std::uint32_t>(value));
}

// decomp: SLUS_010.40 func_80041AF4 (src/SLUS_010.40/32154.s:162), the body
// of VectorNormal (32154.s:125), VectorNormalS (32154.s:112) and
// VectorNormalSS (32154.s:141).
// The squared length is read as unsigned. The original counted signed leading
// bits, so lengths of 2^31 and above indexed outside D_80040D98; they now use
// the matching table entry.
NormalizedVector NormalizeVector(Vec3 input) noexcept {
  const std::int32_t x = WrapSigned16(static_cast<std::uint32_t>(input.x));
  const std::int32_t y = WrapSigned16(static_cast<std::uint32_t>(input.y));
  const std::int32_t z = WrapSigned16(static_cast<std::uint32_t>(input.z));
  const std::uint32_t squared = SquaredVectorLength(x, y, z);
  if (squared == 0) {
    return NormalizedVector{};
  }
  const ReciprocalRoot root = ReciprocalRootOf(squared);
  return NormalizedVector{Vec3{(x * root.mantissa) >> root.exponent,
                               (y * root.mantissa) >> root.exponent,
                               (z * root.mantissa) >> root.exponent},
                          squared};
}
}  // namespace VS::Core::Math
