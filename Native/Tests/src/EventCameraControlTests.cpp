#include <array>
#include <cstdint>
#include <deque>
#include <optional>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Core/Math/Distance.hpp"
#include "Core/Math/ScalarMath.hpp"
#include "Event/Camera.hpp"
#include "Event/EventCameraControl.hpp"
#include "Event/EventCameraControlLive.hpp"
#include "Event/EventCameraOwnership.hpp"
#include "Event/EventCameraTrack.hpp"
#include "Event/ScriptCamera.hpp"
#include "Event/ScriptCommand.hpp"
#include "Event/ScriptControlOpcodes.hpp"
#include "Event/ScriptVm.h"

namespace {
namespace event = VS::Event;
using VS::Core::Math::Position3;
using VS::Core::Math::Vec3;

constexpr std::int32_t kUnit = 4096;

constexpr Vec3 Units(std::int32_t x, std::int32_t y, std::int32_t z) {
  return {x * kUnit, y * kUnit, z * kUnit};
}

// Queued placements (then the origin) and the recorded requests.
class FakeFieldCamera final : public event::EventFieldCameraHost {
 public:
  std::deque<event::FieldCameraPlacement> placements;
  std::vector<std::pair<std::int32_t, std::int32_t>> requests;

  event::FieldCameraPlacement PlaceFieldCamera(
      std::int32_t facing, std::int32_t distance_mode) override {
    requests.emplace_back(facing, distance_mode);
    if (placements.empty()) return {};
    const event::FieldCameraPlacement placement = placements.front();
    placements.pop_front();
    return placement;
  }
};

// The linear and orbit motions call no service: any call is counted.
class CountingMotionHost final : public event::EventCameraTrackHost {
 public:
  int calls = 0;

  std::int16_t FloorHeight(std::int32_t /*world_x*/,
                           std::int32_t /*world_z*/) override {
    ++calls;
    return 0;
  }
  std::int32_t Ease(std::uint8_t /*curve*/, std::uint8_t /*frame*/,
                    std::uint8_t /*duration*/) override {
    ++calls;
    return 0;
  }
  int ResolveActorReference(std::uint16_t /*reference*/) override {
    ++calls;
    return 0;
  }
  std::optional<std::int16_t> ActorFacing(int /*actor*/) override {
    ++calls;
    return std::nullopt;
  }
  std::optional<Position3> TaggedBonePosition(int /*actor*/,
                                              int /*tag*/) override {
    ++calls;
    return std::nullopt;
  }
};

// Moving tracks with distinct positions and follow state.
event::EventCameraTracks MovingTracks() {
  event::EventCameraTracks tracks;
  tracks.look_at.motion_mode = event::kCameraMotionLinear;
  tracks.look_at.follow_request = 1;
  tracks.look_at.following = 1;
  tracks.look_at.duration_frames = 9;
  tracks.look_at.current_position = Units(1, 2, 3);
  tracks.look_at.destination = Units(4, 5, 6);
  tracks.look_at.partner_position = Units(7, 8, 9);
  tracks.eye.motion_mode = event::kCameraMotionOrbit;
  tracks.eye.follow_request = 1;
  tracks.eye.following = 1;
  tracks.eye.duration_frames = 11;
  tracks.eye.current_position = Units(-1, -2, -3);
  tracks.eye.destination = Units(-4, -5, -6);
  tracks.eye.partner_position = Units(-7, -8, -9);
  return tracks;
}

// The tracks after the motion commands, in order.
template <typename... Commands>
event::EventCameraTracks AfterMotion(event::EventCameraTracks tracks,
                                     const Commands&... commands) {
  CountingMotionHost host;
  (static_cast<void>(event::CommandCameraMotion(event::ScriptCommand{commands},
                                                tracks, host)),
   ...);
  return tracks;
}

Vec3 Lifted(Vec3 position, std::int32_t whole_units) {
  position.y += whole_units * kUnit;
  return position;
}

// A field return from the origin to a field view `eye_units` along x and
// `look_at_units` along z.
struct FieldReturn {
  event::EventCameraTracks tracks;
  FakeFieldCamera field;
  CountingMotionHost motion;
  event::FieldCameraReturnGlide glide;
};

void RunFieldReturn(FieldReturn& run, std::int32_t eye_units,
                    std::int32_t look_at_units,
                    event::FieldCameraReturnRequest request = {}) {
  run.field.placements.push_back(
      {.eye = Units(eye_units, 0, 0), .look_at = Units(0, 0, look_at_units)});
  run.glide = event::RestoreEventFieldCamera(run.tracks, request, run.field,
                                             run.motion);
}

// Points VS::Event::cameraWorkspace at a zeroed workspace and keeps the restore
// globals.
struct LiveControlWorkspace {
  VS::Event::CameraState camera{};
  VS::Event::CameraState* saved_camera = event::cameraWorkspace;
  std::uint8_t saved_skip = event::skipNextFieldCameraRestore;
  std::int16_t saved_scripted = Event_isFieldCameraRestoreScripted;

