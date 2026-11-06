#include <array>
#include <cstddef>
#include <cstdint>
#include <format>
#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Event/ScriptEffectCommands.hpp"
#include "TestSupport/ScriptSteps.hpp"

namespace {
namespace event = VS::Event;
using event::ScriptEffectRefusal;
using event::ScriptStep;
using Calls = std::vector<std::string>;
using Refusal = std::pair<ScriptEffectRefusal, int>;
using Bytes = std::vector<std::uint8_t>;

template <typename... Values>
void Log(Calls& calls, std::string_view name, const Values&... values) {
  std::string line(name);
  ((line += std::format(" {}", values)), ...);
  calls.push_back(std::move(line));
}

class RecordingHost final : public event::ScriptEffectHost {
 public:
  Calls calls;
  std::vector<Refusal> refusals;
  bool skip = false;
  std::map<std::uint16_t, int> references;
  std::map<int, event::EffectActorSlot> slots;
  int playback_state = 0;
  bool trigger_ready = false;
  bool wait_pending = false;
  std::array<bool, event::kVibrationPatternCount> patterns{};
  bool screen_loading = false;
  std::uint8_t scanline = 0;

  void Refuse(ScriptEffectRefusal refusal, int value) override {
    refusals.emplace_back(refusal, value);
  }
  [[nodiscard]] int ResolveActor(std::uint16_t reference) override {
    const auto found = references.find(reference);
    return found != references.end() ? found->second : reference;
  }
  [[nodiscard]] bool SkipActive() const override { return skip; }
  [[nodiscard]] event::EffectActorSlot ActorSlot(int actor) const override {
    const auto found = slots.find(actor);
    return found != slots.end() ? found->second : event::EffectActorSlot{};
  }
  void ReportUnavailableActor(int effect, std::uint16_t selector,
                              int actor) override {
    Log(calls, "Unavailable", effect, selector, actor);
  }
  void ReportAbsentActor(int effect, std::uint16_t selector,
                         int actor) override {
    Log(calls, "Absent", effect, selector, actor);
  }
  void StartActorEffect(int effect, int actor) override {
    Log(calls, "Start", effect, actor);
  }
  void ReleaseEffectTrigger(int track) override {
    Log(calls, "Release", track);
  }
  void RequeueEffectTrack(int track) override { Log(calls, "Requeue", track); }
  void ClearPrimaryEffects() override { Log(calls, "Clear"); }
  [[nodiscard]] int EffectPlaybackState() override { return playback_state; }
  [[nodiscard]] bool EffectTriggerReady() override { return trigger_ready; }
  [[nodiscard]] bool EffectWaitPending() const override { return wait_pending; }
  void SetEffectWaitPending(bool pending) override { wait_pending = pending; }
  [[nodiscard]] bool VibrationPatternLoaded(int pattern) const override {
    return patterns.at(static_cast<std::size_t>(pattern));
  }
  void StartVibrationPattern(int pattern, bool buffered) override {
    Log(calls, "Vibrate", pattern, buffered);
  }
  void StopVibration() override { Log(calls, "StopVibration"); }
  void StartWeaponTrail(int actor, int bone) override {
    Log(calls, "Trail", actor, bone);
  }
  void FadeWeaponTrails(int actor, int bone) override {
    Log(calls, "FadeTrails", actor, bone);
  }
  [[nodiscard]] bool ScreenEffectsLoading() const override {
    return screen_loading;
  }
  void AcquireScreenEffects() override { Log(calls, "Acquire"); }
  void ReleaseScreenEffects() override { Log(calls, "ReleaseScreen"); }
  void SetDistortionLayer(int layer) override {
    Log(calls, "Distortion", layer);
  }
  void SetScanlineOverlay(std::uint8_t enabled) override { scanline = enabled; }
};

using Handler = ScriptStep (*)(event::ScriptCommand, event::ScriptEffectHost&);

ScriptStep Run(Handler handler, const Bytes& bytes, RecordingHost& host) {
  return handler(event::ScriptCommand{bytes}, host);
}

using Step = VS::test_support::ScriptStepCase<event::ScriptEffectHost>;
using Results = std::vector<ScriptStep>;

Results RunSteps(const std::vector<Step>& steps, RecordingHost& host) {
  return VS::test_support::RunScriptSteps<event::ScriptEffectHost>(steps, host);
}

struct Command {
  Handler handler;
  std::size_t length;
};

constexpr std::array kCommands{
    Command{event::CommandPlayActorEffect, event::kPlayActorEffectLength},
    Command{event::CommandAdvanceEffectStage, event::kAdvanceEffectStageLength},
    Command{event::CommandWaitEffectState, event::kWaitEffectStateLength},
    Command{event::CommandYieldEffectRelease, event::kYieldEffectReleaseLength},
    Command{event::CommandWaitEffectContinuation,
            event::kWaitEffectContinuationLength},
    Command{event::CommandWaitEffectRelease, event::kWaitEffectReleaseLength},
    Command{event::CommandMarkEffectStageReady,
            event::kMarkEffectStageReadyLength},
    Command{event::CommandPlayVibrationPattern,
            event::kPlayVibrationPatternLength},
    Command{event::CommandSetActorTrail, event::kSetActorTrailLength},
    Command{event::CommandWaitScreenEffectLoad,
            event::kWaitScreenEffectLoadLength},
    Command{event::CommandSetScreenDistortion,
            event::kSetScreenDistortionLength},
    Command{event::CommandLoadScreenEffects, event::kLoadScreenEffectsLength},
    Command{event::CommandReservedScreenEffect,
            event::kReservedScreenEffectLength},
    Command{event::CommandFreeScreenEffects, event::kFreeScreenEffectsLength},
    Command{event::CommandSetScanlineOverlay, event::kSetScanlineOverlayLength},
};

// Effect 0x0102 on the actor `selector` names.
Bytes ActorEffect(std::uint16_t selector) {
  return {0xA0, 0x02, 0x01, static_cast<std::uint8_t>(selector & 0xFF),
          static_cast<std::uint8_t>(selector >> 8)};
}

// A weather tween's frame, wind direction and strength, intensity and
// remaining duration.
std::array<int, 5> Summary(const event::WeatherTween& tween) {
  return {tween.elapsed, tween.wind_direction, tween.wind_strength,
          tween.intensity, tween.duration};
}

class RecordingCanvas final : public event::ScanlineCanvas {
 public:
  Calls calls;
  void DrawLine(int y, int width, int grey) override {
    Log(calls, "Line", y, width, grey);
  }
  void SetSubtractiveBlend() override { Log(calls, "Blend"); }
};

class LinearTweenHost final : public event::ImageTweenHost {
 public:
  Calls calls;
  [[nodiscard]] int EaseFraction(int mode, int frame, int duration) override {
    Log(calls, "Ease", mode, frame, duration);
    return (frame * 4096) / duration;
  }
  void PlaceImageLayer(int layer,
                       const event::ImageLayerPlacement& placement) override {
    Log(calls, "Place", layer, placement.x, placement.y, placement.scale,
        placement.brightness);
  }
};
}  // namespace

