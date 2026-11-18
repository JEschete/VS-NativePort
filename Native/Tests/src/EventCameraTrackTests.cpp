#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <limits>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Core/Math/Distance.hpp"
#include "Core/Math/Matrix.hpp"
#include "Core/Math/Vector.hpp"
#include "Event/Camera.hpp"
#include "Event/EventCameraOwnership.hpp"
#include "Event/EventCameraTrack.hpp"
#include "Event/EventCameraTrackLive.hpp"
#include "Event/ScriptCamera.h"
#include "Event/ScriptCommand.hpp"
#include "State/Timers.hpp"

namespace {
namespace event = VS::Event;
using VS::Core::Math::Position3;
using VS::Core::Math::Transform3;
using VS::Core::Math::Vec3;

constexpr std::int32_t kUnit = 4096;
constexpr std::int32_t kQuarterTurn = 1024;

struct EaseCall {
  std::uint8_t curve;
  std::uint8_t frame;
  std::uint8_t duration;
  friend bool operator==(const EaseCall&, const EaseCall&) = default;
};

// Linear easing, queued floor heights (then a flat floor) and recorded
// calls.
class FakeTrackHost final : public event::EventCameraTrackHost {
 public:
  std::int16_t flat_floor = 0;
  std::deque<std::int16_t> floors;
  std::vector<std::pair<std::int32_t, std::int32_t>> floor_queries;
  std::vector<EaseCall> eases;
  int resolved = 0;
  std::vector<std::uint16_t> references;
  std::optional<std::int16_t> facing;
  std::vector<int> facing_actors;
  std::optional<Position3> bone;
  std::vector<std::pair<int, int>> bone_queries;
  // The floor a world x at or past `rise_x` reads, when set.
  std::optional<std::int32_t> rise_x;
  std::int16_t risen_floor = 0;

  std::int16_t FloorHeight(std::int32_t world_x,
                           std::int32_t world_z) override {
    floor_queries.emplace_back(world_x, world_z);
    if (rise_x.has_value() && world_x >= *rise_x) return risen_floor;
    if (floors.empty()) return flat_floor;
    const std::int16_t height = floors.front();
    floors.pop_front();
    return height;
  }

  std::int32_t Ease(std::uint8_t curve, std::uint8_t frame,
                    std::uint8_t duration) override {
    eases.push_back({curve, frame, duration});
    if (duration == 0) return kUnit;
    return frame * kUnit / duration;
  }

  int ResolveActorReference(std::uint16_t reference) override {
    references.push_back(reference);
    return resolved;
  }

  std::optional<std::int16_t> ActorFacing(int actor) override {
    facing_actors.push_back(actor);
    return facing;
  }

  std::optional<Position3> TaggedBonePosition(int actor, int tag) override {
    bone_queries.emplace_back(actor, tag);
    return bone;
  }
};

constexpr Vec3 Units(std::int32_t x, std::int32_t y, std::int32_t z) {
  return {x * kUnit, y * kUnit, z * kUnit};
}

constexpr Vec3 kTerrainUnset{
    event::kTerrainPositionUnset,
    event::kTerrainPositionUnset,
    event::kTerrainPositionUnset,
};

event::CameraMotionOutcome Run(std::span<const std::uint8_t> bytes,
                               event::EventCameraTracks& tracks,
                               FakeTrackHost& host) {
  return event::CommandCameraMotion(event::ScriptCommand{bytes}, tracks, host);
}

// A track at rest with distinct positions.
event::EventCameraTrack RestingTrack() {
  event::EventCameraTrack track;
  track.elapsed_frames = 5;
  track.current_position = Units(1, 2, 3);
  track.destination = Units(11, -8, 23);
  track.partner_position = Units(-4, 5, -6);
  return track;
}

event::EventCameraTracks RestingTracks() {
  event::EventCameraTracks tracks;
  tracks.look_at = RestingTrack();
  tracks.eye = RestingTrack();
  tracks.eye.current_position = Units(7, 8, 9);
  tracks.last_terrain_position = Units(1, 1, 1);
  return tracks;
}

// An orbit start from 64 units along z of the origin partner, three units
// up, towards `destination`.
event::EventCameraTrack OrbitStartTrack(Vec3 destination) {
  event::EventCameraTrack track;
  track.current_position = {0, 0x3000, 0x40000};
  track.destination = destination;
  return track;
}

std::int16_t OrbitTurnTo(Vec3 destination, std::uint8_t direction) {
  event::EventCameraTracks tracks;
  tracks.look_at = OrbitStartTrack(destination);
  FakeTrackHost host;
  const std::array<std::uint8_t, 4> bytes{0x04, direction, 0x21, 40};
  static_cast<void>(Run(bytes, tracks, host));
  return tracks.look_at.orbit_angle_delta;
}

// A terrain glide along x on a flat floor 200 units above y = 0: the
// clearance puts the track at y = -200 units. Power-of-two distances keep the
// table square root and the normalized direction exact.
event::EventCameraTrack FlatGlideTrack(std::int32_t step_units,
                                       std::int32_t distance_units) {
  event::EventCameraTrack track;
  track.motion_mode = event::kCameraMotionTerrainGlide;
  track.duration_frames = 30;
  track.elapsed_frames = 2;
  track.motion_scalar = step_units * kUnit;
  track.computed_position = Units(0, -200, 0);
  track.destination = Units(distance_units, -200, 0);
  track.partner_position = Units(0, -200, 20000);
  return track;
}

Vec3 VectorOf(const VECTOR& vector) {
  return {static_cast<std::int32_t>(vector.vx),
          static_cast<std::int32_t>(vector.vy),
          static_cast<std::int32_t>(vector.vz)};
}

// Points Event_camera at a zeroed workspace for the entry tests.
struct LiveTrackWorkspace {
  VS::Event::CameraState camera{};
  VS::Event::CameraState* saved_camera = Event_camera;
  int saved_tick_speed = VS::State::tickSpeed;