  LiveControlWorkspace() { event::cameraWorkspace = &camera; }
  LiveControlWorkspace(const LiveControlWorkspace&) = delete;
  LiveControlWorkspace& operator=(const LiveControlWorkspace&) = delete;
  LiveControlWorkspace(LiveControlWorkspace&&) = delete;
  LiveControlWorkspace& operator=(LiveControlWorkspace&&) = delete;
  ~LiveControlWorkspace() {
    event::cameraWorkspace = saved_camera;
    event::skipNextFieldCameraRestore = saved_skip;
    Event_isFieldCameraRestoreScripted = saved_scripted;
  }
};

void SetMoving(VS::Event::CameraTrack& track, std::uint8_t mode) {
  track.motion_mode = mode;
  track.follow_request = 1;
  track.is_following = 1;
}
}  // namespace

// F01237 func_800BE3A0

TEST_CASE("Event camera stop rests both tracks and clears the follow requests",
          "[event][camera][follow]") {
  event::EventCameraTracks tracks = MovingTracks();
  event::EventCameraTracks expected = tracks;
  expected.look_at.motion_mode = event::kEventCameraTrackIdle;
  expected.eye.motion_mode = event::kEventCameraTrackIdle;
  expected.look_at.follow_request = 0;
  expected.look_at.following = 0;
  expected.eye.follow_request = 0;
  expected.eye.following = 0;

  event::StopEventCameraMotion(tracks);

  CHECK(tracks == expected);
}

TEST_CASE("Event camera stop entry writes the workspace tracks",
          "[event][camera][follow]") {
  LiveControlWorkspace workspace;
  SetMoving(workspace.camera.look_at, event::kCameraMotionLinear);
  SetMoving(workspace.camera.eye, event::kCameraMotionTerrainGlide);
  workspace.camera.eye.duration_frames = 21;

  event::StopEventCameraMotionEntry();

  CHECK(workspace.camera.look_at.motion_mode == event::kEventCameraTrackIdle);
  CHECK(workspace.camera.eye.motion_mode == event::kEventCameraTrackIdle);
  CHECK(workspace.camera.look_at.follow_request == 0);
  CHECK(workspace.camera.look_at.is_following == 0);
  CHECK(workspace.camera.eye.follow_request == 0);
  CHECK(workspace.camera.eye.is_following == 0);
  CHECK(workspace.camera.eye.duration_frames == 21);
}

// F01193 func_800BB33C

TEST_CASE("Camera stop opcode rests the workspace without reading an operand",
          "[event][camera][follow]") {
  LiveControlWorkspace workspace;
  SetMoving(workspace.camera.look_at, event::kCameraMotionOrbit);
  SetMoving(workspace.camera.eye, event::kCameraMotionBone);

  CHECK(VS::Event::CommandStopCameraMotionOpcode(nullptr, 0) == 0);

  CHECK(workspace.camera.look_at.motion_mode == event::kEventCameraTrackIdle);
  CHECK(workspace.camera.eye.motion_mode == event::kEventCameraTrackIdle);
  CHECK(workspace.camera.look_at.follow_request == 0);
  CHECK(workspace.camera.eye.is_following == 0);
}

