#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Core/Math/Distance.hpp"
#include "Core/Math/ScalarMath.hpp"
#include "Event/Camera.hpp"
#include "Event/EventCameraFrame.hpp"
#include "Event/EventCameraFrameLive.hpp"
#include "Event/ScriptCamera.h"
#include "Event/ScriptCommand.hpp"
#include "Event/ScriptControlOpcodes.h"
#include "Field/CameraLive.hpp"

namespace {
namespace ev = VS::Event;
using VS::Core::Math::Mat3;
using VS::Core::Math::Vec3;

std::string Text(Vec3 vector) {
  return std::to_string(vector.x) + " " + std::to_string(vector.y) + " " +
         std::to_string(vector.z);
}

// Records every port call in order as text. The field camera, the angle, the
// rotation and the rotated vectors are set values.
class RecordingOutput final : public ev::EventCameraOutputPort {
 public:
  ev::EventCameraPair field_camera{};
  std::int32_t yaw = 0;
  Mat3 rotation{};
  Vec3 look_at_turned{};
  Vec3 eye_turned{};
  std::vector<std::string> calls;

  ev::EventCameraPair InitializeFieldCamera() override {
    calls.emplace_back("field");
    return field_camera;
  }
  std::int32_t FixedAtan2(std::int32_t y, std::int32_t x) override {
    calls.push_back("atan " + std::to_string(y) + " " + std::to_string(x));
    return yaw;
  }
  Mat3 BuildRotation(VS::Core::Math::Angles3 angles) override {
    calls.push_back("rotation " + std::to_string(angles.x) + " " +
                    std::to_string(angles.y) + " " + std::to_string(angles.z));
    return rotation;
  }
  Vec3 ApplyMatrix(const Mat3& matrix, Vec3 vector,
                   ev::EventShakeTarget target) override {
    const bool same = matrix.m == rotation.m;
    const bool look_at = target == ev::EventShakeTarget::kLookAt;
    calls.push_back(std::string(look_at ? "turn look-at " : "turn eye ") +
                    Text(vector) + (same ? "" : " other matrix"));
    return look_at ? look_at_turned : eye_turned;
  }
  void SetLookAt(Vec3 look_at) override {
    calls.push_back("look-at " + Text(look_at));
  }
  void SetPosition(Vec3 position) override {
    calls.push_back("position " + Text(position));
  }
  void SetRoll(std::int16_t angle) override {
    calls.push_back("roll " + std::to_string(angle));
  }
  void SetScreenRotation(std::int16_t angle) override {
    calls.push_back("screen " + std::to_string(angle));
  }
  void SetProjectionDistance(std::int16_t distance) override {
    calls.push_back("projection " + std::to_string(distance));
  }
  void SetNearClip(std::int16_t near_clip) override {
    calls.push_back("near " + std::to_string(near_clip));
  }
  void SetFarClip(std::int16_t far_clip,
                  std::uint8_t preserve_texture_lod) override {
    calls.push_back("far " + std::to_string(far_clip) + " " +
                    std::to_string(preserve_texture_lod));
  }
};

class RecordingSteps final : public ev::EventCameraStepPort {
 public:
  ev::EventCameraPair current{};
  std::vector<std::string> calls;

  ev::EventCameraPair CurrentPositions() override {
    calls.emplace_back("current");
    return current;
  }
  void SetPartnerPositions(const ev::EventCameraPair& partners) override {
    calls.push_back("partners " + Text(partners.look_at) + " / " +
                    Text(partners.eye));
  }
  void Advance(ev::EventCameraStep step) override {
    calls.push_back("step " + std::to_string(static_cast<int>(step)));
  }
};

ev::EventCameraProgress IdleProgress() { return {}; }

ev::EventCameraFrame StillFrame() {
  ev::EventCameraFrame frame;
  frame.look_at = {.x = 0x10000, .y = -0x2000, .z = 0x30000};
  frame.eye = {.x = 0x4000, .y = 0x5000, .z = -0x6000};
  frame.origin_actor = ev::kEventCameraWorldOrigin;
  frame.origin_offset = {.x = 7, .y = 8, .z = 9};
  return frame;
}
}  // namespace

