// Hand-maintained (tools/port/owned.csv).
// Binds the typed event script effect commands and frame updates
// (ScriptEffectCommands.cpp) to the event VM's opcode slots and frame update,
// the effect runtime, the controller vibration, the weapon trails, the screen
// effects, the splash image layers, the weather and the frame's primitive
// buffer.
#include "Event/ScriptEffectCommandsLive.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <span>

#include "Actors/ActorRecord.hpp"
#include "Actors/ActorStateLive.hpp"
#include "Battle/Actor.hpp"
#include "Core/Diagnostics.hpp"
#include "Effects/BoneTrailLive.hpp"
#include "Effects/EffectRuntimeLive.hpp"
#include "Effects/PrimaryEffectLive.hpp"
#include "Effects/Runtime.hpp"
#include "Effects/ScreenDistortion/ScreenOverlaysLive.hpp"
#include "Effects/ScreenEffectState.hpp"
#include "Effects/ScriptedEffectTriggersLive.hpp"
#include "Event/InterpolationCurveLive.hpp"
#include "Event/ScriptActors.hpp"
#include "Event/ScriptEffectCommands.hpp"
#include "Event/ScriptEffectOpcodes.hpp"
#include "Event/ScriptEffects.hpp"
#include "Event/ScriptField.hpp"
#include "Event/ScriptScreen.hpp"
#include "Event/ScriptVmLive.hpp"
#include "Event/ScriptVmLiveState.hpp"
#include "Event/Splash.hpp"
#include "Event/Weather.hpp"
#include "Field/EnvironmentLive.hpp"
#include "Field/Gimmicks.hpp"
#include "Input/ControllerLive.hpp"
#include "Legacy/Psx.hpp"
#include "Render/Display.hpp"
#include "Render/Frame.hpp"
#include "Render/GimImagesLive.hpp"
#include "Render/Render.hpp"
#include "Render/UiPrimitives.hpp"
#include "State/Timers.hpp"

using VS::Legacy::AddPrim;
using VS::Legacy::DR_TPAGE;
using VS::Legacy::GetTPage;
using VS::Legacy::LINE_F2;
using VS::Legacy::SetDrawTPage;
using VS::Legacy::SetLineF2;
using VS::Legacy::setSemiTrans;

namespace {
namespace event = VS::Event;

static_assert(std::size(VS::Actors::actorRecords) >=
              static_cast<std::size_t>(event::kEffectActorSlots));
static_assert(std::size(VS::Field::gimmickInstances) >=
              static_cast<std::size_t>(event::kEffectActorSlots));
static_assert(event::kSplashTweenExtent >= event::kImageTweenCount);
// The original advanced the cursor past a texture page packet to the first
// line's x0, a packet's length further.
static_assert(offsetof(LINE_F2, x0) == sizeof(DR_TPAGE));

constexpr char kSkipActive = 2;
// Vibration patterns open with an 8-byte header; the controller plays all
// motors and the script owns the request.
constexpr std::size_t kVibrationHeaderBytes = 8;
constexpr std::uint32_t kAllMotors = 0xFF;
constexpr int kScriptVibrationOwner = 2;
constexpr std::size_t kPrimitiveBufferBytes = 0x20000;

void Require(bool valid, const char* message) {
  if (!valid) {
    VS::Core::Errorf("event", "%s", message);
    std::abort();
  }
}

const char* RefusalText(event::ScriptEffectRefusal refusal) {
  switch (refusal) {
    case event::ScriptEffectRefusal::kVibrationPattern:
      return "a vibration pattern past the loaded patterns";
    case event::ScriptEffectRefusal::kShortCommand:
      return "an instruction shorter than its operands";
  }
  return "an unknown request";
}

class LiveScriptEffectHost final : public event::ScriptEffectHost {
 public:
  void Refuse(event::ScriptEffectRefusal refusal, int value) override {
    VS::Core::Warnf("event", "Script effect command refused %s (%d)",
                    RefusalText(refusal), value);
  }

  [[nodiscard]] int ResolveActor(std::uint16_t reference) override {
    return VS::Event::ResolveLiveScriptActorReference(reference);
  }
  [[nodiscard]] bool SkipActive() const override {
    return event::skipState == kSkipActive;
  }
  // The core checks the actor against kEffectActorSlots first.
  [[nodiscard]] event::EffectActorSlot ActorSlot(int actor) const override {
    const VS::Battle::Actor* target = VS::Actors::LiveActorAtSlot(actor);
    return {.battle_actor = target != nullptr,
            .model_id = target != nullptr ? target->model_id
                                          : static_cast<std::uint16_t>(0),
            .occupied = VS::Actors::actorRecords[actor] != nullptr ||
                        VS::Field::gimmickInstances[actor] != nullptr};
  }