TEST_CASE("Camera stop opcode repeats on a resting workspace",
          "[event][camera][follow]") {
  LiveControlWorkspace workspace;
  std::array<std::uint8_t, 2> instruction{0xDE, 0x03};

  CHECK(VS::Event::CommandStopCameraMotionOpcode(instruction.data(), 0) == 0);
  CHECK(VS::Event::CommandStopCameraMotionOpcode(instruction.data(), 0) == 0);

  CHECK(workspace.camera.look_at.motion_mode == event::kEventCameraTrackIdle);
  CHECK(workspace.camera.eye.motion_mode == event::kEventCameraTrackIdle);
  CHECK(workspace.camera.eye.follow_request == 0);
}

// F01236 func_800BE36C

TEST_CASE("Event camera following sets both requests and waits to follow",
          "[event][camera][follow]") {
  event::EventCameraTracks tracks = MovingTracks();
  event::EventCameraTracks expected = tracks;
  expected.look_at.follow_request = 0;
  expected.look_at.following = 0;
  expected.eye.follow_request = 1;
  expected.eye.following = 0;

  event::SetEventCameraFollowing(tracks, 0, 1);

  CHECK(tracks == expected);
}

TEST_CASE("Event camera following entry keeps each request's low byte",
          "[event][camera][follow]") {
  LiveControlWorkspace workspace;
  SetMoving(workspace.camera.look_at, event::kCameraMotionLinear);
  SetMoving(workspace.camera.eye, event::kCameraMotionLinear);

  event::SetEventCameraFollowingEntry(0x101, 0x2FF);

  CHECK(workspace.camera.look_at.follow_request == 1);
  CHECK(workspace.camera.look_at.is_following == 0);
  CHECK(workspace.camera.eye.follow_request == 0xFF);
  CHECK(workspace.camera.eye.is_following == 0);
  CHECK(workspace.camera.eye.motion_mode == event::kCameraMotionLinear);
}

// F01194 func_800BB35C

TEST_CASE("Camera following opcode reads the look-at bit and the eye bit",
          "[event][camera][follow]") {
  struct Row {
    std::uint8_t operand;
    std::uint8_t look_at;
    std::uint8_t eye;
  };
  constexpr std::array<Row, 6> kRows{{
      {.operand = 0x00, .look_at = 0, .eye = 0},
      {.operand = 0x01, .look_at = 1, .eye = 0},
      {.operand = 0x02, .look_at = 0, .eye = 1},
      {.operand = 0x03, .look_at = 1, .eye = 1},
      {.operand = 0xFC, .look_at = 0, .eye = 0},
      {.operand = 0xFE, .look_at = 0, .eye = 1},
  }};
  for (const Row& row : kRows) {
    CAPTURE(row.operand);
    event::EventCameraTracks tracks = MovingTracks();
    const std::array<std::uint8_t, 2> bytes{0xDF, row.operand};
    REQUIRE(
        event::CommandSetCameraFollowing(event::ScriptCommand{bytes}, tracks));
    CHECK(tracks.look_at.follow_request == row.look_at);
    CHECK(tracks.eye.follow_request == row.eye);
    CHECK(tracks.look_at.following == 0);
    CHECK(tracks.eye.following == 0);
  }
}

TEST_CASE("Camera following opcode refuses a command without its operand",
          "[event][camera][follow]") {
  event::EventCameraTracks tracks = MovingTracks();
  const event::EventCameraTracks before = tracks;
  const std::array<std::uint8_t, 1> bytes{0xDF};

  CHECK_FALSE(
      event::CommandSetCameraFollowing(event::ScriptCommand{bytes}, tracks));
  CHECK(tracks == before);
}

TEST_CASE("Camera following opcode entry writes the workspace and returns 0",
          "[event][camera][follow]") {
  LiveControlWorkspace workspace;
  SetMoving(workspace.camera.look_at, event::kCameraMotionLinear);
  std::array<std::uint8_t, 2> instruction{0xDF, 0x02};

  CHECK(VS::Event::CommandSetCameraFollowingOpcode(instruction.data(), 3) == 0);

  CHECK(workspace.camera.look_at.follow_request == 0);
  CHECK(workspace.camera.look_at.is_following == 0);
  CHECK(workspace.camera.eye.follow_request == 1);
  CHECK(workspace.camera.look_at.motion_mode == event::kCameraMotionLinear);
}