TEST_CASE("Event script effect commands refuse short instructions",
          "[event-script-effects]") {
  for (const Command& command : kCommands) {
    RecordingHost host;
    const Bytes bytes(command.length - 1, 0x01);
    CHECK(Run(command.handler, bytes, host) == ScriptStep::kAdvance);
    CHECK(host.calls.empty());
    REQUIRE(host.refusals.size() == 1);
    CHECK(host.refusals[0] == Refusal{ScriptEffectRefusal::kShortCommand,
                                      static_cast<int>(command.length - 1)});
  }
}

TEST_CASE("Event script actor effects wait for their target's model",
          "[event-script-effects]") {
  RecordingHost host;
  host.references = {{0x0042, 4}, {0x0099, 1}, {0x2000, 3}, {0x0077, 16}};
  host.slots[4] = {.battle_actor = true, .model_id = 0x43, .occupied = true};
  CHECK(Run(event::CommandPlayActorEffect, ActorEffect(0x0042), host) ==
        ScriptStep::kHold);
  host.slots[4].model_id = 0x42;
  CHECK(Run(event::CommandPlayActorEffect, ActorEffect(0x0042), host) ==
        ScriptStep::kAdvance);
  // A model reference that resolves to an occupied slot without a battle
  // actor, a reference type past slots and a slot past the actors wait.
  host.slots[1] = {.battle_actor = false, .model_id = 0, .occupied = true};
  CHECK(Run(event::CommandPlayActorEffect, ActorEffect(0x0099), host) ==
        ScriptStep::kHold);
  CHECK(Run(event::CommandPlayActorEffect, ActorEffect(0x2000), host) ==
        ScriptStep::kHold);
  CHECK(Run(event::CommandPlayActorEffect, ActorEffect(0x0077), host) ==
        ScriptStep::kHold);
  CHECK(host.calls == Calls{"Unavailable 258 66 4", "Start 258 4",
                            "Unavailable 258 153 1", "Unavailable 258 8192 -1",
                            "Unavailable 258 119 16"});
}

