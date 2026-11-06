#include <algorithm>
#include <cstdint>
#include <format>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Core/Math/ScalarMath.hpp"
#include "Field/Camera.hpp"
#include "Field/CameraLive.hpp"
#include "Legacy/Psx.h"
#include "TestSupport/ChannelWarnings.hpp"

extern "C" {
#include "Field/CameraLiveState.h"
#include "Render/Render.h"
#include "Render/UiPrimitives.h"
}

namespace {
namespace fx = VS::Field;
namespace actors = VS::Actors;
using Calls = std::vector<std::string>;
using VS::test_support::ChannelWarnings;

constexpr std::int32_t kOne = 4096;
constexpr std::int32_t kCentredScreen = (120 << 16) | 160;
constexpr std::int32_t kOffCentreScreen = (120 << 16) | 10;
constexpr std::int32_t kLookAtHeight = 90 * kOne;
// The shared normalizer's table rounds an axis direction to near 4096.
std::int32_t UnitAxis(std::int32_t length) {
  return VS::Core::Math::NormalizeVector({length, 0, 0}).direction.x;
}

std::string Flag(bool value) { return value ? "1" : "0"; }

// Catch2's REQUIRE throws, but clang-tidy cannot see that; the explicit throw
// lets bugprone-unchecked-optional-access prove the accesses through it.
template <typename T>
T& Required(std::optional<T>& value) {
  REQUIRE(value.has_value());
  if (!value) {
    throw std::logic_error("REQUIRE did not stop the test");
  }
  return *value;
}

class FakeCameraHost final : public fx::CameraHost {
 public:
  [[nodiscard]] std::optional<actors::ActorPlacementView> ActorPlacement(
      int /*actor_id*/) override {
    return placement;
  }
  void UpdateRoomActorMovement() override {
    calls.emplace_back("UpdateRoomActorMovement");
  }
  void TurnActorTo(int actor_id, std::int32_t yaw) override {
    calls.push_back(std::format("TurnActorTo {} {}", actor_id, yaw));
  }
  [[nodiscard]] std::uint32_t ActorStateFlags(int /*actor_id*/) override {
    return state_flags;
  }
  void SetActorDisplayFlags(int /*actor_id*/, std::uint32_t flags) override {
    state_flags = flags;
  }
  void RestoreActorIdlePose(int /*actor_id*/) override { ++idle_poses; }
  void SetActorTransitionMode(int mode) override {
    calls.push_back(std::format("SetActorTransitionMode {}", mode));
  }
  [[nodiscard]] bool IsInteractionPaused() override { return paused; }
  [[nodiscard]] fx::CameraBounds ZoneBounds() override { return bounds; }
  [[nodiscard]] std::optional<std::uint8_t> CollisionCellFlags(
      int tile_x, int tile_z, int layer) override {
    calls.push_back(
        std::format("CollisionCellFlags {} {} {}", tile_x, tile_z, layer));
    return cell_flags;
  }
  void ApplyGeometryOcclusion(int tile_x, int tile_z, int layer,
                              bool far_camera) override {
    calls.push_back(std::format("ApplyGeometryOcclusion {} {} {} {}", tile_x,
                                tile_z, layer, Flag(far_camera)));
  }
  void ClearGeometryOcclusion() override {
    calls.emplace_back("ClearGeometryOcclusion");
  }
  void SetMinimapVisible(bool visible) override {
    calls.push_back("SetMinimapVisible " + Flag(visible));
  }
  void SetMovingPlatformsPaused(bool value) override {
    calls.push_back("SetMovingPlatformsPaused " + Flag(value));
  }
  void EnterFieldExploration() override {
    calls.emplace_back("EnterFieldExploration");
  }
  [[nodiscard]] std::uint32_t ControlMode() override { return control_mode; }
  void SetControlMode(std::uint32_t mode) override {
    control_mode = mode;
    calls.push_back(std::format("SetControlMode {}", mode));
  }
  [[nodiscard]] std::uint32_t ActionFlowMode() override { return flow_mode; }
  void SetActionFlowMode(std::uint32_t mode) override {
    flow_mode = mode;
    calls.push_back(std::format("SetActionFlowMode {}", mode));
  }
  [[nodiscard]] bool TargetPagesUseShoulders() override {
    return shoulder_pages;
  }
  [[nodiscard]] int SphereInteractionMode() override { return sphere_mode; }
  [[nodiscard]] std::optional<fx::ActionCameraTargets> ActionTargets()
      override {
    return action;
  }
  void SetPreferredActionCameraDistance(std::int32_t distance) override {
    calls.push_back(
        std::format("SetPreferredActionCameraDistance {:#x}", distance));
  }
  void SetHudHidden(bool hidden) override {
    calls.push_back("SetHudHidden " + Flag(hidden));
  }
  void SetActorMarkersHidden(bool hidden) override {
    calls.push_back("SetActorMarkersHidden " + Flag(hidden));
  }
  [[nodiscard]] std::uint32_t HeldButtons() override { return held; }
  [[nodiscard]] std::uint32_t PressedButtons() override { return pressed; }
  [[nodiscard]] std::optional<fx::StickPosition> AnalogRightStick() override {
    return stick;
  }
  [[nodiscard]] std::int32_t ProjectToScreen(
      const actors::MotionVector& point) override {
    projected = point;
    return screen;
  }
  [[nodiscard]] std::int32_t NearClip() override { return near_clip; }
  [[nodiscard]] std::int32_t ProjectionDistance() override {
    return projection;
  }
  void SetNearClip(std::int32_t value) override { near_clip = value; }
  void SetProjectionDistance(std::int32_t distance) override {
    projection = distance;
  }
  [[nodiscard]] std::optional<fx::FirstPersonTransition> AllocateTransition()
      override {
    return allocation;
  }
  void FreeTransition() override { calls.emplace_back("FreeTransition"); }

