#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <type_traits>

#include <catch2/catch_test_macros.hpp>

#include "Battle/HudOrchestrationLive.hpp"
#include "Battle/HudOrchestrationLiveFixture.hpp"
#include "Content/ModuleData.h"

namespace {
namespace b = VS::Battle;
namespace f = hud_orchestration_live_fixture;

template <typename Function>
Function Entry(unsigned address, Function expected) {
  const auto symbols =
      std::span(Content_moduleSymbols, Content_moduleSymbolCount);
  void* const entry =
      VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG", address);
  if (entry == nullptr || entry != reinterpret_cast<void*>(expected)) {
    FAIL("HUD orchestration entry has the wrong native binding or signature");
    return nullptr;
  }
  return reinterpret_cast<Function>(entry);
}

static_assert(
    std::is_same_v<decltype(&b::SetLiveActorMarkersHidden), void (*)(int)>);
static_assert(
    std::is_same_v<decltype(&b::SetLiveActorMarker), void (*)(int, int)>);
static_assert(
    std::is_same_v<decltype(&b::SuppressLiveAshleyStatusLabels), void (*)()>);
static_assert(std::is_same_v<decltype(&b::DrawLiveShortcutHud), void (*)()>);
static_assert(std::is_same_v<decltype(&b::DrawLiveBattleHud), void (*)()>);
static_assert(std::is_same_v<decltype(&b::DrawLivePuzzleTimer),
                             void (*)(const State_SceneTimer*)>);
}  // namespace

TEST_CASE("F00729 production module preserves the complete visibility command",
          "[hud-orchestration-module][F00729]") {
  const auto hide = Entry(0x80095B70, &b::SetLiveActorMarkersHidden);
  if (hide == nullptr) return;
  f::SavedValue<Battle_actorMarkersHidden> saved;
  hide(0x12348001);
  CHECK(Battle_actorMarkersHidden == -32767);
}

TEST_CASE("F00730 production module keeps marker command priority",
          "[hud-orchestration-module][F00730]") {
  const auto set = Entry(0x80095B7C, &b::SetLiveActorMarker);
  if (set == nullptr) return;
  f::SavedArray<Battle_actorMarkers> saved;
  Battle_actorMarkers[15] = 7;
  set(15, 0xC0);
  CHECK(static_cast<std::uint8_t>(Battle_actorMarkers[15]) == 0x87);
}

TEST_CASE("F00743 production module suppression is independently owned",
          "[hud-orchestration-module][F00743]") {
  const auto suppress = Entry(0x8009722C, &b::SuppressLiveAshleyStatusLabels);
  if (suppress == nullptr) return;
  f::SavedValue<Battle_ashleyStatusLabelsSuppressed> saved;
  Battle_ashleyStatusLabelsSuppressed = -1;
  suppress();
  CHECK(Battle_ashleyStatusLabelsSuppressed == 1);
}

TEST_CASE("F01361 production module consumes the final shortcut request",
          "[hud-orchestration-module][F01361]") {
  const auto draw = Entry(0x800C9FE8, &b::DrawLiveShortcutHud);
  if (draw == nullptr) return;
  f::LiveState fixture;
  Battle_shortcutHudRequested = static_cast<char>(255);
  Battle_shortcutHudAnimationStep = 11;
  fixture.frame.Disconnect();
  draw();
  CHECK(Battle_shortcutHudAnimationStep == 10);
  CHECK(Battle_shortcutHudRequested == 0);
}

TEST_CASE("F01362 production module preserves the fully hidden no-draw gate",
          "[hud-orchestration-module][F01362]") {
  const auto draw = Entry(0x800CA2DC, &b::DrawLiveBattleHud);
  if (draw == nullptr) return;
  f::LiveState fixture;
  Battle_hudHidden = 2;
  Battle_hudSlideOffset = 0x200000;
  fixture.frame.Disconnect();
  draw();
  CHECK(fixture.frame.UsedWords() == 0);
}

TEST_CASE("F01384 production module receives the authoritative typed timer",
          "[hud-orchestration-module][F01384]") {
  const auto draw = Entry(0x800CB56C, &b::DrawLivePuzzleTimer);
  if (draw == nullptr) return;
  f::LiveState fixture;
  State_SceneTimer timer{};
  timer.minutes = 12;
  timer.seconds = 34;
  Audio_Seq_unusedStateFlags.puzzleTimerHundredths = 56;
  draw(&timer);
  CHECK(fixture.frame.UsedWords() == 42);
  CHECK(Battle_puzzleTimerText[0] == '1');
  CHECK(Battle_puzzleTimerText[7] == '6');
}