  LiveTrackWorkspace() { Event_camera = &camera; }
  LiveTrackWorkspace(const LiveTrackWorkspace&) = delete;
  LiveTrackWorkspace& operator=(const LiveTrackWorkspace&) = delete;
  LiveTrackWorkspace(LiveTrackWorkspace&&) = delete;
  LiveTrackWorkspace& operator=(LiveTrackWorkspace&&) = delete;
  ~LiveTrackWorkspace() {
    Event_camera = saved_camera;
    VS::State::tickSpeed = saved_tick_speed;
  }
};

// The orbit command 0x04 on a look-at track a quarter turn from its partner.
event::EventCameraTrack RunOrbit() {
  event::EventCameraTracks tracks;
  tracks.look_at = OrbitStartTrack({0x80000, 0x7000, 0});
  FakeTrackHost host;
  const std::array<std::uint8_t, 4> bytes{0x04, 0x30, 0x21, 40};
  REQUIRE(Run(bytes, tracks, host) == event::CameraMotionOutcome::kApplied);
  return tracks.look_at;
}

// The partner orbit command 0x05 around a partner 0x9000 above the track.
event::EventCameraTrack RunPartnerOrbit() {
  event::EventCameraTracks tracks;
  tracks.look_at = OrbitStartTrack({});
  tracks.look_at.partner_position = {0, 0x9000, 0};
  FakeTrackHost host;
  const std::array<std::uint8_t, 9> bytes{0x05, 0x00, 0x02, 0xFE, 0xFF,
                                          0x80, 0x21, 0x53, 30};
  REQUIRE(Run(bytes, tracks, host) == event::CameraMotionOutcome::kApplied);
  return tracks.look_at;
}

// The follow-bone command 0x08: actor reference 0x1003, bone tag 12.
void RunFollowBone(event::EventCameraTracks& tracks, FakeTrackHost& host) {
  host.resolved = 0x1003;
  const std::array<std::uint8_t, 11> bone{0x08, 3, 0x10, 12, 1, 0,
                                          0,    0, 0,    0,  45};
  REQUIRE(Run(bone, tracks, host) == event::CameraMotionOutcome::kApplied);
}
}  // namespace

// F01246 _setVectorImm

TEST_CASE("Script fixed vector reads signed whole-unit halfwords",
          "[event][camera][track]") {
  const std::array<std::uint8_t, 6> bytes{0x01, 0x00, 0xFF, 0xFF, 0x00, 0x80};
  CHECK(event::ReadScriptFixedVector(bytes) ==
        Vec3{kUnit, -kUnit, -0x8000 * kUnit});
}

TEST_CASE("Script fixed vector entry writes three words and keeps the pad",
          "[event][camera][track]") {
  std::array<std::uint8_t, 7> bytes{0x02, 0x00, 0xFE, 0xFF, 0xFF, 0x7F, 0x99};
  VECTOR destination{1, 2, 3, 0x55};
  CHECK(event::ReadScriptFixedVectorEntry(&destination, bytes.data()) ==
        &destination);
  CHECK(VectorOf(destination) == Vec3{2 * kUnit, -2 * kUnit, 0x7FFF * kUnit});
  CHECK(destination.pad == 0x55);
}

// F01225 func_800BD2B8

TEST_CASE("Event camera ground clearance keeps the lowest of four probes",
          "[event][camera][track]") {
  FakeTrackHost host;
  host.floors = {-300, -500, -100, -400};
  const Transform3 probe{.rotation = VS::Core::Math::IdentityRotation(),
                         .translation = {100, 7, 200}};
  CHECK(event::EventCameraGroundClearance(probe, host) == -700 * kUnit);
  const std::vector<std::pair<std::int32_t, std::int32_t>> expected{
      {180, 350}, {20, 350}, {180, 350}, {20, 350}};
  CHECK(host.floor_queries == expected);
}

TEST_CASE("Event camera ground clearance reads the low 15 floor bits",
          "[event][camera][track]") {
  const Transform3 probe{.rotation = VS::Core::Math::IdentityRotation(),
                         .translation = {}};
  FakeTrackHost missing;
  missing.flat_floor = std::numeric_limits<std::int16_t>::min();
  CHECK(event::EventCameraGroundClearance(probe, missing) == -200 * kUnit);
  FakeTrackHost high;
  high.flat_floor = 0x4000;
  CHECK(event::EventCameraGroundClearance(probe, high) == -16584 * kUnit);
  FakeTrackHost low_bits;
  low_bits.floors = {0x7FFF, 0, 0, 0};
  CHECK(event::EventCameraGroundClearance(probe, low_bits) == -201 * kUnit);
}

