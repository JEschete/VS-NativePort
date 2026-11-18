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

#include "Actors/ActorStateLive.hpp"
#include "Actors/Movement.h"
#include "Battle/Actor.h"
#include "Core/Diagnostics.hpp"
#include "Effects/BoneTrailLive.hpp"
#include "Effects/EffectRuntimeLive.hpp"
#include "Effects/PrimaryEffectLive.hpp"
#include "Effects/Runtime.h"
#include "Effects/ScreenDistortion/ScreenOverlaysLive.hpp"
#include "Effects/ScreenEffects.h"
#include "Effects/ScriptedEffectTriggersLive.hpp"
#include "Event/InterpolationCurveLive.hpp"
#include "Event/ScriptActors.h"
#include "Event/ScriptEffectCommands.hpp"
#include "Event/ScriptEffectOpcodes.hpp"
#include "Event/ScriptEffects.h"
#include "Event/ScriptField.h"
#include "Event/ScriptScreen.h"
#include "Event/ScriptVm.h"
#include "Event/ScriptVmLive.hpp"
#include "Event/Splash.h"
#include "Event/Weather.h"
#include "Field/EnvironmentLive.hpp"
#include "Field/Gimmicks.h"
#include "Input/ControllerLive.hpp"
#include "Legacy/Psx.h"
#include "Render/Display.h"
#include "Render/Frame.h"
#include "Render/GimImagesLive.hpp"
#include "Render/Render.h"
#include "Render/UiPrimitives.h"
#include "State/Timers.hpp"

namespace {
namespace event = VS::Event;

static_assert(std::size(Actors_actorRecords) >=
              static_cast<std::size_t>(event::kEffectActorSlots));
static_assert(std::size(Field_gimmickInstances) >=
              static_cast<std::size_t>(event::kEffectActorSlots));
static_assert(std::size(Event_vibrationPatterns) >=
              static_cast<std::size_t>(event::kVibrationPatternCount));
static_assert(std::size(Event_splashTweens) >= event::kImageTweenCount);
// The original advanced the cursor past a texture page packet to the first
// line's x0, a packet's length further.
static_assert(offsetof(LINE_F2, x0) == sizeof(DR_TPAGE));

constexpr char kSkipActive = 2;
// Vibration patterns open with an 8-byte header; the controller plays all
// motors and the script owns the request.
constexpr std::size_t kVibrationHeaderBytes = 8;
constexpr std::uint32_t kVibrationPatternBytes = 0x80;
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
    return Event_skipState == kSkipActive;
  }
  // The core checks the actor against kEffectActorSlots first.
  [[nodiscard]] event::EffectActorSlot ActorSlot(int actor) const override {
    const Battle_Actor* target = VS::Actors::LiveActorAtSlot(actor);
    return {.battle_actor = target != nullptr,
            .model_id = target != nullptr ? target->modelId
                                          : static_cast<std::uint16_t>(0),
            .occupied = Actors_actorRecords[actor] != nullptr ||
                        Field_gimmickInstances[actor] != nullptr};
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
    return Event_isEffectWaitPending != 0;
  }
  void SetEffectWaitPending(bool pending) override {
    Event_isEffectWaitPending = pending ? 1 : 0;
  }

  [[nodiscard]] bool VibrationPatternLoaded(int pattern) const override {
    const auto lease = event::LeaseLiveVibrationPattern(pattern);
    if (lease) {
      const auto bytes = lease->Bytes();
      return bytes && bytes->size() > kVibrationHeaderBytes;
    }
    if (lease.error() != event::ScriptSourceError::kMissingOwner) {
      return false;
    }
    return Event_vibrationPatterns[pattern] != nullptr;
  }
  // Buffered requests own separate copies, not the original shared scratch.
  void StartVibrationPattern(int pattern, bool buffered) override {
    const auto lease = event::LeaseLiveVibrationPattern(pattern);
    if (lease) {
      const auto bytes = lease->Bytes();
      if (!bytes || bytes->size() <= kVibrationHeaderBytes) {
        Refuse(event::ScriptEffectRefusal::kVibrationPattern, pattern);
        return;
      }
      static_cast<void>(VS::Input::StartLiveOwnedVibrationPattern(
          std::as_bytes(*bytes).subspan(kVibrationHeaderBytes), kAllMotors,
          kScriptVibrationOwner));
      return;
    }
    if (lease.error() != event::ScriptSourceError::kMissingOwner) {
      Refuse(event::ScriptEffectRefusal::kVibrationPattern, pattern);
      return;
    }
    const char* source = Event_vibrationPatterns[pattern];
    if (buffered) {
      const std::span bytes(reinterpret_cast<const std::byte*>(source),
                            kVibrationPatternBytes);
      static_cast<void>(VS::Input::StartLiveOwnedVibrationPattern(
          bytes.subspan(kVibrationHeaderBytes), kAllMotors,
          kScriptVibrationOwner));
      return;
    }
    static_cast<void>(VS::Input::StartLiveVibrationPattern(
        source + kVibrationHeaderBytes, kAllMotors, kScriptVibrationOwner));
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
    return Effects_screenEffectOverlay != nullptr &&
           Effects_screenEffectOverlay->loading != 0;
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
    Event_isScanlineOverlayEnabled = static_cast<char>(enabled);
  }
};

