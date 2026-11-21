#include <array>
#include <cstdint>
#include <limits>
#include <span>
#include <type_traits>

#include <catch2/catch_test_macros.hpp>

#include "Core/Gte.hpp"
#include "Core/Math/Interpolation.hpp"
#include "Core/Math/InterpolationLive.hpp"
#include "Core/Math/Matrix.hpp"
#include "Core/Math/MatrixLive.hpp"
#include "Core/Math/Projection.hpp"
#include "Core/Math/ScalarMath.hpp"
#include "Core/Math/Vector.hpp"
#include "Core/Math/VectorLive.hpp"
#include "Legacy/Psx.hpp"

using VS::Core::Gte::MATRIX;
using VS::Core::Gte::RotTransPers;
using VS::Core::Gte::RotTransPers4;
using VS::Core::Gte::SetGeomOffset;
using VS::Core::Gte::SetGeomScreen;
using VS::Core::Gte::SetRotMatrix;
using VS::Core::Gte::SetTransMatrix;
using VS::Core::Gte::SVECTOR;
using VS::Core::Gte::VECTOR;

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
static_assert(Math::R3000Remainder(7, 0) == 7);
static_assert(Math::R3000Remainder(-7, 0) == -7);
static_assert(Math::R3000Remainder(kMin, -1) == 0);
static_assert(Math::R3000Remainder(-7, 2) == -1);
static_assert(Math::R3000Remainder(7, -2) == 1);
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

namespace {
struct ProjectionFixture {
  ProjectionFixture() {
    VS::Core::Gte::Reset();
    SetGeomOffset(160, 120);
    SetGeomScreen(256);
  }
  ProjectionFixture(const ProjectionFixture&) = delete;
  ProjectionFixture& operator=(const ProjectionFixture&) = delete;
  ~ProjectionFixture() { VS::Core::Gte::Reset(); }
};

struct ProjectionWitness {
  std::int16_t x;
  std::int16_t z;
  std::int16_t screen_x;
};

// Issue #371 vectors with SDK RotTransPers results (identity transform, H=256,
// origin 160,120): ideal rounded division moved the second to fifth and the
// ninth by one pixel; the rest were already equal.
constexpr std::array<ProjectionWitness, 10> kProjectionWitnesses{{
    {64, 1024, 176},
    {-47, 188, 96},
    {-87, 232, 64},
    {-77, 308, 95},
    {-79, 316, 95},
    {128, 1024, 192},
    {132, 1024, 193},
    {0, 1024, 160},
    {85, 170, 287},
    {-128, 1024, 128},
}};

struct ProjectionFlagCase {
  MATRIX transform;
  SVECTOR point;
  VS::Core::Gte::Word depth;
  std::int16_t screen_x;
  std::int16_t screen_y;
  std::uint32_t flags;
};
constexpr MATRIX kIdentityTransform{
    {{4096, 0, 0}, {0, 4096, 0}, {0, 0, 4096}},
    {0, 0, 0},
};
// Original RotTransPers results: signed44 MAC1 positive and MAC2 negative
// overflow with IR1-IR3 saturation; a zero depth (divide overflow); a
// negative depth (SZ3 and divide); SX2/SY2 saturation.
constexpr std::array<ProjectionFlagCase, 4> kProjectionFlagCases{{
    {{{{32767, 32767, 32767}, {-32768, -32768, -32768}, {0, 0, 4096}},
      {INT32_MAX, INT32_MIN, 100}},
     {32767, 32767, 32767, 0},
     8216,
     -95,
     374,
     0xC5C00000u},
    {kIdentityTransform, {10, -10, 0, 0}, 0, 179, 100, 0x80020000u},
    {kIdentityTransform, {-500, 300, -5, 0}, 0, -840, 719, 0x80060000u},
    {kIdentityTransform, {-4000, 4000, 300, 0}, 75, -1024, 1023, 0x80006000u},
}};

// The projection ProjectionFixture loads into the shared registers.
Math::PerspectiveProjection FixtureProjection() {
  Math::PerspectiveProjection projection;
  projection.offset_x = Math::ScreenOffsetFixed(160);
  projection.offset_y = Math::ScreenOffsetFixed(120);
  projection.distance = 256;
  return projection;
}

VS::Core::Gte::Word PackScreen(std::int16_t x, std::int16_t y) {
  return static_cast<VS::Core::Gte::Word>(
      (static_cast<std::uint32_t>(static_cast<std::uint16_t>(y)) << 16) |
      static_cast<std::uint16_t>(x));
}
}  // namespace