// F01196 func_800BB3BC

TEST_CASE("Field camera restore opcode turns the facing half round",
          "[event][camera][follow]") {
  constexpr std::array<std::pair<std::uint8_t, std::int16_t>, 6> kFacings{{
      {0x00, 4},
      {0x03, 7},
      {0x04, 0},
      {0x07, 3},
      {0xFE, 2},
      {0xFF, event::kKeepFieldCameraSetting},
  }};
  for (const auto& [operand, facing] : kFacings) {
    CAPTURE(operand);
    const std::array<std::uint8_t, 3> bytes{0xE9, operand, 1};
    const std::optional<event::FieldCameraReturnRequest> request =
        event::CommandRestoreFieldCamera(event::ScriptCommand{bytes});
    REQUIRE(request.has_value());
    if (!request.has_value()) continue;
    CHECK(request->facing == facing);
    CHECK(request->distance_mode == 1);
  }
}

TEST_CASE("Field camera restore opcode keeps the distance for operand 0",
          "[event][camera][follow]") {
  constexpr std::array<std::pair<std::uint8_t, std::int16_t>, 4> kDistances{{
      {0x00, event::kKeepFieldCameraSetting},
      {0x01, 1},
      {0x02, 2},
      {0xFF, 0xFF},
  }};
  for (const auto& [operand, distance_mode] : kDistances) {
    CAPTURE(operand);
    const std::array<std::uint8_t, 3> bytes{0xE9, 0xFF, operand};
    const std::optional<event::FieldCameraReturnRequest> request =
        event::CommandRestoreFieldCamera(event::ScriptCommand{bytes});
    REQUIRE(request.has_value());
    if (!request.has_value()) continue;
    CHECK(request->distance_mode == distance_mode);
  }
}

TEST_CASE("Field camera restore opcode refuses a command without its distance",
          "[event][camera][follow]") {
  const std::array<std::uint8_t, 2> bytes{0xE9, 0x02};
  CHECK_FALSE(event::CommandRestoreFieldCamera(event::ScriptCommand{bytes})
                  .has_value());
}

TEST_CASE("Field camera restore opcode marks the restore scripted if skipped",
          "[event][camera][follow]") {
  LiveControlWorkspace workspace;
  event::cameraWorkspace = nullptr;
  event::skipNextFieldCameraRestore = 1;
  Event_isFieldCameraRestoreScripted = 0;
  std::array<std::uint8_t, 3> instruction{0xE9, 0x02, 0x01};

  CHECK(VS::Event::CommandRestoreFieldCameraOpcode(instruction.data(), 0) == 0);

  CHECK(event::skipNextFieldCameraRestore == 0);
  CHECK(Event_isFieldCameraRestoreScripted == 1);
}

// F01220 func_800BC1CC

TEST_CASE("Field camera override is consumed by one restore",
          "[event][camera][follow]") {
  std::uint8_t skip = 1;
  CHECK(event::ConsumeFieldCameraOverride(skip));
  CHECK(skip == 0);
  CHECK_FALSE(event::ConsumeFieldCameraOverride(skip));
  CHECK(skip == 0);

  skip = 0x80;
  CHECK(event::ConsumeFieldCameraOverride(skip));
  CHECK(skip == 0);
}

TEST_CASE("Field camera restore entry needs no workspace when overridden",
          "[event][camera][follow]") {
  LiveControlWorkspace workspace;
  event::cameraWorkspace = nullptr;
  event::skipNextFieldCameraRestore = 1;

  event::RestoreEventFieldCameraEntry(-1, -1);

  CHECK(event::skipNextFieldCameraRestore == 0);
}

TEST_CASE("Field camera return command bytes carry the duration",
          "[event][camera][follow]") {
  const event::FieldCameraReturnCommands commands =
      event::FieldCameraReturnCommandsFor(0x2A);
  CHECK(commands.look_at == event::LinearCommand{0xC2, 0x02, 0x2A});
  CHECK(commands.eye == event::OrbitCommand{0xD4, 0x20, 0x22, 0x2A});
}

