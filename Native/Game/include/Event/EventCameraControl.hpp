#pragma once
// Hand-maintained (tools/port/owned.csv).
// The event camera's stop and follow requests (opcodes 0xDE and 0xDF) and its
// returns to the field camera: the field camera restore opcode (0xE9) with
// its glide back to the field view, and the orbit back to the saved field
// view. Both returns start their motion through the typed motion command
// (EventCameraTrack.hpp). The live binding (EventCameraControlLive.hpp) maps
// them to the camera workspace, the field camera and the script flags.
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

#include "Core/Math/ScalarMath.hpp"
#include "Event/EventCameraTrack.hpp"
#include "Event/ScriptCommand.hpp"

namespace VS::Event {

// The follow and restore opcodes' sizes; the stop opcode reads no operand.
inline constexpr std::size_t kSetCameraFollowingLength = 2;
inline constexpr std::size_t kRestoreFieldCameraLength = 3;
// The field camera placement's argument that keeps its yaw or its distance.
inline constexpr std::int16_t kKeepFieldCameraSetting = -1;
// The 20.12 fixed distance the field return glide covers per frame.
inline constexpr std::int32_t kFieldReturnTravelPerFrame = 0x28000;

// The returns' camera motion commands (CameraMotionCommandLength): a linear
// motion takes three bytes and an orbit four, each ending with its duration.
inline constexpr std::size_t kLinearCommandLength = 3;
inline constexpr std::size_t kOrbitCommandLength = 4;
using LinearCommand = std::array<std::uint8_t, kLinearCommandLength>;
using OrbitCommand = std::array<std::uint8_t, kOrbitCommandLength>;

// The field return glide's motion commands (D_800E9C1C) before the glide
// stores its duration in their last bytes: the look-at's linear motion with
// easing 2, then the eye's orbit (easings 2, 2 and 2, the shorter turn).
inline constexpr LinearCommand kFieldReturnLookAtCommand{0xC2, 0x02, 0x00};
inline constexpr OrbitCommand kFieldReturnEyeCommand{0xD4, 0x20, 0x22, 0x00};

struct FieldCameraReturnCommands {
  LinearCommand look_at{};
  OrbitCommand eye{};

  friend bool operator==(const FieldCameraReturnCommands&,
                         const FieldCameraReturnCommands&) = default;
};

// Both field return commands over `duration_frames`, as the original stores
// the narrowed duration at D_800E9C1C + 2 and + 6.
[[nodiscard]] constexpr FieldCameraReturnCommands FieldCameraReturnCommandsFor(
    std::uint8_t duration_frames) {
  FieldCameraReturnCommands commands{.look_at = kFieldReturnLookAtCommand,
                                     .eye = kFieldReturnEyeCommand};
  commands.look_at.back() = duration_frames;
  commands.eye.back() = duration_frames;
  return commands;
}

// The orbit return's motion commands (D_800E9C28, D_800E9C2C): the eye's
// orbit (easings 1, 0 and 1, the shorter turn), then the look-at's linear
// motion with easing 1, both over 85 frames.
inline constexpr OrbitCommand kOrbitReturnEyeCommand{0xD4, 0x10, 0x10, 0x55};
inline constexpr LinearCommand kOrbitReturnLookAtCommand{0xC2, 0x01, 0x55};
// The orbit return's start height by the saved distance preset index
// (D_800E9C24), in whole units: near, then far.
inline constexpr std::size_t kDistancePresetCount = 2;
using OrbitReturnHeights = std::array<std::int16_t, kDistancePresetCount>;
inline constexpr OrbitReturnHeights kOrbitReturnHeightOffsets{-600, -700};

// Where the field camera placement put the eye and the look-at (20.12).
struct FieldCameraPlacement {
  Core::Math::Vec3 eye;
  Core::Math::Vec3 look_at;

  friend bool operator==(const FieldCameraPlacement&,
                         const FieldCameraPlacement&) = default;
};

class EventFieldCameraHost {
 public:
  EventFieldCameraHost() = default;
  virtual ~EventFieldCameraHost() = default;
  EventFieldCameraHost(const EventFieldCameraHost&) = delete;
  EventFieldCameraHost& operator=(const EventFieldCameraHost&) = delete;
  EventFieldCameraHost(EventFieldCameraHost&&) = delete;
  EventFieldCameraHost& operator=(EventFieldCameraHost&&) = delete;

  // decomp: BATTLE.PRG vs_battle_initialiseCameraFromSpherical
  // (src/BATTLE/BATTLE.PRG/146C.c:9071) Turns the field camera to `facing`
  // eighths (-1 keeps the yaw) at distance preset `distance_mode` (1 near,
  // 2 far, any other keeps the distance) around Ashley and returns its eye
  // and look-at.
  // NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
  [[nodiscard]] virtual FieldCameraPlacement PlaceFieldCamera(
      std::int32_t facing, std::int32_t distance_mode) = 0;
};

// The field view the restore opcode asks for.
struct FieldCameraReturnRequest {
  std::int16_t facing = kKeepFieldCameraSetting;
  std::int16_t distance_mode = kKeepFieldCameraSetting;