  void ReportUnavailableActor(int effect, std::uint16_t selector,
                              int actor) override {
    VS::Core::Errorf(
        "event", "effect=%d awaits unavailable actor selector=0x%04X slot=%d",
        effect, static_cast<unsigned>(selector), actor);
  }
  // A model no actor has resolves to slot 1 and a slot reference is used
  // unchecked; the effect starts at once and its reads of the empty slot
  // leave its target-dependent tracks at their defaults.
  void ReportAbsentActor(int effect, std::uint16_t selector,
                         int actor) override {
    VS::Core::Debugf(
        "event", "effect=%d absent actor selector=0x%04X targets empty slot %d",
        effect, static_cast<unsigned>(selector), actor);
  }
  void StartActorEffect(int effect, int actor) override {
    VS::Effects::LiveStartScriptedActorEffect(effect, actor);
  }
  void ReleaseEffectTrigger(int track) override {
    VS::Effects::LiveReleaseScriptedEffectTrigger(track);
  }
  void RequeueEffectTrack(int track) override {
    VS::Effects::LiveRequeueEffectTrack(track);
  }
  void ClearPrimaryEffects() override {
    VS::Effects::LiveClearPrimaryEffects();
  }
  [[nodiscard]] int EffectPlaybackState() override {
    return VS::Effects::LiveScriptedEffectPlaybackState();
  }
  [[nodiscard]] bool EffectTriggerReady() override {
    return VS::Effects::LiveIsScriptedEffectTriggerReady() == 1;
  }
  [[nodiscard]] bool EffectWaitPending() const override {
    return event::isEffectWaitPending != 0;
  }
  void SetEffectWaitPending(bool pending) override {
    event::isEffectWaitPending = pending ? 1 : 0;
  }

  [[nodiscard]] bool VibrationPatternLoaded(int pattern) const override {
    const auto lease = event::LeaseLiveVibrationPattern(pattern);
    if (!lease) return false;
    const auto bytes = lease->Bytes();
    return bytes && bytes->size() > kVibrationHeaderBytes;
  }
  // Every request owns a bounded copy of the pattern, buffered or not, so no
  // request reads the original shared scratch.
  void StartVibrationPattern(int pattern, bool /*buffered*/) override {
    const auto lease = event::LeaseLiveVibrationPattern(pattern);
    if (!lease) {
      Refuse(event::ScriptEffectRefusal::kVibrationPattern, pattern);
      return;
    }
    const auto bytes = lease->Bytes();
    if (!bytes || bytes->size() <= kVibrationHeaderBytes) {
      Refuse(event::ScriptEffectRefusal::kVibrationPattern, pattern);
      return;
    }
    static_cast<void>(VS::Input::StartLiveOwnedVibrationPattern(
        std::as_bytes(*bytes).subspan(kVibrationHeaderBytes), kAllMotors,
        kScriptVibrationOwner));
  }
  void StopVibration() override {
    static_cast<void>(
        VS::Input::StopLiveVibrationPatterns(0, kScriptVibrationOwner));
  }

  void StartWeaponTrail(int actor, int bone) override {
    VS::Effects::LiveStartWeaponTrail(actor, bone);
  }
  void FadeWeaponTrails(int /*actor*/, int /*bone*/) override {
    VS::Effects::LiveFadeWeaponTrails();
  }

