#pragma once
// Hand-maintained (tools/port/owned.csv).
// The event script's effect opcodes and the per-frame event effects: scripted
// actor effects and their waits, controller vibration, weapon trails, the
// screen effect overlay, the scanline overlay, and the image and weather
// tweens. The opcode handlers check their instruction's length and drive the
// effect systems only through ScriptEffectHost; the frame updates work on
// typed tween records and draw through a ScanlineCanvas, so a mod can run
// them against any renderer.
#include <cstddef>
#include <cstdint>
#include <span>

#include "Event/ScriptCommand.hpp"
#include "Event/ScriptStep.hpp"

namespace VS::Event {

// Instruction lengths in bytes, opcode included, from the event VM's opcode
// size table (BATTLE.PRG D_800E9C30).
inline constexpr std::size_t kPlayActorEffectLength = 5;
inline constexpr std::size_t kAdvanceEffectStageLength = 2;
inline constexpr std::size_t kWaitEffectStateLength = 2;
inline constexpr std::size_t kYieldEffectReleaseLength = 1;
inline constexpr std::size_t kWaitEffectContinuationLength = 1;
inline constexpr std::size_t kWaitEffectReleaseLength = 1;
inline constexpr std::size_t kMarkEffectStageReadyLength = 2;
inline constexpr std::size_t kPlayVibrationPatternLength = 2;
inline constexpr std::size_t kSetActorTrailLength = 5;
inline constexpr std::size_t kWaitScreenEffectLoadLength = 1;
inline constexpr std::size_t kSetScreenDistortionLength = 3;
inline constexpr std::size_t kLoadScreenEffectsLength = 1;
inline constexpr std::size_t kReservedScreenEffectLength = 2;
inline constexpr std::size_t kFreeScreenEffectsLength = 1;
inline constexpr std::size_t kSetScanlineOverlayLength = 2;

// The actor slots an effect can target.
inline constexpr int kEffectActorSlots = 16;
// The vibration patterns an event script loads, the selector that stops
// vibration, and the selector bit that plays a pattern from a private copy.
inline constexpr int kVibrationPatternCount = 8;
inline constexpr std::uint8_t kStopVibrationSelector = 0xFF;
inline constexpr std::uint8_t kBufferedVibrationBit = 0x8;
// The scripted effect playback states a skipped wait accepts.
inline constexpr int kEffectStateIdle = 0;
inline constexpr int kEffectStateFinished = 5;
// The splash image tweens the frame update runs (VS::Event::splashTweens).
inline constexpr std::size_t kImageTweenCount = 3;

// Script requests the commands refuse rather than overrun.
enum class ScriptEffectRefusal : std::uint8_t {
  kVibrationPattern,  // a selector past the loaded patterns, or none (#307)
  kShortCommand,      // an instruction shorter than its operands
};

// What an actor slot holds, for the actor effect's target check.
struct EffectActorSlot {
  bool battle_actor = false;   // a battle actor occupies the slot
  std::uint16_t model_id = 0;  // that actor's model, when there is one
  bool occupied = false;       // an actor record or gimmick occupies it
};

// The event state and effect systems the effect opcodes read and change.
class ScriptEffectHost {
 public:
  ScriptEffectHost() = default;
  ScriptEffectHost(const ScriptEffectHost&) = delete;
  ScriptEffectHost& operator=(const ScriptEffectHost&) = delete;
  ScriptEffectHost(ScriptEffectHost&&) = delete;
  ScriptEffectHost& operator=(ScriptEffectHost&&) = delete;
  virtual ~ScriptEffectHost() = default;

  virtual void Refuse(ScriptEffectRefusal refusal, int value) = 0;

  // Actor references and the event VM.
  [[nodiscard]] virtual int ResolveActor(std::uint16_t reference) = 0;
  [[nodiscard]] virtual bool SkipActive() const = 0;
  // `actor` is in [0, kEffectActorSlots).
  [[nodiscard]] virtual EffectActorSlot ActorSlot(int actor) const = 0;

  // Scripted effects; the reports log an effect's target as it starts.
  virtual void ReportUnavailableActor(int effect, std::uint16_t selector,
                                      int actor) = 0;
  virtual void ReportAbsentActor(int effect, std::uint16_t selector,
                                 int actor) = 0;
  virtual void StartActorEffect(int effect, int actor) = 0;
  virtual void ReleaseEffectTrigger(int track) = 0;
  virtual void RequeueEffectTrack(int track) = 0;
  virtual void ClearPrimaryEffects() = 0;
  [[nodiscard]] virtual int EffectPlaybackState() = 0;
  [[nodiscard]] virtual bool EffectTriggerReady() = 0;
  [[nodiscard]] virtual bool EffectWaitPending() const = 0;
  virtual void SetEffectWaitPending(bool pending) = 0;

