#pragma once
// Hand-maintained (tools/port/owned.csv).
// The event camera tracks' scripted motion: the motion command (opcodes
// 0xC0-0xDF), each frame's track update, the terrain glide that keeps a track
// above the floor and the distance-scaled durations. The live binding
// (EventCameraTrackLive.hpp) maps the tracks to the camera workspace.
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "Core/Math/Distance.hpp"
#include "Core/Math/Matrix.hpp"
#include "Core/Math/ScalarMath.hpp"
#include "Event/EventCameraOwnership.hpp"
#include "Event/ScriptCommand.hpp"

namespace VS::Event {

// The motion modes the track update animates. kEventCameraTrackIdle is at
// rest; the others are left alone apart from the follow blend.
inline constexpr std::uint8_t kCameraMotionLinear = 2;
inline constexpr std::uint8_t kCameraMotionPartnerRadius = 3;
inline constexpr std::uint8_t kCameraMotionOrbit = 4;
inline constexpr std::uint8_t kCameraMotionPartnerOrbit = 5;
inline constexpr std::uint8_t kCameraMotionBone = 8;
inline constexpr std::uint8_t kCameraMotionPartnerOffset = 9;
inline constexpr std::uint8_t kCameraMotionTerrainGlide = 11;

// A motion opcode's 0x10 bit moves the eye (its partner is the look-at).
inline constexpr std::uint8_t kCameraMotionEyeTrackBit = 0x10;
// The orbit command's turn direction: the other values take the shorter way.
inline constexpr std::uint8_t kCameraTurnNegative = 0xF;
inline constexpr std::uint8_t kCameraTurnPositive = 1;
// The terrain glide's starting position before its first step.
inline constexpr std::int32_t kTerrainPositionUnset = INT32_MIN;
// The duration selector: one track, else 2 for both.
inline constexpr std::int16_t kScaleLookAtDuration = 0;
inline constexpr std::int16_t kScaleBothDurations = 2;
// Three little-endian halfwords of a fixed vector operand.
inline constexpr std::size_t kScriptFixedVectorBytes = 6;

// One camera track (Event_CameraTrack). The positions are 20.12 fixed point.
// The flags are the stored bytes: the update tests them bitwise.
struct EventCameraTrack {
  std::uint8_t motion_mode = kEventCameraTrackIdle;
  std::uint8_t primary_easing = 0;
  std::uint8_t secondary_easing = 0;
  std::uint8_t tertiary_easing = 0;
  std::uint8_t elapsed_frames = 0;
  std::uint8_t duration_frames = 0;
  std::uint8_t follow_request = 0;
  std::uint8_t following = 0;
  // A radius, a resolved actor or a terrain step length, by mode.
  std::int32_t motion_scalar = 0;
  // A radius change, a bone tag or the terrain distance, by mode.
  std::int32_t motion_scalar_delta = 0;
  std::int16_t orbit_angle = 0;
  std::int16_t orbit_angle_delta = 0;
  Core::Math::Vec3 start_position;
  Core::Math::Vec3 destination;
  Core::Math::Vec3 displacement;
  Core::Math::Vec3 height_offset;
  Core::Math::Vec3 follow_delta;
  Core::Math::Vec3 follow_position;
  Core::Math::Vec3 previous_position;
  Core::Math::Vec3 computed_position;
  Core::Math::Vec3 current_position;
  Core::Math::Vec3 partner_position;

  friend bool operator==(const EventCameraTrack&,
                         const EventCameraTrack&) = default;
};

// The camera fields the motion command reads and writes.
struct EventCameraTracks {
  EventCameraTrack look_at;
  EventCameraTrack eye;
  std::uint16_t origin_actor = kEventCameraWorldOrigin;
  Core::Math::Vec3 origin_offset;
  Core::Math::Vec3 last_terrain_position;

  friend bool operator==(const EventCameraTracks&,
                         const EventCameraTracks&) = default;
};

class EventCameraFloorHost {
 public:
  EventCameraFloorHost() = default;
  virtual ~EventCameraFloorHost() = default;
  EventCameraFloorHost(const EventCameraFloorHost&) = delete;
  EventCameraFloorHost& operator=(const EventCameraFloorHost&) = delete;
  EventCameraFloorHost(EventCameraFloorHost&&) = delete;
  EventCameraFloorHost& operator=(EventCameraFloorHost&&) = delete;

