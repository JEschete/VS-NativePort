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
#include "Core/Unported.hpp"
#include "Legacy/Psx.h"

extern "C" {
#include "Core/Math/FixedPoint.h"
#include "Core/Math/Matrix.h"
#include "Core/Math/ScalarMath.h"
#include "Core/Math/TileDistance.h"
#include "Core/Math/Trig.h"
#include "Core/Runtime.h"
}

static_assert(
    std::is_same_v<decltype(&Core_Math_SineFixedPoint), int (*)(int)>);
static_assert(
    std::is_same_v<decltype(&Core_Math_CosineFixedPoint), int (*)(int)>);
static_assert(std::is_same_v<decltype(&Core_Math_Sine), int (*)(int)>);
static_assert(std::is_same_v<decltype(&Core_Math_Cosine), int (*)(int)>);
static_assert(std::is_same_v<decltype(&Core_Math_MultiplyMatrixIntoLeft),
                             MATRIX* (*)(MATRIX*, MATRIX*)>);
static_assert(std::is_same_v<decltype(&Core_Math_MultiplyRotationMatrixInPlace),
                             MATRIX* (*)(MATRIX*, MATRIX*)>);

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
  REQUIRE(Core_Math_MultiplyMatrixIntoLeft(&left, &right) == &left);
  CHECK(MatrixBytes(left) == MatrixBytes(left_expected));
  CHECK(MatrixBytes(right) == MatrixBytes(second));
  std::memcpy(&left, &first, sizeof left);
  Core_Math_MultiplyMatrixIntoRight(&left, &right);
  CHECK(MatrixBytes(right) == MatrixBytes(right_expected));
  CHECK(MatrixBytes(left) == MatrixBytes(first));
  std::memcpy(&right, &second, sizeof right);
  REQUIRE(Core_Math_MultiplyRotationMatrixInPlace(&left, &right) == &right);
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
  REQUIRE(Core_Math_MultiplyMatrixIntoLeft(&left, &right) == &left);
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
    REQUIRE(Core_Math_MultiplyMatrixIntoLeft(&self, &self) == &self);
    CHECK(MatrixBytes(self) == MatrixBytes(expected));
    std::memcpy(&self, &first, sizeof self);
    REQUIRE(Core_Math_MultiplyRotationMatrixInPlace(&self, &self) == &self);
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

TEST_CASE("Left matrix entry retains its original resident module binding",
          "[math][core-leaves]") {
  const auto symbols =
      std::span(Content_moduleSymbols, Content_moduleSymbolCount);
  auto* resolved =
      VS::Content::ResolveModuleAddress(symbols, "SLUS_010.40", 0x80041C60u);
  REQUIRE(resolved != nullptr);
  using Product = MATRIX* (*)(MATRIX*, MATRIX*);
  const auto product = reinterpret_cast<Product>(resolved);
  CHECK(product == Core_Math_MultiplyMatrixIntoLeft);
  MATRIX left{}, right{};
  CHECK(product(&left, &right) == &left);
  CHECK(VS::Content::ResolveModuleAddress(symbols, "SLUS_010.40",
                                          0x80041C61u) == nullptr);
}

TEST_CASE(
    "BATTLE rotation product returns its right destination through its binding",
    "[math][core-leaves]") {
  const auto symbols =
      std::span(Content_moduleSymbols, Content_moduleSymbolCount);
  auto* resolved = VS::Content::ResolveModuleAddress(
      symbols, "BATTLE/BATTLE.PRG", 0x800989B8u);
  REQUIRE(resolved != nullptr);
  using Product = MATRIX* (*)(MATRIX*, MATRIX*);
  const auto product = reinterpret_cast<Product>(resolved);
  CHECK(product == Core_Math_MultiplyRotationMatrixInPlace);
  MATRIX left{}, right{};
  CHECK(product(&left, &right) == &right);
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x800989B9u) == nullptr);
}

TEST_CASE("Battle fixed trigonometry uses the canonical native implementations",
          "[math][sdk-gaps][fixed-trig]") {
  CHECK(&Core_Math_SineFixedPoint == &Core_Gte_Sin);
  CHECK(&Core_Math_CosineFixedPoint == &Core_Gte_Cos);
  CHECK(&Core_Math_Sine == &Core_Gte_Sin);
  CHECK(&Core_Math_Cosine == &Core_Gte_Cos);
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
  for (unsigned x = 0; x < 256; ++x) {
    for (unsigned z = 0; z < 256; ++z) {
      const auto packed = 0xA5005A00u | x | (z << 16);
      CHECK(Core_Math_SquaredTileDistance(packed, 0xAB00CD00) == x * x + z * z);
    }
  }
  CHECK(Core_Math_SquaredTileDistance(0x00FF0000, 0x000000FF) == 130050);
}