  [[nodiscard]] bool Called(const std::string& call) const {
    return std::ranges::find(calls, call) != calls.end();
  }

  std::optional<actors::ActorPlacementView> placement =
      actors::ActorPlacementView{};
  std::uint32_t state_flags = 0;
  bool paused = false;
  fx::CameraBounds bounds{
      .min_x = -100, .min_z = -100, .max_x = 100, .max_z = 100};
  std::optional<std::uint8_t> cell_flags = std::uint8_t{0};
  std::uint32_t control_mode = 0;
  std::uint32_t flow_mode = 0;
  bool shoulder_pages = false;
  int sphere_mode = 0;
  std::optional<fx::ActionCameraTargets> action;
  std::uint32_t held = 0;
  std::uint32_t pressed = 0;
  std::optional<fx::StickPosition> stick;
  std::int32_t screen = kCentredScreen;
  actors::MotionVector projected{};
  std::int32_t near_clip = 16;
  std::int32_t projection = 512;
  std::optional<fx::FirstPersonTransition> allocation =
      fx::FirstPersonTransition{};
  int idle_poses = 0;
  Calls calls;
};

fx::OrbitAngles Orbit(std::int32_t yaw, std::int32_t pitch,
                      std::int32_t distance) {
  return {.yaw = yaw, .pitch = pitch, .distance = distance, .mode = 0};
}

// The camera Ashley's first-person view starts from: far behind and above
// her, looking at her head, at the near zoom.
fx::FieldCameraState FirstPersonStart(FakeCameraHost& host) {
  fx::FieldCameraState state;
  state.camera.position = {0, -0x400000, 0x600000};
  state.camera.look_at = {0, -kLookAtHeight, 0};
  state.camera.far_clip = 0x1000;
  state.spherical.values = Orbit(0, -0x180, fx::kNearCameraDistance);
  state.spherical.motion.zoom_mode = fx::kNearZoom;
  state.spherical.preferred_distance = fx::kNearCameraDistance;
  Required(host.placement).position = {.x = 0, .y = 0, .z = 0, .w = 0x400};
  return state;
}

template <typename Step>
int RunFrames(int limit, Step step) {
  int frames = 0;
  while (frames < limit && step()) ++frames;
  return frames;
}

// Begins the first-person view and runs its entry transition to the end.
void EnterFirstPerson(FakeCameraHost& host, fx::FieldCameraState& state) {
  fx::BeginFirstPersonView(host, state);
  RunFrames(64, [&] {
    fx::UpdateFirstPersonViewEntry(host, state);
    return host.flow_mode != fx::kFirstPersonLooking;
  });
}

// Presses Cross to leave the view and runs the exit transition to the end;
// returns the flow mode the press set.
std::uint32_t LeaveFirstPerson(FakeCameraHost& host,
                               fx::FieldCameraState& state) {
  host.calls.clear();
  host.pressed = fx::kPadCross;
  fx::CheckFirstPersonViewExit(host, state);
  host.pressed = 0;
  const std::uint32_t leaving = host.flow_mode;
  RunFrames(64, [&] {
    fx::UpdateFirstPersonViewExit(host, state);
    return state.transition.has_value();
  });
  return leaving;
}

struct LiveCameraFixture {
  Render_Camera saved_camera = Render_frameScratch.camera;
  Field_SphericalCamera saved_spherical = Field_sphericalCamera;
  ~LiveCameraFixture() {
    Render_frameScratch.camera = saved_camera;
    Field_sphericalCamera = saved_spherical;
  }
  LiveCameraFixture() = default;
  LiveCameraFixture(const LiveCameraFixture&) = delete;
  LiveCameraFixture& operator=(const LiveCameraFixture&) = delete;
  LiveCameraFixture(LiveCameraFixture&&) = delete;
  LiveCameraFixture& operator=(LiveCameraFixture&&) = delete;
};
}  // namespace

TEST_CASE("Camera steps take the step or the remaining distance",
          "[field-camera][field-camera-core]") {
  CHECK(fx::AddCameraStepWithFallback(100, 10, 50) == 110);
  CHECK(fx::AddCameraStepWithFallback(100, 0, 50) == 150);
  CHECK(fx::AddCameraStepWithFallback(100, 60, 50) == 150);
  CHECK(fx::AddCameraStepWithFallback(100, -10, -50) == 90);
  CHECK(fx::AddCameraStepWithFallback(100, -60, -50) == 50);
  CHECK(fx::AddClampedCameraStep(100, 10, 50) == 110);
  CHECK(fx::AddClampedCameraStep(100, 60, 50) == 150);
  CHECK(fx::AddClampedCameraStep(100, -60, -50) == 50);
  CHECK(fx::AddClampedCameraStep(100, -10, -50) == 90);
  CHECK(fx::AddClampedCameraStep(100, 0, 50) == 100);
}

