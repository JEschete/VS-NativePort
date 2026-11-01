#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <cstring>
#include <limits>
#include <type_traits>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Content/ModuleData.h"
#include "Core/Gte.h"
#include "Core/Math/Distance.hpp"
#include "Core/Math/ScalarMath.hpp"
#include "Core/Unported.hpp"
#include "Legacy/Psx.h"

extern "C" {
#include "Core/Math/FixedPoint.h"
#include "Core/Math/Matrix.h"
#include "Core/Math/Trig.h"
#include "Core/Math/Vector.h"
#include "Core/Runtime.h"
#include "Render/RoomDraw.h"
#include "Render/UiPrimitives.h"
}

static_assert(
    std::is_same_v<decltype(&Core_Math_SineFixedPoint), int (*)(int)>);
static_assert(
    std::is_same_v<decltype(&Core_Math_CosineFixedPoint), int (*)(int)>);
static_assert(std::is_same_v<decltype(&Core_Math_Sine), int (*)(int)>);
static_assert(std::is_same_v<decltype(&Core_Math_Cosine), int (*)(int)>);
namespace Math = VS::Core::Math;
using MatrixProduct = Math::Mat3 (*)(const Math::Mat3&,
                                     const Math::Mat3&) noexcept;
static_assert(
    std::is_same_v<decltype(&Math::MultiplyMatrixIntoLeft), MatrixProduct>);
static_assert(std::is_same_v<decltype(&Math::MultiplyRotationMatrixInPlace),
                             MatrixProduct>);
static_assert(std::is_same_v<decltype(&Math::SquareVectorComponents),
                             Math::Vec3 (*)(Math::Vec3) noexcept>);
static_assert(std::is_same_v<decltype(&Math::BuildEulerRotationMatrix),
                             Math::Mat3 (*)(Math::Angles3) noexcept>);
static_assert(std::is_same_v<decltype(&Math::LookupRotationSineCosine),
                             Math::SineCosine (*)(std::int32_t) noexcept>);

namespace {
auto MatrixBytes(const MATRIX& matrix) {
  std::array<std::byte, sizeof(matrix)> bytes{};
  std::memcpy(bytes.data(), &matrix, bytes.size());
  return bytes;
}

MATRIX ReferenceProduct(const MATRIX& first, const MATRIX& second,
                        const MATRIX& destination) {
  MATRIX result;
  std::memcpy(&result, &destination, sizeof result);
  for (int row = 0; row < 3; ++row) {
    for (int column = 0; column < 3; ++column) {
      std::int64_t sum = 0;
      for (int index = 0; index < 3; ++index) {
        sum += std::int64_t{first.m[row][index]} * second.m[index][column];
      }
      auto value = sum / 4096;
      if (sum < 0 && sum % 4096 != 0) --value;
      value = std::clamp<std::int64_t>(
          value, std::numeric_limits<std::int16_t>::min(),
          std::numeric_limits<std::int16_t>::max());
      result.m[row][column] = static_cast<std::int16_t>(value);
    }
  }
  return result;
}

MATRIX MatrixPattern(std::uint32_t& state) {
  MATRIX matrix;
  std::memset(&matrix, 0xA5, sizeof matrix);
  for (auto& row : matrix.m) {
    for (auto& value : row) {
      state = state * 1664525u + 1013904223u;
      value =
          std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(state >> 16));
    }
  }
  matrix.t[0] = 123;
  matrix.t[1] = -456;
  matrix.t[2] = 789;
  return matrix;
}

void CheckMatrixDestinations(const MATRIX& first, const MATRIX& second) {
  MATRIX left, right;
  std::memcpy(&left, &first, sizeof left);
  std::memcpy(&right, &second, sizeof right);
  const auto left_expected = ReferenceProduct(first, second, first);
  const auto right_expected = ReferenceProduct(first, second, second);
  Math::ApplyMultiplyMatrixIntoLeft(left, right);
  CHECK(MatrixBytes(left) == MatrixBytes(left_expected));
  CHECK(MatrixBytes(right) == MatrixBytes(second));
  std::memcpy(&left, &first, sizeof left);
  Math::ApplyMultiplyMatrixIntoRight(left, right);
  CHECK(MatrixBytes(right) == MatrixBytes(right_expected));
  CHECK(MatrixBytes(left) == MatrixBytes(first));
  std::memcpy(&right, &second, sizeof right);
  Math::ApplyMultiplyRotationMatrixInPlace(left, right);
  CHECK(MatrixBytes(right) == MatrixBytes(right_expected));
  CHECK(MatrixBytes(left) == MatrixBytes(first));
}