// F01224 func_800BCFB4

TEST_CASE("Terrain camera step first use records its start",
          "[event][camera][track]") {
  FakeTrackHost host;
  event::EventCameraTrack track = FlatGlideTrack(16, 256);
  Vec3 last = kTerrainUnset;
  event::AdvanceTerrainCameraTrack(track, last, host);
  CHECK(track.computed_position == Units(16, -200, 0));
  CHECK(last == Units(0, -200, 0));
  CHECK(track.elapsed_frames == 2);
  // The destination's four probes and four for each of the 16 steps capped
  // at ten.
  CHECK(host.floor_queries.size() == 44);
}

TEST_CASE("Terrain camera step repeat use moves on from the last start",
          "[event][camera][track]") {
  FakeTrackHost host;
  event::EventCameraTrack track = FlatGlideTrack(64, 256);
  Vec3 last = kTerrainUnset;
  event::AdvanceTerrainCameraTrack(track, last, host);
  track.destination = Units(320, -200, 0);
  event::AdvanceTerrainCameraTrack(track, last, host);
  CHECK(track.computed_position == Units(128, -200, 0));
  CHECK(last == Units(64, -200, 0));
  CHECK(track.elapsed_frames == 2);
}

TEST_CASE("Terrain camera step back onto the last start ends the motion",
          "[event][camera][track]") {
  FakeTrackHost host;
  event::EventCameraTrack track = FlatGlideTrack(16, 256);
  Vec3 last = Units(16, -200, 0);
  event::AdvanceTerrainCameraTrack(track, last, host);
  CHECK(track.computed_position == Units(0, -200, 0));
  CHECK(last == Units(16, -200, 0));
  CHECK(track.elapsed_frames == track.duration_frames);
}

TEST_CASE("Terrain camera glide restart resets the last start sentinel",
          "[event][camera][track]") {
  FakeTrackHost host;
  event::EventCameraTracks tracks;
  tracks.look_at = FlatGlideTrack(1, 256);
  tracks.look_at.current_position = Units(0, 0, 0);
  // A stale sentinel the restarted glide's first step would land on.
  tracks.last_terrain_position = Units(16, -200, 0);
  const std::array<std::uint8_t, 3> restart{0x0B, 0x00, 16};
  REQUIRE(Run(restart, tracks, host) == event::CameraMotionOutcome::kApplied);
  CHECK(tracks.last_terrain_position == kTerrainUnset);
  event::AdvanceTerrainCameraTrack(tracks.look_at, tracks.last_terrain_position,
                                   host);
  CHECK(tracks.look_at.computed_position == Units(16, -200, 0));
  CHECK(tracks.look_at.elapsed_frames == 0);
  CHECK(tracks.last_terrain_position == Units(0, -200, 0));
}

TEST_CASE("Terrain camera step under two step lengths ends the motion",
          "[event][camera][track]") {
  FakeTrackHost host;
  event::EventCameraTrack track = FlatGlideTrack(16, 16);
  Vec3 last = kTerrainUnset;
  event::AdvanceTerrainCameraTrack(track, last, host);
  CHECK(track.elapsed_frames == track.duration_frames);
  CHECK(track.computed_position == Units(16, -200, 0));
  CHECK(host.floor_queries.size() == 8);
}

TEST_CASE("Terrain camera step turns up where the floor rises",
          "[event][camera][track]") {
  FakeTrackHost host;
  host.rise_x = 350;
  host.risen_floor = -100;
  event::EventCameraTrack track = FlatGlideTrack(100, 1000);
  Vec3 last = kTerrainUnset;
  event::AdvanceTerrainCameraTrack(track, last, host);
  // The destination, then three steps until a probe 80 units right of
  // the third reaches the rise.
  CHECK(host.floor_queries.size() == 16);
  const Vec3 step = VS::Core::Math::SubtractVectors(track.computed_position,
                                                    Units(0, -200, 0));
  // Steeper than the straight path's one unit up in ten.
  CHECK(step.x > 0);
  CHECK(-step.y * 10 > step.x);
  CHECK(step.z == 0);
}

TEST_CASE("Terrain camera step with a zero or negative length ends at once",
          "[event][camera][track]") {
  FakeTrackHost host;
  // A zero-duration glide's step length is the zero-divisor quotient -1:
  // the distance over -1 is negative, so the step reverses.
  event::EventCameraTrack reversed = FlatGlideTrack(0, 128);
  reversed.motion_scalar = -1;
  Vec3 last = kTerrainUnset;
  event::AdvanceTerrainCameraTrack(reversed, last, host);
  CHECK(reversed.elapsed_frames == reversed.duration_frames);
  CHECK(reversed.computed_position == Vec3{-1, -200 * kUnit, 0});
  // A zero length divides to -1 steps and stays put.
  event::EventCameraTrack stalled = FlatGlideTrack(0, 128);
  Vec3 stalled_last = kTerrainUnset;
  event::AdvanceTerrainCameraTrack(stalled, stalled_last, host);
  CHECK(stalled.elapsed_frames == stalled.duration_frames);
  CHECK(stalled.computed_position == Units(0, -200, 0));
  CHECK(stalled_last == Units(0, -200, 0));
}