TEST_CASE("Event script actor effects start on an absent model's empty slot",
          "[event-script-effects]") {
  RecordingHost host;
  host.references = {{0x0099, 1}, {0x1005, 5}, {0x0055, 6}};
  CHECK(Run(event::CommandPlayActorEffect, ActorEffect(0x0099), host) ==
        ScriptStep::kAdvance);
  CHECK(Run(event::CommandPlayActorEffect, ActorEffect(0x1005), host) ==
        ScriptStep::kAdvance);
  // A model reference resolving to another empty slot still waits.
  CHECK(Run(event::CommandPlayActorEffect, ActorEffect(0x0055), host) ==
        ScriptStep::kHold);
  host.slots[5] = {.battle_actor = false, .model_id = 0, .occupied = true};
  CHECK(Run(event::CommandPlayActorEffect, ActorEffect(0x1005), host) ==
        ScriptStep::kAdvance);
  CHECK(host.calls == Calls{"Absent 258 153 1", "Start 258 1",
                            "Absent 258 4101 5", "Start 258 5",
                            "Unavailable 258 85 6", "Start 258 5"});
}

TEST_CASE("Event script effect waits follow the effect playback",
          "[event-script-effects]") {
  RecordingHost host;
  host.playback_state = 3;
  CHECK(Run(event::CommandWaitEffectState, {0xA2, 5}, host) ==
        ScriptStep::kHold);
  host.playback_state = 5;
  CHECK(Run(event::CommandWaitEffectState, {0xA2, 5}, host) ==
        ScriptStep::kAdvance);
  // During a skip a settled state clears the effects first.
  host.skip = true;
  host.playback_state = 2;
  CHECK(Run(event::CommandWaitEffectState, {0xA2, 0}, host) ==
        ScriptStep::kHold);
  CHECK(host.wait_pending);
  CHECK(Run(event::CommandWaitEffectState, {0xA2, 0}, host) ==
        ScriptStep::kHold);
  host.playback_state = 0;
  CHECK(Run(event::CommandWaitEffectState, {0xA2, 0}, host) ==
        ScriptStep::kAdvance);
  CHECK_FALSE(host.wait_pending);
  CHECK(Run(event::CommandWaitEffectState, {0xA2, 3}, host) ==
        ScriptStep::kHold);
  CHECK(host.calls == Calls{"Clear"});
}

