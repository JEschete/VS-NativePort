#pragma once
// Hand-maintained (tools/port/owned.csv).
// The event camera's start state, its field camera override request and its
// origin actor. The live binding (EventCameraOwnershipLive.hpp) allocates the
// camera workspace, applies the start state in the original order and maps
// the origin fields to and from the workspace.
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

#include "Core/Math/Distance.hpp"
#include "Core/Math/ScalarMath.hpp"
#include "Event/EventCameraLens.hpp"
#include "Event/ScriptCommand.hpp"

namespace VS::Event {

// The origin actor id meaning the world origin: no actor offset.
inline constexpr std::uint16_t kEventCameraWorldOrigin = 0x2000;
// Opcode 0xE8: the opcode byte and a 16-bit actor reference.
inline constexpr std::size_t kSetCameraOriginActorLength = 3;
// A camera track's motion mode with no scripted motion.
inline constexpr std::uint8_t kEventCameraTrackIdle = 0xF;
// 1.0 in the screen tweens' 4.12 fixed point.
inline constexpr std::int16_t kEventScreenUnitScale = 0x1000;
inline constexpr std::int16_t kEventCameraProjectionDistance = 0x200;
inline constexpr std::int16_t kEventCameraNearClip = 0x40;
inline constexpr std::int16_t kEventCameraFarClip = 0x1000;
// The framebuffer effect blend mode an event starts with.
inline constexpr int kEventScreenBlendMode = 1;

struct EventCameraTrackStart {
  Core::Math::Vec3 position;
  Core::Math::Vec3 partner;
  std::uint8_t motion_mode = kEventCameraTrackIdle;
  bool follow_requested = false;
  bool following = false;

  friend bool operator==(const EventCameraTrackStart&,
                         const EventCameraTrackStart&) = default;
};

// An angle tween at rest at `angle`; `changed` asks the next update to apply
// it.
struct EventAngleStart {
  bool changed = true;
  std::int16_t angle = 0;
  std::int16_t duration_frames = 0;

  friend bool operator==(const EventAngleStart&,
                         const EventAngleStart&) = default;
};

// A lens tween (projection distance or a clip plane) at rest at `value`.
struct EventLensStart {
  bool changed = true;
  std::int16_t value = 0;
  std::uint8_t duration_frames = 0;

  friend bool operator==(const EventLensStart&,
                         const EventLensStart&) = default;
};

// A screen tween at rest. The start writes the third value only for the tint.
struct EventScreenTweenStart {
  std::int16_t value0 = 0;
  std::int16_t value1 = 0;
  std::optional<std::int16_t> value2{};
  std::int16_t duration_frames = 0;

  friend bool operator==(const EventScreenTweenStart&,
                         const EventScreenTweenStart&) = default;
};

// Every field the event camera start writes. The fields it leaves alone keep
// the allocation's previous contents, as in the original.
struct EventCameraStart {
  EventCameraTrackStart look_at;
  EventCameraTrackStart eye;
  EventAngleStart camera_roll;
  EventAngleStart screen_roll;
  EventScreenTweenStart screen_scale;
  EventScreenTweenStart screen_tint;
  EventScreenTweenStart screen_translation;
  // The radial screen warp (Effects/ScreenDistortion).
  EventScreenTweenStart screen_warp;
  EventLensStart projection_distance;
  EventLensStart near_clip;
  EventLensStart far_clip;
  // The first shake oscillator's envelope word counts the active shakes.
  std::int16_t shake_count = 0;
  // The oscillators' duration bytes; the completion mask sums them (bit 0x80).
  std::array<std::uint8_t, kEventShakeOscillatorCount> shake_durations{};
  std::uint16_t origin_actor = kEventCameraWorldOrigin;
  // The framebuffer effect requests interleaved with the stores.
  bool screen_capture_enabled = false;
  int screen_blend_mode = kEventScreenBlendMode;

  friend bool operator==(const EventCameraStart&,
                         const EventCameraStart&) = default;
};

// The camera fields the origin actor changes.
struct EventCameraOrigin {
  std::uint16_t actor = kEventCameraWorldOrigin;
  // The origin actor's position in 20.12 fixed point, zero for the world.
  Core::Math::Vec3 offset;
  Core::Math::Vec3 eye_position;
  Core::Math::Vec3 look_at_position;

  friend bool operator==(const EventCameraOrigin&,
                         const EventCameraOrigin&) = default;
};

class EventCameraOriginHost {
 public:
  EventCameraOriginHost() = default;
  virtual ~EventCameraOriginHost() = default;
  EventCameraOriginHost(const EventCameraOriginHost&) = delete;
  EventCameraOriginHost& operator=(const EventCameraOriginHost&) = delete;
  EventCameraOriginHost(EventCameraOriginHost&&) = delete;
  EventCameraOriginHost& operator=(EventCameraOriginHost&&) = delete;

  // decomp: BATTLE.PRG resolveActorId (src/BATTLE/BATTLE.PRG/573B8.c:110; was
  // func_800BFE50)
  [[nodiscard]] virtual int ResolveActorReference(std::uint16_t reference) = 0;
  // decomp: BATTLE.PRG vs_battle_getActorPosition
  // (src/BATTLE/BATTLE.PRG/30DB0.c:4299; was func_800A1108) The actor's or slot
  // gimmick's position; nullopt for an empty slot.
  [[nodiscard]] virtual std::optional<Core::Math::Position3> ActorPosition(
      int actor) = 0;
};

// decomp: BATTLE.PRG func_800BBBE8 (F01214, src/BATTLE/BATTLE.PRG/4DEA8.c:3664,
// 0x800BBBE8-0x800BBDDC) The start state from the field camera: each track
// starts at its field position with the other track's position as its partner.
[[nodiscard]] EventCameraStart StartEventCamera(
    Core::Math::Vec3 field_eye, Core::Math::Vec3 field_look_at) noexcept;

// decomp: BATTLE.PRG func_800BBE04 (F01216, src/BATTLE/BATTLE.PRG/4DEA8.c:3734,
// 0x800BBE04-0x800BBE10) The stored field camera override: the request's low
// byte (a byte store).
[[nodiscard]] std::uint8_t EventFieldCameraOverride(int request) noexcept;

// decomp: BATTLE.PRG func_800BBE94 (F01218, src/BATTLE/BATTLE.PRG/4DEA8.c:3751,
// 0x800BBE94-0x800BBF14) Recomputes the offset from the origin actor's
// position. An empty slot gives a zero offset; the original stored stale stack
// bytes.
void UpdateEventCameraOrigin(EventCameraOrigin& origin,
                             EventCameraOriginHost& host);

// decomp: BATTLE.PRG func_800BBE10 (F01217, src/BATTLE/BATTLE.PRG/4DEA8.c:3736,
// 0x800BBE10-0x800BBE94) Returning to the world origin from an actor moves both
// camera positions by the old offset into world space. Leaving the world origin
// moves nothing.
void SetEventCameraOriginActor(EventCameraOrigin& origin, std::uint16_t actor,
                               EventCameraOriginHost& host);

// decomp: BATTLE.PRG vs_battle_script_E8 (F01195,
// src/BATTLE/BATTLE.PRG/4DEA8.c:3423; was func_800BB38C, 0x800BB38C-0x800BB3BC)
// Opcode 0xE8. The resolved actor is truncated to 16 bits. False for a command
// shorter than kSetCameraOriginActorLength, with nothing changed.
[[nodiscard]] bool CommandSetCameraOriginActor(ScriptCommand command,
                                               EventCameraOrigin& origin,
                                               EventCameraOriginHost& host);

}  // namespace VS::Event