// F01223 func_800BCA8C

TEST_CASE("Camera track update advances half the tick speed",
          "[event][camera][track]") {
  FakeTrackHost host;
  event::EventCameraTrack track;
  track.motion_mode = event::kCameraMotionLinear;
  track.primary_easing = 3;
  track.elapsed_frames = 4;
  track.duration_frames = 10;
  track.displacement = Units(16, 0, 0);
  track.current_position = Units(5, 5, 5);
  Vec3 last = kTerrainUnset;
  event::UpdateEventCameraTrack(track, last, 4, host);
  CHECK(track.elapsed_frames == 6);
  CHECK(host.eases == std::vector<EaseCall>{{3, 6, 10}});
  CHECK(track.computed_position == Vec3{16 * 2457, 0, 0});
  CHECK(track.current_position == track.computed_position);
  CHECK(track.previous_position == Units(5, 5, 5));
  CHECK(track.motion_mode == event::kCameraMotionLinear);
  CHECK(last == kTerrainUnset);
}

TEST_CASE("Camera track update stops at the duration",
          "[event][camera][track]") {
  FakeTrackHost host;
  event::EventCameraTrack track;
  track.motion_mode = event::kCameraMotionLinear;
  track.elapsed_frames = 9;
  track.duration_frames = 10;
  track.start_position = Units(1, 1, 1);
  track.displacement = Units(16, 0, 0);
  Vec3 last;
  event::UpdateEventCameraTrack(track, last, 4, host);
  CHECK(track.elapsed_frames == 10);
  CHECK(track.current_position == Units(17, 1, 1));
  CHECK(track.motion_mode == event::kEventCameraTrackIdle);
  CHECK(track.following == 0);
}

TEST_CASE("Camera track update wraps the elapsed byte before the cap",
          "[event][camera][track]") {
  FakeTrackHost host;
  event::EventCameraTrack track;
  track.motion_mode = event::kCameraMotionPartnerOffset;
  track.elapsed_frames = 254;
  track.duration_frames = 255;
  Vec3 last;
  event::UpdateEventCameraTrack(track, last, 6, host);
  CHECK(track.elapsed_frames == 1);
  CHECK(track.motion_mode == event::kCameraMotionPartnerOffset);
}

TEST_CASE("Camera track update leaves an idle track alone",
          "[event][camera][track]") {
  FakeTrackHost host;
  event::EventCameraTrack track = RestingTrack();
  const event::EventCameraTrack before = track;
  Vec3 last;
  event::UpdateEventCameraTrack(track, last, 4, host);
  CHECK(track == before);
  CHECK(host.eases.empty());
}

TEST_CASE("Camera track update blends into the follow path",
          "[event][camera][track]") {
  FakeTrackHost host;
  event::EventCameraTrack track;
  track.motion_mode = event::kCameraMotionPartnerOffset;
  track.duration_frames = 4;
  track.follow_request = 1;
  track.following = 1;
  track.displacement = Units(16, 0, 0);
  track.follow_delta = {0x1000, 0, 0};
  Vec3 last;
  event::UpdateEventCameraTrack(track, last, 2, host);
  CHECK(host.eases == std::vector<EaseCall>{{1, 1, 4}});
  CHECK(track.follow_position == Vec3{0x1000, 0, 0});
  CHECK(track.computed_position == Units(16, 0, 0));
  CHECK(track.current_position == Vec3{0x4C00, 0, 0});
}

TEST_CASE("Camera track update starts following at the end of the motion",
          "[event][camera][track]") {
  FakeTrackHost host;
  event::EventCameraTrack track;
  track.motion_mode = event::kCameraMotionPartnerOffset;
  track.elapsed_frames = 3;
  track.duration_frames = 4;
  track.follow_request = 1;
  track.current_position = Units(1, 0, 0);
  track.displacement = Units(4, 0, 0);
  Vec3 last;
  event::UpdateEventCameraTrack(track, last, 2, host);
  CHECK(track.motion_mode == event::kEventCameraTrackIdle);
  CHECK(track.following == 1);
  CHECK(track.follow_position == Units(4, 0, 0));
  CHECK(track.follow_delta == Units(3, 0, 0));
}

TEST_CASE("Camera track update follows a bone or the origin without one",
          "[event][camera][track]") {
  FakeTrackHost host;
  event::EventCameraTrack track;
  track.motion_mode = event::kCameraMotionBone;
  track.duration_frames = 100;
  track.motion_scalar = 3;
  track.motion_scalar_delta = 9;
  track.displacement = Units(0, 1, 0);
  host.bone = Position3{.x = 1, .y = 2, .z = 3};
  Vec3 last;
  event::UpdateEventCameraTrack(track, last, 2, host);
  CHECK(track.computed_position == Units(1, 3, 3));
  host.bone.reset();
  event::UpdateEventCameraTrack(track, last, 2, host);
  CHECK(track.computed_position == Units(0, 1, 0));
  CHECK(host.bone_queries == std::vector<std::pair<int, int>>{{3, 9}, {3, 9}});
}

