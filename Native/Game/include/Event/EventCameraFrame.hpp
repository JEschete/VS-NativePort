#pragma once
// Hand-maintained (tools/port/owned.csv).
// The event camera's per-frame step: advancing its tracks, tweens and shake,
// handing the result to the field camera and renderer, the completion bits the
// scripts wait on and the opcode that waits for them. The renderer, the field
// camera and the GTE are injected ports; EventCameraFrameLive.hpp binds them
// to Event_camera and the live services.
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

#include "Core/Math/Distance.hpp"
#include "Core/Math/ScalarMath.hpp"
#include "Event/EventCameraLens.hpp"
#include "Event/EventCameraOwnership.hpp"
#include "Event/ScriptCommand.hpp"

namespace VS::Event {
// The wait opcode: the opcode byte and the completion bits it waits for.
inline constexpr std::size_t kWaitCameraTracksLength = 2;

// EventCameraCompletionMask's bits: each is set while its part is at rest.
inline constexpr std::uint8_t kCameraLookAtIdle = 0x01;
inline constexpr std::uint8_t kCameraEyeIdle = 0x02;
inline constexpr std::uint8_t kCameraRollIdle = 0x04;
inline constexpr std::uint8_t kCameraProjectionIdle = 0x08;
inline constexpr std::uint8_t kCameraNearClipIdle = 0x10;
inline constexpr std::uint8_t kCameraFarClipIdle = 0x20;
inline constexpr std::uint8_t kCameraScreenEffectIdle = 0x40;
inline constexpr std::uint8_t kCameraShakeIdle = 0x80;

// The workspace fields the completion mask reads: the tracks' motion modes
// and the remaining durations of the roll, the lenses, the radial warp tween
// and the two shake oscillators. The captured screen roll does not count.
struct EventCameraProgress {
  std::uint8_t look_at_mode = kEventCameraTrackIdle;
  std::uint8_t eye_mode = kEventCameraTrackIdle;
  std::int16_t camera_roll_duration = 0;
  std::uint8_t projection_duration = 0;
  std::uint8_t near_clip_duration = 0;
  std::uint8_t far_clip_duration = 0;
  std::int16_t screen_effect_duration = 0;
  std::array<std::uint8_t, kEventShakeOscillatorCount> shake_durations{};

  friend bool operator==(const EventCameraProgress&,
                         const EventCameraProgress&) = default;
};

// A value the camera applies once when `changed` is set (an angle tween's or
// a lens tween's flag and value).
struct EventCameraSetting {
  std::uint8_t changed = 0;
  std::int16_t value = 0;

  friend bool operator==(const EventCameraSetting&,
                         const EventCameraSetting&) = default;
};

// The workspace fields ApplyEventCameraState reads and writes. The positions
// are 20.12 fixed point.
struct EventCameraFrame {
  Core::Math::Vec3 look_at;
  Core::Math::Vec3 eye;
  std::uint16_t origin_actor = kEventCameraWorldOrigin;
  Core::Math::Vec3 origin_offset;
  // The shake's active oscillator count; the shake applies while it is not 0.
  std::int16_t shake_active_count = 0;
  // The shake offsets in whole units: the workspace's first triple (0x1BE,
  // CameraState::shake_look_at_offset) moves the look-at, the second (0x1C6,
  // shake_eye_offset) the eye.
  Core::Math::Position3 look_at_shake;
  Core::Math::Position3 eye_shake;
  EventCameraSetting camera_roll;
  EventCameraSetting screen_roll;
  EventCameraSetting projection_distance;
  EventCameraSetting near_clip;
  EventCameraSetting far_clip;

  friend bool operator==(const EventCameraFrame&,
                         const EventCameraFrame&) = default;
};

// A look-at and an eye (camera) position, or a value for each track.
struct EventCameraPair {
  Core::Math::Vec3 look_at;
  Core::Math::Vec3 eye;

  friend bool operator==(const EventCameraPair&,
                         const EventCameraPair&) = default;
};

// The shake offset being rotated; the original used one scratch vector for
// each (0x1F800098 for the look-at, 0x1F8000A8 for the eye).
enum class EventShakeTarget : std::uint8_t { kLookAt, kEye };

// The field camera, the renderer and the GTE helpers the camera state is
// applied through.
class EventCameraOutputPort {
 public:
  EventCameraOutputPort() = default;
  virtual ~EventCameraOutputPort() = default;
  EventCameraOutputPort(const EventCameraOutputPort&) = delete;
  EventCameraOutputPort& operator=(const EventCameraOutputPort&) = delete;
  EventCameraOutputPort(EventCameraOutputPort&&) = delete;
  EventCameraOutputPort& operator=(EventCameraOutputPort&&) = delete;

