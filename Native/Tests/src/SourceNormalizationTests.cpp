#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <type_traits>

#include <catch2/catch_test_macros.hpp>

#include "Content/ModuleData.h"
#include "Core/Gte.h"
#include "Legacy/Psx.h"

extern "C" {
#include "Core/Math/FixedPoint.h"
#include "Core/Math/ScalarMath.h"
}

namespace {
using LookupBytes = std::array<u_char, 1296>;

struct LookupFixture {
  LookupBytes saved = std::to_array(Core_Math_kArcTangentTable);
  LookupBytes lookup{};

  void StoreHalfword(std::size_t offset, unsigned value) {
    lookup[offset] = static_cast<u_char>(value);
    lookup[offset + 1] = static_cast<u_char>(value >> 8);
  }

  LookupFixture() {
    for (std::size_t index = 0; index < lookup.size(); ++index) {
      lookup[index] = static_cast<u_char>((index * 37 + 13) & 255);
    }
    for (unsigned index = 64; index < 256; ++index) {
      StoreHalfword(516 + std::size_t{index - 64} * 2,
                    static_cast<unsigned>(
                        std::sqrt(static_cast<double>(index * 262144))));
      StoreHalfword(900 + std::size_t{index - 64} * 2,
                    static_cast<unsigned>(
                        32768.0 / std::sqrt(static_cast<double>(index))));
    }
    std::copy(lookup.begin(), lookup.end(), Core_Math_kArcTangentTable);
  }

  ~LookupFixture() {
    std::copy(saved.begin(), saved.end(), Core_Math_kArcTangentTable);
  }
};

int ShortBits(std::uint32_t value) {
  return static_cast<int>(value & 0x7FFF) - static_cast<int>(value & 0x8000);
}

std::int64_t FloorDivide(std::int64_t value, std::int64_t divisor) {
  return value / divisor - (value < 0 && value % divisor != 0);
}

struct RootParts {
  int mantissa;
  int exponent;
};

RootParts ReferenceParts(int value, const LookupBytes& lookup) {
  const auto bits = static_cast<std::uint32_t>(value);
  const unsigned leading =
      value < 0 ? std::countl_one(bits) : std::countl_zero(bits);
  if (leading == 32) {
    throw std::logic_error(
        "Reciprocal-root oracle requires a nondegenerate input");
  }
  const unsigned even = leading & ~1u;
  const std::int64_t normalized =
      even >= 24 ? std::bit_cast<std::int32_t>(
                       static_cast<std::uint32_t>(bits << (even - 24)))
                 : FloorDivide(value, std::int64_t{1} << (24 - even));
  const auto offset = static_cast<std::size_t>(900 + (normalized - 64) * 2);
  const unsigned halfword =
      lookup.at(offset) | (unsigned{lookup.at(offset + 1)} << 8);
  return RootParts{ShortBits(halfword), static_cast<int>((31 - even) / 2)};
}

struct NormalizedVector {
  unsigned squared;
  std::array<int, 3> components;
};

NormalizedVector ReferenceNormalize(const VECTOR& input,
                                    const LookupBytes& lookup) {
  const std::array components{ShortBits(static_cast<unsigned>(input.vx)),
                              ShortBits(static_cast<unsigned>(input.vy)),
                              ShortBits(static_cast<unsigned>(input.vz))};
  unsigned squared = 0;
  for (int value : components) squared += static_cast<unsigned>(value * value);
  NormalizedVector result{squared, {}};
  if (squared == 0) return result;
  const auto parts =
      ReferenceParts(std::bit_cast<std::int32_t>(squared), lookup);
  for (std::size_t axis = 0; axis < components.size(); ++axis) {
    result.components[axis] = static_cast<int>(
        FloorDivide(std::int64_t{components[axis]} * parts.mantissa,
                    std::int64_t{1} << parts.exponent));
  }
  return result;
}

void CheckParts(int input, const LookupBytes& lookup) {
  const auto expected = ReferenceParts(input, lookup);
  int mantissa = -77;
  int exponent = -88;
  Core_Math_InverseSquareRootParts(input, &mantissa, &exponent);
  CHECK(mantissa == expected.mantissa);
  CHECK(exponent == expected.exponent);
}

void CheckNormalization(const VECTOR& input, const LookupBytes& lookup) {
  const auto expected = ReferenceNormalize(input, lookup);
  VECTOR output{111, 222, 333, 55};
  CHECK(Core_Math_NormalizeVectorRegisters(&input, &output) ==
        expected.squared);
  CHECK(output.vx == expected.components[0]);
  CHECK(output.vy == expected.components[1]);
  CHECK(output.vz == expected.components[2]);
  CHECK(output.pad == 55);
}
}  // namespace