TEST_CASE("Camera completion mask sets every bit when all parts rest",
          "[event][camera][camera-frame]") {
  CHECK(ev::EventCameraCompletionMask(IdleProgress()) == 0xFF);
  ev::EventCameraProgress busy{.look_at_mode = 0,
                               .eye_mode = 0x0E,
                               .camera_roll_duration = 1,
                               .projection_duration = 1,
                               .near_clip_duration = 1,
                               .far_clip_duration = 1,
                               .screen_effect_duration = 1,
                               .shake_durations = {1, 0}};
  CHECK(ev::EventCameraCompletionMask(busy) == 0);
}

template <typename Change>
std::uint8_t MaskWithout(Change change) {
  ev::EventCameraProgress progress = IdleProgress();
  change(progress);
  return ev::EventCameraCompletionMask(progress);
}

TEST_CASE("Camera completion mask clears only a busy track's bit",
          "[event][camera][camera-frame]") {
  CHECK(MaskWithout([](auto& p) { p.look_at_mode = 0x0B; }) == 0xFE);
  CHECK(MaskWithout([](auto& p) { p.eye_mode = 0; }) == 0xFD);
  CHECK(MaskWithout([](auto& p) { p.camera_roll_duration = -1; }) == 0xFB);
  CHECK(MaskWithout([](auto& p) { p.projection_duration = 0x80; }) == 0xF7);
}

TEST_CASE("Camera completion mask clears only a busy effect's bit",
          "[event][camera][camera-frame]") {
  CHECK(MaskWithout([](auto& p) { p.near_clip_duration = 2; }) == 0xEF);
  CHECK(MaskWithout([](auto& p) { p.far_clip_duration = 0xFF; }) == 0xDF);
  CHECK(MaskWithout([](auto& p) { p.screen_effect_duration = 0x100; }) == 0xBF);
  CHECK(MaskWithout([](auto& p) { p.shake_durations = {0, 3}; }) == 0x7F);
}

TEST_CASE("Camera completion mask adds the shake bytes unsigned",
          "[event][camera][camera-frame]") {
  ev::EventCameraProgress progress = IdleProgress();
  progress.shake_durations = {0xFF, 0x01};
  CHECK(ev::EventCameraCompletionMask(progress) == 0x7F);
  progress.shake_durations = {0x80, 0x80};
  CHECK(ev::EventCameraCompletionMask(progress) == 0x7F);
  progress.camera_roll_duration = 0x100;
  CHECK(ev::EventCameraCompletionMask(progress) == 0x7B);
}

TEST_CASE("Camera wait opcode waits for every requested bit",
          "[event][camera][camera-frame]") {
  std::array<std::uint8_t, 2> instruction{0xE0, 0x05};
  const ev::ScriptCommand command{instruction.data(), instruction.size()};
  CHECK(ev::CommandWaitCameraTracks(command, 0x05) ==
        std::optional<std::intptr_t>{0});
  CHECK(ev::CommandWaitCameraTracks(command, 0xFF) ==
        std::optional<std::intptr_t>{0});
  CHECK(ev::CommandWaitCameraTracks(command, 0x04) ==
        std::optional<std::intptr_t>{1});
  CHECK(ev::CommandWaitCameraTracks(command, 0xFA) ==
        std::optional<std::intptr_t>{1});
  instruction[1] = 0;
  CHECK(ev::CommandWaitCameraTracks(command, 0) ==
        std::optional<std::intptr_t>{0});
  CHECK_FALSE(
      ev::CommandWaitCameraTracks({instruction.data(), 1}, 0xFF).has_value());
}

TEST_CASE("Camera state uses the tracks at the world origin without a shake",
          "[event][camera][camera-frame]") {
  RecordingOutput output;
  ev::EventCameraFrame frame = StillFrame();
  const ev::EventCameraFrame before = frame;
  ev::ApplyEventCameraState(frame, false, 1, output);
  CHECK(output.calls ==
        std::vector<std::string>{"look-at 65536 -8192 196608",
                                 "position 16384 20480 -24576"});
  CHECK(frame == before);
}