  // decomp: BATTLE.PRG vs_battle_initialiseCameraFromSpherical (146C.c:9071)
  // with facing and mode -1: the field camera's look-at and position.
  [[nodiscard]] virtual EventCameraPair InitializeFieldCamera() = 0;
  // decomp: BATTLE.PRG _atan2FixedPoint (4A0A8.c:4177): the angle of (x, y) in
  // 4096-per-turn units after dropping six fraction bits.
  [[nodiscard]] virtual std::int32_t FixedAtan2(std::int32_t y,
                                                std::int32_t x) = 0;
  // decomp: RotMatrix (SLUS_010.40 0x80041200), built at 0x1F8000F8 from the
  // angles at 0x1F8000D8.
  [[nodiscard]] virtual Core::Math::Mat3 BuildRotation(
      Core::Math::Angles3 rotation) = 0;
  // decomp: ApplyMatrixLV (SLUS_010.40 0x800415C8) on `target`'s scratch
  // vector.
  [[nodiscard]] virtual Core::Math::Vec3 ApplyMatrix(
      const Core::Math::Mat3& rotation, Core::Math::Vec3 vector,
      EventShakeTarget target) = 0;
  // decomp: BATTLE.PRG vs_battle_setCameraLookAt (146C.c:8146)
  virtual void SetLookAt(Core::Math::Vec3 look_at) = 0;
  // decomp: BATTLE.PRG vs_battle_setCameraPosition (146C.c:8132)
  virtual void SetPosition(Core::Math::Vec3 position) = 0;
  // decomp: BATTLE.PRG vs_battle_setCameraRoll (146C.c:8153)
  virtual void SetRoll(std::int16_t angle) = 0;
  // decomp: BATTLE.PRG func_8007DDAC (146C.c:9430): the captured screen's
  // rotation.
  virtual void SetScreenRotation(std::int16_t angle) = 0;
  // decomp: BATTLE.PRG vs_battle_setProjectionDistance (146C.c:9047)
  virtual void SetProjectionDistance(std::int16_t distance) = 0;
  // decomp: BATTLE.PRG vs_battle_setNearClip (146C.c:9041)
  virtual void SetNearClip(std::int16_t near_clip) = 0;
  // decomp: BATTLE.PRG vs_battle_setFarClip (146C.c:9053), with the script
  // backdrop flag (D_800F4C6A).
  virtual void SetFarClip(std::int16_t far_clip,
                          std::uint8_t preserve_texture_lod) = 0;
};

// The updates UpdateEventCameraTracks runs, in its order.
enum class EventCameraStep : std::uint8_t {
  kLookAtTrack,
  kEyeTrack,
  kCameraRoll,
  kScreenRoll,
  kProjectionDistance,
  kNearClip,
  kFarClip,
  kShake,
};

// The workspace's track positions and the per-part updates
// (UpdateEventCameraTrack, UpdateEventAngleTween, UpdateEventLensTween and
// UpdateEventCameraShake on the live workspace).
class EventCameraStepPort {
 public:
  EventCameraStepPort() = default;
  virtual ~EventCameraStepPort() = default;
  EventCameraStepPort(const EventCameraStepPort&) = delete;
  EventCameraStepPort& operator=(const EventCameraStepPort&) = delete;
  EventCameraStepPort(EventCameraStepPort&&) = delete;
  EventCameraStepPort& operator=(EventCameraStepPort&&) = delete;

  // Each track's current position.
  [[nodiscard]] virtual EventCameraPair CurrentPositions() = 0;
  // Each track's partner position.
  virtual void SetPartnerPositions(const EventCameraPair& partners) = 0;
  virtual void Advance(EventCameraStep step) = 0;
};

// decomp: BATTLE.PRG func_800BD610 (F01228, 4A0A8.c:3663,
// 0x800BD610-0x800BD6C4) The kCamera*Idle bits of the parts at rest. The
// shake is at rest when both oscillators' duration bytes, added unsigned, are
// zero.
[[nodiscard]] std::uint8_t EventCameraCompletionMask(
    const EventCameraProgress& progress) noexcept;

// decomp: BATTLE.PRG func_800BB41C (F01197, 4A0A8.c:2788,
// 0x800BB41C-0x800BB450) The wait opcode's result for `completion` (the
// completion mask): 1 while any bit of its operand is not yet set, else 0.
// Nullopt for a command shorter than kWaitCameraTracksLength.
[[nodiscard]] std::optional<std::intptr_t> CommandWaitCameraTracks(
    ScriptCommand command, std::uint8_t completion);

// decomp: BATTLE.PRG vs_battle_applyCameraState (F01219, 4A0A8.c:3117,
// 0x800BBF14-0x800BC1CC) Hands the look-at and eye to the field camera: with
// `use_field_camera` (D_800F4BA0) both tracks take the field camera's
// positions; otherwise the tracks' positions are used, moved by the origin
// offset unless the origin is the world. While the shake runs, its offsets,
// turned by the yaw from the eye to the look-at, are added. Then each changed
// roll, lens and screen rotation is applied and its flag cleared. All vector
// sums wrap at 32 bits.
void ApplyEventCameraState(EventCameraFrame& frame, bool use_field_camera,
                           std::uint8_t preserve_texture_lod,
                           EventCameraOutputPort& output);

// decomp: BATTLE.PRG func_800BC9E0 (F01222, 4A0A8.c:3371,
// 0x800BC9E0-0x800BCA8C) Makes each track's partner the other track's
// current position, then advances the look-at and eye tracks, the camera and
// screen rolls, the three lenses and the shake in that order.
void UpdateEventCameraTracks(EventCameraStepPort& steps);
}  // namespace VS::Event