using Handler = event::ScriptStep (*)(event::ScriptCommand,
                                      event::ScriptEffectHost&);

intptr_t RunCommand(Handler handler, const u_char* command,
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
    line->r0 = line->g0 = line->b0 = static_cast<u_char>(grey);
    setSemiTrans(line, 1);
    line->x0 = 0;
    line->y0 = line->y1 = static_cast<std::int16_t>(y);
    line->x1 = static_cast<std::int16_t>(width);
    AddPrim(Render_frameScratch.overlayOrderingTable, line);
    Render_frameScratch.primitiveCursor = line + 1;
  }
  void SetSubtractiveBlend() override {
    auto* page = static_cast<DR_TPAGE*>(Reserve(sizeof(DR_TPAGE)));
    if (page == nullptr) {
      return;
    }
    std::memset(page, 0, sizeof *page);
    SetDrawTPage(page, 1, 1, GetTPage(0, 2, 0, 0));
    AddPrim(Render_frameScratch.overlayOrderingTable, page);
    Render_frameScratch.primitiveCursor = page + 1;
  }

 private:
  [[nodiscard]] static void* Reserve(std::size_t bytes) {
    const auto base = reinterpret_cast<std::uintptr_t>(
        Render_legacyPrimitiveBuffers[Render_frameBuf & 1]);
    const auto cursor =
        reinterpret_cast<std::uintptr_t>(Render_frameScratch.primitiveCursor);
    if (base == 0 || cursor < base ||
        cursor - base > kPrimitiveBufferBytes - bytes ||
        Render_frameScratch.overlayOrderingTable == nullptr) {
      return nullptr;
    }
    return Render_frameScratch.primitiveCursor;
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

// The copied tween records hold their frame counts in plain chars; read
// them as the unsigned bytes the original loaded.
event::SplashTween FromRecord(const Event_SplashTween& record) {
  return {.easing_mode = static_cast<std::uint8_t>(record.easingMode),
          .elapsed = static_cast<std::uint8_t>(record.elapsedFrames),
          .duration = static_cast<std::uint8_t>(record.durationFrames),
          .start_x = record.startX,
          .start_y = record.startY,
          .delta_x = record.deltaX,
          .delta_y = record.deltaY,
          .start_scale = record.startScaleFixed12,
          .delta_scale = record.deltaScaleFixed12,
          .start_brightness = record.startBrightness,
          .delta_brightness = record.deltaBrightness};
}

event::WeatherTween FromRecord(const Event_WeatherTween& record) {
  return {.elapsed = static_cast<std::uint8_t>(record.elapsedFrames),
          .duration = static_cast<std::uint8_t>(record.durationFrames),
          .wind_direction = record.windDirection,
          .start_wind_direction = record.startWindDirection,
          .delta_wind_direction = record.deltaWindDirection,
          .wind_strength = record.windStrength,
          .start_wind_strength = record.startWindStrength,
          .delta_wind_strength = record.deltaWindStrength,
          .intensity = record.intensity,
          .start_intensity = record.startIntensity,
          .delta_intensity = record.deltaIntensity};
}
}  // namespace

