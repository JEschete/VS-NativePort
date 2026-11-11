#include <array>
#include <cstdint>
#include <limits>
#include <span>
#include <type_traits>

#include <catch2/catch_test_macros.hpp>

#include "Core/Math/Interpolation.hpp"
#include "Core/Math/InterpolationLive.hpp"
#include "Core/Math/Matrix.hpp"
#include "Core/Math/MatrixLive.hpp"
#include "Core/Math/ScalarMath.hpp"
#include "Core/Math/Vector.hpp"
#include "Core/Math/VectorLive.hpp"
#include "Legacy/Psx.h"

// Contracts of the #276 slice A3 (#278) typed math kernels and their record
// bindings: every input is defined, and adapters keep the original shapes.
namespace {
namespace Math = VS::Core::Math;
constexpr std::int32_t kMin = std::numeric_limits<std::int32_t>::min();
constexpr std::int32_t kMax = std::numeric_limits<std::int32_t>::max();

static_assert(Math::R3000Quotient(7, 0) == -1);
static_assert(Math::R3000Quotient(0, 0) == -1);
static_assert(Math::R3000Quotient(-7, 0) == 1);
static_assert(Math::R3000Quotient(kMin, -1) == kMin);
static_assert(Math::R3000Quotient(-7, 2) == -3);
static_assert(Math::R3000Quotient(7, -2) == -3);
static_assert(Math::R3000UnsignedQuotient(7, 0) == 0xFFFFFFFFu);
static_assert(Math::R3000UnsignedQuotient(0, 0) == 0xFFFFFFFFu);
static_assert(Math::R3000UnsignedQuotient(0xFFFFFFFFu, 2) == 0x7FFFFFFFu);
static_assert(Math::R3000UnsignedQuotient(7, 2) == 3);

static_assert(
    std::is_same_v<decltype(&Math::InterpolateLiveSevenBitVector<VECTOR>),
                   void (*)(std::span<const std::int16_t, 6>, std::int32_t,
                            VECTOR*) noexcept>);
static_assert(
    std::is_same_v<decltype(&Math::InterpolateLiveSevenBitPlane),
                   void (*)(std::span<const std::int16_t, 4>, std::int32_t,
                            std::span<std::int32_t, 2>) noexcept>);
static_assert(std::is_same_v<decltype(&Math::InterpolateLiveShortPair),
                             std::int32_t (*)(std::span<const std::int16_t, 2>,
                                              std::int32_t) noexcept>);

static_assert(Math::EaseCurveOf(0) == Math::EaseCurve::kLinear);
static_assert(Math::EaseCurveOf(1) == Math::EaseCurve::kEaseOut);
static_assert(Math::EaseCurveOf(2) == Math::EaseCurve::kEaseInOut);
static_assert(Math::EaseCurveOf(3) == Math::EaseCurve::kEaseIn);
static_assert(Math::EaseCurveOf(-1) == Math::EaseCurve::kEaseOut);
static_assert(Math::EaseCurveOf(std::numeric_limits<std::int16_t>::min()) ==
              Math::EaseCurve::kEaseOut);
static_assert(Math::EaseCurveOf(4) == Math::EaseCurve::kEaseIn);
static_assert(Math::EaseCurveOf(std::numeric_limits<std::int16_t>::max()) ==
              Math::EaseCurve::kEaseIn);
}  // namespace

TEST_CASE("Ease curves run from zero to one at every mode",
          "[core-math][ease]") {
  for (const auto curve :
       {Math::EaseCurve::kLinear, Math::EaseCurve::kEaseOut,
        Math::EaseCurve::kEaseInOut, Math::EaseCurve::kEaseIn}) {
    CAPTURE(static_cast<int>(curve));
    CHECK(Math::EvaluateEase(curve, {0, 16}) == 0);
    CHECK(Math::EvaluateEase(curve, {16, 16}) == 4096);
  }
  CHECK(Math::EvaluateEase(Math::EaseCurve::kLinear, {4, 16}) == 1024);
  CHECK(Math::EvaluateEase(Math::EaseCurve::kEaseOut, {8, 16}) ==
        Math::Sine(512));
  CHECK(Math::EvaluateLiveEase(9, 16, 16) == 4096);
  CHECK(Math::EvaluateLiveEase(-9, 8, 16) == Math::Sine(512));
}

