#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Event/CutsceneLifecycle.hpp"
#include "Event/CutsceneLifecycleLive.hpp"
#include "TestSupport/ChannelWarnings.hpp"

extern "C" {
#include "Battle/AbilityState.h"
#include "Battle/ActionFlow.h"
#include "Event/ScriptVm.h"
#include "Field/Interactions.h"
}

namespace {
namespace event = VS::Event;

struct NativeCutsceneScope {
  decltype(Battle_characterState) player = Battle_characterState;
  decltype(Field_controlMode) control = Field_controlMode;
  decltype(Battle_actionFlowMode) action = Battle_actionFlowMode;
  decltype(Event_eventSource) source = Event_eventSource;
  decltype(Event_eventId) event_id = Event_eventId;
  decltype(Event_playbackPhase) phase = Event_playbackPhase;
  decltype(Event_isStartConfirmed) confirmed = Event_isStartConfirmed;

  NativeCutsceneScope() {
    Battle_characterState = nullptr;
    Field_controlMode = 2;
    Battle_actionFlowMode = 7;
    Event_eventSource = 0;
    Event_eventId = 0;
    Event_playbackPhase = 1;
    Event_isStartConfirmed = 0;
  }
  ~NativeCutsceneScope() {
    Battle_characterState = player;
    Field_controlMode = control;
    Battle_actionFlowMode = action;
    Event_eventSource = source;
    Event_eventId = event_id;
    Event_playbackPhase = phase;
    Event_isStartConfirmed = confirmed;
  }
};

class CutsceneRecordingHost final : public event::CutsceneHost {
 public:
  std::optional<event::CutscenePreferences> preferences =
      event::CutscenePreferences{true, 1024, 0x600};
  std::array<std::optional<std::uint8_t>, 17> actors{};
  std::array<std::uint8_t, 256> flags{};
  std::array<std::uint32_t, 4> handles{21, 32, 43, 54};
  event::CutscenePreparation preparation = event::CutscenePreparation::kWaiting;
  int room_slot = 0;
  std::uint32_t playing_handle = 0xFFFF;
  std::uint16_t selector = 0;
  bool follow_on = false;
  bool timer_active = false;
  bool timer_trigger = false;
  std::vector<std::string> calls;
  std::vector<std::pair<event::CutsceneRefusal, int>> refusals;
  std::vector<int> head_resets;
  std::vector<int> reservations;
  std::vector<int> fades;
  std::vector<int> tints;
  std::vector<int> music_plays;
  std::vector<int> music_fades;
  std::vector<std::pair<int, std::uint8_t>> commands;
  std::function<void(int)> on_head_reset;