TEST_CASE("Field camera return glides both tracks to the field view",
          "[event][camera][follow]") {
  FieldReturn run;
  event::EventCameraTracks before = run.tracks;
  RunFieldReturn(run, 256, 64, {.facing = 5, .distance_mode = 2});

  CHECK(run.field.requests ==
        std::vector<std::pair<std::int32_t, std::int32_t>>{{5, 2}});
  CHECK(run.glide.travel_frames == 8);
  CHECK(run.glide.started);
  before.eye.destination = Units(256, 0, 0);
  before.look_at.destination = Units(0, 0, 64);
  const event::FieldCameraReturnCommands commands =
      event::FieldCameraReturnCommandsFor(8);
  CHECK(run.tracks == AfterMotion(before, commands.look_at, commands.eye));
  CHECK(run.motion.calls == 0);
}

TEST_CASE("Field camera return starts a look-at line and an eye orbit",
          "[event][camera][follow]") {
  FieldReturn run;
  RunFieldReturn(run, 256, 64, {.facing = 5, .distance_mode = 2});

  CHECK(run.tracks.look_at.motion_mode == event::kCameraMotionLinear);
  CHECK(run.tracks.look_at.primary_easing == 2);
  CHECK(run.tracks.look_at.duration_frames == 8);
  CHECK(run.tracks.eye.motion_mode == event::kCameraMotionOrbit);
  CHECK(run.tracks.eye.duration_frames == 8);
}

TEST_CASE("Field camera return under one frame of travel starts nothing",
          "[event][camera][follow]") {
  FieldReturn run;
  run.tracks = MovingTracks();
  run.tracks.eye.current_position = Units(0, 0, 0);
  run.tracks.look_at.current_position = Units(0, 0, 0);
  event::EventCameraTracks expected = run.tracks;
  expected.eye.destination = Units(16, 0, 0);
  expected.look_at.destination = Units(0, 0, 16);

  RunFieldReturn(run, 16, 16);

  CHECK(run.glide.travel_frames == 0);
  CHECK_FALSE(run.glide.started);
  CHECK(run.tracks == expected);
}

TEST_CASE("Field camera return of 256 frames starts a glide of duration 0",
          "[event][camera][follow]") {
  FieldReturn run;
  RunFieldReturn(run, 8192, 2048);

  CHECK(run.glide.travel_frames == 256);
  CHECK(run.glide.started);
  CHECK(run.tracks.look_at.motion_mode == event::kCameraMotionLinear);
  CHECK(run.tracks.look_at.duration_frames == 0);
  CHECK(run.tracks.eye.motion_mode == event::kCameraMotionOrbit);
  CHECK(run.tracks.eye.duration_frames == 0);
}

TEST_CASE("Field camera return rounds current less destination down per axis",
          "[event][camera][follow]") {
  // 32 units of look-at travel and an eye 7 units and 1/4096 past the field
  // eye, which rounds to 8 units only as current less destination: 40 units
  // make one frame.
  FieldReturn run;
  run.tracks.look_at.current_position = Units(0, 0, 32);
  run.tracks.eye.current_position = {0, 0, 0};
  run.field.placements.push_back(
      {.eye = {(7 * kUnit) + 1, 0, 0}, .look_at = Units(0, 0, 0)});
  run.glide =
      event::RestoreEventFieldCamera(run.tracks, {}, run.field, run.motion);

  CHECK(run.field.requests ==
        std::vector<std::pair<std::int32_t, std::int32_t>>{{-1, -1}});
  CHECK(run.glide.travel_frames == 1);
  CHECK(run.tracks.eye.duration_frames == 1);
}

// F01238 func_800BE3D0

