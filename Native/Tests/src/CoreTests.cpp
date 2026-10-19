#include <array>
#include <bit>
#include <cstdint>
#include <limits>

#include <catch2/catch_test_macros.hpp>

#include "Core/FrameClock.hpp"
#include "Core/Math/ScalarMath.h"
#include "Core/Random.hpp"
#include "Legacy/Core/LibEtc.h"

using VS::i32;
namespace Core = VS::Core;

TEST_CASE("AI distances preserve signed GTE components and vertical offsets",
          "[core][math][distance]") {
  CHECK(Core_Math_SquaredVectorLength(-3, 4, 12) == 169);
  CHECK(Core_Math_SquaredVectorLength(0x10003, 0x10004, -0x1000C) == 169);
  CHECK(Core_Math_SquaredVectorLength(-32768, -32768, -32768) == 0xC0000000u);
  short source[3] = {10, 20, 30};
  short target[3] = {13, 24, 30};
  CHECK(Core_Math_SquaredDistanceWithHeightOffsets(source, target, 0, 0) == 25);
  CHECK(Core_Math_SquaredDistanceWithHeightOffsets(source, target, 9, 3) == 13);
}

TEST_CASE("Rand15 reproduces the BIOS rand() sequence for seed 1",
          "[core][random]") {
  Core::Rand15 r(1);
  CHECK(r.Next() == 16838);
  CHECK(r.Next() == 5758);
  CHECK(r.Next() == 10113);
  CHECK(r.Next() == 17515);
}

TEST_CASE("Random::Next stays in range and is deterministic",
          "[core][random]") {
  Core::Random a;
  Core::Random b;
  for (int i = 0; i < 10000; ++i) {
    const i32 max = 1 + (i % 300);
    const i32 va = a.Next(max);
    REQUIRE(va >= 0);
    REQUIRE(va < max);
    REQUIRE(va == b.Next(max));
  }
}

TEST_CASE("Random::Reset restarts the sequence", "[core][random]") {
  Core::Random a;
  const i32 first = a.Next(1000);
  a.Next(1000);
  a.Reset();
  CHECK(a.Next(1000) == first);
}

TEST_CASE("FrameClock advances at NTSC field rate", "[core][clock]") {
  Core::FrameClock c;
  CHECK(c.Advance(1.0 / Core::FrameClock::TicksPerSecond + 1e-9) == 1);
  CHECK(c.Advance(0.0) == 0);
  CHECK(c.Advance(1.0, 4) == 4);  // stall: capped, backlog dropped
  CHECK(c.Advance(0.0) == 0);
  CHECK(c.Ticks() == 5);
}

TEST_CASE(
    "Sound counter callbacks advance at 240 Hz and respect event lifecycle",
    "[core][clock][audio]") {
  static int calls = 0;
  calls = 0;
  REQUIRE(SetRCnt(RCntCNT2, 0x44E8, RCntMdINTR) == 1);
  REQUIRE(StartRCnt(RCntCNT2) == 1);
  const long event = OpenEvent(RCntCNT2, EvSpINT, EvMdINTR, []() -> long {
    ++calls;
    return 0;
  });
  REQUIRE(event == 1);
  Legacy_AdvanceCounter2(70560);
  CHECK(calls == 0);
  EnableEvent(event);
  Legacy_AdvanceCounter2(70560);
  CHECK(calls == 4);
  DisableEvent(event);
  Legacy_AdvanceCounter2(70560);
  CHECK(calls == 4);
  EnableEvent(event);
  Legacy_AdvanceCounter2(35280);
  CHECK(calls == 6);
  StopRCnt(RCntCNT2);
  Legacy_AdvanceCounter2(70560);
  CHECK(calls == 6);
  StartRCnt(RCntCNT2);
  CloseEvent(event);
  Legacy_AdvanceCounter2(70560);
  CHECK(calls == 6);
  StopRCnt(RCntCNT2);
}

TEST_CASE("Fixed multiply preserves the original signed product bits",
          "[core][math]") {
  for (int left :
       {0, 1, -1, 4096, -4096, 1234567, std::numeric_limits<int>::max(),
        std::numeric_limits<int>::min()}) {
    for (int right : {0, 1, -1, 4096, -4096, 7654321}) {
      const auto product =
          static_cast<std::uint64_t>(static_cast<std::int64_t>(left) * right);
      const auto expected =
          std::bit_cast<int>(static_cast<std::uint32_t>(product >> 12));
      CHECK(Core_Math_MultiplyFixed(left, right) == expected);
    }
  }
}

TEST_CASE("Integer square root retains the original normalized lookup rounding",
          "[core][math]") {
  CHECK(Core_Math_SquareRootInteger(-1) == 0);
  CHECK(Core_Math_SquareRootInteger(0) == 0);
  CHECK(Core_Math_SquareRootInteger(1) == 1);
  CHECK(Core_Math_SquareRootInteger(4) == 2);
  CHECK(Core_Math_SquareRootInteger(255) == 15);
  CHECK(Core_Math_SquareRootInteger(257) == 16);
  CHECK(Core_Math_SquareRootInteger(65025) == 254);
  CHECK(Core_Math_SquareRootInteger(65536) == 256);
  CHECK(Core_Math_SquareRootInteger(std::numeric_limits<int>::max()) == 46152);
}

