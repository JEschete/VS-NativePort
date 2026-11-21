#pragma once
// Hand-maintained (tools/port/owned.csv).
// The event camera's lens tweens (projection distance, near and far clip),
// its two-oscillator shake and the script opcode that configures them. This is
// the event script camera (Event/Camera.hpp); the Effects camera shake is a
// separate system. The live binding (EventCameraLensLive.hpp) maps the
// workspace and the shared scratch words to and from these records.
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

#include "Core/Math/Distance.hpp"
#include "Event/ScriptCommand.hpp"

namespace VS::Event {

inline constexpr std::uint8_t kOpcodeTweenProjectionDistance = 0xEA;
inline constexpr std::uint8_t kOpcodeTweenNearClip = 0xEB;
inline constexpr std::uint8_t kOpcodeTweenFarClip = 0xEC;
inline constexpr std::uint8_t kOpcodeStartCameraShake = 0xEF;
// A lens opcode: the opcode byte, the target, the easing mode and the duration.
inline constexpr std::size_t kCameraLensCommandLength = 4;
// The shake opcode: the opcode byte, cycles, amplitude, axis mask, envelope
// and duration.
inline constexpr std::size_t kCameraShakeCommandLength = 6;
inline constexpr std::size_t kEventShakeOscillatorCount = 2;
// The envelope shapes; any other shape keeps the shared envelope word.
inline constexpr std::uint8_t kShakeEnvelopeFlat = 0;
inline constexpr std::uint8_t kShakeEnvelopeFadeOut = 1;
inline constexpr std::uint8_t kShakeEnvelopeSwell = 2;
inline constexpr std::uint8_t kShakeEnvelopeFadeIn = 3;
// The axis mask's low bits select the shaken tracks: bit 0 sums into the
// look-at offset (workspace 0x1BE), bit 1 into the eye offset (0x1C6), which
// vs_battle_applyCameraState adds to the look-at point and the eye.
inline constexpr std::uint8_t kShakeAxisLookAt = 0x01;
inline constexpr std::uint8_t kShakeAxisEye = 0x02;

// A lens value tweened from `start_value` by `delta_value` over
// `duration_frames`; a zero duration is at rest. `changed` asks the camera
// update to apply the value.
struct EventLensTween {
  std::uint8_t changed = 0;
  std::uint8_t easing_mode = 0;
  std::int16_t value = 0;
  std::int16_t start_value = 0;
  std::int16_t delta_value = 0;
  std::uint8_t elapsed_frames = 0;
  std::uint8_t duration_frames = 0;

  friend bool operator==(const EventLensTween&,
                         const EventLensTween&) = default;
};

struct EventCameraLenses {
  EventLensTween projection_distance;
  EventLensTween near_clip;
  EventLensTween far_clip;

  friend bool operator==(const EventCameraLenses&,
                         const EventCameraLenses&) = default;
};

// One shake oscillator: `cycles` sine turns of `amplitude` over
// `duration_frames`, scaled by the `envelope` shape. A zero duration is idle.
// The axis mask's bits 2-3, 4-5 and 6-7 hold each axis's factor plus one.
struct EventShakeOscillator {
  std::uint8_t envelope = 0;
  std::int16_t displacement = 0;
  std::int16_t cycles = 0;
  std::int16_t amplitude = 0;
  std::uint8_t elapsed_frames = 0;
  std::uint8_t duration_frames = 0;
  std::uint8_t axis_mask = 0;

  friend bool operator==(const EventShakeOscillator&,
                         const EventShakeOscillator&) = default;
};

struct EventCameraShake {
  std::array<EventShakeOscillator, kEventShakeOscillatorCount> oscillators{};
  // The oscillators active in the last update.
  std::int16_t active_count = 0;
  Core::Math::Position3 look_at_offset;
  Core::Math::Position3 eye_offset;

  friend bool operator==(const EventCameraShake&,
                         const EventCameraShake&) = default;
};

// The scratch words the updates share. The lens tweens leave their eased
// fraction in `envelope`, and a shake oscillator with an unknown envelope shape
// reuses whatever it holds, as in the original.
struct EventCameraScratch {
  std::int32_t envelope = 0;
  Core::Math::Position3 look_at_shake;
  Core::Math::Position3 eye_shake;
  Core::Math::Position3 axis_shake;

  friend bool operator==(const EventCameraScratch&,
                         const EventCameraScratch&) = default;
};

class EventCameraLensHost {
 public:
  EventCameraLensHost() = default;
  virtual ~EventCameraLensHost() = default;
  EventCameraLensHost(const EventCameraLensHost&) = delete;
  EventCameraLensHost& operator=(const EventCameraLensHost&) = delete;
  EventCameraLensHost(EventCameraLensHost&&) = delete;
  EventCameraLensHost& operator=(EventCameraLensHost&&) = delete;

  // decomp: vs_gametime_tickspeed (the frames a tick advances)
  [[nodiscard]] virtual int TickSpeed() = 0;
  // decomp: BATTLE.PRG func_800BFEBC (F01276): the Q12 eased fraction.
  [[nodiscard]] virtual int EaseFraction(int mode, int frame, int duration) = 0;
  // decomp: rsin (SLUS_010.40 0x80040F28): the Q12 sine.
  [[nodiscard]] virtual int Sine(int angle) = 0;
};

// decomp: BATTLE.PRG vs_battle_script_EA_EB_EC_EE_EF_camera (F01232,
// src/BATTLE/BATTLE.PRG/4DEA8.c:4512; was func_800BDC9C, 0x800BDC9C-0x800BDF6C)
// Opcodes 0xEA-0xEC start a lens tween or set the lens value at once; 0xEF
// starts the first idle shake oscillator, or stops both when its amplitude is
// zero. The result is the opcode's return value: 0 for those opcodes, and for
// any other opcode byte the register value the original fell through with (0xEA
// below 0xEC, 0xEF from 0xEC up). Nullopt for a command shorter than its
// opcode's length, with nothing changed.
[[nodiscard]] std::optional<std::intptr_t> CommandCameraLensOrShake(
    ScriptCommand command, EventCameraLenses& lenses, EventCameraShake& shake);

// decomp: BATTLE.PRG func_800BDF6C (F01233, src/BATTLE/BATTLE.PRG/4DEA8.c:4577,
// 0x800BDF6C-0x800BE01C) Advances an active lens tween by half the tick speed,
// clamped to its duration, and stores the eased value. The eased fraction is
// left in `fraction_scratch`.
void UpdateEventLensTween(EventLensTween& tween, std::int32_t& fraction_scratch,
                          EventCameraLensHost& host);

// decomp: BATTLE.PRG func_800BE01C (F01234, src/BATTLE/BATTLE.PRG/4DEA8.c:4595,
// 0x800BE01C-0x800BE180) Advances an active oscillator and stores its
// displacement. False for an idle oscillator, with nothing changed. A known
// envelope shape stores its envelope in `envelope_scratch`; another shape uses
// the value already there.
[[nodiscard]] bool UpdateEventShakeOscillator(EventShakeOscillator& oscillator,
                                              std::int32_t& envelope_scratch,
                                              EventCameraLensHost& host);

// decomp: BATTLE.PRG func_800BE180 (F01235, src/BATTLE/BATTLE.PRG/4DEA8.c:4633,
// 0x800BE180-0x800BE36C) Updates both oscillators in order and sums their
// displacements per axis into the eye and look-at offsets, which change only
// while an oscillator is active. The sums wrap at 16 bits.
void UpdateEventCameraShake(EventCameraShake& shake,
                            EventCameraScratch& scratch,
                            EventCameraLensHost& host);

}  // namespace VS::Event