void CheckTrigBounds(int value) {
  CHECK(value >= -4096);
  CHECK(value <= 4096);
}

std::uint64_t HashFixedSample(std::uint64_t hash, int value) {
  constexpr std::uint64_t kFnvPrime = 1099511628211;
  const auto bits = static_cast<std::uint32_t>(value);
  for (unsigned byte = 0; byte < sizeof(bits); ++byte) {
    hash = (hash ^ ((bits >> (byte * 8)) & 0xFF)) * kFnvPrime;
  }
  return hash;
}
std::int32_t SquareHalfword(std::uint32_t bits) {
  const auto value = bits < 0x8000u ? static_cast<std::int32_t>(bits)
                                    : static_cast<std::int32_t>(bits) - 65536;
  return value * value;
}

void CheckVectorSquare(std::uint32_t bits) {
  const auto second = bits ^ 0x8000u;
  const auto third = bits ^ 0xFFFFu;
  VECTOR input{std::bit_cast<std::int32_t>(0x5A5A0000u | bits),
               std::bit_cast<std::int32_t>(0xA5A50000u | second),
               std::bit_cast<std::int32_t>(0xFFFF0000u | third), 123456789};
  CHECK(Math::SquareVectorComponents(Math::VectorOf(input)) ==
        Math::Vec3{SquareHalfword(bits), SquareHalfword(second),
                   SquareHalfword(third)});
}

int FloorDivide(std::int64_t value, int divisor) {
  return static_cast<int>(value / divisor -
                          (value < 0 && value % divisor != 0));
}

void ReferenceRoomEuler(const SVECTOR& rotation, MATRIX& output) {
  const int sx = Core_Gte_Sin(rotation.vx), cx = Core_Gte_Cos(rotation.vx);
  const int sy = Core_Gte_Sin(rotation.vy), cy = Core_Gte_Cos(rotation.vy);
  const int sz = Core_Gte_Sin(rotation.vz), cz = Core_Gte_Cos(rotation.vz);
  // Original GPF(1) stages: IR0=2*sz, then 2*cz; IR1..3=2*sy,2*cy,cx.
  const auto stage = [sy, cy, cx](int scalar) {
    return std::array{FloorDivide(std::int64_t{scalar} * 2 * sy, 4096),
                      FloorDivide(std::int64_t{scalar} * 2 * cy, 4096),
                      FloorDivide(std::int64_t{scalar} * cx, 4096)};
  };
  const auto first = stage(2 * sz), second = stage(2 * cz);
  const auto third = [sx](int value) {
    return FloorDivide(std::int64_t{sx} * value, 4096);
  };
  output.m[0][0] =
      static_cast<std::int16_t>(FloorDivide(third(first[0]) + second[1], 4));
  output.m[0][1] =
      static_cast<std::int16_t>(FloorDivide(third(second[0]) - first[1], 4));
  output.m[0][2] =
      static_cast<std::int16_t>(FloorDivide(std::int64_t{cx} * 2 * sy, 8192));
  output.m[1][0] = static_cast<std::int16_t>(FloorDivide(first[2], 2));
  output.m[1][1] = static_cast<std::int16_t>(FloorDivide(second[2], 2));
  output.m[1][2] = static_cast<std::int16_t>(-sx);
  output.m[2][0] =
      static_cast<std::int16_t>(FloorDivide(third(first[1]) - second[0], 4));
  output.m[2][1] =
      static_cast<std::int16_t>(FloorDivide(third(second[1]) + first[0], 4));
  output.m[2][2] =
      static_cast<std::int16_t>(FloorDivide(std::int64_t{cx} * 2 * cy, 8192));
}