TEST_CASE("Camera pitch presets and roll keep the original values",
          "[field-camera][field-camera-core]") {
  fx::FieldCameraState state;
  for (const std::int32_t kept : {-0x300, -0x80}) {
    fx::SetCameraPitchPreset(state, kept);
    CHECK(state.spherical.values.pitch == kept);
  }
  for (const std::int32_t other : {-0x180, 0, 0x300}) {
    state.spherical.values.pitch = 7;
    fx::SetCameraPitchPreset(state, other);
    CHECK(state.spherical.values.pitch == -0x180);
  }
  fx::SetCameraRoll(state, 0x1ABC);
  CHECK(state.camera.angles.z == 0x1ABC);
  CHECK(fx::CameraRoll(state) == 0xABC);
  fx::SetCameraRoll(state, -1);
  CHECK(fx::CameraRoll(state) == 0xFFF);
}

TEST_CASE("Camera facing word carries the first-person flag",
          "[field-camera][field-camera-core]") {
  fx::FieldCameraState state;
  fx::SetCameraPosition(state, {100 * kOne, 0, 0});
  fx::SetCameraLookAt(state, {});
  state.first_person_view_enabled = 1;
  state.view_facing_word = 0xFFFF8123U;
  fx::UpdateCameraFacingState(state);
  CHECK(state.view_facing_word == 0x10123U);
  CHECK(state.camera.angles.y == 0x400);
  CHECK(state.camera.angles.x == 0);
  CHECK(fx::CameraPosition(state) == fx::CameraVector{100 * kOne, 0, 0});
  CHECK(fx::CameraLookAt(state) == fx::CameraVector{});
  state.first_person_view_enabled = 0;
  fx::UpdateCameraFacingState(state);
  CHECK(state.view_facing_word == 0x0123U);
}

TEST_CASE("First-person disable and the action distance follow the flags",
          "[field-camera][field-camera-core]") {
  FakeCameraHost host;
  fx::FieldCameraState state;
  CHECK(fx::SetFirstPersonViewDisabled(host, state, 1) == 0);
  CHECK(fx::SetFirstPersonViewDisabled(host, state, 0) == 1);
  CHECK(fx::SetFirstPersonViewDisabled(host, state, 2) == 1);
  CHECK(state.first_person_view_enabled == 1);
  CHECK(std::ranges::count(host.calls, "ClearGeometryOcclusion") == 3);
  host.calls.clear();
  for (const std::int32_t zoom :
       {fx::kZoomingIn, fx::kNearZoom, fx::kZoomingOut, fx::kFarZoom, 4}) {
    state.spherical.motion.zoom_mode = zoom;
    fx::StorePreferredActionCameraDistance(host, state);
  }
  CHECK(host.calls == Calls{"SetPreferredActionCameraDistance 0x600",
                            "SetPreferredActionCameraDistance 0x600",
                            "SetPreferredActionCameraDistance 0x900",
                            "SetPreferredActionCameraDistance 0x900"});
}

TEST_CASE("Camera orbit positions round trip through the synced angles",
          "[field-camera][field-camera-core]") {
  fx::FieldCameraState state;
  state.camera.look_at = {10 * kOne, -90 * kOne, 20 * kOne};
  fx::SetCameraPositionFromAngles(state, Orbit(0x400, 0, 0x600));
  CHECK(state.camera.position ==
        fx::CameraVector{(10 + 0x600) * kOne, -90 * kOne, 20 * kOne});
  fx::OrbitAngles synced = Orbit(1, 2, 3);
  CHECK(fx::SyncCameraAnglesFromPosition(state, &synced) == 0x400);
  CHECK(synced.yaw == 0x400);
  CHECK(synced.pitch == 0);
  CHECK(synced.distance == 0x600);
  CHECK(fx::ComputeSphericalCameraPosition({}, Orbit(0, 0, 0x100)) ==
        fx::CameraVector{0, 0, 0x100 * kOne});
  fx::SetCameraLookAtFromAngles(state, Orbit(0x400, 0, 0x600));
  CHECK(state.camera.look_at ==
        fx::CameraVector{10 * kOne, -90 * kOne, 20 * kOne});
}