  void Refuse(event::CutsceneRefusal reason, int value) override {
    refusals.emplace_back(reason, value);
  }
  std::optional<event::CutscenePreferences> Preferences() override {
    return preferences;
  }
  void SetPreparation(event::CutscenePreparation value) override {
    preparation = value;
    calls.emplace_back("prepare");
  }
  void SetFlag(int flag, std::uint8_t value) override {
    flags.at(flag) = value;
  }
  std::optional<std::uint8_t> ActorCommand(int actor) override {
    return actors.at(actor);
  }
  void SetActorCommand(int actor, std::uint8_t command) override {
    REQUIRE(actors.at(actor).has_value());
    actors.at(actor) = command;
    commands.emplace_back(actor, command);
  }
  void ResetActorHeadTracking(int actor) override {
    head_resets.push_back(actor);
    if (on_head_reset) {
      on_head_reset(actor);
    }
  }
  void RefreshActorReservations(int actor) override {
    reservations.push_back(actor);
  }
  void SetMarkersHidden(bool hidden) override {
    calls.emplace_back(hidden ? "hide-markers" : "show-markers");
  }
  void ClearMenuRequest() override { calls.emplace_back("clear-menu"); }
  void PauseMovingPlatforms(bool paused) override {
    calls.emplace_back(paused ? "pause-platforms" : "resume-platforms");
  }
  void RequestEventStart() override { calls.emplace_back("request"); }
  std::uint16_t PendingEventSelector() override { return selector; }
  void DisableFirstPersonView() override {
    calls.emplace_back("end-first-person");
  }
  void RebuildActionPlans() override { calls.emplace_back("plans"); }
  void UpdateRouteGates() override { calls.emplace_back("gates"); }
  bool EvaluateFollowOnTrigger() override {
    calls.emplace_back("follow-on");
    return follow_on;
  }
  bool SceneTimerActive() override {
    calls.emplace_back("timer-active");
    return timer_active;
  }
  bool EvaluateTimerTrigger() override {
    calls.emplace_back("timer-trigger");
    return timer_trigger;
  }
  void ClearActorSoundOverrides() override {
    calls.emplace_back("clear-sound");
  }
  void ResetActorPaletteFade(int actor) override { fades.push_back(actor); }
  void ResetActorTint(int actor) override { tints.push_back(actor); }
  void RestoreRoomEffectVolume() override {
    calls.emplace_back("effect-volume");
  }
  void RestoreCamera() override { calls.emplace_back("camera"); }
  void ShowGameplayHud() override { calls.emplace_back("hud"); }
  event::CutsceneMusic Music() override {
    return {room_slot, playing_handle, handles};
  }
  void PlayRoomMusic(int slot) override { music_plays.push_back(slot); }
  void FadeRoomMusic(int slot) override { music_fades.push_back(slot); }
  void EnterFieldExploration() override { calls.emplace_back("explore"); }
};

void CheckPreparationCalls(const CutsceneRecordingHost& host, bool requested) {
  std::vector<std::string> expected;
  if (requested) {
    expected.emplace_back("request");
  }
  expected.insert(expected.end(),
                  {"prepare", "hide-markers", "clear-menu", "pause-platforms"});
  CHECK(host.calls == expected);
}

void CheckDefaultExitCalls(const CutsceneRecordingHost& host) {
  const std::vector<std::string> expected{"resume-platforms",
                                          "show-markers",
                                          "end-first-person",
                                          "plans",
                                          "gates",
                                          "follow-on",
                                          "timer-active",
                                          "clear-sound",
                                          "effect-volume",
                                          "camera",
                                          "hud",
                                          "explore"};
  CHECK(host.calls == expected);
}

}  // namespace

TEST_CASE("Cutscene preparation saves weapon and camera preferences",
          "[cutscene-lifecycle]") {
  CutsceneRecordingHost host;
  const int pitch = GENERATE(-32768, -1023, -512, -1, 0, 511, 512, 4096, 32767);
  const event::CutscenePreferences preferences{GENERATE(false, true), pitch,
                                               GENERATE(0x600, 0x601)};
  host.preferences = preferences;
  REQUIRE(event::PrepareCutsceneActors(host));
  CHECK(host.flags[0xAC] == (preferences.weapon_drawn ? 1 : 0));
  CHECK(host.flags[0xAF] == ((pitch / 512) & 7));
  CHECK(host.flags[0xB0] == (preferences.camera_distance == 0x600 ? 0 : 1));
  CHECK(host.preparation == event::CutscenePreparation::kPreparing);
  CheckPreparationCalls(host, false);
}

TEST_CASE("Cutscene preparation maps every actor command byte",
          "[cutscene-lifecycle]") {
  for (unsigned command = 0; command < 256; ++command) {
    CutsceneRecordingHost host;
    host.actors[0] = static_cast<std::uint8_t>(command);
    REQUIRE(event::PrepareCutsceneActors(host));
    const unsigned expected = command == 0x40                      ? 0x10
                              : command == 0x10 || command == 0x20 ? command
                                                                   : 0;
    CHECK(host.actors[0] == expected);
    CHECK(host.head_resets == std::vector<int>{0});
    CHECK(host.reservations ==
          (command == 0x80 ? std::vector<int>{0} : std::vector<int>{}));
  }
}

TEST_CASE("Cutscene preparation preserves missing slots and actor sixteen",
          "[cutscene-lifecycle]") {
  CutsceneRecordingHost host;
  host.actors[15] = 0x40;
  host.actors[16] = 0x80;
  REQUIRE(event::PrepareCutsceneActors(host));
  CHECK_FALSE(host.actors[0].has_value());
  CHECK(host.actors[15] == 0x10);
  CHECK(host.actors[16] == 0x80);
  CHECK(host.head_resets == std::vector<int>{15});
  CHECK(host.reservations.empty());
}

TEST_CASE("Prepared cutscene confirms start before preparation",
          "[cutscene-lifecycle]") {
  CutsceneRecordingHost host;
  REQUIRE(event::StartPreparedCutscene(host));
  CheckPreparationCalls(host, true);
}

