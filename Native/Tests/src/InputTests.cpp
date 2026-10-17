#include "Input/Pad.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace VS;

TEST_CASE("Pad edges: pressed and released are single-tick", "[input]")
{
    Input::InputSystem in;
    in.Step(Input::Cross, 1);
    CHECK(in.State().pressed == Input::Cross);
    CHECK(in.State().held == Input::Cross);
    in.Step(Input::Cross, 1);
    CHECK(in.State().pressed == 0);
    in.Step(0, 1);
    CHECK(in.State().released == Input::Cross);
}

TEST_CASE("Pad repeat: first press, then after 20 ticks every 4", "[input]")
{
    Input::InputSystem in;
    int repeats = 0;
    int firstRepeatTick = -1;
    for (int t = 1; t <= 28; ++t) {
        in.Step(Input::Up, 1);
        if (in.State().repeat & Input::Up) {
            ++repeats;
            if (t > 1 && firstRepeatTick < 0) {
                firstRepeatTick = t;
            }
        }
    }
    CHECK(firstRepeatTick == 20);
    CHECK(repeats == 1 + 3);  // tick 1 (press), 20, 24, 28
}