TEST_CASE("Camera track update orbits the start and the destination",
          "[event][camera][track]") {
  FakeTrackHost host;
  event::EventCameraTrack orbit;
  orbit.motion_mode = event::kCameraMotionOrbit;
  orbit.elapsed_frames = 9;
  orbit.duration_frames = 10;
  orbit.motion_scalar = 0x40000;
  orbit.motion_scalar_delta = 0x40000;
  orbit.orbit_angle_delta = kQuarterTurn;
  orbit.start_position = Units(1, 1, 1);
  orbit.height_offset.y = 3 * kUnit;
  orbit.displacement.y = 4 * kUnit;
  event::EventCameraTrack partner_orbit = orbit;
  partner_orbit.motion_mode = event::kCameraMotionPartnerOrbit;
  partner_orbit.destination = Units(-5, 50, -5);
  Vec3 last;
  event::UpdateEventCameraTrack(orbit, last, 2, host);
  event::UpdateEventCameraTrack(partner_orbit, last, 2, host);
  CHECK(orbit.computed_position == Units(129, 8, 1));
  CHECK(partner_orbit.computed_position == Units(123, 5, -5));
}

TEST_CASE("Camera track update leaves the terrain glide's time to its steps",
          "[event][camera][track]") {
  FakeTrackHost host;
  host.flat_floor = 0;
  event::EventCameraTrack track = FlatGlideTrack(16, 256);
  Vec3 last = kTerrainUnset;
  event::UpdateEventCameraTrack(track, last, 4, host);
  CHECK(track.elapsed_frames == 2);
  CHECK(track.current_position == Units(16, -200, 0));
  CHECK(last == Units(0, -200, 0));
  CHECK(track.motion_mode == event::kCameraMotionTerrainGlide);
}

// F01221 func_800BC2E8

TEST_CASE("Camera motion terrain glide divides the distance by the duration",
          "[event][camera][track]") {
  FakeTrackHost host;
  host.flat_floor = -100;
  event::EventCameraTracks tracks = RestingTracks();
  tracks.look_at.current_position = Units(0, 0, 0);
  tracks.look_at.destination = Units(256, -300, 0);
  tracks.look_at.partner_position = Units(0, 0, 50);
  const std::array<std::uint8_t, 3> bytes{0x0B, 0x77, 10};
  REQUIRE(Run(bytes, tracks, host) == event::CameraMotionOutcome::kApplied);
  const event::EventCameraTrack& track = tracks.look_at;
  CHECK(track.motion_mode == event::kCameraMotionTerrainGlide);
  CHECK(track.elapsed_frames == 0);
  CHECK(track.duration_frames == 10);
  CHECK(track.current_position == Units(0, -300, 0));
  CHECK(track.computed_position == track.current_position);
  CHECK(track.motion_scalar_delta == 256 * kUnit);
  // Truncated: 104857.6.
  CHECK(track.motion_scalar == 104857);
  CHECK(tracks.last_terrain_position == kTerrainUnset);
}

TEST_CASE("Camera motion terrain glide with a zero duration steps by -1",
          "[event][camera][track]") {
  FakeTrackHost host;
  event::EventCameraTracks tracks = RestingTracks();
  tracks.eye.current_position = Units(0, 0, 0);
  tracks.eye.destination = Units(0, -200, 0);
  const std::array<std::uint8_t, 3> bytes{0x1B, 0x00, 0};
  REQUIRE(Run(bytes, tracks, host) == event::CameraMotionOutcome::kApplied);
  // The R3000 quotient by zero: -1 for the non-negative distance, here 0.
  CHECK(tracks.eye.motion_scalar_delta == 0);
  CHECK(tracks.eye.motion_scalar == -1);
  CHECK(tracks.eye.duration_frames == 0);
  CHECK(tracks.look_at == RestingTracks().look_at);
}

TEST_CASE("Camera motion operation 6 is unassigned and 12 to 15 are ignored",
          "[event][camera][track]") {
  FakeTrackHost host;
  event::EventCameraTracks tracks = RestingTracks();
  const event::EventCameraTracks before = tracks;
  const std::array<std::uint8_t, 1> unassigned{0x16};
  CHECK(Run(unassigned, tracks, host) ==
        event::CameraMotionOutcome::kUnassigned);
  constexpr std::array<std::uint8_t, 4> kIgnored{0x0C, 0x0D, 0x1E, 0x1F};
  for (const std::uint8_t opcode : kIgnored) {
    const std::array<std::uint8_t, 1> ignored{opcode};
    CHECK(Run(ignored, tracks, host) == event::CameraMotionOutcome::kIgnored);
  }
  CHECK(tracks == before);
}

TEST_CASE("Camera motion refuses a command shorter than its operation",
          "[event][camera][track]") {
  FakeTrackHost host;
  event::EventCameraTracks tracks = RestingTracks();
  const event::EventCameraTracks before = tracks;
  const std::array<std::uint8_t, 9> bone{0x07, 1, 0, 2, 0, 0, 0, 0, 0};
  CHECK(Run(bone, tracks, host) == event::CameraMotionOutcome::kRefused);
  CHECK(Run({}, tracks, host) == event::CameraMotionOutcome::kRefused);
  CHECK(event::CameraMotionCommandLength(0xC7) == 10);
  CHECK(event::CameraMotionCommandLength(0xD8) == 11);
  CHECK(tracks == before);
  CHECK(host.references.empty());
}