TEST_CASE("Camera divisions by zero take the R3000 quotient with a warning",
          "[field-camera][field-camera-core]") {
  ChannelWarnings warnings{"field.camera"};
  const fx::CameraVector straight_up =
      fx::ComputeSphericalCameraPosition({}, Orbit(0, 0x400, 0x600));
  CHECK(straight_up.z == 0x600 * kOne);
  fx::FieldCameraState state;
  state.camera.position = {kOne, 0, 0x7000 * kOne};
  fx::OrbitAngles synced;
  fx::SyncCameraAnglesFromPosition(state, &synced);
  CHECK(synced.distance == -1);
  FakeCameraHost host;
  state.transition = fx::FirstPersonTransition{};
  state.camera.position = {};
  fx::ComputeFirstPersonCameraTransition(host, state, {0x10 * kOne, 0, 0});
  CHECK(state.transition->frames == -1);
  REQUIRE(warnings.messages.size() == 3);
  CHECK(warnings.messages[0] ==
        "camera pitch 1024 has a zero cosine; its slope takes the R3000 "
        "quotient");
  CHECK(warnings.messages[1] ==
        "camera offset 1 normalizes to a zero axis; its distance takes the "
        "R3000 quotient");
  CHECK(warnings.messages[2] ==
        "first-person camera transition has no vertical travel; its frame "
        "count takes the R3000 quotient");
}

TEST_CASE("Camera look-at clamps inside the zone less the orbit radius",
          "[field-camera][field-camera-core]") {
  FakeCameraHost host;
  host.bounds = {.min_x = 0, .min_z = 0, .max_x = 10, .max_z = 10};
  fx::FieldCameraState state;
  state.spherical.values.distance = fx::kFarCameraDistance;
  state.camera.look_at.y = -0x100000;
  CHECK(fx::ClampCameraLookAtToZone(host, state, {-5, -0x110000, 0x7FFFFFFF}) ==
        fx::CameraVector{0x168000, -0x100000, 0x398000});
  CHECK(fx::ClampCameraLookAtToZone(host, state,
                                    {0x200FFF, -0x200000, 0x200123}) ==
        fx::CameraVector{0x200000, -0x200000, 0x200000});
  state.camera.look_at.y = 0x10000;
  CHECK(fx::ClampCameraLookAtToZone(host, state, {0x200000, 0x20000, 0x200000})
            .y == 0);
  host.bounds = {.min_x = 0, .min_z = 0, .max_x = 1, .max_z = 1};
  CHECK(fx::ClampCameraLookAtToZone(host, state, {}) ==
        fx::CameraVector{0x40000, 0, 0x40000});
}

TEST_CASE("Camera tracking eases toward a target off the screen centre",
          "[field-camera][field-camera-core]") {
  FakeCameraHost host;
  fx::FieldCameraState state;
  state.spherical.tracking_speed = 20;
  const fx::CameraTrackingStep centred = fx::ComputeCameraTrackingStep(
      host, state, {.x = 10, .y = 0, .z = 0, .w = 0}, false, 0x28);
  CHECK_FALSE(centred.not_centered);
  CHECK(centred.step == fx::CameraVector{12 * UnitAxis(10), 0, 0});
  CHECK(centred.displacement == fx::CameraVector{10 * kOne, 0, 0});
  CHECK(host.projected == actors::MotionVector{10, 0, 0, 0});
  CHECK(fx::StepCameraLookAtTowardTarget(
            host, state, {.x = 10, .y = 0, .z = 0, .w = 0}, false, 0x28) == 4);
  CHECK(state.camera.look_at.x == 4 * UnitAxis(10));
}

TEST_CASE("Camera tracking speeds up while the target is off centre",
          "[field-camera][field-camera-core]") {
  FakeCameraHost host;
  host.screen = kOffCentreScreen;
  fx::FieldCameraState state;
  std::vector<std::int32_t> speeds;
  for (int frame = 0; frame < 6; ++frame) {
    const fx::CameraTrackingStep step = fx::ComputeCameraTrackingStep(
        host, state, {.x = 100, .y = 0, .z = 0, .w = 0}, false, 0x28);
    CHECK(step.not_centered);
    speeds.push_back(state.spherical.tracking_speed);
  }
  CHECK(speeds == std::vector<std::int32_t>{8, 16, 24, 32, 40, 40});
  state.spherical.tracking_speed = 0;
  static_cast<void>(fx::ComputeCameraTrackingStep(
      host, state, {.x = 20, .y = 0, .z = 0, .w = 0}, false, 0x28));
  CHECK(state.spherical.tracking_speed == 8);
}

TEST_CASE("Camera dead zone lags Ashley by a fixed step",
          "[field-camera][field-camera-core]") {
  FakeCameraHost host;
  fx::FieldCameraState state;
  state.camera.look_at = {0, -0x100000, 0};
  fx::TrackCameraLookAtWithinDeadZone(host, state,
                                      {0x20000, -0x200000, -0x5000});
  CHECK(state.camera.look_at == fx::CameraVector{0x12000, -0x1A0000, -0x5000});
  state.camera.look_at = {0, -0x100000, 0};
  fx::TrackCameraLookAtWithinDeadZone(host, state, {-0x20000, 0, 0x1000});
  CHECK(state.camera.look_at == fx::CameraVector{-0x12000, -0x60000, 0x1000});
}