TEST_CASE("Cutscene preparation observes command state after head reset",
          "[cutscene-lifecycle]") {
  CutsceneRecordingHost host;
  host.actors[0] = 0x40;
  host.on_head_reset = [&host](int actor) { host.actors.at(actor) = 0x20; };
  REQUIRE(event::PrepareCutsceneActors(host));
  CHECK(host.actors[0] == 0x20);
}

TEST_CASE("Cutscene preparation does not retain an actor removed by a callback",
          "[cutscene-lifecycle]") {
  CutsceneRecordingHost host;
  host.actors[0] = 0x40;
  host.on_head_reset = [&host](int actor) { host.actors.at(actor).reset(); };
  REQUIRE(event::PrepareCutsceneActors(host));
  CHECK_FALSE(host.actors[0].has_value());
  CHECK(host.commands.empty());
}

TEST_CASE("Queued cutscene waits for any nonzero sixteen-bit selector",
          "[cutscene-lifecycle]") {
  CutsceneRecordingHost host;
  host.selector = GENERATE(1, 0x7FFF, 0x8000, 0xFFFF);
  REQUIRE(event::BeginQueuedCutscene(host));
  CHECK(host.preparation == event::CutscenePreparation::kWaiting);
  CHECK(host.calls == std::vector<std::string>{"prepare"});
  CHECK(host.head_resets.empty());
}

TEST_CASE("Queued cutscene starts immediately without a selector",
          "[cutscene-lifecycle]") {
  CutsceneRecordingHost host;
  REQUIRE(event::BeginQueuedCutscene(host));
  CheckPreparationCalls(host, true);
}

TEST_CASE("Cutscene preparation refuses an absent player without side effects",
          "[cutscene-lifecycle]") {
  CutsceneRecordingHost host;
  host.preferences.reset();
  using Prepare = bool (*)(event::CutsceneHost&);
  const Prepare prepare =
      GENERATE(&event::PrepareCutsceneActors, &event::StartPreparedCutscene,
               &event::BeginQueuedCutscene);
  CHECK_FALSE(prepare(host));
  CHECK(host.calls.empty());
  REQUIRE(host.refusals.size() == 1);
  CHECK(host.refusals.front().first == event::CutsceneRefusal::kMissingPlayer);
}

TEST_CASE("Cutscene end restores only script-ready actor commands",
          "[cutscene-lifecycle]") {
  for (unsigned command = 0; command < 256; ++command) {
    CutsceneRecordingHost host;
    host.actors[15] = static_cast<std::uint8_t>(command);
    host.actors[16] = 0x10;
    event::EndFieldCutscene(host);
    CHECK(host.actors[15] == (command == 0x10 ? 0x40 : command));
    CHECK(host.actors[16] == 0x10);
    CHECK_FALSE(host.actors[0].has_value());
  }
}

TEST_CASE("Cutscene end restores field presentation in order",
          "[cutscene-lifecycle]") {
  CutsceneRecordingHost host;
  event::EndFieldCutscene(host);
  CheckDefaultExitCalls(host);
  REQUIRE(host.fades.size() == 16);
  REQUIRE(host.tints.size() == 16);
  for (int actor = 0; actor < 16; ++actor) {
    CHECK(host.fades[actor] == actor);
    CHECK(host.tints[actor] == actor);
  }
}

TEST_CASE("Cutscene follow-on skips timer and normal field restoration",
          "[cutscene-lifecycle]") {
  CutsceneRecordingHost host;
  host.follow_on = true;
  host.timer_active = true;
  host.actors[2] = 0x10;
  event::EndFieldCutscene(host);
  const std::vector<std::string> expected{"resume-platforms",
                                          "show-markers",
                                          "end-first-person",
                                          "plans",
                                          "gates",
                                          "follow-on",
                                          "request",
                                          "prepare",
                                          "hide-markers",
                                          "clear-menu",
                                          "pause-platforms"};
  CHECK(host.calls == expected);
  CHECK(host.actors[2] == 0x10);
  CHECK(host.fades.empty());
  CHECK(host.music_plays.empty());
}

TEST_CASE("Cutscene timer continuation requires an active timer",
          "[cutscene-lifecycle]") {
  CutsceneRecordingHost host;
  host.timer_trigger = true;
  event::EndFieldCutscene(host);
  CheckDefaultExitCalls(host);
}