TEST_CASE("Perspective gain keeps the SDK reciprocal precision",
          "[core-math][projection-rounding]") {
  struct GainCase {
    std::uint16_t projection;
    std::uint16_t depth;
    std::uint32_t gain;
    bool overflows;
  };
  // Values from the original RotTransPers RTPS divide.
  constexpr std::array<GainCase, 14> kCases{{
      {0, 0, 131071, true},
      {1, 0, 131071, true},
      {0, 1, 0, false},
      {1, 1, 65536, false},
      {2, 1, 131071, true},
      {256, 188, 89240, false},
      {256, 232, 72315, false},
      {256, 308, 54472, false},
      {256, 316, 53093, false},
      {256, 170, 98689, false},
      {65535, 32768, 131070, false},
      {65535, 65535, 65535, false},
      {65087, 32544, 131071, false},
      {61461, 30731, 131071, false},
  }};
  for (const auto& [projection, depth, gain, overflows] : kCases) {
    CAPTURE(projection, depth);
    CHECK(Math::DividePerspective(projection, depth) == gain);
    CHECK(Math::PerspectiveDivideOverflows(projection, depth) == overflows);
  }
}

TEST_CASE("Single-point projection matches the SDK issue vectors",
          "[core-math][projection-rounding]") {
  for (const auto& [x, z, screen_x] : kProjectionWitnesses) {
    CAPTURE(x, z);
    ProjectionFixture fixture;
    SVECTOR point{x, 0, z, 91};
    VS::Core::Gte::Word screen = -1;
    VS::Core::Gte::Word cue = -1;
    VS::Core::Gte::Word flags = -1;
    CHECK(RotTransPers(&point, &screen, &cue, &flags) == z / 4);
    CHECK(screen == PackScreen(screen_x, 120));
    CHECK(cue == 0);
    CHECK(flags == 0);
    CHECK(point.vx == x);
    CHECK(point.vz == z);
    CHECK(point.pad == 91);
  }
}

TEST_CASE("Four-point projection matches the SDK issue vectors",
          "[core-math][projection-rounding]") {
  for (const auto& [x, z, screen_x] : kProjectionWitnesses) {
    CAPTURE(x, z);
    ProjectionFixture fixture;
    SVECTOR point{x, 0, z, 91};
    VS::Core::Gte::Word cue = -1;
    VS::Core::Gte::Word flags = -1;
    const auto expected = PackScreen(screen_x, 120);
    std::array<VS::Core::Gte::Word, 4> screens{};
    CHECK(RotTransPers4(&point, &point, &point, &point, &screens[0],
                        &screens[1], &screens[2], &screens[3], &cue,
                        &flags) == z / 4);
    CHECK(screens == std::array<VS::Core::Gte::Word, 4>{expected, expected,
                                                        expected, expected});
    CHECK(cue == 0);
    CHECK(flags == 0);
  }
}

TEST_CASE("Projection reports the SDK status flags and saturated results",
          "[core-math][projection-rounding]") {
  for (auto [transform, point, depth, screen_x, screen_y, flags] :
       kProjectionFlagCases) {
    CAPTURE(point.vx, point.vy, point.vz);
    ProjectionFixture fixture;
    SetRotMatrix(&transform);
    SetTransMatrix(&transform);
    VS::Core::Gte::Word screen = -1;
    VS::Core::Gte::Word cue = -1;
    VS::Core::Gte::Word status = -1;
    CHECK(RotTransPers(&point, &screen, &cue, &status) == depth);
    CHECK(screen == PackScreen(screen_x, screen_y));
    CHECK(cue == 0);
    CHECK(static_cast<std::uint32_t>(status) == flags);
  }
}