namespace VS::Event {

void DrawLiveScanlineOverlay() {
  LiveScanlineCanvas canvas;
  Event_scanlineOverlayFrame =
      DrawEventScanlineOverlay(Event_scanlineOverlayFrame, Render_viewportWidth,
                               Render_viewportHeight, canvas);
}

void UpdateLiveImageTweens() {
  std::array<SplashTween, kImageTweenCount> tweens{};
  for (std::size_t i = 0; i < tweens.size(); ++i) {
    tweens[i] = FromRecord(Event_splashTweens[i]);
  }
  LiveImageTweenHost host;
  UpdateEventImageTweens(tweens, VS::State::tickSpeed, host);
  for (std::size_t i = 0; i < tweens.size(); ++i) {
    Event_splashTweens[i].elapsedFrames = static_cast<char>(tweens[i].elapsed);
    Event_splashTweens[i].durationFrames =
        static_cast<char>(tweens[i].duration);
  }
}

void UpdateLiveWeatherTween() {
  WeatherTween tween = FromRecord(Event_weatherTween);
  if (!UpdateEventWeatherTween(tween)) {
    return;
  }
  Event_weatherTween.elapsedFrames = static_cast<char>(tween.elapsed);
  Event_weatherTween.durationFrames = static_cast<char>(tween.duration);
  Event_weatherTween.windDirection = tween.wind_direction;
  Event_weatherTween.windStrength = tween.wind_strength;
  Event_weatherTween.intensity = tween.intensity;
  Field::LiveSetWeatherWindAngles(tween.wind_direction, tween.wind_strength);
  Field::LiveSetWeatherIntensity(tween.intensity);
}

SplashTween LiveSplashTween(int layer) {
  if (layer < 0 ||
      static_cast<std::size_t>(layer) >= std::size(Event_splashTweens)) {
    return {};
  }
  return FromRecord(Event_splashTweens[static_cast<std::size_t>(layer)]);
}

void SetLiveSplashTween(int layer, const SplashTween& tween) {
  if (layer < 0 ||
      static_cast<std::size_t>(layer) >= std::size(Event_splashTweens)) {
    return;
  }
  Event_SplashTween& record =
      Event_splashTweens[static_cast<std::size_t>(layer)];
  record.easingMode = static_cast<char>(tween.easing_mode);
  record.elapsedFrames = static_cast<char>(tween.elapsed);
  record.durationFrames = static_cast<char>(tween.duration);
  record.startX = tween.start_x;
  record.startY = tween.start_y;
  record.deltaX = tween.delta_x;
  record.deltaY = tween.delta_y;
  record.startScaleFixed12 = tween.start_scale;
  record.deltaScaleFixed12 = tween.delta_scale;
  record.startBrightness = tween.start_brightness;
  record.deltaBrightness = tween.delta_brightness;
}

WeatherTween LiveWeatherTween() { return FromRecord(Event_weatherTween); }

void StoreLiveWeatherTween(const WeatherTween& tween) {
  Event_weatherTween.elapsedFrames = static_cast<char>(tween.elapsed);
  Event_weatherTween.durationFrames = static_cast<char>(tween.duration);
  Event_weatherTween.windDirection = tween.wind_direction;
  Event_weatherTween.startWindDirection = tween.start_wind_direction;
  Event_weatherTween.deltaWindDirection = tween.delta_wind_direction;
  Event_weatherTween.windStrength = tween.wind_strength;
  Event_weatherTween.startWindStrength = tween.start_wind_strength;
  Event_weatherTween.deltaWindStrength = tween.delta_wind_strength;
  Event_weatherTween.intensity = tween.intensity;
  Event_weatherTween.startIntensity = tween.start_intensity;
  Event_weatherTween.deltaIntensity = tween.delta_intensity;
}

}  // namespace VS::Event

// decomp: func_800BA8BC (src/BATTLE/BATTLE.PRG/4A0A8.c:2454), opcode 0xA0
std::intptr_t VS::Event::CommandPlayActorEffectOpcode(
    std::uint8_t* command, std::int16_t /*thread_id*/) {
  return RunCommand(event::CommandPlayActorEffect, command,
                    event::kPlayActorEffectLength);
}

// decomp: func_800BA90C (src/BATTLE/BATTLE.PRG/4A0A8.c:2461), opcode 0xA1
std::intptr_t VS::Event::CommandAdvanceEffectStageOpcode(
    std::uint8_t* command, std::int16_t /*thread_id*/) {
  return RunCommand(event::CommandAdvanceEffectStage, command,
                    event::kAdvanceEffectStageLength);
}

// decomp: func_800BA94C (src/BATTLE/BATTLE.PRG/4A0A8.c:2470), opcode 0xA2
std::intptr_t VS::Event::CommandWaitEffectStateOpcode(
    std::uint8_t* command, std::int16_t /*thread_id*/) {
  return RunCommand(event::CommandWaitEffectState, command,
                    event::kWaitEffectStateLength);
}

// decomp: func_800BAA04 (src/BATTLE/BATTLE.PRG/4A0A8.c:2487), opcode 0xA3
std::intptr_t VS::Event::CommandYieldEffectReleaseOpcode(
    std::uint8_t* command, std::int16_t /*thread_id*/) {
  return RunCommand(event::CommandYieldEffectRelease, command,
                    event::kYieldEffectReleaseLength);
}