TEST_CASE("A zero ease duration takes the R3000 quotient instead of trapping",
          "[core-math][ease]") {
  // DIV by zero leaves -1 in LO for a non-negative dividend.
  CHECK(Math::EvaluateEase(Math::EaseCurve::kLinear, {0, 0}) == -1);
  CHECK(Math::EvaluateEase(Math::EaseCurve::kEaseOut, {0, 0}) ==
        Math::Sine(-1));
  CHECK(Math::EvaluateEase(Math::EaseCurve::kLinear, {-3, 0}) == 1);
}

TEST_CASE("Magnitude selection ranks INT32_MIN below every other value",
          "[core-math][vector]") {
  CHECK(Math::GreatestMagnitudeComponent({kMin, 1, 0}) == 1);
  CHECK(Math::GreatestMagnitudeComponent({kMin, 0, 0}) == 0);
  CHECK(Math::GreatestMagnitudeComponent({kMin, kMin, kMin}) == kMin);
  CHECK(Math::GreatestMagnitudeComponent({kMax, kMin, -kMax}) == kMax);
  CHECK(Math::LargerMagnitude(kMin, 0) == 0);
  CHECK(Math::LargerMagnitude(-kMax, kMax) == -kMax);
  CHECK(Math::SelectLiveGreatestMagnitude(-5, 5, -5) == -5);
}

TEST_CASE("Vector arithmetic wraps like the original registers",
          "[core-math][vector]") {
  CHECK(Math::AddVectors({kMax, kMin, 1}, {1, -1, -1}) ==
        Math::Vec3{kMin, kMax, 0});
  CHECK(Math::SubtractVectors({kMin, kMax, 0}, {1, -1, 0}) ==
        Math::Vec3{kMax, kMin, 0});
  CHECK(Math::CopyVector({1, -2, 3}) == Math::Vec3{1, -2, 3});
}

TEST_CASE("Record bindings read every input before writing an alias",
          "[core-math][vector][aliasing]") {
  VECTOR value{3, -4, 5, 91};
  CHECK(Math::AddLiveVectors(&value, &value, &value) == &value);
  CHECK(value.vx == 6);
  CHECK(value.vy == -8);
  CHECK(value.vz == 10);
  CHECK(value.pad == 91);
  CHECK(Math::SubtractLiveVectors(&value, &value, &value) == &value);
  CHECK(value.vx == 0);
  CHECK(value.pad == 91);
}

TEST_CASE("Record bindings keep destination widths and padding",
          "[core-math][vector][aliasing]") {
  SVECTOR narrow{-1, 2, -3, 17};
  VECTOR wide{0, 0, 0, 23};
  Math::CopyLiveVectorTo(&narrow, &wide);
  CHECK(wide.vx == -1);
  CHECK(wide.vz == -3);
  CHECK(wide.pad == 23);
  VECTOR left{0x10000 + 4096, 0, 0, 1};
  const VECTOR right{0, 4096, 0, 2};
  Math::StoreLiveCrossProduct(&left, &right, &left);
  CHECK(left.vx == 0);
  CHECK(left.vy == 0);
  CHECK(left.vz == 4096);
  CHECK(left.pad == 1);
}

TEST_CASE("The cross product reads each component's low halfword",
          "[core-math][vector]") {
  CHECK(Math::CrossProductFixed({4096, 0, 0}, {0, 4096, 0}) ==
        Math::Vec3{0, 0, 4096});
  CHECK(Math::CrossProductFixed({0x12340000 + 4096, 0, 0},
                                {0, 0x7FFF0000 + 4096, 0}) ==
        Math::Vec3{0, 0, 4096});
}