void CheckRoomEuler(const SVECTOR& rotation) {
  struct GuardedMatrix {
    std::uint32_t prefix;
    MATRIX value;
    std::uint32_t suffix;
  } actual;
  std::memset(&actual, 0xA5, sizeof actual);
  GuardedMatrix expected;
  std::memcpy(&expected, &actual, sizeof expected);
  ReferenceRoomEuler(rotation, expected.value);
  const auto input = rotation;
  Math::ApplyBuildEulerRotationMatrix(rotation, actual.value);
  std::array<std::byte, sizeof actual> actual_bytes{}, expected_bytes{};
  std::memcpy(actual_bytes.data(), &actual, actual_bytes.size());
  std::memcpy(expected_bytes.data(), &expected, expected_bytes.size());
  CHECK(actual_bytes == expected_bytes);
  CHECK(std::memcmp(&rotation, &input, sizeof input) == 0);
}

void ReferenceRoomView(const MATRIX& view, int scale, bool compensated,
                       MATRIX& output) {
  const SVECTOR rotation{123, 234, 345, 0};
  ReferenceRoomEuler(rotation, output);
  MATRIX camera = view;
  if (compensated) {
    MATRIX inverse{};
    inverse.m[0][0] = inverse.m[1][1] = inverse.m[2][2] =
        static_cast<std::int16_t>(16777216 / scale);
    camera = ReferenceProduct(inverse, camera, camera);
  }
  output = ReferenceProduct(camera, output, output);
  const std::array<int, 3> coordinates{-32767, 123, 3};
  for (int row = 0; row < 3; ++row) {
    std::int64_t sum = 0;
    for (int axis = 0; axis < 3; ++axis) {
      sum += std::int64_t{camera.m[row][axis]} * coordinates[axis];
    }
    output.t[row] = FloorDivide(sum, 4096) + view.t[row];
  }
}

struct RestoreRoomCamera {
  MATRIX view = Render_frameScratch.viewMatrix;
  int scale = Render_frameScratch.camera.farClip;
  ~RestoreRoomCamera() {
    Render_frameScratch.viewMatrix = view;
    Render_frameScratch.camera.farClip = scale;
  }
};
}  // namespace

TEST_CASE("The original inline idle instruction is a native no-op",
          "[core][core-leaves]") {
  const auto hits = [] {
    std::uint64_t count = 0;
    for (const auto& entry : VS::Core::UnportedSnapshot()) {
      if (entry.site == "Core::IdleInstruction") count = entry.count;
    }
    return count;
  };
  const auto before = hits();
  Core_IdleInstruction();
  Core_IdleInstruction();
  CHECK(hits() == before);
  CHECK(&Core_IdleInstruction == &Core_IgnoreCall);
}

TEST_CASE("Left matrix multiply returns its destination and preserves its tail",
          "[math][core-leaves]") {
  MATRIX left{{{4096, 0, 0}, {0, 0, -4096}, {0, 4096, 0}}, {11, -22, 33}};
  MATRIX right{{{0, -4096, 0}, {4096, 0, 0}, {0, 0, 4096}}, {-44, 55, -66}};
  const auto before_right = MatrixBytes(right);
  const auto before_tail = MatrixBytes(left);
  Math::ApplyMultiplyMatrixIntoLeft(left, right);
  const std::array<std::int16_t, 9> expected{0,     -4096, 0, 0, 0,
                                             -4096, 4096,  0, 0};
  CHECK(std::memcmp(left.m, expected.data(), sizeof(left.m)) == 0);
  const auto after = MatrixBytes(left);
  CHECK(std::equal(after.begin() + sizeof(left.m), after.end(),
                   before_tail.begin() + sizeof(left.m)));
  CHECK(MatrixBytes(right) == before_right);
}

TEST_CASE(
    "Both matrix destinations preserve original Q12 saturation and aliases",
    "[math][core-leaves]") {
  std::uint32_t random = 0x32154275;
  for (int iteration = 0; iteration < 256; ++iteration) {
    const auto first = MatrixPattern(random);
    const auto second = MatrixPattern(random);
    CheckMatrixDestinations(first, second);
    MATRIX self;
    std::memcpy(&self, &first, sizeof self);
    const auto expected = ReferenceProduct(first, first, first);
    Math::ApplyMultiplyMatrixIntoLeft(self, self);
    CHECK(MatrixBytes(self) == MatrixBytes(expected));
    std::memcpy(&self, &first, sizeof self);
    Math::ApplyMultiplyRotationMatrixInPlace(self, self);
    CHECK(MatrixBytes(self) == MatrixBytes(expected));
  }
  MATRIX positive{}, negative{};
  for (auto& row : positive.m) {
    std::fill(std::begin(row), std::end(row), INT16_MAX);
  }
  for (auto& row : negative.m) {
    std::fill(std::begin(row), std::end(row), INT16_MIN);
  }
  CheckMatrixDestinations(positive, positive);
  CheckMatrixDestinations(positive, negative);
}