  [[nodiscard]] bool ScreenEffectsLoading() const override {
    return VS::Effects::screenEffectOverlay != nullptr &&
           VS::Effects::screenEffectOverlay->loading != 0;
  }
  void AcquireScreenEffects() override {
    VS::Effects::ScreenDistortion::AcquireLiveScreenOverlay();
  }
  void ReleaseScreenEffects() override {
    VS::Effects::ScreenDistortion::ReleaseLiveScreenOverlay();
  }
  void SetDistortionLayer(int layer) override {
    VS::Effects::ScreenDistortion::SetLiveFeedbackLayer(layer);
  }
  void SetScanlineOverlay(std::uint8_t enabled) override {
    event::isScanlineOverlayEnabled = enabled;
  }
};

using Handler = event::ScriptStep (*)(event::ScriptCommand,
                                      event::ScriptEffectHost&);

intptr_t RunCommand(Handler handler, const std::uint8_t* command,
                    std::size_t length) {
  Require(command != nullptr, "Script effect command is missing");
  LiveScriptEffectHost host;
  return static_cast<intptr_t>(
      handler(event::ScriptCommand{command, length}, host));
}

// The current frame's primitive buffer and overlay ordering table. A packet
// the buffer cannot hold is dropped.
class LiveScanlineCanvas final : public event::ScanlineCanvas {
 public:
  // NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the line fields.
  void DrawLine(int y, int width, int grey) override {
    auto* line = static_cast<LINE_F2*>(Reserve(sizeof(LINE_F2)));
    if (line == nullptr) {
      return;
    }
    std::memset(line, 0, sizeof *line);
    SetLineF2(line);
    line->r0 = line->g0 = line->b0 = static_cast<std::uint8_t>(grey);
    setSemiTrans(line, 1);
    line->x0 = 0;
    line->y0 = line->y1 = static_cast<std::int16_t>(y);
    line->x1 = static_cast<std::int16_t>(width);
    AddPrim(VS::Render::FrameScratchpad().overlay_ordering_table, line);
    VS::Render::FrameScratchpad().primitive_cursor = line + 1;
  }
  void SetSubtractiveBlend() override {
    auto* page = static_cast<DR_TPAGE*>(Reserve(sizeof(DR_TPAGE)));
    if (page == nullptr) {
      return;
    }
    std::memset(page, 0, sizeof *page);
    SetDrawTPage(page, 1, 1, GetTPage(0, 2, 0, 0));
    AddPrim(VS::Render::FrameScratchpad().overlay_ordering_table, page);
    VS::Render::FrameScratchpad().primitive_cursor = page + 1;
  }

 private:
  [[nodiscard]] static void* Reserve(std::size_t bytes) {
    const auto base = reinterpret_cast<std::uintptr_t>(
        VS::Render::legacyPrimitiveBuffers[VS::Render::frameBuf & 1]);
    const auto cursor = reinterpret_cast<std::uintptr_t>(
        VS::Render::FrameScratchpad().primitive_cursor);
    if (base == 0 || cursor < base ||
        cursor - base > kPrimitiveBufferBytes - bytes ||
        VS::Render::FrameScratchpad().overlay_ordering_table == nullptr) {
      return nullptr;
    }
    return VS::Render::FrameScratchpad().primitive_cursor;
  }
};

class LiveImageTweenHost final : public event::ImageTweenHost {
 public:
  [[nodiscard]] int EaseFraction(int mode, int frame, int duration) override {
    return VS::Event::EvaluateInterpolationCurveEntry(
        static_cast<std::int16_t>(mode), static_cast<std::int16_t>(frame),
        static_cast<std::int16_t>(duration));
  }
  void PlaceImageLayer(int layer,
                       const event::ImageLayerPlacement& placement) override {
    VS::Render::PlaceLiveGimLayer(layer, placement.x, placement.y,
                                  placement.scale, placement.brightness);
  }
};

event::SplashTween FromRecord(const event::SplashTweenRecord& record) {
  return {.easing_mode = record.easing_mode,
          .elapsed = record.elapsed_frames,
          .duration = record.duration_frames,
          .start_x = record.start_x,
          .start_y = record.start_y,
          .delta_x = record.delta_x,
          .delta_y = record.delta_y,
          .start_scale = record.start_scale_fixed12,
          .delta_scale = record.delta_scale_fixed12,
          .start_brightness = record.start_brightness,
          .delta_brightness = record.delta_brightness};
}

event::WeatherTween FromRecord(const event::WeatherTweenRecord& record) {
  return {.elapsed = record.elapsed_frames,
          .duration = record.duration_frames,
          .wind_direction = record.wind_direction,
          .start_wind_direction = record.start_wind_direction,
          .delta_wind_direction = record.delta_wind_direction,
          .wind_strength = record.wind_strength,
          .start_wind_strength = record.start_wind_strength,
          .delta_wind_strength = record.delta_wind_strength,
          .intensity = record.intensity,
          .start_intensity = record.start_intensity,
          .delta_intensity = record.delta_intensity};
}
}  // namespace