// decomp: func_800BAA34 (src/BATTLE/BATTLE.PRG/4A0A8.c:2493), opcode 0xA4
std::intptr_t VS::Event::CommandWaitEffectContinuationOpcode(
    std::uint8_t* command, std::int16_t /*thread_id*/) {
  return RunCommand(event::CommandWaitEffectContinuation, command,
                    event::kWaitEffectContinuationLength);
}

// decomp: func_800BAA58 (src/BATTLE/BATTLE.PRG/4A0A8.c:2495), opcode 0xA5
std::intptr_t VS::Event::CommandWaitEffectReleaseOpcode(
    std::uint8_t* command, std::int16_t /*thread_id*/) {
  return RunCommand(event::CommandWaitEffectRelease, command,
                    event::kWaitEffectReleaseLength);
}

// decomp: func_800BAACC (src/BATTLE/BATTLE.PRG/4A0A8.c:2507), opcode 0xA6
std::intptr_t VS::Event::CommandMarkEffectStageReadyOpcode(
    std::uint8_t* command, std::int16_t /*thread_id*/) {
  return RunCommand(event::CommandMarkEffectStageReady, command,
                    event::kMarkEffectStageReadyLength);
}

// decomp: func_800BAB14 (src/BATTLE/BATTLE.PRG/4A0A8.c:2519), opcode 0xA9
std::intptr_t VS::Event::CommandPlayVibrationPatternOpcode(
    std::uint8_t* command, std::int16_t /*thread_id*/) {
  return RunCommand(event::CommandPlayVibrationPattern, command,
                    event::kPlayVibrationPatternLength);
}

// decomp: func_800BABCC (src/BATTLE/BATTLE.PRG/4A0A8.c:2537), opcode 0xAA
std::intptr_t VS::Event::CommandSetActorTrailOpcode(
    std::uint8_t* command, std::int16_t /*thread_id*/) {
  return RunCommand(event::CommandSetActorTrail, command,
                    event::kSetActorTrailLength);
}

// decomp: func_800BB288 (src/BATTLE/BATTLE.PRG/4A0A8.c:2733), opcode 0xB8
std::intptr_t VS::Event::CommandWaitScreenEffectLoadOpcode(
    std::uint8_t* command, std::int16_t /*thread_id*/) {
  return RunCommand(event::CommandWaitScreenEffectLoad, command,
                    event::kWaitScreenEffectLoadLength);
}

// decomp: func_800BB2B8 (src/BATTLE/BATTLE.PRG/4A0A8.c:2741), opcode 0xBB
std::intptr_t VS::Event::CommandSetScreenDistortionOpcode(
    std::uint8_t* command, std::int16_t /*thread_id*/) {
  return RunCommand(event::CommandSetScreenDistortion, command,
                    event::kSetScreenDistortionLength);
}

// decomp: func_800BB2E4 (src/BATTLE/BATTLE.PRG/4A0A8.c:2747), opcode 0xBC
std::intptr_t VS::Event::CommandLoadScreenEffectsOpcode(
    std::uint8_t* command, std::int16_t /*thread_id*/) {
  return RunCommand(event::CommandLoadScreenEffects, command,
                    event::kLoadScreenEffectsLength);
}

// decomp: func_800BB304 (src/BATTLE/BATTLE.PRG/4A0A8.c:2753), opcode 0xBD
std::intptr_t VS::Event::CommandReservedScreenEffectOpcode(
    std::uint8_t* command, std::int16_t /*thread_id*/) {
  return RunCommand(event::CommandReservedScreenEffect, command,
                    event::kReservedScreenEffectLength);
}

// decomp: func_800BB30C (src/BATTLE/BATTLE.PRG/4A0A8.c:2755), opcode 0xBE
std::intptr_t VS::Event::CommandFreeScreenEffectsOpcode(
    std::uint8_t* command, std::int16_t /*thread_id*/) {
  return RunCommand(event::CommandFreeScreenEffects, command,
                    event::kFreeScreenEffectsLength);
}

// decomp: func_800BB4F0 (src/BATTLE/BATTLE.PRG/4A0A8.c:2821), opcode 0xF3
std::intptr_t VS::Event::CommandSetScanlineOverlayOpcode(
    std::uint8_t* command, std::int16_t /*thread_id*/) {
  return RunCommand(event::CommandSetScanlineOverlay, command,
                    event::kSetScanlineOverlayLength);
}