TEST_CASE("Promoted scalar math has no original module addresses",
          "[math][core-leaves][redirect-binding]") {
  const auto symbols =
      std::span(Content_moduleSymbols, Content_moduleSymbolCount);
  // These entries were only reached by direct jal instructions (or not at
  // all), so callers use the typed API by name and no address resolves.
  for (const unsigned address :
       {0x80041104u, 0x8004140Cu, 0x80041980u, 0x80041A1Cu, 0x80041AF4u,
        0x80041B94u, 0x80041C60u, 0x80041C68u}) {
    CAPTURE(address);
    CHECK(VS::Content::ResolveModuleAddress(symbols, "SLUS_010.40", address) ==
          nullptr);
  }
  for (const unsigned address :
       {0x8009874Cu, 0x800988BCu, 0x800989B8u, 0x800B16F4u, 0x800B17F0u,
        0x800B196Cu, 0x800B396Cu, 0x800BE858u}) {
    CAPTURE(address);
    CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                            address) == nullptr);
  }
}

TEST_CASE("Vector square keeps full products of every signed input halfword",
          "[math][core-leaves]") {
  for (std::uint32_t bits = 0; bits <= UINT16_MAX; ++bits) {
    CheckVectorSquare(bits);
  }
}

TEST_CASE("Vector square keeps its low-halfword golden",
          "[math][core-leaves]") {
  const VECTOR input{65539, -65540, INT32_MIN, 321};
  CHECK(Math::SquareVectorComponents(Math::VectorOf(input)) ==
        Math::Vec3{9, 16, 0});
}

TEST_CASE("Battle fixed trigonometry uses the canonical native implementations",
          "[math][sdk-gaps][fixed-trig]") {
  CHECK(&Core_Math_SineFixedPoint == &Core_Gte_Sin);
  CHECK(&Core_Math_CosineFixedPoint == &Core_Gte_Cos);
  CHECK(&Core_Math_Sine == &Core_Gte_Sin);
  CHECK(&Core_Math_Cosine == &Core_Gte_Cos);
}

TEST_CASE("Room view rotation retains the original staged YXZ precision",
          "[math][room-rotation]") {
  const RestoreRoomCamera restore;
  Render_frameScratch.viewMatrix = {};
  for (int axis = 0; axis < 3; ++axis) {
    Render_frameScratch.viewMatrix.m[axis][axis] = 4096;
  }
  VECTOR position{}, angles{123, 234, 345, 0};
  MATRIX output{};
  Render_BuildRoomGeometryViewMatrix(&position, &angles, &output);
  // Expanded original2EA3C.s:1499 stages, not successive matrix products.
  constexpr std::array<std::int16_t, 9> kExpected{3446, -1704, 1413, 2031, 3472,
                                                  -768, -880,  1347, 3766};
  for (int row = 0; row < 3; ++row) {
    for (int column = 0; column < 3; ++column) {
      CAPTURE(row, column);
      CHECK(output.m[row][column] == kExpected[row * 3 + column]);
    }
  }
}

TEST_CASE(
    "Native room Euler stages preserve mixed angles and destination bytes",
    "[math][room-rotation]") {
  for (int x = 0; x < 4096; x += 512) {
    for (int y = 0; y < 4096; y += 512) {
      for (int z = 0; z < 4096; z += 512) {
        CheckRoomEuler({static_cast<std::int16_t>(x),
                        static_cast<std::int16_t>(y),
                        static_cast<std::int16_t>(z), 123});
      }
    }
  }
  std::uint32_t state = 0x9874C;
  const auto angle = [&state] {
    state = state * 1664525u + 1013904223u;
    return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(state >> 16));
  };
  for (int index = 0; index < 4096; ++index) {
    const SVECTOR rotation{angle(), angle(), angle(), -123};
    CheckRoomEuler(rotation);
  }
}