TEST_CASE(
    "Cutscene active timer continuation starts without normal restoration",
    "[cutscene-lifecycle]") {
  CutsceneRecordingHost host;
  host.timer_active = true;
  host.timer_trigger = true;
  event::EndFieldCutscene(host);
  const std::vector<std::string> expected{"resume-platforms",
                                          "show-markers",
                                          "end-first-person",
                                          "plans",
                                          "gates",
                                          "follow-on",
                                          "timer-active",
                                          "timer-trigger",
                                          "request",
                                          "prepare",
                                          "hide-markers",
                                          "clear-menu",
                                          "pause-platforms"};
  CHECK(host.calls == expected);
  CHECK(host.fades.empty());
}

TEST_CASE("Cutscene end leaves music alone without a room slot",
          "[cutscene-lifecycle]") {
  CutsceneRecordingHost host;
  host.playing_handle = GENERATE(0xFFFF, 21);
  event::EndFieldCutscene(host);
  CHECK(host.music_plays.empty());
  CHECK(host.music_fades.empty());
  CHECK(host.refusals.empty());
}

TEST_CASE("Cutscene end starts room music when nothing is playing",
          "[cutscene-lifecycle]") {
  CutsceneRecordingHost host;
  host.room_slot = GENERATE(1, 4);
  event::EndFieldCutscene(host);
  CHECK(host.music_plays == std::vector<int>{host.room_slot});
  CHECK(host.music_fades.empty());
}

TEST_CASE("Cutscene end fades only matching room music",
          "[cutscene-lifecycle]") {
  CutsceneRecordingHost host;
  host.room_slot = GENERATE(1, 4);
  host.playing_handle = host.handles[host.room_slot - 1];
  event::EndFieldCutscene(host);
  CHECK(host.music_plays.empty());
  CHECK(host.music_fades == std::vector<int>{host.room_slot});
}

TEST_CASE("Cutscene end preserves different playing music",
          "[cutscene-lifecycle]") {
  CutsceneRecordingHost host;
  host.room_slot = 1;
  host.playing_handle = GENERATE(99u, 0x10015u, 0x1FFFFu);
  event::EndFieldCutscene(host);
  CHECK(host.music_plays.empty());
  CHECK(host.music_fades.empty());
}

TEST_CASE("Cutscene end refuses invalid music slots but releases field control",
          "[cutscene-lifecycle]") {
  CutsceneRecordingHost host;
  host.room_slot = GENERATE(-1, 5, 255);
  host.playing_handle = GENERATE(0xFFFF, 21);
  event::EndFieldCutscene(host);
  CHECK(host.music_plays.empty());
  CHECK(host.music_fades.empty());
  REQUIRE(host.refusals.size() == 1);
  CHECK(host.refusals.front().first == event::CutsceneRefusal::kMusicSlot);
  CHECK(host.refusals.front().second == host.room_slot);
  CheckDefaultExitCalls(host);
}

TEST_CASE("Live cutscene preparation refuses an absent player and logs once",
          "[cutscene-lifecycle]") {
  const NativeCutsceneScope state;
  const VS::test_support::ChannelWarnings warnings("cutscene");
  const auto prepare = GENERATE(&event::PrepareLiveCutsceneActors,
                                &event::StartLivePreparedCutscene,
                                &event::BeginLiveQueuedCutscene);
  CHECK_FALSE(prepare());
  CHECK(warnings.messages ==
        std::vector<std::string>{"Preparation refused: missing player"});
  CHECK(Field_controlMode == 2);
  CHECK(Battle_actionFlowMode == 7);
  CHECK(Event_isStartConfirmed == 0);
}

TEST_CASE("Live cutscene waits for signed high-bit selectors without starting",
          "[cutscene-lifecycle]") {
  const NativeCutsceneScope state;
  const VS::test_support::ChannelWarnings warnings("cutscene");
  Event_eventId = GENERATE(-32768, -1, 1);
  REQUIRE(event::BeginLiveQueuedCutscene());
  CHECK(Field_controlMode == 3);
  CHECK(Battle_actionFlowMode == 1);
  CHECK(Event_isStartConfirmed == 0);
  CHECK(warnings.messages.empty());
}

TEST_CASE("Live cutscene selector includes a nonzero source with event zero",
          "[cutscene-lifecycle]") {
  const NativeCutsceneScope state;
  Event_eventSource = GENERATE(1, 2);
  REQUIRE(event::BeginLiveQueuedCutscene());
  CHECK(Field_controlMode == 3);
  CHECK(Battle_actionFlowMode == 1);
  CHECK(Event_isStartConfirmed == 0);
}