TEST_CASE("Camera state moves both tracks by the origin offset",
          "[event][camera][camera-frame]") {
  RecordingOutput output;
  ev::EventCameraFrame frame = StillFrame();
  frame.origin_actor = 0x1FFF;
  frame.look_at.x = 0x7FFFFFFF;
  ev::ApplyEventCameraState(frame, false, 0, output);
  CHECK(output.calls ==
        std::vector<std::string>{"look-at -2147483642 -8184 196617",
                                 "position 16391 20488 -24567"});
  CHECK(frame.look_at.x == 0x7FFFFFFF);
}

TEST_CASE("Camera state takes the field camera into both tracks",
          "[event][camera][camera-frame]") {
  RecordingOutput output;
  output.field_camera = {.look_at = {.x = 1, .y = 2, .z = 3},
                         .eye = {.x = 4, .y = 5, .z = 6}};
  ev::EventCameraFrame frame = StillFrame();
  frame.origin_actor = 0;
  ev::ApplyEventCameraState(frame, true, 0, output);
  CHECK(output.calls ==
        std::vector<std::string>{"field", "look-at 1 2 3", "position 4 5 6"});
  CHECK(frame.look_at == output.field_camera.look_at);
  CHECK(frame.eye == output.field_camera.eye);
}

TEST_CASE("Camera state turns the shake offsets by the view yaw",
          "[event][camera][camera-frame]") {
  RecordingOutput output;
  output.yaw = 0x12345;
  output.rotation.m = {{{1, 2, 3}, {4, 5, 6}, {7, 8, 9}}};
  output.look_at_turned = {.x = 0x100, .y = -0x200, .z = 0x300};
  output.eye_turned = {.x = -1, .y = 2, .z = -3};
  ev::EventCameraFrame frame = StillFrame();
  frame.shake_active_count = -1;
  frame.look_at_shake = {.x = 1, .y = -2, .z = 0x7FFF};
  frame.eye_shake = {.x = -0x8000, .y = 0, .z = 3};
  ev::ApplyEventCameraState(frame, false, 0, output);
  CHECK(output.calls ==
        std::vector<std::string>{
            "atan 49152 221184", "rotation 0 9029 0",
            "turn look-at 4096 -8192 134213632", "turn eye -134217728 0 12288",
            "look-at 65792 -8704 197376", "position 16383 20482 -24579"});
}

TEST_CASE("Camera state applies each changed setting once",
          "[event][camera][camera-frame]") {
  RecordingOutput output;
  ev::EventCameraFrame frame = StillFrame();
  frame.camera_roll = {.changed = 1, .value = -0x400};
  frame.screen_roll = {.changed = 0x80, .value = 0x200};
  frame.projection_distance = {.changed = 0, .value = 0x150};
  frame.near_clip = {.changed = 2, .value = 0x20};
  frame.far_clip = {.changed = 1, .value = 0x2000};
  ev::ApplyEventCameraState(frame, false, 0xFF, output);
  CHECK(output.calls == std::vector<std::string>{"look-at 65536 -8192 196608",
                                                 "position 16384 20480 -24576",
                                                 "roll -1024", "screen 512",
                                                 "near 32", "far 8192 255"});
  CHECK(frame.camera_roll ==
        ev::EventCameraSetting{.changed = 0, .value = -0x400});
  CHECK(frame.screen_roll.changed == 0);
  CHECK(frame.near_clip.changed == 0);
  CHECK(frame.far_clip ==
        ev::EventCameraSetting{.changed = 0, .value = 0x2000});
  CHECK(frame.projection_distance.value == 0x150);
}

TEST_CASE("Camera track update crosses the partners then steps in order",
          "[event][camera][camera-frame]") {
  RecordingSteps steps;
  steps.current = {.look_at = {.x = 1, .y = 2, .z = 3},
                   .eye = {.x = -4, .y = -5, .z = -6}};
  ev::UpdateEventCameraTracks(steps);
  CHECK(steps.calls ==
        std::vector<std::string>{"current", "partners -4 -5 -6 / 1 2 3",
                                 "step 0", "step 1", "step 2", "step 3",
                                 "step 4", "step 5", "step 6", "step 7"});
}