TEST_CASE("Both rotation octant entries retain signed pairs for every halfword",
          "[math][room-rotation][fixed-trig]") {
  for (std::uint32_t bits = 0; bits <= UINT16_MAX; ++bits) {
    const int angle =
        std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(bits));
    const auto pair = Math::LookupRotationSineCosine(angle);
    CHECK(pair.sine == Core_Gte_Sin(angle));
    CHECK(pair.cosine == Core_Gte_Cos(angle));
    const int wide = std::bit_cast<std::int32_t>(bits | 0xA5A50000u);
    CHECK(Math::LookupRotationSineCosine(wide) == pair);
  }
  CHECK(Math::LookupRotationSineCosine(1536) == Math::SineCosine{2896, -2896});
}

TEST_CASE("Room view producers retain source stages with camera and scale",
          "[math][room-rotation]") {
  const RestoreRoomCamera restore;
  std::uint32_t state = 0x985AC;
  Render_frameScratch.viewMatrix = MatrixPattern(state);
  Render_frameScratch.viewMatrix.t[0] = 321;
  const auto before = MatrixBytes(Render_frameScratch.viewMatrix);
  const auto scale = GENERATE(2048, 4096, 8192);
  const auto compensated = GENERATE(false, true);
  Render_frameScratch.camera.farClip = scale;
  VECTOR position{0x12348001, -65413, 65539, -1};
  VECTOR angles{0x1234007B, -65302, 65881, -2};
  MATRIX actual;
  std::memset(&actual, 0xA5, sizeof actual);
  MATRIX expected;
  std::memcpy(&expected, &actual, sizeof expected);
  ReferenceRoomView(Render_frameScratch.viewMatrix, scale, compensated,
                    expected);
  if (compensated) {
    Render_BuildScaleCompensatedRoomView(&position, &angles, &actual);
  } else {
    Render_BuildRoomGeometryViewMatrix(&position, &angles, &actual);
  }
  CHECK(MatrixBytes(actual) == MatrixBytes(expected));
  CHECK(MatrixBytes(Render_frameScratch.viewMatrix) == before);
  CHECK(Render_frameScratch.camera.farClip == scale);
  CHECK(position.pad == -1);
  CHECK(angles.pad == -2);
}

TEST_CASE("Room Euler rotation keeps its original golden element",
          "[math][room-rotation]") {
  const auto output = Math::BuildEulerRotationMatrix({123, 234, 345});
  CHECK(output.m[0][1] == -1704);
}

TEST_CASE(
    "Native trig redirects remain available through original module addresses",
    "[math][sdk-gaps][fixed-trig][redirect-binding]") {
  const bool cosine = GENERATE(false, true);
  const bool resident = GENERATE(false, true);
  const char* module = resident ? "SLUS_010.40" : "BATTLE/BATTLE.PRG";
  const unsigned address = resident ? (cosine ? 0x80040F24u : 0x80040F28u)
                                    : (cosine ? 0x80098AB4u : 0x80098AB8u);
  const auto symbols =
      std::span(Content_moduleSymbols, Content_moduleSymbolCount);
  void* resolved = VS::Content::ResolveModuleAddress(symbols, module, address);
  REQUIRE(resolved != nullptr);
  const auto function = reinterpret_cast<int (*)(int)>(resolved);
  CHECK(function == (cosine ? Core_Gte_Cos : Core_Gte_Sin));
  CHECK(function(1024) == (cosine ? 0 : 4096));
  CHECK(VS::Content::ResolveModuleAddress(symbols, module, address + 1) ==
        nullptr);
}

TEST_CASE("Fixed trigonometry retains mathematical octants at Q12 scale",
          "[math][sdk-gaps][fixed-trig]") {
  constexpr std::array kSine{0, 2896, 4096, 2896, 0, -2896, -4096, -2896};
  for (int octant = 0; octant < 8; ++octant) {
    CHECK(Core_Math_SineFixedPoint(octant * 512) == kSine[octant]);
    CHECK(Core_Math_CosineFixedPoint(octant * 512) == kSine[(octant + 2) & 7]);
  }
}