TEST_CASE("Source reciprocal root exposes its native input and output parts",
          "[math][source-normalization]") {
  using NativeParts = void (*)(int, int*, int*);
  CHECK((std::is_same_v<decltype(&Core_Math_InverseSquareRootParts),
                        NativeParts>));
}

TEST_CASE(
    "Source register normalization exposes native vector inputs and output",
    "[math][source-normalization]") {
  using NativeNormalize = unsigned int (*)(const VECTOR*, VECTOR*);
  CHECK((std::is_same_v<decltype(&Core_Math_NormalizeVectorRegisters),
                        NativeNormalize>));
}

TEST_CASE("Source reciprocal root covers its ordinary table and binary scales",
          "[math][source-normalization]") {
  LookupFixture fixture;
  for (unsigned index = 64; index < 256; ++index) {
    for (unsigned shift = 0; shift <= 22; shift += 2) {
      CheckParts(static_cast<int>(index << shift), fixture.lookup);
    }
  }
  CHECK(std::to_array(Core_Math_kArcTangentTable) == fixture.lookup);
}

TEST_CASE(
    "Source reciprocal root retains signed prefix and small-input lookups",
    "[math][source-normalization]") {
  LookupFixture fixture;
  constexpr std::array kInputs{
      1,    2,    3,    15,   16,  63,        64,        65,
      127,  128,  255,  256,  257, INT32_MAX, INT32_MIN, -1073741824,
      -257, -256, -255, -128, -65, -64,       -3,        -2};
  for (int input : kInputs) CheckParts(input, fixture.lookup);
}

TEST_CASE("Source reciprocal root preserves no-output and output-alias order",
          "[math][source-normalization]") {
  LookupFixture fixture;
  int mantissa = 123;
  int exponent = 456;
  Core_Math_kArcTangentTable[900] = 7;
  Core_Math_InverseSquareRootParts(0, &mantissa, &exponent);
  CHECK(mantissa == 123);
  CHECK(exponent == 456);
  Core_Math_InverseSquareRootParts(-1, &mantissa, &exponent);
  CHECK(mantissa == 123);
  CHECK(exponent == 456);
  Core_Math_kArcTangentTable[900] = fixture.lookup[900];
  Core_Math_InverseSquareRootParts(25, &mantissa, &mantissa);
  CHECK(mantissa == 3276);
}

TEST_CASE(
    "Source normalization retains the original quantized unit-vector result",
    "[math][source-normalization]") {
  LookupFixture fixture;
  VECTOR input{4000, 1000, 0, 77};
  VECTOR source{0, 0, 0, 99};
  CHECK(Core_Math_NormalizeVectorRegisters(&input, &source) == 17000000);
  CHECK(source.vx == 4000);
  CHECK(source.vy == 1000);
  CHECK(source.vz == 0);
  CHECK(source.pad == 99);
}

TEST_CASE(
    "Accepted native normalization keeps its existing ordinary-vector result",
    "[math][source-normalization]") {
  LookupFixture fixture;
  VECTOR input{4000, 1000, 0, 77};
  VECTOR native{0, 0, 0, 55};
  CHECK(Core_Gte_VectorNormal(&input, &native) == 17000000);
  CHECK(native.vx == 3973);
  CHECK(native.vy == 993);
  CHECK(native.vz == 0);
  CHECK(native.pad == 55);
}