TEST_CASE("Scalar kernels give defined results for every edge input",
          "[core-math][scalar]") {
  CHECK(Math::Sine(kMin) == 0);
  CHECK(Math::Cosine(kMin) == 4096);
  CHECK(Math::Sine(-1024) == -4096);
  CHECK(Math::Sine(kMax) == -Math::Sine(1));
  CHECK(Math::SquareRootFixed(-5) == 0);
  CHECK(Math::SquareRootFixed(kMin) == 0);
  CHECK(Math::SquareRootInteger(kMin) == 0);
}

TEST_CASE("Arctangent and decimal split follow the resident tables",
          "[core-math][scalar]") {
  CHECK(Math::Arctangent2(0, 0) == 0);
  CHECK(Math::Arctangent2(4096, 0) == 1024);
  // D_80040A14[512] on the disc is the byte cap 255, not 256.
  CHECK(Math::Arctangent2(4096, 4096) == 510);
  CHECK(Math::Arctangent2(-4096, -4096) == -1538);
  const auto extreme = Math::Arctangent2(kMin, kMin);
  CHECK(extreme >= -2048);
  CHECK(extreme <= 2048);
}

TEST_CASE("Decimal split packs both digits", "[core-math][scalar]") {
  CHECK(Math::SplitDecimalDigit(0) == 0);
  CHECK(Math::SplitDecimalDigit(47) == 0x47);
  CHECK(Math::SplitDecimalDigit(99) == 0x99);
}

TEST_CASE("Matrix kernels keep identity and translation contracts",
          "[core-math][matrix]") {
  Math::Mat3 identity;
  for (int axis = 0; axis < 3; ++axis) identity.m[axis][axis] = 4096;
  CHECK(Math::IdentityRotation() == identity);
  CHECK(Math::BuildRotationXyz({0, 0, 0}) == identity);
  CHECK(Math::BuildEulerRotationMatrix({0, 0, 0}) == identity);
  CHECK(Math::BuildRotationXyz({4096, -4096, 8192}) == identity);
  const auto transform = Math::TranslationTransform({-3, 17, 29});
  CHECK(transform.rotation == identity);
  CHECK(transform.translation == Math::Vec3{-3, 17, 29});
}

TEST_CASE("Matrix record bindings keep the record translation",
          "[core-math][matrix]") {
  Math::Mat3 identity;
  for (int axis = 0; axis < 3; ++axis) identity.m[axis][axis] = 4096;
  MATRIX record{};
  record.t[0] = 5;
  const SVECTOR rotation{0, 0, 0, 0};
  CHECK(Math::BuildLiveRotationXyz(&rotation, &record) == &record);
  CHECK(Math::MatrixOf(record) == identity);
  CHECK(record.t[0] == 5);
}

TEST_CASE("Seven-bit plane and vector spans read exactly their pairs",
          "[core-math][interpolation]") {
  constexpr std::array<std::int16_t, 4> kPlane{0, 128, -128, 0};
  std::array<std::int32_t, 2> plane{};
  Math::InterpolateLiveSevenBitPlane(kPlane, 64, plane);
  CHECK(plane == std::array<std::int32_t, 2>{64, -64});
  constexpr std::array<std::int16_t, 6> kVector{0, 128, -128, 0, 10, 10};
  SVECTOR vector{0, 0, 0, 41};
  Math::InterpolateLiveSevenBitVector(kVector, 64, &vector);
  CHECK(vector.vx == 64);
  CHECK(vector.vy == -64);
  CHECK(vector.vz == 10);
  CHECK(vector.pad == 41);
}

TEST_CASE("Timed interpolation with no duration stays defined",
          "[core-math][interpolation]") {
  // A zero total takes the R3000 quotient, so the weight is -1 for a
  // non-negative step and 1 for a negative one.
  CHECK(Math::InterpolateTimedScalar({0, 8192}, {5, 0}) ==
        Math::InterpolateThirteenBitScalar({0, 8192}, -1));
  CHECK(Math::InterpolateTimedScalar({0, 8192}, {-5, 0}) ==
        Math::InterpolateThirteenBitScalar({0, 8192}, 1));
}