TEST_CASE("Every fixed-trig angle retains range phase sign and turn period",
          "[math][sdk-gaps][fixed-trig]") {
  for (int angle = 0; angle < 4096; ++angle) {
    const int sine = Core_Math_SineFixedPoint(angle);
    const int cosine = Core_Math_CosineFixedPoint(angle);
    CheckTrigBounds(sine);
    CheckTrigBounds(cosine);
    CHECK(Core_Math_Sine(angle) == sine);
    CHECK(Core_Math_Cosine(angle) == cosine);
    CHECK(sine == -Core_Math_SineFixedPoint(angle + 2048));
    CHECK(cosine == Core_Math_SineFixedPoint(angle + 1024));
    CHECK(sine == Core_Math_SineFixedPoint(angle - 4096));
    CHECK(cosine == Core_Math_CosineFixedPoint(angle + 4096));
  }
}

TEST_CASE(
    "Every fixed-trig result matches the original signed table fingerprint",
    "[math][sdk-gaps][fixed-trig]") {
  std::uint64_t hash = 14695981039346656037ull;
  std::uint64_t resident_hash = hash;
  for (int angle = 0; angle < 4096; ++angle) {
    hash = HashFixedSample(hash, Core_Math_SineFixedPoint(angle));
    hash = HashFixedSample(hash, Core_Math_CosineFixedPoint(angle));
    resident_hash = HashFixedSample(resident_hash, Core_Math_Sine(angle));
    resident_hash = HashFixedSample(resident_hash, Core_Math_Cosine(angle));
  }
  // Original sine/cosine pairs in angle order, each encoded as signed LE32.
  CHECK(hash == 0x8E620CA359EB4481ull);
  CHECK(resident_hash == 0x8E620CA359EB4481ull);
}

TEST_CASE("Fixed-trig inputs wrap safely across the full signed integer range",
          "[math][sdk-gaps][fixed-trig]") {
  constexpr std::array kAngles{std::numeric_limits<int>::min(),
                               std::numeric_limits<int>::min() + 1,
                               std::numeric_limits<int>::max(),
                               std::numeric_limits<int>::max() - 1,
                               -4097,
                               -4096,
                               -2048,
                               -1024,
                               -1,
                               0,
                               1,
                               1024,
                               2048,
                               4095,
                               4096,
                               4097};
  for (int angle : kAngles) {
    const int phase = static_cast<int>(static_cast<unsigned>(angle) & 0xFFFu);
    CHECK(Core_Math_SineFixedPoint(angle) == Core_Math_SineFixedPoint(phase));
    CHECK(Core_Math_CosineFixedPoint(angle) ==
          Core_Math_CosineFixedPoint(phase));
    CHECK(Core_Math_Sine(angle) == Core_Math_SineFixedPoint(phase));
    CHECK(Core_Math_Cosine(angle) == Core_Math_CosineFixedPoint(phase));
  }
}

TEST_CASE("Retired SDK math entries are the canonical native implementations",
          "[math][sdk-gaps]") {
  CHECK(&Core_Math_BuildRotationYxz == &Core_Gte_RotMatrixYXZ);
  CHECK(&Core_Math_BuildRotationXyz == &Core_Gte_RotMatrix);
  CHECK(&Core_Math_CrossProductFixed == &Core_Gte_OuterProduct0);
  CHECK(&Core_Math_SquareRootFixed == &Core_Gte_SquareRoot12);
  CHECK(&Core_Math_NormalizeVector == &Core_Gte_VectorNormal);
  CHECK(&Core_Math_NormalizeVectorToShort == &Core_Gte_VectorNormalS);
  CHECK(&Core_Math_NormalizeShortVector == &Core_Gte_VectorNormalSS);
}

