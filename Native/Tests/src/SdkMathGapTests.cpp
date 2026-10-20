#include <array>
#include <cstdint>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Core/Gte.h"
#include "Legacy/Psx.h"

extern "C" {
#include "Core/Math/FixedPoint.h"
#include "Core/Math/TileDistance.h"
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
