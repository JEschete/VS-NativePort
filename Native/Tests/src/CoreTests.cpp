#include "Core/FrameClock.hpp"
#include "Core/Math/ScalarMath.h"
#include "Core/Random.hpp"
#include "Legacy/Core/LibEtc.h"

#include <catch2/catch_test_macros.hpp>

#include <bit>
#include <cstdint>
#include <limits>

using namespace VS;

TEST_CASE("Rand15 reproduces the BIOS rand() sequence for seed 1", "[core][random]")
{
    Core::Rand15 r(1);
    CHECK(r.Next() == 16838);
    CHECK(r.Next() == 5758);
    CHECK(r.Next() == 10113);
    CHECK(r.Next() == 17515);
}

TEST_CASE("Random::Next stays in range and is deterministic", "[core][random]")
{
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

TEST_CASE("Random::Reset restarts the sequence", "[core][random]")
{
    Core::Random a;
    const i32 first = a.Next(1000);
    a.Next(1000);
    a.Reset();
    CHECK(a.Next(1000) == first);
}

TEST_CASE("FrameClock advances at NTSC field rate", "[core][clock]")
{
    Core::FrameClock c;
    CHECK(c.Advance(1.0 / Core::FrameClock::TicksPerSecond + 1e-9) == 1);
    CHECK(c.Advance(0.0) == 0);
    CHECK(c.Advance(1.0, 4) == 4);  // stall: capped, backlog dropped
    CHECK(c.Advance(0.0) == 0);
    CHECK(c.Ticks() == 5);
}

TEST_CASE("Sound counter callbacks advance at 240 Hz and respect event lifecycle", "[core][clock][audio]")
{
    static int calls = 0;
    calls = 0;
    REQUIRE(SetRCnt(RCntCNT2, 0x44E8, RCntMdINTR) == 1);
    REQUIRE(StartRCnt(RCntCNT2) == 1);
    const long event = OpenEvent(RCntCNT2, EvSpINT, EvMdINTR, []() -> long { ++calls; return 0; });
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

TEST_CASE("Fixed multiply preserves the original signed product bits", "[core][math]")
{
    for (int left : {0, 1, -1, 4096, -4096, 1234567, std::numeric_limits<int>::max(), std::numeric_limits<int>::min()}) {
        for (int right : {0, 1, -1, 4096, -4096, 7654321}) {
            const auto product = static_cast<std::uint64_t>(static_cast<std::int64_t>(left) * right);
            const auto expected = std::bit_cast<int>(static_cast<std::uint32_t>(product >> 12));
            CHECK(Core_Math_MultiplyFixed(left, right) == expected);
        }
    }
}

TEST_CASE("Integer square root retains the original normalized lookup rounding", "[core][math]")
{
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

TEST_CASE("Rotation multiplication preserves translation and supports aliasing", "[core][math]")
{
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

TEST_CASE("Actor Euler rotation uses ZYX order and column scaling preserves translation", "[core][math]")
{
    SVECTOR rotation{0, 1024, 1024, 0};
    MATRIX matrix{};
    matrix.t[0] = 17;
    Core_Math_BuildEulerRotationMatrixQuadrant(&rotation, &matrix);
    CHECK(matrix.m[0][0] == 0);
    CHECK(matrix.m[0][1] == -4096);
    CHECK(matrix.m[0][2] == 0);
    CHECK(matrix.m[1][2] == 4096);
    CHECK(matrix.m[2][0] == -4096);
    VECTOR scale{8192, -4096, 2048, 0};
    CHECK(Core_Math_ScaleMatrixColumns(&matrix, &scale) == &matrix);
    CHECK(matrix.m[0][1] == 4096);
    CHECK(matrix.m[1][2] == 2048);
    CHECK(matrix.m[2][0] == -8192);
    CHECK(matrix.t[0] == 17);
}
