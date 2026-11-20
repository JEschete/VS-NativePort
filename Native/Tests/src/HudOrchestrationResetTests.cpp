#include <algorithm>
#include <cstddef>
#include <span>

#include <catch2/catch_test_macros.hpp>

#include "Battle/FloatingLabelQueueLive.hpp"
#include "Battle/HudOrchestrationLive.hpp"
#include "Battle/HudOrchestrationLiveFixture.hpp"

namespace {
namespace b = VS::Battle;
namespace f = hud_orchestration_live_fixture;
}  // namespace

TEST_CASE("HUD feedback entry reset clears commands but preserves marker BSS",
          "[hud-orchestration-reset][F00729][F00730][F00743]") {
  f::LiveState fixture;
  f::SavedValue<VS::Battle::floatingLabels> saved_owner;
  std::array<VS::Battle::FloatingLabelSlots, 1> labels{};
  VS::Battle::floatingLabels = labels.data();
  std::ranges::fill(std::span(VS::Battle::actorMarkers).first<16>(), char{7});
  b::SetLiveActorMarkersHidden(0xFFFF);
  b::SuppressLiveAshleyStatusLabels();
  b::ResetLiveFloatingLabels();
  CHECK(VS::Battle::actorMarkersHidden == 0);
  CHECK(VS::Battle::ashleyStatusLabelsSuppressed == 0);
  CHECK(std::ranges::all_of(std::span(VS::Battle::actorMarkers).first<16>(),
                            [](char marker) { return marker == 7; }));
  b::SetLiveActorMarkersHidden(2);
  b::SuppressLiveAshleyStatusLabels();
  CHECK(VS::Battle::actorMarkersHidden == 2);
  CHECK(VS::Battle::ashleyStatusLabelsSuppressed == 1);
}

TEST_CASE("Data rebinds do not invent a feedback BSS reset hook",
          "[hud-orchestration-reset][F00729][F00743]") {
  f::LiveState fixture;
  b::SetLiveActorMarkersHidden(-123);
  b::SuppressLiveAshleyStatusLabels();
  f::ProductionTables rebound;
  CHECK(VS::Battle::actorMarkersHidden == -123);
  CHECK(VS::Battle::ashleyStatusLabelsSuppressed == 1);
}