TEST_CASE("Event script effect release waits clear once during a skip",
          "[event-script-effects]") {
  RecordingHost host;
  host.skip = true;
  host.playback_state = 4;
  CHECK(Run(event::CommandWaitEffectRelease, {0xA5}, host) ==
        ScriptStep::kHold);
  CHECK(Run(event::CommandWaitEffectRelease, {0xA5}, host) ==
        ScriptStep::kHold);
  host.playback_state = 5;
  CHECK(Run(event::CommandWaitEffectRelease, {0xA5}, host) ==
        ScriptStep::kAdvance);
  CHECK_FALSE(host.wait_pending);
  CHECK(host.calls == Calls{"Clear"});
}

TEST_CASE("Event script effect continuation waits for its trigger",
          "[event-script-effects]") {
  RecordingHost host;
  CHECK(Run(event::CommandWaitEffectContinuation, {0xA4}, host) ==
        ScriptStep::kHold);
  host.trigger_ready = true;
  CHECK(Run(event::CommandWaitEffectContinuation, {0xA4}, host) ==
        ScriptStep::kAdvance);
}

TEST_CASE("Event script effect stages release and requeue their tracks",
          "[event-script-effects]") {
  RecordingHost host;
  CHECK(Run(event::CommandAdvanceEffectStage, {0xA1, 2}, host) ==
        ScriptStep::kAdvance);
  CHECK(Run(event::CommandYieldEffectRelease, {0xA3}, host) ==
        ScriptStep::kAdvance);
  CHECK(Run(event::CommandMarkEffectStageReady, {0xA6, 3}, host) ==
        ScriptStep::kAdvance);
  host.skip = true;
  CHECK(Run(event::CommandAdvanceEffectStage, {0xA1, 4}, host) ==
        ScriptStep::kAdvance);
  CHECK(Run(event::CommandYieldEffectRelease, {0xA3}, host) ==
        ScriptStep::kAdvanceAndYield);
  CHECK(host.calls == Calls{"Release 2", "Requeue 3", "Release 4", "Clear"});
}

// #307: the original indexed its vibration patterns with the script's byte
// and copied an absent buffered pattern from a null pointer.
TEST_CASE("Event script vibration plays only loaded patterns",
          "[event-script-effects]") {
  RecordingHost host;
  host.patterns[3] = true;
  host.patterns[2] = true;
  CHECK(Run(event::CommandPlayVibrationPattern, {0xA7, 0x0B}, host) ==
        ScriptStep::kAdvance);
  CHECK(Run(event::CommandPlayVibrationPattern, {0xA7, 0x02}, host) ==
        ScriptStep::kAdvance);
  CHECK(Run(event::CommandPlayVibrationPattern, {0xA7, 0x01}, host) ==
        ScriptStep::kAdvance);
  CHECK(Run(event::CommandPlayVibrationPattern, {0xA7, 0xFF}, host) ==
        ScriptStep::kAdvance);
  CHECK(host.refusals.empty());
  CHECK(Run(event::CommandPlayVibrationPattern, {0xA7, 0x09}, host) ==
        ScriptStep::kAdvance);
  CHECK(Run(event::CommandPlayVibrationPattern, {0xA7, 0x10}, host) ==
        ScriptStep::kAdvance);
  CHECK(host.calls ==
        Calls{"Vibrate 3 true", "Vibrate 2 false", "StopVibration"});
  CHECK(host.refusals ==
        std::vector<Refusal>{{ScriptEffectRefusal::kVibrationPattern, 0x09},
                             {ScriptEffectRefusal::kVibrationPattern, 0x10}});
}

