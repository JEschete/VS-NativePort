#include <catch2/catch_test_macros.hpp>

extern "C" int Tests_CombatTargetResults(void);

TEST_CASE(
    "Combat sandbox applies matching target HP and MP effects with clamping",
    "[combat][sandbox]") {
  REQUIRE(Tests_CombatTargetResults() == 1);
}