TEST_CASE("Camera quarter turns ease to the next eighth",
          "[field-camera][field-camera-core]") {
  FakeCameraHost host;
  fx::FieldCameraState state;
  state.spherical.values = Orbit(0x400, 0, fx::kNearCameraDistance);
  state.spherical.motion.zoom_mode = fx::kNearZoom;
  host.held = fx::kPadR1;
  fx::UpdateCameraOrbitInput(host, state);
  CHECK(state.spherical.motion.turn == fx::kTurningRight);
  CHECK(state.spherical.motion.target_yaw == 0x200);
  host.held = 0;
  const int frames = RunFrames(64, [&] {
    fx::UpdateCameraOrbitInput(host, state);
    return state.spherical.motion.turn != fx::kNoTurn;
  });
  CHECK(frames == 20);
  CHECK(state.spherical.values.yaw == 0x200);
  CHECK(state.spherical.motion.turn_speed == 8);
  CHECK(state.spherical.values.distance == fx::kNearCameraDistance);

  host.held = fx::kPadL1;
  fx::UpdateCameraOrbitInput(host, state);
  CHECK(state.spherical.motion.turn == fx::kTurningLeft);
  CHECK(state.spherical.motion.target_yaw == 0x400);
}

TEST_CASE("Camera quarter turns wait while the input is locked",
          "[field-camera][field-camera-core]") {
  FakeCameraHost host;
  fx::FieldCameraState state;
  state.spherical.values = Orbit(0x400, 0, fx::kNearCameraDistance);
  state.spherical.motion.zoom_mode = fx::kNearZoom;
  host.held = fx::kPadR1;
  host.control_mode = fx::kControlModeLocked;
  fx::UpdateCameraOrbitInput(host, state);
  CHECK(state.spherical.motion.turn == fx::kNoTurn);
  host.control_mode = fx::kControlModeAction;
  host.flow_mode = fx::kFlowFollowActor;
  fx::UpdateCameraOrbitInput(host, state);
  CHECK(state.spherical.motion.turn == fx::kNoTurn);
  host.flow_mode = fx::kFlowChooseDirectTarget;
  host.shoulder_pages = true;
  fx::UpdateCameraOrbitInput(host, state);
  CHECK(state.spherical.motion.turn == fx::kNoTurn);
  host.held = fx::kPadR1 | fx::kPadL1;
  host.shoulder_pages = false;
  fx::UpdateCameraOrbitInput(host, state);
  CHECK(state.spherical.motion.turn == fx::kNoTurn);
}

TEST_CASE("Camera zoom toggles between the near and far distances",
          "[field-camera][field-camera-core]") {
  FakeCameraHost host;
  fx::FieldCameraState state;
  state.spherical.values = Orbit(0, 0, fx::kNearCameraDistance);
  state.spherical.motion.zoom_mode = fx::kNearZoom;
  state.spherical.preferred_distance = fx::kNearCameraDistance;
  host.pressed = fx::kPadSelect;
  fx::UpdateCameraOrbitInput(host, state);
  CHECK(state.spherical.motion.zoom_mode == fx::kZoomingOut);
  CHECK(state.spherical.preferred_distance == fx::kFarCameraDistance);
  CHECK(state.spherical.values.distance == 0x610);
  host.pressed = 0;
  RunFrames(200, [&] {
    fx::UpdateCameraOrbitInput(host, state);
    return state.spherical.motion.zoom_mode != fx::kFarZoom;
  });
  CHECK(state.spherical.values.distance == fx::kFarCameraDistance);

  host.control_mode = fx::kControlModeLocked;
  state.spherical.preferred_distance = fx::kNearCameraDistance;
  host.pressed = fx::kPadSelect;
  fx::UpdateCameraOrbitInput(host, state);
  CHECK(state.spherical.motion.zoom_mode == fx::kFarZoom);
  CHECK(state.spherical.preferred_distance == fx::kFarCameraDistance);
}

TEST_CASE("Action camera without a battle action holds still",
          "[field-camera][field-camera-core]") {
  ChannelWarnings warnings{"field.camera"};
  FakeCameraHost host;
  Required(host.placement).tile = {.x = 1, .layer = 0, .z = 2, .spare = 0};
  fx::FieldCameraState state;
  state.camera.look_at = {kOne, -kOne, kOne};
  for (const std::uint32_t flow :
       {fx::kFlowChooseDirectTarget, fx::kFlowPlaceArea}) {
    host.flow_mode = flow;
    fx::TrackActionCameraTarget(host, state, 0);
  }
  CHECK(state.camera.look_at == fx::CameraVector{kOne, -kOne, kOne});
  CHECK(host.Called("CollisionCellFlags 1 2 0"));
  CHECK(host.Called("ClearGeometryOcclusion"));
  host.placement.reset();
  host.flow_mode = 0;
  fx::TrackActionCameraTarget(host, state, 3);
  REQUIRE(warnings.messages.size() == 3);
  CHECK(warnings.messages[0] ==
        "no battle action for the camera to frame; it holds still");
  CHECK(warnings.messages[2] ==
        "actor 3 has no placement; the camera uses the origin");
}