TEST_CASE("Explicit projection matches the SDK vectors without shared state",
          "[core-math][projection-rounding]") {
  const Math::PerspectiveProjection projection = FixtureProjection();
  const Math::Transform3 identity = Math::TransformOf(kIdentityTransform);
  for (const auto& [x, z, screen_x] : kProjectionWitnesses) {
    CAPTURE(x, z);
    const Math::PerspectiveVertex vertex =
        Math::ProjectVertex(identity, projection, {x, 0, z});
    CHECK(vertex.QuarterDepth() == z / 4);
    CHECK(vertex.PackedScreen() == PackScreen(screen_x, 120));
    CHECK(vertex.ir0 == 0);
    CHECK(vertex.flag == 0);
  }
  for (const auto& [transform, point, depth, screen_x, screen_y, flags] :
       kProjectionFlagCases) {
    CAPTURE(point.vx, point.vy, point.vz);
    const Math::PerspectiveVertex vertex =
        Math::ProjectVertex(Math::TransformOf(transform), projection,
                            {point.vx, point.vy, point.vz});
    CHECK(vertex.QuarterDepth() == depth);
    CHECK(vertex.PackedScreen() == PackScreen(screen_x, screen_y));
    CHECK(vertex.ir0 == 0);
    CHECK(vertex.flag == flags);
  }
}

TEST_CASE("Explicit rotate-translate reports the RTV0TR saturation flags",
          "[core-math][projection-rounding]") {
  // The first flag case without the divide. MAC1's positive and MAC2's
  // negative 44-bit overflow wrap, so IR1 saturates low and IR2 high (the
  // same IRs give that case's SX2 -95 and SY2 374); IR3 saturates high.
  const ProjectionFlagCase& overflow = kProjectionFlagCases[0];
  const Math::RotatedVertex vertex = Math::RotateTranslateVertex(
      Math::TransformOf(overflow.transform),
      {overflow.point.vx, overflow.point.vy, overflow.point.vz});
  CHECK(vertex.ir == std::array<std::int16_t, 3>{-32768, 32767, 32767});
  CHECK((vertex.flag & 0x7FC00000u) == (overflow.flags & 0x7FC00000u));
  CHECK((vertex.flag & 0x0003F000u) == 0);
  // Without overflow the result is RotateTranslate's MAC and no flag.
  const Math::Transform3 shifted{Math::IdentityRotation(), {7, -9, 11}};
  const Math::RotatedVertex plain =
      Math::RotateTranslateVertex(shifted, {100, 200, -300});
  CHECK(plain.mac ==
        Math::RotateTranslate(shifted, Math::Vec3{100, 200, -300}));
  CHECK(plain.mac == Math::Vec3{107, 191, -289});
  CHECK(plain.ir == std::array<std::int16_t, 3>{107, 191, -289});
  CHECK(plain.flag == 0);
}

TEST_CASE("Explicit projection and the shared registers do not interfere",
          "[core-math][projection-rounding]") {
  ProjectionFixture fixture;
  SVECTOR point{-47, 0, 188, 0};
  VS::Core::Gte::Word before = 0;
  VS::Core::Gte::Word cue = 0;
  VS::Core::Gte::Word flags = 0;
  RotTransPers(&point, &before, &cue, &flags);
  const Math::PerspectiveProjection shared = VS::Core::Gte::SharedProjection();
  CHECK(shared == FixtureProjection());
  // A different explicit view and projection between two shared projections.
  Math::PerspectiveProjection other;
  other.distance = 1000;
  const Math::Transform3 moved{Math::IdentityRotation(), {500, -500, 4000}};
  const Math::PerspectiveVertex explicit_result =
      Math::ProjectVertex(moved, other, {point.vx, point.vy, point.vz});
  (void)Math::RotateTranslateVertex(moved, {1, 2, 3});
  VS::Core::Gte::Word after = 0;
  RotTransPers(&point, &after, &cue, &flags);
  CHECK(after == before);
  CHECK(after == PackScreen(96, 120));
  CHECK(VS::Core::Gte::SharedProjection() == shared);
  // Changing the shared registers leaves the explicit result unchanged.
  SetGeomOffset(0, 0);
  SetGeomScreen(77);
  const Math::PerspectiveVertex repeated =
      Math::ProjectVertex(moved, other, {point.vx, point.vy, point.vz});
  CHECK(repeated.PackedScreen() == explicit_result.PackedScreen());
  CHECK(repeated.depth == explicit_result.depth);
  CHECK(repeated.flag == explicit_result.flag);
}