TEST_CASE("Camera motion sets the destination and jumps to it",
          "[event][camera][track]") {
  FakeTrackHost host;
  event::EventCameraTracks tracks = RestingTracks();
  const std::array<std::uint8_t, 7> destination{0x10, 1, 0, 2, 0, 0xFD, 0xFF};
  REQUIRE(Run(destination, tracks, host) ==
          event::CameraMotionOutcome::kApplied);
  CHECK(tracks.eye.destination == Units(1, 2, -3));
  tracks.eye.motion_mode = event::kCameraMotionLinear;
  const std::array<std::uint8_t, 1> jump{0x11};
  REQUIRE(Run(jump, tracks, host) == event::CameraMotionOutcome::kApplied);
  CHECK(tracks.eye.motion_mode == event::kEventCameraTrackIdle);
  CHECK(tracks.eye.current_position == Units(1, 2, -3));
  CHECK(tracks.look_at.partner_position == Units(1, 2, -3));
  CHECK(tracks.eye.partner_position == RestingTrack().partner_position);
}

TEST_CASE("Camera motion linear starts from the current position",
          "[event][camera][track]") {
  FakeTrackHost host;
  event::EventCameraTracks tracks = RestingTracks();
  const std::array<std::uint8_t, 3> linear{0x02, 3, 20};
  REQUIRE(Run(linear, tracks, host) == event::CameraMotionOutcome::kApplied);
  CHECK(tracks.look_at.motion_mode == event::kCameraMotionLinear);
  CHECK(tracks.look_at.elapsed_frames == 0);
  CHECK(tracks.look_at.primary_easing == 3);
  CHECK(tracks.look_at.duration_frames == 20);
  CHECK(tracks.look_at.start_position == Units(1, 2, 3));
  CHECK(tracks.look_at.displacement == Units(10, -10, 20));
}

TEST_CASE("Camera motion partner radius keeps the whole-unit distance",
          "[event][camera][track]") {
  FakeTrackHost host;
  event::EventCameraTracks tracks = RestingTracks();
  const std::array<std::uint8_t, 3> radius{0x13, 4, 9};
  REQUIRE(Run(radius, tracks, host) == event::CameraMotionOutcome::kApplied);
  CHECK(tracks.eye.motion_mode == event::kCameraMotionPartnerRadius);
  // |(-4, 5, -6) - (7, 8, 9)| = |(-11, -3, -15)| = 18.84: whole units.
  CHECK(tracks.eye.motion_scalar == 18 * kUnit);
}

TEST_CASE("Camera motion orbit reads its easings and duration",
          "[event][camera][track]") {
  const event::EventCameraTrack track = RunOrbit();
  CHECK(track.motion_mode == event::kCameraMotionOrbit);
  CHECK(track.primary_easing == 3);
  CHECK(track.secondary_easing == 1);
  CHECK(track.tertiary_easing == 2);
  CHECK(track.duration_frames == 40);
}

TEST_CASE("Camera motion orbit measures the radius and the turn",
          "[event][camera][track]") {
  const event::EventCameraTrack track = RunOrbit();
  CHECK(track.motion_scalar == 0x40000);
  CHECK(track.motion_scalar_delta == 0x40000);
  CHECK(track.orbit_angle == 0);
  CHECK(track.orbit_angle_delta == kQuarterTurn);
  CHECK(track.height_offset.y == 0x3000);
  CHECK(track.displacement.y == 0x4000);
}

TEST_CASE("Camera motion orbit takes the shorter turn unless told a way",
          "[event][camera][track]") {
  const Vec3 right{0x80000, 0, 0};
  const Vec3 left{-0x80000, 0, 0};
  const Vec3 ahead{0, 0, 0x80000};
  CHECK(OrbitTurnTo(right, 0) == kQuarterTurn);
  CHECK(OrbitTurnTo(right, event::kCameraTurnNegative) == -3 * kQuarterTurn);
  CHECK(OrbitTurnTo(left, 0) == -kQuarterTurn);
  CHECK(OrbitTurnTo(left, event::kCameraTurnPositive) == 3 * kQuarterTurn);
  CHECK(OrbitTurnTo(ahead, 0) == 0);
  CHECK(OrbitTurnTo(ahead, event::kCameraTurnPositive) == 4 * kQuarterTurn);
  CHECK(OrbitTurnTo(ahead, event::kCameraTurnNegative) == -4 * kQuarterTurn);
}

TEST_CASE("Camera motion partner orbit reads its easings and duration",
          "[event][camera][track]") {
  const event::EventCameraTrack track = RunPartnerOrbit();
  CHECK(track.motion_mode == event::kCameraMotionPartnerOrbit);
  CHECK(track.primary_easing == 1);
  CHECK(track.secondary_easing == 2);
  CHECK(track.tertiary_easing == 3);
  CHECK(track.duration_frames == 30);
  CHECK(track.destination == Vec3{0, 0x9000, 0});
}