TEST_CASE("Rotation multiplication preserves translation and supports aliasing",
          "[core][math]") {
  MATRIX left{};
  left.m[0][2] = 4096;
  left.m[1][1] = 4096;
  left.m[2][0] = -4096;
  left.t[0] = 17;
  Core_Math_MultiplyMatrixIntoRight(&left, &left);
  CHECK(left.m[0][0] == -4096);
  CHECK(left.m[1][1] == 4096);
  CHECK(left.m[2][2] == -4096);
  CHECK(left.m[0][2] == 0);
  CHECK(left.t[0] == 17);
  MATRIX right{};
  left.m[0][0] = 32767;
  left.m[1][1] = -32768;
  right.m[0][0] = right.m[1][1] = 32767;
  right.t[2] = -81;
  Core_Math_MultiplyMatrixIntoRight(&left, &right);
  CHECK(right.m[0][0] == 32767);
  CHECK(right.m[1][1] == -32768);
  CHECK(right.t[2] == -81);
}

TEST_CASE("Actor Euler rotation and column scaling preserve translation",
          "[core][math]") {
  constexpr std::array<std::int16_t, 5> kRotated{0, -4096, 0, 4096, -4096};
  constexpr std::array<std::int16_t, 3> kScaled{4096, 2048, -8192};
  SVECTOR rotation{0, 1024, 1024, 0};
  MATRIX matrix{};
  matrix.t[0] = 17;
  Core_Math_BuildEulerRotationMatrixQuadrant(&rotation, &matrix);
  const std::array rotated{matrix.m[0][0], matrix.m[0][1], matrix.m[0][2],
                           matrix.m[1][2], matrix.m[2][0]};
  CHECK(rotated == kRotated);
  VECTOR scale{8192, -4096, 2048, 0};
  CHECK(Core_Math_ScaleMatrixColumns(&matrix, &scale) == &matrix);
  const std::array scaled{matrix.m[0][1], matrix.m[1][2], matrix.m[2][0]};
  CHECK(scaled == kScaled);
  CHECK(matrix.t[0] == 17);
}

TEST_CASE("Short and wide matrix inputs share signed and alias-safe arithmetic",
          "[core][math][gte]") {
  constexpr std::int16_t kFixedOne = 4096;
  constexpr std::int16_t kFixedHalf = 2048;
  constexpr std::int16_t kFixedQuarter = 1024;
  constexpr SVECTOR kInput{-7, 3, 12, 99};
  constexpr VECTOR kExpected{-6, 10, 10, 53};
  constexpr VECTOR kWideExpected{kExpected.vx, kExpected.vy, kExpected.vz, 71};
  MATRIX matrix{};
  matrix.m[0][0] = kFixedOne;
  matrix.m[0][1] = kFixedHalf;
  matrix.m[1][0] = -kFixedOne;
  matrix.m[1][2] = kFixedQuarter;
  matrix.m[2][1] = -kFixedHalf;
  matrix.m[2][2] = kFixedOne;
  SVECTOR narrow = kInput;
  VECTOR wide{narrow.vx, narrow.vy, narrow.vz, kWideExpected.pad};
  VECTOR result{0, 0, 0, kExpected.pad};
  constexpr std::array kExpectedValues{kExpected.vx, kExpected.vy, kExpected.vz,
                                       kExpected.pad};
  constexpr std::array kWideExpectedValues{kWideExpected.vx, kWideExpected.vy,
                                           kWideExpected.vz, kWideExpected.pad};
  CHECK(Core_Gte_ApplyMatrix(&matrix, &narrow, &result) == &result);
  const std::array values{result.vx, result.vy, result.vz, result.pad};
  CHECK(values == kExpectedValues);
  CHECK(Core_Gte_ApplyMatrixLV(&matrix, &wide, &wide) == &wide);
  const std::array wide_values{wide.vx, wide.vy, wide.vz, wide.pad};
  CHECK(wide_values == kWideExpectedValues);
}

TEST_CASE("Shuffle RNG preserves 32-bit product wrap at range boundaries",
          "[core][random]") {
  Core::Random reference;
  Core::Random actual;
  constexpr int kUnscaledRange = 1 << Core::Rand15::kValueBits;
  const std::array ranges{1,          65536, std::numeric_limits<int>::max(),
                          0x12345678, -1,    std::numeric_limits<int>::min()};
  for (int round = 0; round < 20; ++round) {
    for (const int range : ranges) {
      const int picked = reference.Next(kUnscaledRange);
      const auto product = static_cast<std::uint32_t>(picked) *
                           static_cast<std::uint32_t>(range);
      int expected = std::bit_cast<std::int32_t>(product);
      if (expected < 0) expected += Core::Rand15::kMaximum;
      expected >>= Core::Rand15::kValueBits;
      CHECK(actual.Next(range) == expected);
    }
  }
}

TEST_CASE("Signed vector components preserve their low sixteen bits",
          "[core][math]") {
  for (const std::uint32_t value :
       {0u, 0x7FFFu, 0x8000u, 0xFFFFu, 0x12348001u, UINT32_MAX}) {
    const auto expected =
        std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(value));
    CHECK(Core_Math_WrapSigned16(value) == expected);
  }
}
