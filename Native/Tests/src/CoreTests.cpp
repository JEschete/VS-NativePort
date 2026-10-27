#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cstdint>
#include <limits>
#include <thread>

#include <catch2/catch_test_macros.hpp>

#include "Core/BiosRand.h"
#include "Core/FrameClock.hpp"
#include "Core/FrameTiming.h"
#include "Core/Math/ScalarMath.h"
#include "Core/Random.hpp"
#include "Legacy/Core/LibEtc.h"

extern "C" {
#include "Core/Math/FixedPoint.h"
#include "Core/Math/Interpolation.h"
#include "Core/Math/Matrix.h"
#include "Core/Math/Trig.h"
#include "Core/Math/Vector.h"
#include "Core/Random.h"
}

using VS::i32;
namespace Core = VS::Core;

namespace {
template <typename VectorType>
void CheckVectorComponents(const VectorType& actual,
                           const VectorType& expected) {
  CHECK(actual.vx == expected.vx);
  CHECK(actual.vy == expected.vy);
  CHECK(actual.vz == expected.vz);
  CHECK(actual.pad == expected.pad);
}

void CheckRotationDiagonal(const MATRIX& matrix, int x, int y, int z) {
  const std::array<int, 9> actual{
      matrix.m[0][0], matrix.m[0][1], matrix.m[0][2],
      matrix.m[1][0], matrix.m[1][1], matrix.m[1][2],
      matrix.m[2][0], matrix.m[2][1], matrix.m[2][2]};
  const std::array<int, 9> expected{x, 0, 0, 0, y, 0, 0, 0, z};
  CHECK(actual == expected);
}

struct OriginalRandomState {
  decltype(std::to_array(Core_randomShuffleTable)) table =
      std::to_array(Core_randomShuffleTable);
  int index = Core_randomShuffleIndex;
  unsigned int seed = Core_Random_State();
  unsigned int interface_state = Core_interfaceRandomState;

  ~OriginalRandomState() {
    std::copy(table.begin(), table.end(), Core_randomShuffleTable);
    Core_randomShuffleIndex = index;
    Core_Random_Seed(seed);
    Core_interfaceRandomState = interface_state;
  }
};
}  // namespace