TEST_CASE("Camera motion partner orbit scales the radius in 2.6 fixed point",
          "[event][camera][track]") {
  const event::EventCameraTrack track = RunPartnerOrbit();
  CHECK(track.motion_scalar == 0x40000);
  CHECK(track.motion_scalar_delta == 0x40000);
  CHECK(track.orbit_angle_delta == 0x200);
  CHECK(track.start_position.y == 0x3000);
  CHECK(track.displacement.y == -0x5000);
}

TEST_CASE("Camera motion bone destination subtracts the origin offset",
          "[event][camera][track]") {
  FakeTrackHost host;
  host.resolved = 5;
  host.bone = Position3{.x = 10, .y = -20, .z = 30};
  event::EventCameraTracks tracks = RestingTracks();
  tracks.origin_actor = 7;
  tracks.origin_offset = {0x1000, 0x2000, 0x3000};
  const std::array<std::uint8_t, 10> bytes{0x07, 0x34, 0x12, 9, 1,
                                           0,    2,    0,    3, 0};
  REQUIRE(Run(bytes, tracks, host) == event::CameraMotionOutcome::kApplied);
  CHECK(host.references == std::vector<std::uint16_t>{0x1234});
  CHECK(host.bone_queries == std::vector<std::pair<int, int>>{{5, 9}});
  CHECK(tracks.look_at.destination == Vec3{0xA000, -0x14000, 0x1E000});
  CHECK(tracks.look_at.motion_mode == event::kEventCameraTrackIdle);
  tracks.origin_actor = event::kEventCameraWorldOrigin;
  REQUIRE(Run(bytes, tracks, host) == event::CameraMotionOutcome::kApplied);
  CHECK(tracks.look_at.destination == Units(11, -18, 33));
}

TEST_CASE("Camera motion bone destination counts a missing bone as the origin",
          "[event][camera][track]") {
  FakeTrackHost host;
  event::EventCameraTracks tracks = RestingTracks();
  const std::array<std::uint8_t, 10> bytes{0x17, 1, 0, 2, 1, 0, 2, 0, 3, 0};
  REQUIRE(Run(bytes, tracks, host) == event::CameraMotionOutcome::kApplied);
  CHECK(tracks.eye.destination == Units(1, 2, 3));
}

TEST_CASE("Camera motion facing bone destination turns the offset",
          "[event][camera][track]") {
  FakeTrackHost host;
  host.resolved = 4;
  host.facing = kQuarterTurn;
  host.bone = Position3{.x = 100, .y = 0, .z = 0};
  event::EventCameraTracks tracks = RestingTracks();
  const std::array<std::uint8_t, 10> bytes{0x0A, 2, 0, 6, 0, 0, 0, 0, 5, 0};
  REQUIRE(Run(bytes, tracks, host) == event::CameraMotionOutcome::kApplied);
  CHECK(host.facing_actors == std::vector<int>{4});
  CHECK(host.bone_queries == std::vector<std::pair<int, int>>{{4, 6}});
  CHECK(tracks.look_at.destination == Units(105, 0, 0));
}

TEST_CASE("Camera motion facing bone destination falls back to the angles",
          "[event][camera][track]") {
  FakeTrackHost host;
  host.facing = kQuarterTurn;
  event::EventCameraTracks tracks = RestingTracks();
  const std::array<std::uint8_t, 10> bytes{0x0A, 2, 0, 6, 0, 0, 0, 0, 5, 0};
  REQUIRE(Run(bytes, tracks, host) == event::CameraMotionOutcome::kApplied);
  // The failed query left the rotation angles (0, facing, 0) in the output.
  CHECK(tracks.look_at.destination == Vec3{5 * kUnit, kQuarterTurn * kUnit, 0});
  // An empty slot faces 0.
  host.facing.reset();
  REQUIRE(Run(bytes, tracks, host) == event::CameraMotionOutcome::kApplied);
  CHECK(tracks.look_at.destination == Units(0, 0, 5));
}

TEST_CASE("Camera motion follow bone starts its motion",
          "[event][camera][track]") {
  FakeTrackHost host;
  event::EventCameraTracks tracks = RestingTracks();
  RunFollowBone(tracks, host);
  CHECK(tracks.look_at.motion_mode == event::kCameraMotionBone);
  CHECK(tracks.look_at.motion_scalar == 0x1003);
  CHECK(tracks.look_at.motion_scalar_delta == 12);
  CHECK(tracks.look_at.elapsed_frames == 0);
  CHECK(tracks.look_at.duration_frames == 45);
  CHECK(tracks.look_at.displacement == Units(1, 0, 0));
}

TEST_CASE("Camera motion partner offset starts its motion",
          "[event][camera][track]") {
  FakeTrackHost host;
  event::EventCameraTracks tracks = RestingTracks();
  RunFollowBone(tracks, host);
  const std::array<std::uint8_t, 2> offset{0x19, 6};
  REQUIRE(Run(offset, tracks, host) == event::CameraMotionOutcome::kApplied);
  CHECK(tracks.eye.motion_mode == event::kCameraMotionPartnerOffset);
  CHECK(tracks.eye.duration_frames == 6);
  CHECK(tracks.eye.displacement == Units(11, 3, 15));
}

