// Isolated probes use actual HUD cores/bindings, font and sprite policies,
// GPU link handles, bounded module copying and repository diagnostics.
// Remaining production bindings come from explicitly hashed library inputs.
#include <crtdbg.h>

#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string_view>

#include "../../../Native/Tests/include/Battle/HudOrchestrationLiveFixture.hpp"
#include "Battle/HudOrchestrationLive.hpp"
#include "Core/Log.hpp"

namespace {
namespace b = VS::Battle;
namespace f = hud_orchestration_live_fixture;
unsigned diagnostics = 0;
}  // namespace

#ifndef VS_HUD_ORCHESTRATION_STORAGE_ONLY
namespace {
bool FeedbackProbe(std::string_view selected) {
  f::LiveState fixture;
  if (selected == "marker-slot-low") b::SetLiveActorMarker(-1, 0);
  if (selected == "marker-slot-high") b::SetLiveActorMarker(16, 0);
  if (selected == "marker-slot-no-byte-alias") b::SetLiveActorMarker(256, 0);
  if (selected == "marker-owner-and-priority") {
    Battle_actorMarkers[16] = 77;
    b::SetLiveActorMarker(0, 7);
    b::SetLiveActorMarker(15, 0xC0);
    return static_cast<unsigned char>(Battle_actorMarkers[15]) == 0x87 &&
           Battle_actorMarkers[16] == 77;
  }
  if (selected == "marker-command-width") {
    b::SetLiveActorMarkersHidden(0x12348001);
    return Battle_actorMarkersHidden == -32767;
  }
  if (selected == "label-independent-reentry") {
    b::SetLiveActorMarkersHidden(-123);
    b::SuppressLiveAshleyStatusLabels();
    b::SuppressLiveAshleyStatusLabels();
    return Battle_ashleyStatusLabelsSuppressed == 1 &&
           Battle_actorMarkersHidden == -123;
  }
  return false;
}

bool ShortcutProbe(std::string_view selected) {
  f::LiveState fixture;
  if (selected == "shortcut-closing-step-bound") {
    Battle_shortcutHudAnimationStep = 12;
    b::DrawLiveShortcutHud();
  }
  if (selected == "shortcut-row-step-bound") {
    Battle_shortcutHudAnimationStep = 21;
    Battle_shortcutHudRequested = 1;
    b::DrawLiveShortcutHud();
  }
  if (selected == "shortcut-hidden-lazy") {
    Battle_characterState = nullptr;
    fixture.frame.Disconnect();
    Battle_shortcutHudAnimationStep = 11;
    Battle_shortcutHudRequested = static_cast<char>(255);
    b::DrawLiveShortcutHud();
    return fixture.frame.UsedWords() == 0 && Battle_shortcutHudRequested == 0;
  }
  Battle_shortcutHudAnimationStep = 0;
  Battle_shortcutHudRequested = 1;
  if (selected == "shortcut-missing-current-actor") {
    Battle_characterState = nullptr;
    b::DrawLiveShortcutHud();
  }
  if (selected == "shortcut-missing-target") {
    fixture.frame.Disconnect();
    b::DrawLiveShortcutHud();
  }
  if (selected == "shortcut-capacity") {
    fixture.frame.FillBuffer();
    b::DrawLiveShortcutHud();
  }
  if (selected == "shortcut-production-packets") {
    b::DrawLiveShortcutHud();
    return fixture.frame.UsedWords() == 42 && Battle_shortcutHudRequested == 0;
  }
  return false;
}

bool BattleProbe(std::string_view selected) {
  f::LiveState fixture;
  if (selected == "battle-missing-initial-actor") {
    Battle_characterState = nullptr;
    b::DrawLiveBattleHud();
  }
  if (selected == "battle-missing-initial-stats") {
    fixture.actor.actor.stats = nullptr;
    b::DrawLiveBattleHud();
  }
  if (selected == "battle-fully-hidden-lazy") {
    fixture.frame.Disconnect();
    Battle_hudHidden = static_cast<char>(255);
    Battle_hudSlideOffset = 0x200000;
    b::DrawLiveBattleHud();
    return fixture.frame.UsedWords() == 0;
  }
  if (selected == "battle-missing-target") {
    fixture.frame.Disconnect();
    b::DrawLiveBattleHud();
  }
  if (selected == "battle-capacity") {
    fixture.frame.FillBuffer();
    b::DrawLiveBattleHud();
  }
  if (selected == "battle-used-carousel-icon-bound") {
    fixture.actor.stats.statuses = 0x00200000;
    Battle_upperStatusIconFlags[0] = 0x8000;
    Battle_statusIconCarousel.bytes[1] = 127;
    b::DrawLiveBattleHud();
  }
  if (selected == "battle-current-owner-reentry" ||
      selected == "battle-missing-current-limb-owner") {
    Battle_Actor replacement = fixture.actor.actor;
    Battle_ActorStats stats = fixture.actor.stats;
    replacement.stats = &stats;
    for (auto& limb : stats.limbs) limb.hp = 0;
    b::DrawLiveBattleHud();
    fixture.frame.primitives.fill(0);
    fixture.frame.ordering.fill(0xFFFFFF);
    Render_frameScratch.primitiveCursor = fixture.frame.primitives.data();
    Battle_characterState = &replacement;
    if (selected == "battle-missing-current-limb-owner") {
      replacement.stats = nullptr;
    }
    b::DrawLiveBattleHud();
    const auto& words = fixture.frame.primitives;
    return Battle_characterState == &replacement &&
           fixture.frame.UsedWords() == 82 &&
           words[2] == (Battle_limbStatusColors[0] | 0x64000000U) &&
           words[4] == Battle_limbDisplaySpriteTexCoords[0];
  }
  return false;
}

bool TimerProbe(std::string_view selected) {
  f::LiveState fixture;
  State_SceneTimer timer{};
  timer.minutes = 12;
  timer.seconds = 34;
  Audio_Seq_unusedStateFlags.puzzleTimerHundredths = 56;
  if (selected == "timer-missing-owner") b::DrawLivePuzzleTimer(nullptr);
  if (selected == "timer-missing-target") {
    fixture.frame.Disconnect();
    b::DrawLivePuzzleTimer(&timer);
  }
  if (selected == "timer-capacity") {
    fixture.frame.FillBuffer();
    b::DrawLivePuzzleTimer(&timer);
  }
  if (selected == "timer-bounded-native-text") {
    Battle_puzzleTimerTextPointer = nullptr;
    b::DrawLivePuzzleTimer(&timer);
    return fixture.frame.UsedWords() == 42 &&
           std::string_view(Battle_puzzleTimerText) == "12:34:56" &&
           Battle_puzzleTimerTextPointer == nullptr;
  }
  return false;
}
}  // namespace

int main(int argc, char** argv) {
  if (argc != 3) return 64;
  _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
  _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
  _CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
  std::signal(SIGABRT, [](int) { std::_Exit(85); });
  VS::Core::SetLogLevel(VS::Core::LogLevel::Debug);
  VS::Core::SetConsoleLogLevel(VS::Core::LogLevel::Warn);
  if (!VS::Core::SetLogFile(argv[2])) return 65;
  VS::Core::SetLogObserver([](const auto& record) {
    if (record.level >= VS::Core::LogLevel::Warn) ++diagnostics;
  });
  const std::string_view selected(argv[1]);
  const bool passed =
      selected.starts_with("shortcut-")
          ? ShortcutProbe(selected)
          : (selected.starts_with("battle-")
                 ? BattleProbe(selected)
                 : (selected.starts_with("timer-") ? TimerProbe(selected)
                                                   : FeedbackProbe(selected)));
  std::printf("%s diagnostics=%u\n", passed ? "PASS" : "FAIL", diagnostics);
  return passed && diagnostics == 0 ? 0 : 1;
}
#endif