TEST_CASE("AI distances preserve signed GTE components and vertical offsets",
          "[core][math][distance]") {
  CHECK(Core_Math_SquaredVectorLength(-3, 4, 12) == 169);
  CHECK(Core_Math_SquaredVectorLength(0x10003, 0x10004, -0x1000C) == 169);
  CHECK(Core_Math_SquaredVectorLength(-32768, -32768, -32768) == 0xC0000000u);
  std::int16_t source[3] = {10, 20, 30};
  std::int16_t target[3] = {13, 24, 30};
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
  const auto event = OpenEvent(RCntCNT2, EvSpINT, EvMdINTR, [] {
    ++calls;
    return 0L;
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

TEST_CASE("VSync keeps the sound counter at 240 Hz across NTSC fields",
          "[core][clock][audio][audio-clock]") {
  static int calls = 0;
  calls = 0;
  REQUIRE(SetRCnt(RCntCNT2, 17640, RCntMdINTR) == 1);
  REQUIRE(StartRCnt(RCntCNT2) == 1);
  const auto event = OpenEvent(RCntCNT2, EvSpINT, EvMdINTR, [] {
    ++calls;
    return 0L;
  });
  REQUIRE(event == 1);
  EnableEvent(event);
  const std::array<std::array<int, 2>, 3> intervals{
      {{250, 1001}, {750, 4004}, {59000, 240240}}};
  for (const auto& [fields, expected] : intervals) {
    VSync(fields);
    CHECK(calls == expected);
  }
  StopRCnt(RCntCNT2);
  VSync(1000);
  CHECK(calls == 240240);
  StartRCnt(RCntCNT2);
  VSync(250);
  CHECK(calls == 241241);
  SetRCnt(RCntCNT2, 17640, RCntMdINTR);
  VSync(250);
  CHECK(calls == 242242);
  CloseEvent(event);
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

TEST_CASE("Vector normalization handles full-range squared lengths",
          "[core][math][normal]") {
  for (const std::int32_t component :
       {std::numeric_limits<std::int32_t>::max(),
        std::numeric_limits<std::int32_t>::min()}) {
    VECTOR vector{component, component, component, 7};
    const std::int64_t wide = component;
    const std::uint64_t squared = static_cast<std::uint64_t>(wide * wide) * 3;
    const auto result = Core_Gte_VectorNormal(&vector, &vector);
    const int expected = component > 0 ? 2364 : -2364;
    const std::array actual{vector.vx, vector.vy, vector.vz, vector.pad};
    const std::array<decltype(vector.vx), 4> expected_values{expected, expected,
                                                             expected, 7};
    CHECK(actual == expected_values);
    CHECK(static_cast<std::uint32_t>(result) ==
          static_cast<std::uint32_t>(squared));
  }
  VECTOR source{3, 4, 0, 0};
  VECTOR output{};
  CHECK(Core_Gte_VectorNormal(&source, &output) == 25);
  const std::array values{output.vx, output.vy, output.vz};
  constexpr std::array<decltype(output.vx), 3> kExpected{2457, 3276, 0};
  CHECK(values == kExpected);
  source = {};
  CHECK(Core_Gte_VectorNormal(&source, &output) == 0);
  const std::array zero_values{output.vx, output.vy, output.vz};
  constexpr decltype(zero_values) kZero{};
  CHECK(zero_values == kZero);
}

TEST_CASE("Frame work timing preserves source units and halfword wrap",
          "[core][clock][frame-budget]") {
  using Nanoseconds = std::chrono::nanoseconds;
  CHECK(Core::FrameCounterTicks(Nanoseconds{0}) == 0);
  CHECK(Core::FrameCounterTicks(Nanoseconds{63556}) == 0);
  CHECK(Core::FrameCounterTicks(Nanoseconds{63557}) == 1);
  CHECK(Core::FrameCounterTicks(std::chrono::seconds{1}) == 15734);
  CHECK(Core::FrameCounterTicks(std::chrono::seconds{5}) == 13134);
  for (std::int64_t seconds = 0; seconds < 65537; ++seconds) {
    CHECK(Core::FrameCounterTicks(std::chrono::seconds{seconds}) ==
          (seconds * 15734) % 65536);
  }
  const auto maximum = std::numeric_limits<std::int64_t>::max();
  const auto expected = ((maximum / 1000000000 % 65536) * 15734 +
                         (maximum % 1000000000) * 15734 / 1000000000) %
                        65536;
  CHECK(Core::FrameCounterTicks(Nanoseconds{maximum}) == expected);
}

TEST_CASE("Frame work baseline resets at actual logical frame boundaries",
          "[core][clock][frame-budget]") {
  Core_ResetFrameTimingBaseline();
  std::this_thread::sleep_for(std::chrono::milliseconds{2});
  CHECK(Core_ElapsedFrameCounterTicks() >= 1);
  const auto before = std::chrono::steady_clock::now();
  VSync(0);
  const auto ticks = Core_ElapsedFrameCounterTicks();
  const auto duration = std::chrono::steady_clock::now() - before;
  CHECK(ticks >= 0);
  CHECK(ticks <=
        Core::FrameCounterTicks(
            std::chrono::duration_cast<std::chrono::nanoseconds>(duration)) +
            1);
}

TEST_CASE("Copied shuffled random draws match the native sequence",
          "[core][random]") {
  OriginalRandomState state;
  Core_Random_InitializeShuffleTable();
  Core::Random expected;
  for (int maximum : {0, 1, 2, 255, 1024, 65536}) {
    CHECK(Core_Random_NextBounded(maximum) == expected.Next(maximum));
    CHECK(Core_Random_NextAveraged(maximum) == expected.NextSmoothed(maximum));
    CHECK(Core_Random_Rand() == expected.Raw());
  }
}

TEST_CASE("Original random ranges preserve unordered and equal bounds",
          "[core][random]") {
  OriginalRandomState state;
  constexpr std::array<std::array<int, 2>, 8> kBounds{{{0, 0},
                                                       {-3, -3},
                                                       {2, 10},
                                                       {10, 2},
                                                       {-8, -2},
                                                       {-2, -8},
                                                       {-7, 5},
                                                       {5, -7}}};
  for (const auto& [first, second] : kBounds) {
    Core_Random_Seed(1);
    Core::Rand15 expected(1);
    const auto [low, high] = std::minmax(first, second);
    for (int sample = 0; sample < 64; ++sample) {
      const auto value =
          low == high ? low : low + expected.Next() % (high - low);
      CHECK(Core_Random_RandomRangeExclusive(first, second) == value);
      CHECK(Core_Random_State() == expected.State());
    }
  }
}

TEST_CASE("Interface random requests return high bits of the previous state",
          "[core][random]") {
  OriginalRandomState state;
  constexpr unsigned int kInitial = 0x81234567;
  for (int bits : {1, 8, 10, 31, 32}) {
    Core_interfaceRandomState = kInitial;
    CHECK(Core_Random_NextInterfaceRandomBits(bits) ==
          (kInitial >> (32 - bits)));
    CHECK(Core_interfaceRandomState == kInitial * 0x19660Du);
  }
}

TEST_CASE("Vector arithmetic preserves destination order and aliases",
          "[core][math][semantic-names]") {
  VECTOR left{7, -3, 11, 101};
  VECTOR right{-2, 5, -4, 102};
  VECTOR result{0, 0, 0, 103};
  CHECK(Core_Math_AddVectors(&result, &left, &right) == &result);
  CheckVectorComponents(result, VECTOR{5, 2, 7, 103});
  CHECK(Core_Math_SubtractVectors(&left, &left, &right) == &left);
  CheckVectorComponents(left, VECTOR{9, -8, 15, 101});
  CHECK(Core_Math_CopyVector(&result, &right) == &result);
  CheckVectorComponents(result, VECTOR{-2, 5, -4, 103});
}

TEST_CASE("Wide and short vector helpers preserve widths and output positions",
          "[core][math][semantic-names]") {
  SVECTOR narrow{2, -3, 4, 91};
  VECTOR wide{9, 8, 7, 93};
  VECTOR result{0, 0, 0, 95};
  Core_Math_AddWideVectors(&wide, &wide, &result);
  CheckVectorComponents(result, VECTOR{18, 16, 14, 95});
  Core_Math_AddWideAndShortVectors(&wide, &narrow, &wide);
  CheckVectorComponents(wide, VECTOR{11, 5, 11, 93});
  Core_Math_WidenShortVector(&narrow, &result);
  CheckVectorComponents(result, VECTOR{2, -3, 4, 95});
  wide = {-65535, 65534, 98304, 93};
  Core_Math_NarrowWideVector(&wide, &narrow);
  CheckVectorComponents(narrow, SVECTOR{1, -2, -32768, 91});
}

TEST_CASE("Fixed vectors preserve scaling and destination padding",
          "[core][math][semantic-names]") {
  SVECTOR source{2, -3, 4, 91};
  VECTOR result{0, 0, 0, 95};
  CHECK(Core_Math_ShortVectorToFixed(&result, &source) == &result);
  CheckVectorComponents(result, VECTOR{8192, -12288, 16384, 95});
  CHECK(Core_Math_ScaleFixedVector(&result, &result, 2048) == &result);
  CheckVectorComponents(result, VECTOR{4096, -6144, 8192, 95});
  VECTOR magnitude{3 * 4096, 4 * 4096, 0, 73};
  CHECK(Core_Math_FixedVectorMagnitude(&magnitude) == 5 * 4096);
  VECTOR direction{4096, 0, 0, 71};
  Core_Math_NormalizeFixedVector(&direction, &result);
  CheckVectorComponents(result, VECTOR{4096, 0, 0, 95});
}

TEST_CASE("Signed clamp preserves unordered bound semantics",
          "[core][math][semantic-names]") {
  for (const int first : {-7, 3}) {
    const int second = first == -7 ? 3 : -7;
    CHECK(Core_Math_ClampSignedShort(-8, first, second) == -7);
    CHECK(Core_Math_ClampSignedShort(-7, first, second) == -7);
    CHECK(Core_Math_ClampSignedShort(0, first, second) == 0);
    CHECK(Core_Math_ClampSignedShort(3, first, second) == 3);
    CHECK(Core_Math_ClampSignedShort(4, first, second) == 3);
  }
  CHECK(Core_Math_ClampSignedShort(5, 2, 2) == 2);
}

TEST_CASE("Magnitude selection retains signs and first-component ties",
          "[core][math][semantic-names]") {
  CHECK(Core_Math_GreatestMagnitudeComponent(-7, 7, 3) == -7);
  CHECK(Core_Math_GreatestMagnitudeComponent(3, -7, 7) == -7);
  CHECK(Core_Math_GreatestMagnitudeComponent(-1, 2, -3) == -3);
  CHECK(Core_Math_GreatestMagnitudeComponent(0, 0, 0) == 0);
  CHECK(Core_Math_LargerMagnitude(-7, 7) == -7);
  CHECK(Core_Math_LargerMagnitude(1, -3) == -3);
}

TEST_CASE("Packed screen addition does not carry between coordinates",
          "[core][math][semantic-names]") {
  CHECK(Core_Math_AddPackedScreenCoordinates(0x0001FFFF, 0x00020001) ==
        0x00030000);
  CHECK(Core_Math_AddPackedScreenCoordinates(std::bit_cast<int>(0xFFFF0001u),
                                             0x00010002) == 3);
}

TEST_CASE("Look-at orientation preserves its negative vertical reference",
          "[core][math][semantic-names]") {
  VECTOR from{10, 20, 30, 0};
  VECTOR to{10, 20, 40, 0};
  MATRIX matrix{};
  matrix.t[0] = 77;
  matrix.t[1] = 88;
  matrix.t[2] = 99;
  Core_Math_BuildLookAtOrientation(&from, &to, &matrix);
  CheckRotationDiagonal(matrix, 4096, -4096, 4096);
  CHECK(matrix.t[0] == 77);
  CHECK(matrix.t[1] == 88);
  CHECK(matrix.t[2] == 99);
}

TEST_CASE("Direction rotations and fixed headings preserve angle units",
          "[core][math][semantic-names]") {
  VECTOR direction{0, 0, 4096, 0};
  MATRIX matrix{};
  matrix.t[0] = 73;
  Core_Math_DirectionRotationMatrix(&direction, &matrix);
  CheckRotationDiagonal(matrix, 4096, 4096, 4096);
  CHECK(matrix.t[0] == 73);
  CHECK(Core_Math_FixedAtan2(0, 4096) == 0);
  CHECK(Core_Math_FixedAtan2(4096, 0) == 1024);
  CHECK(Core_Math_FixedAtan2(-4096, 0) == -1024);
  CHECK(Core_Math_FixedAtan2(4096, 4096) == 512);
}

TEST_CASE("Identity and translation setters preserve their separate contracts",
          "[core][math][semantic-names]") {
  MATRIX matrix{};
  matrix.t[0] = 73;
  Core_Math_SetIdentityRotation(&matrix);
  CheckRotationDiagonal(matrix, 4096, 4096, 4096);
  CHECK(matrix.t[0] == 73);
  SVECTOR translation{-3, 17, 29, 0};
  Core_Math_SetTranslationMatrix(&translation, &matrix);
  CheckRotationDiagonal(matrix, 4096, 4096, 4096);
  CHECK(matrix.t[0] == -3);
  CHECK(matrix.t[1] == 17);
  CHECK(matrix.t[2] == 29);
}

TEST_CASE("Interpolation weights preserve distinct scales and signed rounding",
          "[core][math][interpolation-names]") {
  CHECK(Core_Math_InterpolateSevenBitScalar(10, -10, 0) == 10);
  CHECK(Core_Math_InterpolateSevenBitScalar(10, -10, 64) == 0);
  CHECK(Core_Math_InterpolateSevenBitScalar(10, -10, 128) == -10);
  std::array<std::int16_t, 2> endpoints{10, -11};
  CHECK(Core_Math_InterpolateShortPair(endpoints.data(), 64) == -1);
  CHECK(Core_Math_InterpolateThirteenBitScalar(10, -11, 4096) == 0);
  CHECK(Core_Math_InterpolateFixedPointRatio(10, -11, 1, 2) == 0);
  CHECK(Core_Math_InterpolateFixedPointRatio(0, 8192, 1, 3) == 2730);
}

TEST_CASE("Timed scalar interpolation keeps duration before elapsed steps",
          "[core][math][interpolation-names]") {
  CHECK(Core_Math_InterpolateTimedScalar(10, -10, 8, 2) == 5);
  for (const auto interpolate : {Core_Math_InterpolateCosineScalar,
                                 Core_Math_InterpolateCosineTimedScalar}) {
    CHECK(interpolate(10, -10, 8, 0) == 10);
    CHECK(interpolate(10, -10, 8, 2) == 8);
    CHECK(interpolate(10, -10, 8, 4) == 0);
    CHECK(interpolate(10, -10, 8, 8) == -10);
  }
}

TEST_CASE("Short-vector interpolation retains aliasing and fixed-point phases",
          "[core][math][interpolation-names]") {
  SVECTOR start{0, 32, -32, 71};
  SVECTOR end{16, -16, 48, 73};
  Core_Math_InterpolateThirteenBitShortVector(&start, &end, 4096, &start);
  CheckVectorComponents(start, SVECTOR{8, 8, 8, 71});
  start = {0, 32, -32, 71};
  Core_Math_InterpolateTimedShortVector(&start, &end, 8, 2, &start);
  CheckVectorComponents(start, SVECTOR{4, 20, -12, 71});
  start = {0, 32, -32, 71};
  Core_Math_InterpolateCosineShortVector(&start, &end, 8, 2, &start);
  CheckVectorComponents(start, SVECTOR{2, 25, -21, 71});
}

TEST_CASE("Wide-vector interpolation preserves units and destination padding",
          "[core][math][interpolation-names]") {
  VECTOR start{0, 32, -32, 71};
  VECTOR end{16, -16, 48, 73};
  Core_Math_InterpolateThirteenBitWideVector(&start, &end, 4096, &start);
  CheckVectorComponents(start, VECTOR{8, 8, 8, 71});
  start = {0, 32, -32, 71};
  Core_Math_InterpolateTimedWideVector(&start, &end, 8, 2, &start);
  CheckVectorComponents(start, VECTOR{4, 20, -12, 71});
  start = {0, 32, -32, 71};
  Core_Math_InterpolateCosineWideVector(&start, &end, 8, 2, &start);
  CheckVectorComponents(start, VECTOR{2, 25, -21, 71});
}

TEST_CASE("Lagrange interpolation preserves original fixed-point coefficients",
          "[core][math][interpolation-names]") {
  SVECTOR point0{8, -16, 24, 0};
  SVECTOR point1{16, -8, 32, 0};
  SVECTOR point2{32, 24, -16, 0};
  SVECTOR point3{64, -32, 8, 0};
  constexpr std::array<std::array<int, 4>, 5> kSamples{{{-4096, 7, -16, 23},
                                                        {0, 16, -8, 32},
                                                        {2048, 22, 12, 7},
                                                        {4096, 32, 24, -16},
                                                        {8192, 63, -32, 7}}};
  for (const auto& [phase, x, y, z] : kSamples) {
    SVECTOR result{0, 0, 0, 75};
    Core_Math_InterpolateLagrangeShortVector(&point0, &point1, &point2, &point3,
                                             phase, &result);
    CheckVectorComponents(result, SVECTOR{static_cast<std::int16_t>(x),
                                          static_cast<std::int16_t>(y),
                                          static_cast<std::int16_t>(z), 75});
    int scalar = 0;
    Core_Math_InterpolateLagrangeScalar(8, 16, 32, 64, phase, &scalar);
    CHECK(scalar == x);
  }
}