TEST_CASE("Action camera frames the previewed target or the area",
          "[field-camera][field-camera-core]") {
  FakeCameraHost host;
  host.screen = kOffCentreScreen;
  host.flow_mode = fx::kFlowChooseDirectTarget;
  host.action = fx::ActionCameraTargets{
      .preview = actors::MotionVector{50, 0, 0, 0},
      .area_center = {.x = 100, .y = 0, .z = 0, .w = 0}};
  fx::FieldCameraState state;
  fx::TrackActionCameraTarget(host, state, 0);
  CHECK(state.camera.look_at.x == 8 * UnitAxis(50));
  CHECK(state.camera.look_at.y == 0);
  CHECK(state.camera.look_at.z == 0);
  CHECK(host.projected == actors::MotionVector{50, 0, 0, 0});
  host.flow_mode = fx::kFlowPlaceArea;
  host.sphere_mode = 1;
  state.spherical.tracking_speed = 32;
  fx::TrackActionCameraTarget(host, state, 0);
  CHECK(state.spherical.tracking_speed == 8);
  CHECK(host.projected == actors::MotionVector{100, 0, 0, 0});
  host.sphere_mode = 0;
  state.spherical.tracking_speed = 32;
  fx::TrackActionCameraTarget(host, state, 0);
  CHECK(state.spherical.tracking_speed == 40);
}

TEST_CASE("Camera tile constraint toggles the geometry occlusion",
          "[field-camera][field-camera-core]") {
  ChannelWarnings warnings{"field.camera"};
  FakeCameraHost host;
  fx::FieldCameraState state;
  const actors::PackedTile tile{.x = 1, .layer = 0, .z = 2, .spare = 0};
  host.cell_flags = 1;
  state.spherical.preferred_distance = fx::kFarCameraDistance;
  fx::UpdateCameraTileConstraint(host, state, 0, tile);
  state.spherical.preferred_distance = fx::kNearCameraDistance;
  fx::UpdateCameraTileConstraint(host, state, 0, tile);
  host.cell_flags = 2;
  fx::UpdateCameraTileConstraint(host, state, 0, tile);
  CHECK(host.calls ==
        Calls{"CollisionCellFlags 1 2 0", "ApplyGeometryOcclusion 1 2 0 1",
              "CollisionCellFlags 1 2 0", "ApplyGeometryOcclusion 1 2 0 0",
              "CollisionCellFlags 1 2 0", "ClearGeometryOcclusion"});
  host.calls.clear();
  host.state_flags = 0x30000;
  host.cell_flags.reset();
  fx::UpdateCameraTileConstraint(host, state, 0, tile);
  CHECK(host.calls ==
        Calls{"CollisionCellFlags 0 0 3", "ClearGeometryOcclusion"});
  REQUIRE(warnings.messages.size() == 1);
  CHECK(warnings.messages[0] ==
        "camera tile has no authored collision cell: x=0 z=0 layer=3");
}

TEST_CASE("First-person view is not entered without heap",
          "[field-camera][field-camera-core]") {
  ChannelWarnings warnings{"field.camera"};
  FakeCameraHost host;
  fx::FieldCameraState state = FirstPersonStart(host);
  const fx::FieldCameraState before = state;
  host.allocation.reset();
  fx::BeginFirstPersonView(host, state);
  CHECK(state == before);
  CHECK(host.calls.empty());
  CHECK(host.idle_poses == 0);
  REQUIRE(warnings.messages.size() == 1);
  CHECK(warnings.messages[0] ==
        "no heap for the first-person camera transition; the view is not "
        "entered");
}

TEST_CASE("First-person view entry saves the orbit camera",
          "[field-camera][field-camera-core]") {
  FakeCameraHost host;
  fx::FieldCameraState state = FirstPersonStart(host);
  const fx::FieldCameraState before = state;
  fx::BeginFirstPersonView(host, state);
  const fx::FirstPersonTransition& transition = Required(state.transition);
  CHECK(transition.from_camera == before.camera);
  CHECK(transition.from_spherical == before.spherical);
  CHECK(transition.from_near_clip == 16);
  CHECK(transition.from_projection_distance == 512);
}

TEST_CASE("First-person view entry aims at Ashley's eyes and hides the field",
          "[field-camera][field-camera-core]") {
  FakeCameraHost host;
  fx::FieldCameraState state = FirstPersonStart(host);
  fx::BeginFirstPersonView(host, state);
  const fx::FirstPersonTransition& transition = Required(state.transition);
  CHECK(transition.to_spherical.values == Orbit(0x400, 0, 0x600));
  CHECK(transition.target_position == fx::CameraVector{0, -180 * kOne, 0});
  CHECK(transition.target_look_at ==
        fx::CameraVector{-0x900 * kOne, -180 * kOne, 0});
  CHECK(host.idle_poses == 16);
  CHECK(host.calls ==
        Calls{"SetControlMode 7", "SetActionFlowMode 0", "SetHudHidden 1",
              "SetMinimapVisible 0", "SetMovingPlatformsPaused 1",
              "SetActorMarkersHidden 1", "SetActorTransitionMode 1"});
}

TEST_CASE("First-person view entry ends at Ashley's eyes",
          "[field-camera][field-camera-core]") {
  ChannelWarnings warnings{"field.camera"};
  FakeCameraHost host;
  fx::FieldCameraState state = FirstPersonStart(host);
  EnterFirstPerson(host, state);
  CHECK(state.first_person_view_enabled == 1);
  CHECK(state.geometry_draw_mode == 1);
  CHECK(state.camera.position == fx::CameraVector{0, -180 * kOne, 0});
  CHECK(host.near_clip == 4);
  CHECK(host.projection == 224);
  CHECK(state.camera.far_clip == 0x2000);
  CHECK(warnings.messages.empty());
}