TEST_CASE("Accepted native normalization keeps its full-range 32-bit contract",
          "[math][source-normalization]") {
  VECTOR input{INT32_MAX, INT32_MAX, INT32_MAX, 77};
  VECTOR native{};
  CHECK(Core_Gte_VectorNormal(&input, &native) == 3);
  CHECK(native.vx == 2364);
  CHECK(native.vy == 2364);
  CHECK(native.vz == 2364);
  CHECK(&Core_Math_NormalizeVector == &Core_Gte_VectorNormal);
}

TEST_CASE(
    "Source normalization covers every signed halfword and ignores high bits",
    "[math][source-normalization]") {
  LookupFixture fixture;
  for (std::uint32_t bits = 0; bits <= UINT16_MAX; ++bits) {
    const auto value =
        std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(bits));
    CheckNormalization(VECTOR{value, 127, -63, 77}, fixture.lookup);
    CheckNormalization(
        VECTOR{std::bit_cast<std::int32_t>(bits | 0xA5A50000u), 0x1234007F,
               std::bit_cast<std::int32_t>(0x7FFFFFC1u), 77},
        fixture.lookup);
  }
  CHECK(std::to_array(Core_Math_kArcTangentTable) == fixture.lookup);
}

TEST_CASE("Source normalization supports in-place vector outputs",
          "[math][source-normalization]") {
  LookupFixture fixture;
  VECTOR input{3, 4, 0, 77};
  CHECK(Core_Math_NormalizeVectorRegisters(&input, &input) == 25);
  CHECK(input.vx == 2457);
  CHECK(input.vy == 3276);
  CHECK(input.vz == 0);
  CHECK(input.pad == 77);
}

TEST_CASE("Source-zero normalization does not require imported table contents",
          "[math][source-normalization]") {
  LookupFixture fixture;
  VECTOR input{65536, -65536, INT32_MIN, 99};
  Core_Math_kArcTangentTable[900] = 7;
  CHECK(Core_Math_NormalizeVectorRegisters(&input, &input) == 0);
  CHECK(input.vx == 0);
  CHECK(input.vy == 0);
  CHECK(input.vz == 0);
  CHECK(input.pad == 99);
}

TEST_CASE("Source reciprocal-root parts resolve to the typed resident entry",
          "[math][source-normalization][redirect-binding]") {
  LookupFixture fixture;
  const auto symbols =
      std::span(Content_moduleSymbols, Content_moduleSymbolCount);
  auto* root =
      VS::Content::ResolveModuleAddress(symbols, "SLUS_010.40", 0x80041A1Cu);
  REQUIRE(root != nullptr);
  const auto parts = reinterpret_cast<void (*)(int, int*, int*)>(root);
  CHECK(parts == Core_Math_InverseSquareRootParts);
  int mantissa = 0;
  int exponent = 0;
  parts(25, &mantissa, &exponent);
  CHECK(mantissa == 3276);
  CHECK(exponent == 2);
  CHECK(VS::Content::ResolveModuleAddress(symbols, "SLUS_010.40",
                                          0x80041A1Du) == nullptr);
}

TEST_CASE("Source vector normalization resolves to the typed resident entry",
          "[math][source-normalization][redirect-binding]") {
  LookupFixture fixture;
  const auto symbols =
      std::span(Content_moduleSymbols, Content_moduleSymbolCount);
  auto* normal =
      VS::Content::ResolveModuleAddress(symbols, "SLUS_010.40", 0x80041AF4u);
  REQUIRE(normal != nullptr);
  const auto normalize =
      reinterpret_cast<unsigned int (*)(const VECTOR*, VECTOR*)>(normal);
  CHECK(normalize == Core_Math_NormalizeVectorRegisters);
  VECTOR input{3, 4, 0, 77};
  CHECK(normalize(&input, &input) == 25);
  CHECK(input.vx == 2457);
  CHECK(input.vy == 3276);
  CHECK(VS::Content::ResolveModuleAddress(symbols, "SLUS_010.40",
                                          0x80041AF5u) == nullptr);
}