TEST_CASE("Event script trails and screen effects reach the effect system",
          "[event-script-effects]") {
  RecordingHost host;
  host.references = {{0x8001, -2}};
  CHECK(Run(event::CommandSetActorTrail, {0xA9, 1, 0x05, 0x00, 7}, host) ==
        ScriptStep::kAdvance);
  CHECK(Run(event::CommandSetActorTrail, {0xA9, 0, 0x01, 0x80, 9}, host) ==
        ScriptStep::kAdvance);
  host.screen_loading = true;
  CHECK(Run(event::CommandWaitScreenEffectLoad, {0xBA}, host) ==
        ScriptStep::kHold);
  host.screen_loading = false;
  CHECK(RunSteps({{event::CommandWaitScreenEffectLoad, {0xBA}},
                  {event::CommandSetScreenDistortion, {0xBB, 0xFE, 0xFF}},
                  {event::CommandLoadScreenEffects, {0xBC}},
                  {event::CommandReservedScreenEffect, {0xBD, 9}},
                  {event::CommandFreeScreenEffects, {0xBE}}},
                 host) == Results(5, ScriptStep::kAdvance));
  host.skip = true;
  CHECK(RunSteps({{event::CommandFreeScreenEffects, {0xBE}},
                  {event::CommandSetScanlineOverlay, {0xBF, 1}}},
                 host) ==
        Results{ScriptStep::kAdvanceAndYield, ScriptStep::kAdvance});
  CHECK(host.scanline == 1);
  CHECK(host.calls == Calls{"Trail 5 7", "FadeTrails -2 9", "Distortion -2",
                            "Acquire", "ReleaseScreen", "ReleaseScreen"});
}

TEST_CASE("Event scanline overlay darkens alternate rows each two frames",
          "[event-script-effects]") {
  RecordingCanvas canvas;
  CHECK(event::DrawEventScanlineOverlay(0, 320, 4, canvas) == 1);
  CHECK(event::DrawEventScanlineOverlay(1, 256, 3, canvas) == 2);
  CHECK(event::DrawEventScanlineOverlay(0xFFFF, 8, 2, canvas) == 0);
  CHECK(canvas.calls == Calls{"Line 1 320 32", "Line 3 320 32", "Blend",
                              "Line 0 256 32", "Line 2 256 32", "Blend",
                              "Line 1 8 32", "Blend"});
}

TEST_CASE("Event splash tweens ease their layers and stop when done",
          "[event-script-effects]") {
  std::array<event::SplashTween, event::kImageTweenCount> tweens{};
  tweens[0] = {.easing_mode = 2, .elapsed = 0, .duration = 0};
  tweens[1] = {.easing_mode = 1,
               .elapsed = 2,
               .duration = 8,
               .start_x = 10,
               .start_y = -20,
               .delta_x = 100,
               .delta_y = -40,
               .start_scale = 4096,
               .delta_scale = -2048,
               .start_brightness = 128,
               .delta_brightness = 127};
  tweens[2] = {.easing_mode = 0, .elapsed = 7, .duration = 8, .delta_x = 8};
  LinearTweenHost host;
  event::UpdateEventImageTweens(tweens, 4, host);
  CHECK(host.calls == Calls{"Ease 1 4 8", "Place 1 60 -40 3072 191",
                            "Ease 0 9 8", "Place 2 9 0 0 0"});
  CHECK(tweens[0].duration == 0);
  CHECK(tweens[1].elapsed == 4);
  CHECK(tweens[1].duration == 8);
  CHECK(tweens[2].elapsed == 9);
  CHECK(tweens[2].duration == 0);
}

TEST_CASE("Event weather tween eases wind and intensity over its frames",
          "[event-script-effects]") {
  event::WeatherTween tween{.elapsed = 0,
                            .duration = 4,
                            .start_wind_direction = 4090,
                            .delta_wind_direction = 200,
                            .start_wind_strength = 100,
                            .delta_wind_strength = -40,
                            .start_intensity = 0,
                            .delta_intensity = 8};
  REQUIRE(event::UpdateEventWeatherTween(tween));
  CHECK(Summary(tween) == std::array{1, 44, 90, 2, 4});
  const std::array<bool, 3> running{event::UpdateEventWeatherTween(tween),
                                    event::UpdateEventWeatherTween(tween),
                                    event::UpdateEventWeatherTween(tween)};
  CHECK(running == std::array{true, true, true});
  CHECK(Summary(tween) == std::array{4, 194, 60, 8, 0});
  CHECK_FALSE(event::UpdateEventWeatherTween(tween));
  CHECK(tween.elapsed == 4);
}