TEST_CASE("SDK rotation aliases retain native result pointers and translation",
          "[math][sdk-gaps]") {
  const auto order = GENERATE(0, 1);
  const auto axis = GENERATE(0, 1, 2);
  SVECTOR angles{};
  std::array<int16_t*, 3> components{&angles.vx, &angles.vy, &angles.vz};
  *components[axis] = 1024;
  MATRIX matrix{};
  matrix.t[0] = 17;
  matrix.t[1] = -23;
  matrix.t[2] = 41;
  const std::array<MATRIX* (*)(SVECTOR*, MATRIX*), 2> build{
      Core_Math_BuildRotationYxz, Core_Math_BuildRotationXyz};
  CHECK(build[order](&angles, &matrix) == &matrix);
  CHECK(matrix.t[0] == 17);
  CHECK(matrix.t[1] == -23);
  CHECK(matrix.t[2] == 41);
  const int16_t expected[3][3][3]{{{4096, 0, 0}, {0, 0, -4096}, {0, 4096, 0}},
                                  {{0, 0, 4096}, {0, 4096, 0}, {-4096, 0, 0}},
                                  {{0, -4096, 0}, {4096, 0, 0}, {0, 0, 4096}}};
  for (std::size_t row = 0; row < 3; ++row) {
    for (std::size_t column = 0; column < 3; ++column) {
      CHECK(matrix.m[row][column] == expected[axis][row][column]);
    }
  }
}

TEST_CASE(
    "SDK vector normalization supports each native output shape and padding",
    "[math][sdk-gaps]") {
  VECTOR input{3, 4, 0, 77};
  VECTOR wide{0, 0, 0, 99};
  SVECTOR narrow{0, 0, 0, 55};
  CHECK(Core_Math_NormalizeVector(&input, &wide) == 25);
  CHECK(wide.vx == 2457);
  CHECK(wide.vy == 3276);
  CHECK(wide.pad == 99);
  CHECK(Core_Math_NormalizeVectorToShort(&input, &narrow) == 25);
  CHECK(narrow.vx == wide.vx);
  CHECK(narrow.vy == wide.vy);
  CHECK(narrow.pad == 55);
}

TEST_CASE("SDK short-vector normalization permits in-place and zero inputs",
          "[math][sdk-gaps]") {
  SVECTOR input{-3, 4, 0, 55};
  CHECK(Core_Math_NormalizeShortVector(&input, &input) == 25);
  CHECK(input.vx == -2457);
  CHECK(input.vy == 3276);
  CHECK(input.pad == 55);
  VECTOR zero{};
  CHECK(Core_Math_NormalizeVectorToShort(&zero, &input) == 0);
  CHECK(input.vx == 0);
  CHECK(input.vy == 0);
  CHECK(input.pad == 55);
}

TEST_CASE(
    "SDK fixed cross products use Q12 scale and preserve unrelated padding",
    "[math][sdk-gaps]") {
  VECTOR first{4096, 0, 0, 77};
  VECTOR second{0, 4096, 0, 88};
  Core_Math_CrossProductFixed(&first, &second, &first);
  CHECK(first.vx == 0);
  CHECK(first.vy == 0);
  CHECK(first.vz == 4096);
  CHECK(first.pad == 77);
}

TEST_CASE("SDK mixed rotations preserve the distinct XYZ and YXZ orders",
          "[math][sdk-gaps]") {
  const auto order = GENERATE(0, 1);
  SVECTOR angles{1024, 1024, 0, 0};
  MATRIX result{};
  const std::array<MATRIX* (*)(SVECTOR*, MATRIX*), 2> build{
      Core_Math_BuildRotationYxz, Core_Math_BuildRotationXyz};
  const int16_t expected[2][3][3]{{{0, 4096, 0}, {0, 0, -4096}, {-4096, 0, 0}},
                                  {{0, 0, 4096}, {4096, 0, 0}, {0, 4096, 0}}};
  build[order](&angles, &result);
  for (std::size_t row = 0; row < 3; ++row) {
    for (std::size_t column = 0; column < 3; ++column) {
      CHECK(result.m[row][column] == expected[order][row][column]);
    }
  }
}

TEST_CASE("SDK fixed square root retains its Q12 interpretation",
          "[math][sdk-gaps]") {
  CHECK(Core_Math_SquareRootFixed(0) == 0);
  CHECK(Core_Math_SquareRootFixed(4096) == 4096);
  CHECK(Core_Math_SquareRootFixed(36864) == 12288);
}