  friend bool operator==(const FieldCameraReturnRequest&,
                         const FieldCameraReturnRequest&) = default;
};

// What the field return did.
struct FieldCameraReturnGlide {
  // The R3000 quotient of the eye's and the look-at's distances to the field
  // view by kFieldReturnTravelPerFrame.
  std::int32_t travel_frames = 0;
  // The original tested the quotient's low halfword and stored its low byte
  // as the duration: 256 frames start a glide of duration 0.
  bool started = false;
};

// The field view the orbit return goes back to (script flags 0xAF and 0xB0).
struct SavedFieldCameraView {
  std::uint8_t facing = 0;
  // Into kOrbitReturnHeightOffsets; the distance preset is one more.
  std::uint8_t distance_index = 0;
};

// decomp: BATTLE.PRG func_800BE3A0 (F01237, src/BATTLE/BATTLE.PRG/4A0A8.c:4026,
// 0x800BE3A0-0x800BE3D0) Puts both tracks at rest (the eye first) and clears
// their follow requests.
void StopEventCameraMotion(EventCameraTracks& tracks);

// decomp: BATTLE.PRG func_800BB33C (F01193, src/BATTLE/BATTLE.PRG/4A0A8.c:2761,
// 0x800BB33C-0x800BB35C) Opcode 0xDE: stops both tracks. It reads no operand.
void CommandStopCameraMotion(EventCameraTracks& tracks);

// decomp: BATTLE.PRG func_800BE36C (F01236, src/BATTLE/BATTLE.PRG/4A0A8.c:4018,
// 0x800BE36C-0x800BE3A0) Sets each track's follow request; neither follows
// until the track update starts it.
void SetEventCameraFollowing(EventCameraTracks& tracks,
                             std::uint8_t look_at_follow,
                             std::uint8_t eye_follow);

// decomp: BATTLE.PRG func_800BB35C (F01194, src/BATTLE/BATTLE.PRG/4A0A8.c:2767,
// 0x800BB35C-0x800BB38C) Opcode 0xDF: operand bit 0 asks the look-at to
// follow and bit 1 the eye. False, changing nothing, for a command shorter
// than kSetCameraFollowingLength.
[[nodiscard]] bool CommandSetCameraFollowing(ScriptCommand command,
                                             EventCameraTracks& tracks);

// decomp: BATTLE.PRG func_800BB3BC (F01196, src/BATTLE/BATTLE.PRG/4A0A8.c:2780,
// 0x800BB3BC-0x800BB41C) Opcode 0xE9's field view: facing operand 0xFF keeps
// the yaw and the others turn half round ((facing + 4) & 7); distance operand
// 0 keeps the distance. nullopt for a command shorter than
// kRestoreFieldCameraLength.
[[nodiscard]] std::optional<FieldCameraReturnRequest> CommandRestoreFieldCamera(
    ScriptCommand command);

// decomp: BATTLE.PRG func_800BC1CC (F01220, src/BATTLE/BATTLE.PRG/4A0A8.c:3173,
// 0x800BC1CC-0x800BC2E8), its first half: a set field camera override
// (EventFieldCameraOverride) skips one restore. Clears it and returns true
// when set.
[[nodiscard]] bool ConsumeFieldCameraOverride(
    std::uint8_t& skip_next_field_camera_restore);

// decomp: BATTLE.PRG func_800BC1CC (F01220, src/BATTLE/BATTLE.PRG/4A0A8.c:3173,
// 0x800BC1CC-0x800BC2E8), its second half: places the field camera, makes it
// both tracks' destination and glides there over the distance left (current
// less destination) by kFieldReturnTravelPerFrame through
// FieldCameraReturnCommandsFor. The original measured through scratch
// 0x1F800098 and wrote the duration into its module command table.
FieldCameraReturnGlide RestoreEventFieldCamera(
    EventCameraTracks& tracks, FieldCameraReturnRequest request,
    EventFieldCameraHost& field_camera, EventCameraTrackHost& motion);

// decomp: BATTLE.PRG func_800BE3D0 (F01238, src/BATTLE/BATTLE.PRG/4A0A8.c:4039,
// 0x800BE3D0-0x800BE53C) Starts the eye an eighth turn to one side of the
// saved field view (orbit_side 0 before it, 1 after it), raised by its height
// offset, and orbits it back with the look-at gliding along. False when the
// distance index has no height offset: the start is not raised, where the
// original read past D_800E9C24 into the following module data.
[[nodiscard]] bool StartEventCameraOrbitReturn(
    EventCameraTracks& tracks, SavedFieldCameraView view,
    std::uint8_t orbit_side, EventFieldCameraHost& field_camera,
    EventCameraTrackHost& motion);

}  // namespace VS::Event