  // Controller vibration; `pattern` is below kVibrationPatternCount.
  [[nodiscard]] virtual bool VibrationPatternLoaded(int pattern) const = 0;
  virtual void StartVibrationPattern(int pattern, bool buffered) = 0;
  virtual void StopVibration() = 0;

  // Weapon trails on an actor's bone.
  virtual void StartWeaponTrail(int actor, int bone) = 0;
  virtual void FadeWeaponTrails(int actor, int bone) = 0;

  // The screen effect overlay and screen distortion.
  [[nodiscard]] virtual bool ScreenEffectsLoading() const = 0;
  virtual void AcquireScreenEffects() = 0;
  virtual void ReleaseScreenEffects() = 0;
  virtual void SetDistortionLayer(int layer) = 0;
  virtual void SetScanlineOverlay(std::uint8_t enabled) = 0;
};

// decomp: vs_battle_script_A0 (src/BATTLE/BATTLE.PRG/4DEA8.c:3102; was
// func_800BA8BC), opcode 0xA0: starts an effect on an actor, holding while a
// named actor is unavailable.
ScriptStep CommandPlayActorEffect(ScriptCommand command,
                                  ScriptEffectHost& host);
// decomp: vs_battle_script_A1 (src/BATTLE/BATTLE.PRG/4DEA8.c:3108; was
// func_800BA90C), opcode 0xA1
ScriptStep CommandAdvanceEffectStage(ScriptCommand command,
                                     ScriptEffectHost& host);
// decomp: vs_battle_script_A2 (src/BATTLE/BATTLE.PRG/4DEA8.c:3117; was
// func_800BA94C), opcode 0xA2: holds until the scripted effect reaches a state;
// a skip clears it first.
ScriptStep CommandWaitEffectState(ScriptCommand command,
                                  ScriptEffectHost& host);
// decomp: vs_battle_script_A3 (src/BATTLE/BATTLE.PRG/4DEA8.c:3134; was
// func_800BAA04), opcode 0xA3
ScriptStep CommandYieldEffectRelease(ScriptCommand command,
                                     ScriptEffectHost& host);
// decomp: vs_battle_script_A4 (src/BATTLE/BATTLE.PRG/4DEA8.c:3140; was
// func_800BAA34), opcode 0xA4
ScriptStep CommandWaitEffectContinuation(ScriptCommand command,
                                         ScriptEffectHost& host);
// decomp: vs_battle_script_A5 (src/BATTLE/BATTLE.PRG/4DEA8.c:3145; was
// func_800BAA58), opcode 0xA5: clears the effects, then holds until they are
// idle or finished.
ScriptStep CommandWaitEffectRelease(ScriptCommand command,
                                    ScriptEffectHost& host);
// decomp: vs_battle_script_A6 (src/BATTLE/BATTLE.PRG/4DEA8.c:3157; was
// func_800BAACC), opcode 0xA6
ScriptStep CommandMarkEffectStageReady(ScriptCommand command,
                                       ScriptEffectHost& host);
// decomp: vs_battle_script_A9 (src/BATTLE/BATTLE.PRG/4DEA8.c:3169; was
// func_800BAB14), opcode 0xA9
ScriptStep CommandPlayVibrationPattern(ScriptCommand command,
                                       ScriptEffectHost& host);
// decomp: vs_battle_script_AA (src/BATTLE/BATTLE.PRG/4DEA8.c:3187; was
// func_800BABCC), opcode 0xAA
ScriptStep CommandSetActorTrail(ScriptCommand command, ScriptEffectHost& host);
// decomp: vs_battle_script_B8 (src/BATTLE/BATTLE.PRG/4DEA8.c:3382; was
// func_800BB288), opcode 0xB8
ScriptStep CommandWaitScreenEffectLoad(ScriptCommand command,
                                       ScriptEffectHost& host);
// decomp: vs_battle_script_BB (src/BATTLE/BATTLE.PRG/4DEA8.c:3390; was
// func_800BB2B8), opcode 0xBB
ScriptStep CommandSetScreenDistortion(ScriptCommand command,
                                      ScriptEffectHost& host);
// decomp: vs_battle_script_BC (src/BATTLE/BATTLE.PRG/4DEA8.c:3396; was
// func_800BB2E4), opcode 0xBC
ScriptStep CommandLoadScreenEffects(ScriptCommand command,
                                    ScriptEffectHost& host);
// decomp: vs_battle_script_BD (src/BATTLE/BATTLE.PRG/4DEA8.c:3402; was
// func_800BB304), opcode 0xBD
ScriptStep CommandReservedScreenEffect(ScriptCommand command,
                                       ScriptEffectHost& host);
// decomp: vs_battle_script_BE (src/BATTLE/BATTLE.PRG/4DEA8.c:3404; was
// func_800BB30C), opcode 0xBE
ScriptStep CommandFreeScreenEffects(ScriptCommand command,
                                    ScriptEffectHost& host);
// decomp: vs_battle_script_F3 (src/BATTLE/BATTLE.PRG/4DEA8.c:3470; was
// func_800BB4F0), opcode 0xF3
ScriptStep CommandSetScanlineOverlay(ScriptCommand command,
                                     ScriptEffectHost& host);

// Where the scanline overlay draws.
class ScanlineCanvas {
 public:
  ScanlineCanvas() = default;
  ScanlineCanvas(const ScanlineCanvas&) = delete;
  ScanlineCanvas& operator=(const ScanlineCanvas&) = delete;
  ScanlineCanvas(ScanlineCanvas&&) = delete;
  ScanlineCanvas& operator=(ScanlineCanvas&&) = delete;
  virtual ~ScanlineCanvas() = default;