TEST_CASE(
    "Packed tile distance covers unsigned coordinate bytes and ignores "
    "metadata",
    "[math][sdk-gaps]") {
  using VS::Core::Math::SquaredTileDistance;
  using VS::Core::Math::UnpackTileCoordinate;
  for (unsigned x = 0; x < 256; ++x) {
    for (unsigned z = 0; z < 256; ++z) {
      const auto packed = 0xA5005A00u | x | (z << 16);
      const auto cell = UnpackTileCoordinate(packed);
      CHECK(cell.x == x);
      CHECK(cell.z == z);
      CHECK(SquaredTileDistance(cell, UnpackTileCoordinate(0xAB00CD00)) ==
            x * x + z * z);
    }
  }
  CHECK(SquaredTileDistance(UnpackTileCoordinate(0x00FF0000),
                            UnpackTileCoordinate(0x000000FF)) == 130050);
}

TEST_CASE("Tile distance API takes decoded cells instead of register words",
          "[math][tile-components]") {
  using VS::Core::Math::TileCoordinate;
  STATIC_REQUIRE(std::is_same_v<
                 decltype(&VS::Core::Math::SquaredTileDistanceFromComponents),
                 std::uint32_t (*)(std::int32_t, std::int32_t,
                                   TileCoordinate) noexcept>);
  STATIC_REQUIRE(std::is_same_v<decltype(&VS::Core::Math::SquaredTileDistance),
                                std::uint32_t (*)(TileCoordinate,
                                                  TileCoordinate) noexcept>);
  constexpr auto cell = VS::Core::Math::UnpackTileCoordinate(0xFF34FF12u);
  STATIC_REQUIRE(cell.x == 0x12);
  STATIC_REQUIRE(cell.z == 0x34);
}

TEST_CASE("Tile component entry shares every signed byte-distance result",
          "[math][tile-components]") {
  using VS::Core::Math::TileCoordinate;
  for (int dx = -255; dx <= 255; ++dx) {
    for (int dz = -255; dz <= 255; ++dz) {
      const int first_x = dx < 0 ? 0 : dx;
      const int first_z = dz < 0 ? 0 : dz;
      const TileCoordinate first{static_cast<std::uint8_t>(first_x),
                                 static_cast<std::uint8_t>(first_z)};
      const TileCoordinate second{static_cast<std::uint8_t>(dx < 0 ? -dx : 0),
                                  static_cast<std::uint8_t>(dz < 0 ? -dz : 0)};
      const auto expected = static_cast<unsigned>(dx * dx + dz * dz);
      CHECK(VS::Core::Math::SquaredTileDistanceFromComponents(
                first_x, first_z, second) == expected);
      CHECK(VS::Core::Math::SquaredTileDistance(first, second) == expected);
    }
  }
}

namespace {
unsigned ReferenceTileComponentSquare(int first, unsigned second) {
  const auto bits = (static_cast<unsigned>(first) - second) & 0xFFFF;
  const int delta =
      static_cast<int>(bits & 0x7FFF) - static_cast<int>(bits & 0x8000);
  return static_cast<unsigned>(delta * delta);
}

void CheckTileComponentWidths(int x, int z, unsigned target) {
  const auto expected = ReferenceTileComponentSquare(x, target & 0xFF) +
                        ReferenceTileComponentSquare(z, (target >> 16) & 0xFF);
  CHECK(VS::Core::Math::SquaredTileDistanceFromComponents(
            x, z, VS::Core::Math::UnpackTileCoordinate(target)) == expected);
}
}  // namespace

TEST_CASE("Tile component arithmetic retains the shared GTE halfword inputs",
          "[math][tile-components]") {
  constexpr std::array<int, 9> kComponents{-65536, -32768, -1,    0,    1,
                                           32767,  32768,  65535, 65536};
  constexpr std::array<unsigned, 5> kTargets{0, 0x00FF00FF, 0xFF00FF00,
                                             0xA5815A7F, 0xFFFFFFFF};
  for (const int x : kComponents) {
    for (const int z : kComponents) {
      for (const auto target : kTargets) {
        CheckTileComponentWidths(x, z, target);
      }
    }
  }
}

TEST_CASE("Promoted distance helpers have no original module addresses",
          "[math][tile-components][redirect-binding]") {
  const auto symbols =
      std::span(Content_moduleSymbols, Content_moduleSymbolCount);
  // BATTLE.PRG only reached these through direct jal instructions, so the
  // promoted API is called by name and no address resolves to it.
  for (const unsigned address :
       {0x800E45F4u, 0x800E4604u, 0x800E4624u, 0x800E4660u}) {
    CAPTURE(address);
    CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                            address) == nullptr);
  }
}