TEST_CASE("Camera orbit return starts an eighth turn before the saved facing",
          "[event][camera][follow]") {
  event::EventCameraTracks tracks = MovingTracks();
  FakeFieldCamera field;
  CountingMotionHost motion;
  const event::FieldCameraPlacement start{.eye = Units(10, -20, 30),
                                          .look_at = Units(1, -2, 3)};
  const event::FieldCameraPlacement destination{.eye = Units(40, -50, 60),
                                                .look_at = Units(4, -5, 6)};
  field.placements = {start, destination};
  event::EventCameraTracks expected = tracks;

  CHECK(event::StartEventCameraOrbitReturn(
      tracks, {.facing = 3, .distance_index = 0}, 0, field, motion));

  CHECK(field.requests ==
        std::vector<std::pair<std::int32_t, std::int32_t>>{{2, 1}, {3, 1}});
  expected.look_at.current_position = Lifted(start.look_at, -600);
  expected.look_at.partner_position = Lifted(start.eye, -600);
  expected.eye.partner_position = Lifted(start.look_at, -600);
  expected.eye.current_position = Lifted(start.eye, -600);
  expected.look_at.motion_mode = event::kEventCameraTrackIdle;
  expected.eye.destination = destination.eye;
  expected.look_at.destination = destination.look_at;
  CHECK(tracks == AfterMotion(expected, event::kOrbitReturnEyeCommand,
                              event::kOrbitReturnLookAtCommand));
  CHECK(motion.calls == 0);
}

TEST_CASE("Camera orbit return starts an 85 frame eye orbit and look-at line",
          "[event][camera][follow]") {
  event::EventCameraTracks tracks = MovingTracks();
  FakeFieldCamera field;
  CountingMotionHost motion;

  CHECK(event::StartEventCameraOrbitReturn(
      tracks, {.facing = 3, .distance_index = 0}, 0, field, motion));

  CHECK(tracks.eye.motion_mode == event::kCameraMotionOrbit);
  CHECK(tracks.eye.duration_frames == 0x55);
  CHECK(tracks.eye.primary_easing == 1);
  CHECK(tracks.eye.secondary_easing == 0);
  CHECK(tracks.eye.tertiary_easing == 1);
  CHECK(tracks.look_at.motion_mode == event::kCameraMotionLinear);
  CHECK(tracks.look_at.primary_easing == 1);
  CHECK(tracks.look_at.duration_frames == 0x55);
}

TEST_CASE("Camera orbit return from side 1 starts after the far saved view",
          "[event][camera][follow]") {
  event::EventCameraTracks tracks;
  FakeFieldCamera field;
  CountingMotionHost motion;
  field.placements = {{.eye = Units(0, 100, 0), .look_at = Units(0, 50, 0)}};

  CHECK(event::StartEventCameraOrbitReturn(
      tracks, {.facing = 7, .distance_index = 1}, 1, field, motion));

  CHECK(field.requests ==
        std::vector<std::pair<std::int32_t, std::int32_t>>{{0, 2}, {7, 2}});
  CHECK(tracks.eye.current_position == Units(0, -600, 0));
  CHECK(tracks.look_at.partner_position == Units(0, -600, 0));
  CHECK(tracks.eye.partner_position == Units(0, -650, 0));
}

TEST_CASE("Camera orbit return from facing 0 wraps to the last eighth",
          "[event][camera][follow]") {
  event::EventCameraTracks tracks;
  FakeFieldCamera field;
  CountingMotionHost motion;

  CHECK(event::StartEventCameraOrbitReturn(
      tracks, {.facing = 0, .distance_index = 0}, 0, field, motion));
  CHECK(event::StartEventCameraOrbitReturn(
      tracks, {.facing = 0xF0, .distance_index = 0}, 0xFF, field, motion));

  CHECK(field.requests == std::vector<std::pair<std::int32_t, std::int32_t>>{
                              {7, 1}, {0, 1}, {5, 1}, {0xF0, 1}});
}

TEST_CASE("Camera orbit return past the height table does not raise the start",
          "[event][camera][follow]") {
  event::EventCameraTracks tracks;
  FakeFieldCamera field;
  CountingMotionHost motion;
  field.placements = {{.eye = Units(3, 4, 5), .look_at = Units(6, 7, 8)}};

  CHECK_FALSE(event::StartEventCameraOrbitReturn(
      tracks, {.facing = 2, .distance_index = 2}, 0, field, motion));

  CHECK(field.requests ==
        std::vector<std::pair<std::int32_t, std::int32_t>>{{1, 3}, {2, 3}});
  CHECK(tracks.eye.current_position == Units(3, 4, 5));
  CHECK(tracks.look_at.current_position == Units(6, 7, 8));
}