namespace VS::Event {

void DrawLiveScanlineOverlay() {
  LiveScanlineCanvas canvas;
  scanlineOverlayFrame =
      DrawEventScanlineOverlay(scanlineOverlayFrame, VS::Render::viewportWidth,
                               VS::Render::viewportHeight, canvas);
}

void UpdateLiveImageTweens() {
  std::array<SplashTween, kImageTweenCount> tweens{};
  for (std::size_t i = 0; i < tweens.size(); ++i) {
    tweens[i] = FromRecord(splashTweens[i]);
  }
  LiveImageTweenHost host;
  UpdateEventImageTweens(tweens, VS::State::tickSpeed, host);
  for (std::size_t i = 0; i < tweens.size(); ++i) {
    splashTweens[i].elapsed_frames = tweens[i].elapsed;
    splashTweens[i].duration_frames = tweens[i].duration;
  }
}

void UpdateLiveWeatherTween() {
  WeatherTween tween = FromRecord(weatherTween);
  if (!UpdateEventWeatherTween(tween)) {
    return;
  }
  weatherTween.elapsed_frames = tween.elapsed;
  weatherTween.duration_frames = tween.duration;
  weatherTween.wind_direction = tween.wind_direction;
  weatherTween.wind_strength = tween.wind_strength;
  weatherTween.intensity = tween.intensity;
  Field::LiveSetWeatherWindAngles(tween.wind_direction, tween.wind_strength);
  Field::LiveSetWeatherIntensity(tween.intensity);
}

SplashTween LiveSplashTween(int layer) {
  if (layer < 0 || static_cast<std::size_t>(layer) >= kSplashTweenExtent) {
    return {};
  }
  return FromRecord(splashTweens[static_cast<std::size_t>(layer)]);
}

void SetLiveSplashTween(int layer, const SplashTween& tween) {
  if (layer < 0 || static_cast<std::size_t>(layer) >= kSplashTweenExtent) {
    return;
  }
  SplashTweenRecord& record = splashTweens[static_cast<std::size_t>(layer)];
  record.easing_mode = tween.easing_mode;
  record.elapsed_frames = tween.elapsed;
  record.duration_frames = tween.duration;
  record.start_x = tween.start_x;
  record.start_y = tween.start_y;
  record.delta_x = tween.delta_x;
  record.delta_y = tween.delta_y;
  record.start_scale_fixed12 = tween.start_scale;
  record.delta_scale_fixed12 = tween.delta_scale;
  record.start_brightness = tween.start_brightness;
  record.delta_brightness = tween.delta_brightness;
}

WeatherTween LiveWeatherTween() { return FromRecord(weatherTween); }

void StoreLiveWeatherTween(const WeatherTween& tween) {
  weatherTween.elapsed_frames = tween.elapsed;
  weatherTween.duration_frames = tween.duration;
  weatherTween.wind_direction = tween.wind_direction;
  weatherTween.start_wind_direction = tween.start_wind_direction;
  weatherTween.delta_wind_direction = tween.delta_wind_direction;
  weatherTween.wind_strength = tween.wind_strength;
  weatherTween.start_wind_strength = tween.start_wind_strength;
  weatherTween.delta_wind_strength = tween.delta_wind_strength;
  weatherTween.intensity = tween.intensity;
  weatherTween.start_intensity = tween.start_intensity;
  weatherTween.delta_intensity = tween.delta_intensity;
}

}  // namespace VS::Event

// decomp: vs_battle_script_A0 (src/BATTLE/BATTLE.PRG/4DEA8.c:3102; was
// func_800BA8BC), opcode 0xA0
std::intptr_t VS::Event::CommandPlayActorEffectOpcode(
    std::uint8_t* command, std::int16_t /*thread_id*/) {
  return RunCommand(event::CommandPlayActorEffect, command,
                    event::kPlayActorEffectLength);
}

// decomp: vs_battle_script_A1 (src/BATTLE/BATTLE.PRG/4DEA8.c:3108; was
// func_800BA90C), opcode 0xA1
std::intptr_t VS::Event::CommandAdvanceEffectStageOpcode(
    std::uint8_t* command, std::int16_t /*thread_id*/) {
  return RunCommand(event::CommandAdvanceEffectStage, command,
                    event::kAdvanceEffectStageLength);
}

// decomp: vs_battle_script_A2 (src/BATTLE/BATTLE.PRG/4DEA8.c:3117; was
// func_800BA94C), opcode 0xA2
std::intptr_t VS::Event::CommandWaitEffectStateOpcode(
    std::uint8_t* command, std::int16_t /*thread_id*/) {
  return RunCommand(event::CommandWaitEffectState, command,
                    event::kWaitEffectStateLength);
}

// decomp: vs_battle_script_A3 (src/BATTLE/BATTLE.PRG/4DEA8.c:3134; was
// func_800BAA04), opcode 0xA3
std::intptr_t VS::Event::CommandYieldEffectReleaseOpcode(
    std::uint8_t* command, std::int16_t /*thread_id*/) {
  return RunCommand(event::CommandYieldEffectRelease, command,
                    event::kYieldEffectReleaseLength);
}