TEST_CASE("Live camera frame entries skip a missing camera or instruction",
          "[event][camera][camera-frame]") {
  VS::Event::CameraState* const saved = Event_camera;
  Event_camera = nullptr;
  const std::uint64_t before = ev::RefusedEventCameraFrameCount();
  std::array<std::uint8_t, 2> instruction{0xE0, 0x01};
  CHECK(Event_CommandWaitCameraTracksOpcode(instruction.data(), 0) == 0);
  CHECK(Event_CommandWaitCameraTracksOpcode(nullptr, 0) == 0);
  ev::ApplyEventCameraStateEntry();
  ev::UpdateEventCameraTracksEntry();
  CHECK(ev::EventCameraCompletionMaskEntry() == 0);
  CHECK(ev::RefusedEventCameraFrameCount() == before + 5);
  Event_camera = saved;
}

TEST_CASE("Live camera completion reads the workspace bytes unsigned",
          "[event][camera][camera-frame]") {
  VS::Event::CameraState* const saved = Event_camera;
  VS::Event::CameraState camera{};
  Event_camera = &camera;
  camera.look_at.motion_mode = 0xF;
  camera.eye.motion_mode = 0xF;
  CHECK(ev::EventCameraCompletionMaskEntry() == 0xFF);
  camera.shake_oscillators.at(0).duration_frames = 0xFF;
  camera.shake_oscillators.at(1).duration_frames = 1;
  camera.far_clip.duration_frames = 0x80;
  camera.screen_effect_tween.duration_frames = 0x100;
  CHECK(ev::EventCameraCompletionMaskEntry() == 0x1F);
  std::array<std::uint8_t, 2> instruction{0xE0, 0x1F};
  CHECK(Event_CommandWaitCameraTracksOpcode(instruction.data(), 0) == 0);
  instruction[1] = 0x9F;
  CHECK(Event_CommandWaitCameraTracksOpcode(instruction.data(), 0) == 1);
  Event_camera = saved;
}

TEST_CASE("Live camera entries keep the workspace layout",
          "[event][camera][camera-frame]") {
  VS::Event::CameraState* const saved = Event_camera;
  const char saved_skip = Event_skipNextFieldCameraRestore;
  VECTOR saved_look_at{};
  VECTOR saved_position{};
  VS::Field::LiveCameraLookAt(&saved_look_at);
  VS::Field::LiveCameraPosition(&saved_position);
  const int saved_roll = VS::Field::LiveCameraRoll();
  VS::Event::CameraState camera{};
  Event_camera = &camera;
  Event_skipNextFieldCameraRestore = 0;
  camera.look_at.current_position = {
      .vx = 0x1000, .vy = 0x2000, .vz = 0x3000, .pad = 0x55};
  camera.eye.current_position = {
      .vx = -0x1000, .vy = 0, .vz = 0x500, .pad = 0x66};
  camera.origin_actor_id = 1;
  camera.origin_offset = {.vx = 1, .vy = 2, .vz = 3, .pad = 0};
  camera.camera_roll.has_changed = 1;
  camera.camera_roll.angle = 0x123;
  ev::ApplyEventCameraStateEntry();
  VECTOR look_at{};
  VS::Field::LiveCameraLookAt(&look_at);
  CHECK((look_at.vx == 0x1001 && look_at.vy == 0x2002 && look_at.vz == 0x3003));
  CHECK(VS::Field::LiveCameraRoll() == 0x123);
  CHECK(camera.camera_roll.has_changed == 0);
  CHECK(camera.look_at.current_position.pad == 0x55);
  camera.look_at.motion_mode = 0xF;
  camera.eye.motion_mode = 0xF;
  ev::UpdateEventCameraTracksEntry();
  CHECK(camera.eye.partner_position.vx == 0x1000);
  CHECK(camera.look_at.partner_position.vz == 0x500);
  VS::Field::LiveSetCameraLookAt(&saved_look_at);
  VS::Field::LiveSetCameraPosition(&saved_position);
  VS::Field::LiveSetCameraRoll(saved_roll);
  Event_skipNextFieldCameraRestore = saved_skip;
  Event_camera = saved;
}
