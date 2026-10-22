#include <iterator>

#include <catch2/catch_test_macros.hpp>

#include "Legacy/Psx.h"

extern "C" {
#include "Effects/EffectResources.h"
#include "Effects/PurgeTables.h"
#include "Effects/Templates/Ring.h"
}

extern "C" int Tests_CombatTargetResults(void);

TEST_CASE(
    "Combat sandbox applies matching target HP and MP effects with clamping",
    "[combat][sandbox]") {
  REQUIRE(Tests_CombatTargetResults() == 1);
}

TEST_CASE("Stat item effects bind plugin-local renderers, not EFFPURGE",
          "[combat][items][effects][item-plugin]") {
  const auto count = Effects_effectEntryPointIndices
      [std::size(Effects_effectEntryPointIndices) - 1];
  for (unsigned slot = 0; slot < count; ++slot) {
    CAPTURE(slot);
    CHECK(Effects_effectEntryPoints[slot] != Effects_InvokeAllPluginUpdates);
  }
  CHECK(Effects_effectEntryPoints[Effects_effectEntryPointIndices[198]] ==
        Effects_Plg198_Ring);
  CHECK(Effects_effectEntryPoints[Effects_effectEntryPointIndices[199]] ==
        Effects_Plg198_Ring);
}