TEST_CASE("Camera motion entry warns for operation 6 and stores the tracks",
          "[event][camera][track]") {
  LiveTrackWorkspace workspace;
  workspace.camera.eye.destination = {1, 2, 3, 0x66};
  std::array<std::uint8_t, 7> destination{0xD0, 1, 0, 2, 0, 3, 0};
  CHECK(event::CommandCameraMotionEntry(destination.data(), 0) == 0);
  CHECK(VectorOf(workspace.camera.eye.destination) == Units(1, 2, 3));
  CHECK(workspace.camera.eye.destination.pad == 0x66);
  std::array<std::uint8_t, 1> unassigned{0xC6};
  CHECK(event::CommandCameraMotionEntry(unassigned.data(), 0) == -1);
  CHECK(VectorOf(workspace.camera.eye.destination) == Units(1, 2, 3));
}

TEST_CASE("Camera track update entry reads the tick speed",
          "[event][camera][track]") {
  LiveTrackWorkspace workspace;
  VS::State::tickSpeed = 4;
  VS::Event::CameraTrack& track = workspace.camera.look_at;
  track.motion_mode = event::kCameraMotionPartnerOffset;
  track.duration_frames = 10;
  track.displacement = {0x1000, 0, 0, 0};
  event::UpdateEventCameraTrackEntry(&track, &workspace.camera.eye);
  CHECK(track.elapsed_frames == 2);
  CHECK(VectorOf(track.current_position) == Vec3{0x1000, 0, 0});
}

// F01239 func_800BE53C

TEST_CASE("Camera track duration scales by the distance over 1000",
          "[event][camera][track]") {
  event::EventCameraTrack track;
  track.current_position = Units(512, 0, 0);
  track.duration_frames = 30;
  event::ScaleEventCameraTrackDuration(track);
  CHECK(track.duration_frames == 15);
  // 2048 units at 200 frames is 409 frames: the byte keeps 153.
  track.current_position = Units(0, 0, -2048);
  track.duration_frames = 200;
  event::ScaleEventCameraTrackDuration(track);
  CHECK(track.duration_frames == 153);
  track.current_position = {};
  event::ScaleEventCameraTrackDuration(track);
  CHECK(track.duration_frames == 0);
}

TEST_CASE("Camera track duration counts an overflowing distance as zero",
          "[event][camera][track]") {
  // The squared whole units wrap to a negative word. The original's square
  // root read before its table there; Core::Math defines that root as zero.
  event::EventCameraTrack track;
  track.current_position = {.x = 0x7FFFF000, .y = 0x7FFFF000, .z = 0};
  track.duration_frames = 200;
  event::ScaleEventCameraTrackDuration(track);
  CHECK(track.duration_frames == 0);
}

TEST_CASE("Camera track duration entry writes only the duration",
          "[event][camera][track]") {
  VS::Event::CameraTrack track{};
  track.current_position = {0, 1024 * kUnit, 0, 0};
  track.duration_frames = 100;
  track.motion_mode = 9;
  event::ScaleEventCameraTrackDurationEntry(&track);
  CHECK(track.duration_frames == 102);
  CHECK(track.motion_mode == 9);
}

// F01240 func_800BE5A4

TEST_CASE("Camera duration selector scales one track or both to their mean",
          "[event][camera][track]") {
  event::EventCameraTrack look_at;
  look_at.current_position = Units(512, 0, 0);
  look_at.duration_frames = 30;
  event::EventCameraTrack eye;
  eye.current_position = Units(2048, 0, 0);
  eye.duration_frames = 30;
  event::EventCameraTrack only_look_at = look_at;
  event::EventCameraTrack untouched_eye = eye;
  event::ScaleEventCameraDuration(only_look_at, untouched_eye, 0);
  CHECK(only_look_at.duration_frames == 15);
  CHECK(untouched_eye.duration_frames == 30);
  event::EventCameraTrack untouched_look_at = look_at;
  event::EventCameraTrack only_eye = eye;
  event::ScaleEventCameraDuration(untouched_look_at, only_eye, -1);
  CHECK(untouched_look_at.duration_frames == 30);
  CHECK(only_eye.duration_frames == 61);
  event::ScaleEventCameraDuration(look_at, eye, event::kScaleBothDurations);
  CHECK(look_at.duration_frames == 38);
  CHECK(eye.duration_frames == 38);
}

TEST_CASE("Camera duration entry scales the workspace tracks",
          "[event][camera][track]") {
  LiveTrackWorkspace workspace;
  workspace.camera.look_at.current_position = {512 * kUnit, 0, 0, 0};
  workspace.camera.look_at.duration_frames = 30;
  workspace.camera.eye.current_position = {0, 2048 * kUnit, 0, 0};
  workspace.camera.eye.duration_frames = 60;
  event::ScaleEventCameraDurationEntry(1);
  CHECK(workspace.camera.look_at.duration_frames == 30);
  CHECK(workspace.camera.eye.duration_frames == 122);
  event::ScaleEventCameraDurationEntry(2);
  // (15 + 249) / 2.
  CHECK(workspace.camera.look_at.duration_frames == 132);
  CHECK(workspace.camera.eye.duration_frames == 132);
}