  // A semi-transparent grey line across row `y`, `width` pixels long.
  // NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the line fields.
  virtual void DrawLine(int y, int width, int grey) = 0;
  // Ends the overlay with the subtractive blend the lines draw with.
  virtual void SetSubtractiveBlend() = 0;
};

// The grey a darkened scanline subtracts.
inline constexpr int kScanlineGrey = 0x20;

// decomp: func_800BB874 (src/BATTLE/BATTLE.PRG/4DEA8.c:3586): darkens every
// other row of the viewport, alternating rows every two frames. Takes the
// overlay's frame counter and returns it advanced.
[[nodiscard]] std::uint16_t DrawEventScanlineOverlay(std::uint16_t frame,
                                                     int width, int height,
                                                     ScanlineCanvas& canvas);

// A splash image tween: its eased position, scale and brightness over its
// frames. A zero duration is idle.
struct SplashTween {
  std::uint8_t easing_mode = 0;
  std::uint8_t elapsed = 0;
  std::uint8_t duration = 0;
  std::int16_t start_x = 0;
  std::int16_t start_y = 0;
  std::int16_t delta_x = 0;
  std::int16_t delta_y = 0;
  std::int16_t start_scale = 0;
  std::int16_t delta_scale = 0;
  std::int16_t start_brightness = 0;
  std::int16_t delta_brightness = 0;
};

// A splash image layer's placement for one frame.
struct ImageLayerPlacement {
  int x = 0;
  int y = 0;
  int scale = 0;
  int brightness = 0;
};

// The easing curves and image layers the splash tweens drive.
class ImageTweenHost {
 public:
  ImageTweenHost() = default;
  ImageTweenHost(const ImageTweenHost&) = delete;
  ImageTweenHost& operator=(const ImageTweenHost&) = delete;
  ImageTweenHost(ImageTweenHost&&) = delete;
  ImageTweenHost& operator=(ImageTweenHost&&) = delete;
  virtual ~ImageTweenHost() = default;

  // The eased fraction, in 4096ths, of `frame` through `duration`.
  // NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the curve inputs.
  [[nodiscard]] virtual int EaseFraction(int mode, int frame, int duration) = 0;
  virtual void PlaceImageLayer(int layer,
                               const ImageLayerPlacement& placement) = 0;
};

// decomp: func_800BB9B0 (src/BATTLE/BATTLE.PRG/4DEA8.c:3620): advances each
// running tween by half the frame's ticks and places its layer.
void UpdateEventImageTweens(std::span<SplashTween> tweens, int tick_speed,
                            ImageTweenHost& host);

// The weather tween: wind direction, wind strength and weather intensity
// eased linearly over its frames. A zero duration is idle.
struct WeatherTween {
  std::uint8_t elapsed = 0;
  std::uint8_t duration = 0;
  std::int16_t wind_direction = 0;
  std::int16_t start_wind_direction = 0;
  std::int16_t delta_wind_direction = 0;
  std::int16_t wind_strength = 0;
  std::int16_t start_wind_strength = 0;
  std::int16_t delta_wind_strength = 0;
  std::int16_t intensity = 0;
  std::int16_t start_intensity = 0;
  std::int16_t delta_intensity = 0;
};

// decomp: func_800BBAFC (src/BATTLE/BATTLE.PRG/4DEA8.c:3645): advances a
// running tween one frame and updates its current values. Returns whether
// it ran, so the caller applies them to the weather.
[[nodiscard]] bool UpdateEventWeatherTween(WeatherTween& tween);

}  // namespace VS::Event
