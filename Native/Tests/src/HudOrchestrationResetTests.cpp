#include <algorithm>
#include <cstddef>
#include <span>

#include <catch2/catch_test_macros.hpp>

#include "Battle/HudOrchestrationLive.hpp"
#include "Battle/HudOrchestrationLiveFixture.hpp"

namespace {
namespace b = VS::Battle;
namespace f = hud_orchestration_live_fixture;
}  // namespace

TEST_CASE("HUD feedback entry reset clears commands but preserves marker BSS",
          "[hud-orchestration-reset][F00729][F00730][F00743]") {
  f::LiveState fixture;
  f::SavedValue<Battle_floatingLabels> saved_owner;
  Battle_FloatingLabel labels[1][32]{};
  Battle_floatingLabels = labels;
  std::ranges::fill(std::span(Battle_actorMarkers).first<16>(), char{7});
  b::SetLiveActorMarkersHidden(0xFFFF);
  b::SuppressLiveAshleyStatusLabels();
  Battle_ResetFloatingLabels();
  CHECK(Battle_actorMarkersHidden == 0);
  CHECK(Battle_ashleyStatusLabelsSuppressed == 0);
  CHECK(std::ranges::all_of(std::span(Battle_actorMarkers).first<16>(),
                            [](char marker) { return marker == 7; }));
  b::SetLiveActorMarkersHidden(2);
  b::SuppressLiveAshleyStatusLabels();
  CHECK(Battle_actorMarkersHidden == 2);
  CHECK(Battle_ashleyStatusLabelsSuppressed == 1);
}

TEST_CASE("Data rebinds do not invent a feedback BSS reset hook",
          "[hud-orchestration-reset][F00729][F00743]") {
  f::LiveState fixture;
  b::SetLiveActorMarkersHidden(-123);
  b::SuppressLiveAshleyStatusLabels();
  f::ProductionTables rebound;
  CHECK(Battle_actorMarkersHidden == -123);
  CHECK(Battle_ashleyStatusLabelsSuppressed == 1);
}