TEST_CASE("First-person view exit hands the field back",
          "[field-camera][field-camera-core]") {
  ChannelWarnings warnings{"field.camera"};
  FakeCameraHost host;
  fx::FieldCameraState state = FirstPersonStart(host);
  EnterFirstPerson(host, state);
  CHECK(LeaveFirstPerson(host, state) == fx::kFirstPersonLeaving);
  CHECK_FALSE(state.transition.has_value());
  CHECK(host.Called("FreeTransition"));
  CHECK(host.Called("EnterFieldExploration"));
  CHECK(host.Called("SetHudHidden 0"));
  CHECK(host.near_clip == 16);
  CHECK(host.projection == 512);
  CHECK(warnings.messages.empty());
}

TEST_CASE("First-person view exit restores the orbit camera",
          "[field-camera][field-camera-core]") {
  ChannelWarnings warnings{"field.camera"};
  FakeCameraHost host;
  fx::FieldCameraState state = FirstPersonStart(host);
  const fx::CameraVector look_at = state.camera.look_at;
  EnterFirstPerson(host, state);
  static_cast<void>(LeaveFirstPerson(host, state));
  CHECK(state.spherical.values == Orbit(0x400, -0x180, 0x600));
  CHECK(state.spherical.motion.target_yaw == 0x400);
  CHECK(state.camera.look_at == look_at);
  CHECK(state.camera.position == fx::ComputeSphericalCameraPosition(
                                     look_at, Orbit(0x400, -0x180, 0x600)));
  CHECK(state.camera.far_clip == 0x1000);
  CHECK(warnings.messages.empty());
}

TEST_CASE("First-person transition routines refuse a missing record",
          "[field-camera][field-camera-core]") {
  ChannelWarnings warnings{"field.camera"};
  FakeCameraHost host;
  fx::FieldCameraState state;
  host.near_clip = 99;
  fx::ComputeFirstPersonCameraTransition(host, state, {});
  CHECK_FALSE(fx::StepFirstPersonCameraTransition(host, state));
  fx::CheckFirstPersonViewExit(host, state);
  CHECK(warnings.messages.size() == 2);
  host.pressed = fx::kPadStart;
  fx::CheckFirstPersonViewExit(host, state);
  fx::EndFirstPersonView(host, state);
  CHECK(host.near_clip == 99);
  CHECK_FALSE(host.Called("FreeTransition"));
  CHECK(host.calls ==
        Calls{"SetHudHidden 0", "SetMinimapVisible 1",
              "SetMovingPlatformsPaused 0", "SetActorMarkersHidden 0",
              "SetActorTransitionMode 0", "EnterFieldExploration"});
  const std::string suffix =
      " has no first-person camera transition; the camera is left as it is";
  CHECK(warnings.messages ==
        Calls{"ComputeFirstPersonCameraTransition" + suffix,
              "StepFirstPersonCameraTransition" + suffix,
              "CheckFirstPersonViewExit" + suffix,
              "EndFirstPersonView" + suffix});
}

TEST_CASE("First-person look turns Ashley and clamps the pitch",
          "[field-camera][field-camera-core]") {
  FakeCameraHost host;
  Required(host.placement).position = {.x = 10, .y = 0, .z = 20, .w = 0};
  fx::FieldCameraState state;
  state.spherical.values = Orbit(0, 880, fx::kNearCameraDistance);
  host.held = fx::kPadDown | fx::kPadLeft;
  host.stick = fx::StickPosition{.x = 255, .y = 128};
  fx::UpdateFirstPersonCamera(host, state);
  CHECK(state.spherical.values.pitch == 896);
  CHECK(state.spherical.values.yaw == -11);
  CHECK(host.calls == Calls{"TurnActorTo 0 4085"});
  CHECK(state.camera.position ==
        fx::CameraVector{10 * kOne, -180 * kOne, 20 * kOne});
  host.held = fx::kPadUp;
  host.stick.reset();
  state.spherical.values.pitch = -890;
  fx::UpdateFirstPersonCamera(host, state);
  CHECK(state.spherical.values.pitch == -896);
  host.held = fx::kPadRight;
  host.stick = fx::StickPosition{.x = 0, .y = 0};
  fx::UpdateFirstPersonCamera(host, state);
  CHECK(state.spherical.values.yaw == -11 + 32 - 21);
}

TEST_CASE("Spherical camera initialization picks the zoom preset",
          "[field-camera][field-camera-core]") {
  FakeCameraHost host;
  fx::FieldCameraState state;
  static_cast<void>(fx::InitializeSphericalCamera(host, state, 11, 1, false));
  CHECK(state.spherical.values.yaw == 0x600);
  CHECK(state.spherical.motion.target_yaw == 0x600);
  CHECK(state.spherical.values.distance == fx::kNearCameraDistance);
  CHECK(state.spherical.preferred_distance == fx::kNearCameraDistance);
  CHECK(state.spherical.motion.zoom_mode == fx::kZoomingIn);
}

