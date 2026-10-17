#include "Core/FrameClock.hpp"
#include "Core/Random.hpp"

#include <catch2/catch_test_macros.hpp>

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
