#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>

#include <catch2/catch_test_macros.hpp>

#include "Core/Math/ScalarMath.hpp"
#include "Core/Math/VectorLive.hpp"
#include "Legacy/Psx.h"

namespace {
namespace Math = VS::Core::Math;

// D_80040D98 as published: floor(32768 / sqrt(index)) for index 64..255.
int ReferenceReciprocalRoot(std::uint32_t index) {
  return static_cast<int>(32768.0 / std::sqrt(static_cast<double>(index)));
}

std::int64_t FloorDivide(std::int64_t value, std::int64_t divisor) {
  return value / divisor - (value < 0 && value % divisor != 0);
}

Math::ReciprocalRoot ReferenceParts(std::uint32_t value) {
  const unsigned even = static_cast<unsigned>(std::countl_zero(value)) & ~1u;
  const std::uint32_t normalized =
      even >= 24 ? value << (even - 24) : value >> (24 - even);
  return Math::ReciprocalRoot{ReferenceReciprocalRoot(normalized),
                              static_cast<int>((31 - even) / 2)};
}

int ShortBits(std::int32_t value) {
  return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(value));
}

Math::NormalizedVector ReferenceNormalize(const Math::Vec3& input) {
  const std::array components{ShortBits(input.x), ShortBits(input.y),
                              ShortBits(input.z)};
  std::uint32_t squared = 0;
  for (int value : components) {
    squared += static_cast<std::uint32_t>(value * value);
  }
  Math::NormalizedVector result{{}, squared};
  if (squared == 0) return result;
  const auto parts = ReferenceParts(squared);
  const auto scale = [&parts](int value) {
    return static_cast<std::int32_t>(
        FloorDivide(std::int64_t{value} * parts.mantissa,
                    std::int64_t{1} << parts.exponent));
  };
  result.direction = {scale(components[0]), scale(components[1]),
                      scale(components[2])};
  return result;
}

void CheckParts(std::int32_t input) {
  CAPTURE(input);
  const auto parts = Math::InverseSquareRootParts(input);
  REQUIRE(parts.has_value());
  CHECK(*parts == ReferenceParts(static_cast<std::uint32_t>(input)));
}
}  // namespace

TEST_CASE("Reciprocal root covers its ordinary table and binary scales",
          "[math][source-normalization]") {
  for (std::uint32_t index = 64; index < 256; ++index) {
    for (unsigned shift = 0; shift <= 22; shift += 2) {
      CheckParts(static_cast<std::int32_t>(index << shift));
    }
  }
}

TEST_CASE("Reciprocal root retains small and full-range positive lookups",
          "[math][source-normalization]") {
  constexpr std::array kInputs{1,  2,   3,   15,  16,  63,  64,
                               65, 127, 128, 255, 256, 257, INT32_MAX};
  for (int input : kInputs) CheckParts(input);
  CHECK(Math::InverseSquareRootParts(25) == Math::ReciprocalRoot{3276, 2});
}

TEST_CASE("Non-positive values have no reciprocal root",
          "[math][source-normalization]") {
  for (int input : {0, -1, -2, -64, -256, -1073741824, INT32_MIN}) {
    CAPTURE(input);
    CHECK_FALSE(Math::InverseSquareRootParts(input).has_value());
  }
}

TEST_CASE("Source normalization retains the original quantized unit vector",
          "[math][source-normalization]") {
  const auto result = Math::NormalizeVector({4000, 1000, 0});
  CHECK(result.squared_length == 17000000);
  CHECK(result.direction == Math::Vec3{4000, 1000, 0});
  CHECK(Math::NormalizeVector({3, 4, 0}) ==
        Math::NormalizedVector{{2457, 3276, 0}, 25});
}

TEST_CASE("Record normalization keeps the quantized result and padding",
          "[math][source-normalization][core-math]") {
  // VectorNormal (32154.s) and the old native bridge's double square root
  // differ here: the original quantizes the reciprocal root to its table.
  VECTOR input{4000, 1000, 0, 77};
  VECTOR native{0, 0, 0, 55};
  CHECK(Math::NormalizeLiveVector(&input, &native) == 17000000);
  CHECK(native.vx == 4000);
  CHECK(native.vy == 1000);
  CHECK(native.vz == 0);
  CHECK(native.pad == 55);
  CHECK(input.pad == 77);
}

TEST_CASE("Record normalization reads only each component's low halfword",
          "[math][source-normalization][core-math]") {
  VECTOR input{INT32_MAX, INT32_MAX, INT32_MAX, 77};
  VECTOR native{};
  CHECK(Math::NormalizeLiveVector(&input, &native) == 3);
  CHECK(native.vx == -2364);
  CHECK(native.vy == -2364);
  CHECK(native.vz == -2364);
  SVECTOR narrow{-1, -1, -1, 9};
  CHECK(Math::NormalizeLiveVector(&narrow, &narrow) == 3);
  CHECK(narrow.vx == -2364);
  CHECK(narrow.pad == 9);
}

TEST_CASE(
    "Source normalization covers every signed halfword and ignores high bits",
    "[math][source-normalization]") {
  for (std::uint32_t bits = 0; bits <= UINT16_MAX; ++bits) {
    const Math::Vec3 narrow{ShortBits(static_cast<std::int32_t>(bits)), 127,
                            -63};
    CHECK(Math::NormalizeVector(narrow) == ReferenceNormalize(narrow));
    const Math::Vec3 wide{std::bit_cast<std::int32_t>(bits | 0xA5A50000u),
                          0x1234007F, std::bit_cast<std::int32_t>(0x7FFFFFC1u)};
    CHECK(Math::NormalizeVector(wide) == ReferenceNormalize(narrow));
  }
}

TEST_CASE("Squared lengths of 2^31 and above use the in-table root",
          "[math][source-normalization]") {
  const auto result = Math::NormalizeVector({-32768, -32768, -32768});
  CHECK(result.squared_length == 0xC0000000u);
  CHECK(result.direction == Math::Vec3{-2364, -2364, -2364});
  const Math::Vec3 large{32767, -32768, 1};
  CHECK(Math::NormalizeVector(large) == ReferenceNormalize(large));
}

TEST_CASE("Zero-length normalization gives a zero direction",
          "[math][source-normalization]") {
  CHECK(Math::NormalizeVector({65536, -65536, INT32_MIN}) ==
        Math::NormalizedVector{});
}