TEST_CASE("Spherical camera initialization keeps a setting given -1",
          "[field-camera][field-camera-core]") {
  FakeCameraHost host;
  fx::FieldCameraState state;
  static_cast<void>(fx::InitializeSphericalCamera(host, state, 11, 1, false));
  static_cast<void>(fx::InitializeSphericalCamera(host, state, -1, 2, false));
  CHECK(state.spherical.values.yaw == 0x600);
  CHECK(state.spherical.values.distance == fx::kFarCameraDistance);
  CHECK(state.spherical.motion.zoom_mode == fx::kZoomingOut);
  static_cast<void>(fx::InitializeSphericalCamera(host, state, 1, -1, false));
  CHECK(state.spherical.values.yaw == 0x200);
  CHECK(state.spherical.values.distance == fx::kFarCameraDistance);
}

TEST_CASE("Spherical camera look-at is written only for the view camera",
          "[field-camera][field-camera-core]") {
  FakeCameraHost host;
  fx::FieldCameraState state;
  state.spherical.values = Orbit(0, 0, fx::kNearCameraDistance);
  state.camera.look_at = {7 * kOne, 0, 7 * kOne};
  const fx::CameraPlacement apart =
      fx::InitializeSphericalCamera(host, state, -1, -1, false);
  CHECK(apart.look_at == fx::CameraVector{});
  CHECK(apart.position == fx::CameraVector{0, 0, 0x600 * kOne});
  CHECK(state.camera.look_at == fx::CameraVector{7 * kOne, 0, 7 * kOne});
  const fx::CameraPlacement aliased =
      fx::InitializeSphericalCamera(host, state, -1, -1, true);
  CHECK(aliased.look_at == fx::CameraVector{0, -kLookAtHeight, 0});
  CHECK(state.camera.look_at == aliased.look_at);
  CHECK(host.calls.empty());
}

TEST_CASE("Camera reset and look-at check follow Ashley",
          "[field-camera][field-camera-core]") {
  FakeCameraHost host;
  fx::FieldCameraState state;
  state.spherical.values = Orbit(0, 0, fx::kNearCameraDistance);
  state.camera.angles = {1, 2, 3};
  state.camera.far_clip = 0x2000;
  fx::ResetCameraToAshley(host, state);
  CHECK(state.camera.far_clip == 0x1000);
  CHECK(state.camera.angles == fx::CameraVector{});
  CHECK(state.camera.look_at == fx::CameraVector{0, -kLookAtHeight, 0});
  CHECK(state.camera.position ==
        fx::CameraVector{0, -kLookAtHeight, 0x600 * kOne});
  CHECK(fx::IsCameraLookAtOnAshley(host, state));
  state.camera.look_at.x = 0x123;
  state.camera.look_at.z = 0x789;
  CHECK(fx::IsCameraLookAtOnAshley(host, state));
  state.camera.look_at.x = kOne;
  CHECK_FALSE(fx::IsCameraLookAtOnAshley(host, state));
}

TEST_CASE("Ashley follow camera waits for paused room actors",
          "[field-camera][field-camera-core]") {
  FakeCameraHost host;
  fx::FieldCameraState state;
  state.spherical.values = Orbit(0, 0, fx::kNearCameraDistance);
  host.paused = true;
  fx::UpdateAshleyFollowCamera(host, state);
  CHECK_FALSE(host.Called("UpdateRoomActorMovement"));
  // The look-at height holds until Ashley rises or falls 96 units.
  CHECK(state.camera.look_at == fx::CameraVector{});
  CHECK(state.camera.position == fx::CameraVector{0, 0, 0x600 * kOne});
  state.camera.look_at = {0, -0x100000, 0};
  fx::UpdateAshleyFollowCamera(host, state);
  CHECK(state.camera.look_at == fx::CameraVector{0, -0xBA000, 0});
  host.paused = false;
  fx::UpdateAshleyFollowCamera(host, state);
  CHECK(host.Called("UpdateRoomActorMovement"));
}

TEST_CASE("Live camera entries keep the scratch camera's padding",
          "[field-camera][field-camera-core]") {
  const LiveCameraFixture fixture;
  ChannelWarnings warnings{"field.camera"};
  VS::Field::LiveSetCameraRoll(0x1ABC);
  CHECK(VS::Field::LiveCameraRoll() == 0xABC);
  const VECTOR position{1, 2, 3, 77};
  VS::Field::LiveSetCameraPosition(&position);
  VECTOR out{0, 0, 0, 99};
  VS::Field::LiveCameraPosition(&out);
  CHECK(out.vx == 1);
  CHECK(out.vy == 2);
  CHECK(out.vz == 3);
  CHECK(out.pad == 99);
  for (int call = 0; call < 1024 && warnings.messages.empty(); ++call) {
    VS::Field::LiveSetCameraPosition(nullptr);
  }
  REQUIRE_FALSE(warnings.messages.empty());
  CHECK(warnings.messages[0] ==
        "SetCameraPosition was given a null record; the call is refused");
}