// decomp: vs_battle_script_A4 (src/BATTLE/BATTLE.PRG/4DEA8.c:3140; was
// func_800BAA34), opcode 0xA4
std::intptr_t VS::Event::CommandWaitEffectContinuationOpcode(
    std::uint8_t* command, std::int16_t /*thread_id*/) {
  return RunCommand(event::CommandWaitEffectContinuation, command,
                    event::kWaitEffectContinuationLength);
}

// decomp: vs_battle_script_A5 (src/BATTLE/BATTLE.PRG/4DEA8.c:3145; was
// func_800BAA58), opcode 0xA5
std::intptr_t VS::Event::CommandWaitEffectReleaseOpcode(
    std::uint8_t* command, std::int16_t /*thread_id*/) {
  return RunCommand(event::CommandWaitEffectRelease, command,
                    event::kWaitEffectReleaseLength);
}

// decomp: vs_battle_script_A6 (src/BATTLE/BATTLE.PRG/4DEA8.c:3157; was
// func_800BAACC), opcode 0xA6
std::intptr_t VS::Event::CommandMarkEffectStageReadyOpcode(
    std::uint8_t* command, std::int16_t /*thread_id*/) {
  return RunCommand(event::CommandMarkEffectStageReady, command,
                    event::kMarkEffectStageReadyLength);
}

// decomp: vs_battle_script_A9 (src/BATTLE/BATTLE.PRG/4DEA8.c:3169; was
// func_800BAB14), opcode 0xA9
std::intptr_t VS::Event::CommandPlayVibrationPatternOpcode(
    std::uint8_t* command, std::int16_t /*thread_id*/) {
  return RunCommand(event::CommandPlayVibrationPattern, command,
                    event::kPlayVibrationPatternLength);
}

// decomp: vs_battle_script_AA (src/BATTLE/BATTLE.PRG/4DEA8.c:3187; was
// func_800BABCC), opcode 0xAA
std::intptr_t VS::Event::CommandSetActorTrailOpcode(
    std::uint8_t* command, std::int16_t /*thread_id*/) {
  return RunCommand(event::CommandSetActorTrail, command,
                    event::kSetActorTrailLength);
}

// decomp: vs_battle_script_B8 (src/BATTLE/BATTLE.PRG/4DEA8.c:3382; was
// func_800BB288), opcode 0xB8
std::intptr_t VS::Event::CommandWaitScreenEffectLoadOpcode(
    std::uint8_t* command, std::int16_t /*thread_id*/) {
  return RunCommand(event::CommandWaitScreenEffectLoad, command,
                    event::kWaitScreenEffectLoadLength);
}

// decomp: vs_battle_script_BB (src/BATTLE/BATTLE.PRG/4DEA8.c:3390; was
// func_800BB2B8), opcode 0xBB
std::intptr_t VS::Event::CommandSetScreenDistortionOpcode(
    std::uint8_t* command, std::int16_t /*thread_id*/) {
  return RunCommand(event::CommandSetScreenDistortion, command,
                    event::kSetScreenDistortionLength);
}

// decomp: vs_battle_script_BC (src/BATTLE/BATTLE.PRG/4DEA8.c:3396; was
// func_800BB2E4), opcode 0xBC
std::intptr_t VS::Event::CommandLoadScreenEffectsOpcode(
    std::uint8_t* command, std::int16_t /*thread_id*/) {
  return RunCommand(event::CommandLoadScreenEffects, command,
                    event::kLoadScreenEffectsLength);
}

// decomp: vs_battle_script_BD (src/BATTLE/BATTLE.PRG/4DEA8.c:3402; was
// func_800BB304), opcode 0xBD
std::intptr_t VS::Event::CommandReservedScreenEffectOpcode(
    std::uint8_t* command, std::int16_t /*thread_id*/) {
  return RunCommand(event::CommandReservedScreenEffect, command,
                    event::kReservedScreenEffectLength);
}

// decomp: vs_battle_script_BE (src/BATTLE/BATTLE.PRG/4DEA8.c:3404; was
// func_800BB30C), opcode 0xBE
std::intptr_t VS::Event::CommandFreeScreenEffectsOpcode(
    std::uint8_t* command, std::int16_t /*thread_id*/) {
  return RunCommand(event::CommandFreeScreenEffects, command,
                    event::kFreeScreenEffectsLength);
}

// decomp: vs_battle_script_F3 (src/BATTLE/BATTLE.PRG/4DEA8.c:3470; was
// func_800BB4F0), opcode 0xF3
std::intptr_t VS::Event::CommandSetScanlineOverlayOpcode(
    std::uint8_t* command, std::int16_t /*thread_id*/) {
  return RunCommand(event::CommandSetScanlineOverlay, command,
                    event::kSetScanlineOverlayLength);
}