  // decomp: BATTLE.PRG func_8008DC7C (src/BATTLE/BATTLE.PRG/146C.c:17083)
  // The floor height at a world position, or the missing-floor value.
  [[nodiscard]] virtual std::int16_t FloorHeight(std::int32_t world_x,
                                                 std::int32_t world_z) = 0;
};

class EventCameraTrackHost : public EventCameraFloorHost {
 public:
  // decomp: BATTLE.PRG func_800BFEBC (src/BATTLE/BATTLE.PRG/573B8.c:168)
  // The Q12 progress of an easing curve at `frame` of `duration`.
  [[nodiscard]] virtual std::int32_t Ease(std::uint8_t curve,
                                          std::uint8_t frame,
                                          std::uint8_t duration) = 0;
  // decomp: BATTLE.PRG func_800BFE50 (src/BATTLE/BATTLE.PRG/573B8.c:156)
  [[nodiscard]] virtual int ResolveActorReference(std::uint16_t reference) = 0;
  // decomp: BATTLE.PRG func_800A1108 (src/BATTLE/BATTLE.PRG/30DB0.c:4182)
  // The actor's or slot gimmick's facing; nullopt for an empty slot.
  [[nodiscard]] virtual std::optional<std::int16_t> ActorFacing(int actor) = 0;
  // decomp: BATTLE.PRG func_800A190C (src/BATTLE/BATTLE.PRG/38C1C.c:246)
  // The endpoint of the actor's bone with `tag`; nullopt when the query
  // fails and writes nothing.
  [[nodiscard]] virtual std::optional<Core::Math::Position3> TaggedBonePosition(
      int actor, int tag) = 0;
};

// What the motion command did.
enum class CameraMotionOutcome : std::uint8_t {
  kApplied,
  // Operations 12-15 change nothing.
  kIgnored,
  // Operation 6 has no case: the original returned its epilogue address.
  kUnassigned,
  // The command is shorter than CameraMotionCommandLength; nothing changed.
  kRefused,
};

// The bytes a motion opcode's operation reads, the opcode included: the
// script's opcode size for every assigned operation, 1 for the others.
[[nodiscard]] std::size_t CameraMotionCommandLength(std::uint8_t opcode);

// decomp: BATTLE.PRG _setVectorImm (F01246, 4A0A8.c:4148,
// 0x800BE7B8-0x800BE824) Three signed halfword operands in whole units, as a
// 20.12 fixed vector.
[[nodiscard]] Core::Math::Vec3 ReadScriptFixedVector(
    std::span<const std::uint8_t, kScriptFixedVectorBytes> operands);

// decomp: BATTLE.PRG func_800BD2B8 (F01225, 4A0A8.c:3556,
// 0x800BD2B8-0x800BD444) The lowest floor under four probes 80 units to each
// side of and 150 ahead of the `probe` frame's origin, less 200 units, in
// 20.12 fixed point. Only the floor heights' low 15 bits count, so the
// missing-floor value reads as 0. The original left `probe` in the GTE
// rotation and translation registers.
[[nodiscard]] std::int32_t EventCameraGroundClearance(
    const Core::Math::Transform3& probe, EventCameraFloorHost& host);

// decomp: BATTLE.PRG func_800BCFB4 (F01224, 4A0A8.c:3502,
// 0x800BCFB4-0x800BD2B8) One terrain glide step towards the destination, at
// most ten step lengths ahead, turning up where the floor rises above the
// path. A step that would land on `last_terrain_position` ends the motion;
// otherwise that position becomes the step's start. The step count is the
// R3000 quotient of the distance by the step length. A direction whose squared
// whole units reach 2^31 normalizes with the unsigned length; the original's
// VectorNormal raised an overflow exception there.
void AdvanceTerrainCameraTrack(EventCameraTrack& track,
                               Core::Math::Vec3& last_terrain_position,
                               EventCameraFloorHost& host);

// decomp: BATTLE.PRG func_800BCA8C (F01223, 4A0A8.c:3389,
// 0x800BCA8C-0x800BCFB4) Advances the track by half `tick_speed` frames
// (except the terrain glide), recomputes its position for the motion mode and
// blends it into the follow path. A bone the host cannot find counts as the
// origin; the original read a stale scratch vector. The partner radius
// direction normalizes as in AdvanceTerrainCameraTrack, where the original
// could raise an overflow exception.
void UpdateEventCameraTrack(EventCameraTrack& track,
                            Core::Math::Vec3& last_terrain_position,
                            std::int32_t tick_speed,
                            EventCameraTrackHost& host);

// decomp: BATTLE.PRG func_800BC2E8 (F01221, 4A0A8.c:3200,
// 0x800BC2E8-0x800BC9E0) Opcodes 0xC0-0xDF: sets a track's destination or
// starts its motion. The terrain glide's step length is the R3000 quotient of
// the distance by the duration (-1 for a zero duration). A bone the host
// cannot find counts as the origin for operation 7 and as (0, facing, 0) for
// operation 10, which is what the original's scratch held; an empty facing
// slot counts as facing 0.
[[nodiscard]] CameraMotionOutcome CommandCameraMotion(
    ScriptCommand command, EventCameraTracks& tracks,
    EventCameraTrackHost& host);

// decomp: BATTLE.PRG func_800BE53C (F01239, 4A0A8.c:4074,
// 0x800BE53C-0x800BE5A4) The duration scaled by the whole-unit distance left
// to the destination over 1000, truncated to a byte. A distance whose wrapped
// sum of squared whole units is negative counts as 0; the original's square
// root read before its table there.
void ScaleEventCameraTrackDuration(EventCameraTrack& track);

// decomp: BATTLE.PRG func_800BE5A4 (F01240, 4A0A8.c:4082,
// 0x800BE5A4-0x800BE628) Scales the look-at track's duration for
// kScaleLookAtDuration, both to their mean for kScaleBothDurations and the
// eye's otherwise.
void ScaleEventCameraDuration(EventCameraTrack& look_at, EventCameraTrack& eye,
                              std::int16_t selector);

}  // namespace VS::Event
