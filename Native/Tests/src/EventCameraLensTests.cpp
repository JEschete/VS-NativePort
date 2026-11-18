#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Core/Math/Distance.hpp"
#include "Core/Scratch.hpp"
#include "Event/Camera.hpp"
#include "Event/EventCameraLens.hpp"
#include "Event/EventCameraLensLive.hpp"
#include "Event/ScriptCamera.h"
#include "Event/ScriptCommand.hpp"
#include "Event/ScriptControlOpcodes.hpp"
#include "State/Timers.hpp"

namespace {
namespace event = VS::Event;
using VS::Core::Math::Position3;

struct EaseCall {
  int mode;
  int frame;
  int duration;
};

class FakeLensHost final : public event::EventCameraLensHost {
 public:
  int tick_speed = 2;
  int fraction = 0;
  std::vector<EaseCall> ease_calls;
  std::vector<int> sine_angles;
  std::vector<int> sines;

  int TickSpeed() override { return tick_speed; }

  int EaseFraction(int mode, int frame, int duration) override {
    ease_calls.push_back({mode, frame, duration});
    return fraction;
  }

  int Sine(int angle) override {
    const std::size_t index = sine_angles.size();
    sine_angles.push_back(angle);
    return index < sines.size() ? sines.at(index) : 0;
  }
};

std::optional<std::intptr_t> Run(const std::vector<std::uint8_t>& bytes,
                                 event::EventCameraLenses& lenses,
                                 event::EventCameraShake& shake) {
  return event::CommandCameraLensOrShake(event::ScriptCommand{bytes}, lenses,
                                         shake);
}

event::EventShakeOscillator ActiveOscillator() {
  return {.envelope = event::kShakeEnvelopeFlat,
          .displacement = 0,
          .cycles = 1,
          .amplitude = 0x100,
          .elapsed_frames = 0,
          .duration_frames = 8,
          .axis_mask = 0};
}

// Saves the event camera globals the live entries use.
struct LiveLensFixture {
  VS::Event::CameraState camera{};
  VS::Event::CameraState* saved_camera = Event_camera;
  int saved_tick_speed = VS::State::tickSpeed;
  std::array<unsigned char, 0x100> saved_scratch{};

  LiveLensFixture() {
    std::memcpy(saved_scratch.data(), VS::Core::ScratchBase(),
                saved_scratch.size());
    Event_camera = &camera;
  }
  LiveLensFixture(const LiveLensFixture&) = delete;
  LiveLensFixture& operator=(const LiveLensFixture&) = delete;
  LiveLensFixture(LiveLensFixture&&) = delete;
  LiveLensFixture& operator=(LiveLensFixture&&) = delete;
  ~LiveLensFixture() {
    Event_camera = saved_camera;
    VS::State::tickSpeed = saved_tick_speed;
    std::memcpy(VS::Core::ScratchBase(), saved_scratch.data(),
                saved_scratch.size());
  }
};

std::int32_t ScratchWord(std::size_t offset) {
  std::int32_t value = 0;
  std::memcpy(&value,
              static_cast<unsigned char*>(VS::Core::ScratchBase()) + offset,
              sizeof(value));
  return value;
}

void SetScratchWord(std::size_t offset, std::int32_t value) {
  std::memcpy(static_cast<unsigned char*>(VS::Core::ScratchBase()) + offset,
              &value, sizeof(value));
}

Position3 ScratchPosition(std::size_t offset) {
  Position3 value;
  std::memcpy(&value,
              static_cast<unsigned char*>(VS::Core::ScratchBase()) + offset,
              sizeof(value));
  return value;
}
}  // namespace

// F01232 (func_800BDC9C)
TEST_CASE("Camera lens opcode sets an immediate value for a zero duration",
          "[event][camera][lens]") {
  event::EventCameraLenses lenses;
  event::EventCameraShake shake;
  lenses.projection_distance.start_value = 77;
  const std::optional<std::intptr_t> result =
      Run({0xEA, 0x40, 3, 0}, lenses, shake);
  REQUIRE(result.has_value());
  CHECK(result.value_or(-1) == 0);
  CHECK(lenses.projection_distance.value == 0x100);
  CHECK(lenses.projection_distance.changed == 1);
  CHECK(lenses.projection_distance.start_value == 77);
  CHECK(lenses.projection_distance.easing_mode == 0);
}

TEST_CASE("Camera lens opcode uses the default far and near clip targets",
          "[event][camera][lens]") {
  event::EventCameraLenses lenses;
  event::EventCameraShake shake;
  REQUIRE(Run({0xEC, 0, 3, 0}, lenses, shake).has_value());
  CHECK(lenses.far_clip.value == 0x1000);
  REQUIRE(Run({0xEB, 0, 3, 0}, lenses, shake).has_value());
  CHECK(lenses.near_clip.value == 0x40);
}

TEST_CASE("Camera lens opcode starts a tween from the current value",
          "[event][camera][lens]") {
  event::EventCameraLenses lenses;
  event::EventCameraShake shake;
  lenses.far_clip.value = 0x2000;
  lenses.far_clip.elapsed_frames = 9;
  REQUIRE(Run({0xEC, 0x80, 5, 30}, lenses, shake).has_value());
  CHECK(lenses.far_clip.start_value == 0x2000);
  // 0x80 << 7 is 0x4000.
  CHECK(lenses.far_clip.delta_value == 0x2000);
  CHECK(lenses.far_clip.easing_mode == 5);
  CHECK(lenses.far_clip.elapsed_frames == 0);
  CHECK(lenses.far_clip.duration_frames == 30);
  CHECK(lenses.far_clip.changed == 0);
}

TEST_CASE("Camera lens default target keeps the start except for near clip",
          "[event][camera][lens]") {
  event::EventCameraLenses lenses;
  event::EventCameraShake shake;
  lenses.projection_distance.value = 0x300;
  lenses.projection_distance.start_value = 11;
  lenses.near_clip.value = 0x50;
  lenses.near_clip.start_value = 12;
  REQUIRE(Run({0xEA, 0, 1, 4}, lenses, shake).has_value());
  REQUIRE(Run({0xEB, 0, 1, 4}, lenses, shake).has_value());
  CHECK(lenses.projection_distance.start_value == 11);
  CHECK(lenses.projection_distance.delta_value == 0x200);
  CHECK(lenses.near_clip.start_value == 0x50);
  CHECK(lenses.near_clip.delta_value == -0x10);
}

TEST_CASE("Camera shake opcode fills the first idle oscillator",
          "[event][camera][lens]") {
  event::EventCameraLenses lenses;
  event::EventCameraShake shake;
  shake.oscillators.at(0).duration_frames = 4;
  REQUIRE(Run({0xEF, 3, 0x90, 0x55, 2, 40}, lenses, shake).has_value());
  const event::EventShakeOscillator& filled = shake.oscillators.at(1);
  CHECK(filled.cycles == 3);
  CHECK(filled.amplitude == 0x90);
  CHECK(filled.axis_mask == 0x55);
  CHECK(filled.envelope == 2);
  CHECK(filled.duration_frames == 40);
  CHECK(shake.oscillators.at(0).duration_frames == 4);

  const event::EventCameraShake full = shake;
  REQUIRE(Run({0xEF, 1, 1, 1, 1, 1}, lenses, shake).has_value());
  CHECK(shake == full);
}

TEST_CASE("Camera shake opcode with zero amplitude stops both oscillators",
          "[event][camera][lens]") {
  event::EventCameraLenses lenses;
  event::EventCameraShake shake;
  shake.oscillators.at(0).duration_frames = 4;
  shake.oscillators.at(1).duration_frames = 5;
  shake.oscillators.at(1).cycles = 9;
  REQUIRE(Run({0xEF, 3, 0, 0x55, 2, 40}, lenses, shake).has_value());
  CHECK(shake.oscillators.at(0).duration_frames == 0);
  CHECK(shake.oscillators.at(1).duration_frames == 0);
  CHECK(shake.oscillators.at(1).cycles == 9);
}

TEST_CASE("Camera lens opcode returns the fall-through value for other bytes",
          "[event][camera][lens]") {
  event::EventCameraLenses lenses;
  event::EventCameraShake shake;
  CHECK(Run({0xEE}, lenses, shake) == std::optional<std::intptr_t>{0xEF});
  CHECK(Run({0x10}, lenses, shake) == std::optional<std::intptr_t>{0xEA});
  CHECK_FALSE(Run({0xEA, 1, 2}, lenses, shake).has_value());
  CHECK_FALSE(Run({0xEF, 1, 2, 3, 4}, lenses, shake).has_value());
  CHECK(lenses == event::EventCameraLenses{});
  CHECK(shake == event::EventCameraShake{});
}

// F01233 (func_800BDF6C)
TEST_CASE("Lens tween advances by half the tick speed and eases the value",
          "[event][camera][lens]") {
  FakeLensHost host;
  host.tick_speed = 4;
  host.fraction = 0x800;
  event::EventLensTween tween{.easing_mode = 3,
                              .start_value = 100,
                              .delta_value = -200,
                              .elapsed_frames = 1,
                              .duration_frames = 10};
  std::int32_t fraction = 0;
  event::UpdateEventLensTween(tween, fraction, host);
  CHECK(tween.elapsed_frames == 3);
  REQUIRE(host.ease_calls.size() == 1);
  CHECK(host.ease_calls.front().mode == 3);
  CHECK(host.ease_calls.front().frame == 3);
  CHECK(host.ease_calls.front().duration == 10);
  CHECK(fraction == 0x800);
  CHECK(tween.value == 0);
  CHECK(tween.duration_frames == 10);
  CHECK(tween.changed == 1);
}

TEST_CASE("Lens tween clamps to the duration and comes to rest",
          "[event][camera][lens]") {
  FakeLensHost host;
  host.tick_speed = 4;
  host.fraction = 0x1000;
  event::EventLensTween tween{.start_value = 0x7FFF,
                              .delta_value = 2,
                              .elapsed_frames = 0xFF,
                              .duration_frames = 0xFE};
  std::int32_t fraction = 0;
  event::UpdateEventLensTween(tween, fraction, host);
  // The byte add wraps 0xFF + 2 to 1, which is below the duration.
  CHECK(tween.elapsed_frames == 1);
  CHECK(tween.value == -0x7FFF);
  tween.elapsed_frames = 0xFD;
  event::UpdateEventLensTween(tween, fraction, host);
  CHECK(tween.elapsed_frames == 0xFE);
  CHECK(tween.duration_frames == 0);
}

TEST_CASE("Lens tween at rest changes nothing", "[event][camera][lens]") {
  FakeLensHost host;
  event::EventLensTween tween{.value = 5, .start_value = 1};
  const event::EventLensTween before = tween;
  std::int32_t fraction = 99;
  event::UpdateEventLensTween(tween, fraction, host);
  CHECK(tween == before);
  CHECK(fraction == 99);
  CHECK(host.ease_calls.empty());
}

// F01234 (func_800BE01C)
TEST_CASE("Shake oscillator computes the envelope before the wave",
          "[event][camera][lens]") {
  FakeLensHost host;
  host.sines = {0x800, 0x1000};
  event::EventShakeOscillator oscillator = ActiveOscillator();
  oscillator.envelope = event::kShakeEnvelopeFadeOut;
  oscillator.cycles = 2;
  std::int32_t envelope = 0;
  CHECK(event::UpdateEventShakeOscillator(oscillator, envelope, host));
  REQUIRE(host.sine_angles.size() == 2);
  CHECK(host.sine_angles.at(0) == (1 << 10) / 8 + 0x400);
  CHECK(host.sine_angles.at(1) == (2 * 0x1000) / 8);
  CHECK(envelope == 0x800);
  CHECK(oscillator.displacement == 0x80);
  CHECK(oscillator.elapsed_frames == 1);
}

TEST_CASE("Shake oscillator envelope shapes and stale envelope",
          "[event][camera][lens]") {
  FakeLensHost host;
  host.sines = {0x400, 0x1000, 0x1000};
  event::EventShakeOscillator swell = ActiveOscillator();
  swell.envelope = event::kShakeEnvelopeSwell;
  std::int32_t envelope = 0;
  CHECK(event::UpdateEventShakeOscillator(swell, envelope, host));
  CHECK(host.sine_angles.at(0) == (1 << 11) / 8);
  CHECK(envelope == 0x400);

  event::EventShakeOscillator unknown = ActiveOscillator();
  unknown.envelope = 9;
  envelope = 0x2000;
  CHECK(event::UpdateEventShakeOscillator(unknown, envelope, host));
  CHECK(host.sine_angles.size() == 3);
  CHECK(envelope == 0x2000);
  CHECK(unknown.displacement == 0x200);
}

TEST_CASE("Shake oscillator finishes at its duration and idles",
          "[event][camera][lens]") {
  FakeLensHost host;
  host.tick_speed = 4;
  event::EventShakeOscillator oscillator = ActiveOscillator();
  oscillator.elapsed_frames = 7;
  std::int32_t envelope = 0;
  CHECK(event::UpdateEventShakeOscillator(oscillator, envelope, host));
  CHECK(oscillator.elapsed_frames == 8);
  CHECK(oscillator.duration_frames == 0);
  CHECK(envelope == 0x1000);
  const event::EventShakeOscillator idle = oscillator;
  CHECK_FALSE(event::UpdateEventShakeOscillator(oscillator, envelope, host));
  CHECK(oscillator == idle);
}

// F01235 (func_800BE180)
TEST_CASE("Camera shake sums the oscillators per axis and track",
          "[event][camera][lens]") {
  FakeLensHost host;
  host.sines = {0x1000, 0x1000};
  event::EventCameraShake shake;
  shake.oscillators.at(0) = ActiveOscillator();
  // x factor +1, y factor -1, z factor 0, eye and look-at.
  shake.oscillators.at(0).axis_mask = (2 << 2) | (0 << 4) | (1 << 6) | 3;
  shake.oscillators.at(1) = ActiveOscillator();
  // x factor +2, eye only.
  shake.oscillators.at(1).axis_mask = (3 << 2) | (1 << 4) | (1 << 6) | 2;
  event::EventCameraScratch scratch;
  scratch.look_at_shake = {9, 9, 9};
  event::UpdateEventCameraShake(shake, scratch, host);
  CHECK(shake.active_count == 2);
  CHECK(shake.look_at_offset == Position3{0x100, -0x100, 0});
  CHECK(shake.eye_offset == Position3{0x300, -0x100, 0});
  CHECK(scratch.axis_shake == Position3{0x200, 0, 0});
  CHECK(scratch.look_at_shake == shake.look_at_offset);
}

TEST_CASE("Camera shake keeps the offsets while idle",
          "[event][camera][lens]") {
  FakeLensHost host;
  event::EventCameraShake shake;
  shake.active_count = 2;
  shake.look_at_offset = {1, 2, 3};
  shake.eye_offset = {4, 5, 6};
  event::EventCameraScratch scratch;
  scratch.look_at_shake = {9, 9, 9};
  scratch.eye_shake = {8, 8, 8};
  event::UpdateEventCameraShake(shake, scratch, host);
  CHECK(shake.active_count == 0);
  CHECK(shake.look_at_offset == Position3{1, 2, 3});
  CHECK(shake.eye_offset == Position3{4, 5, 6});
  CHECK(scratch.look_at_shake == Position3{});
  CHECK(scratch.eye_shake == Position3{});
  CHECK(host.sine_angles.empty());
}

TEST_CASE("Camera shake offset sums wrap at 16 bits", "[event][camera][lens]") {
  FakeLensHost host;
  host.sines = {0x1000, 0x1000};
  event::EventCameraShake shake;
  shake.oscillators.at(0) = ActiveOscillator();
  shake.oscillators.at(0).amplitude = 0x7000;
  shake.oscillators.at(0).axis_mask = (2 << 2) | (1 << 4) | (1 << 6) | 1;
  shake.oscillators.at(1) = shake.oscillators.at(0);
  event::EventCameraScratch scratch;
  event::UpdateEventCameraShake(shake, scratch, host);
  CHECK(shake.look_at_offset.x == -0x2000);
  CHECK(shake.eye_offset == Position3{});
}

// Live entries and the opcode wrapper over the camera workspace.
TEST_CASE("Live camera lens opcode writes the workspace",
          "[event][camera][lens]") {
  const LiveLensFixture fixture;
  Event_camera->near_clip.value = 0x20;
  std::array<std::uint8_t, 4> lens{0xEB, 0x60, 2, 6};
  CHECK(VS::Event::CommandCameraLensOrShakeOpcode(lens.data(), 0) == 0);
  CHECK(Event_camera->near_clip.start_value == 0x20);
  CHECK(Event_camera->near_clip.delta_value == 0x40);
  CHECK(Event_camera->near_clip.duration_frames == 6);
}

TEST_CASE("Live camera shake opcode fills the first idle oscillator",
          "[event][camera][lens]") {
  const LiveLensFixture fixture;
  std::array<std::uint8_t, 6> start{0xEF, 2, 0x40, 0x07, 1, 30};
  CHECK(VS::Event::CommandCameraLensOrShakeOpcode(start.data(), 0) == 0);
  const VS::Event::ShakeOscillator& first =
      Event_camera->shake_oscillators.at(0);
  CHECK(first.cycles == 2);
  CHECK(first.amplitude == 0x40);
  CHECK(first.axis_mask == 0x07);
  CHECK(first.envelope_shape == 1);
  CHECK(first.duration_frames == 30);
  CHECK(Event_camera->shake_oscillators.at(1).duration_frames == 0);
  CHECK(Event_camera->shake_active_count == 0);
}

TEST_CASE("Live camera shake entry updates the workspace and scratch",
          "[event][camera][lens]") {
  const LiveLensFixture fixture;
  VS::State::tickSpeed = 2;
  std::array<std::uint8_t, 6> start{0xEF, 0, 0x40, (2 << 2) | 1, 0, 30};
  REQUIRE(VS::Event::CommandCameraLensOrShakeOpcode(start.data(), 0) == 0);
  SetScratchWord(0x88, 5);
  VS::Event::UpdateEventCameraShakeEntry();
  CHECK(Event_camera->shake_active_count == 1);
  // Zero cycles keep the wave at sin(0).
  CHECK(Event_camera->shake_look_at_offset == Position3{});
  CHECK(ScratchWord(0x88) == 0x1000);
  CHECK(Event_camera->shake_oscillators.at(0).elapsed_frames == 1);
}

// func_800BE180 adds axis bit 0 into 0x1BE, which applyCameraState adds to the
// look-at, and bit 1 into 0x1C6, which it adds to the eye.
TEST_CASE("Live camera shake routes each axis bit to its workspace offset",
          "[event][camera][lens]") {
  const LiveLensFixture fixture;
  VS::State::tickSpeed = 2;
  // A quarter cycle on the first tick: sin(0x400) is 1.0. Each axis field holds
  // its direction plus 1.
  std::array<std::uint8_t, 6> look_at{
      0xEF, 1, 0x40, (2 << 2) | (1 << 4) | (1 << 6) | 1, 0, 4};
  std::array<std::uint8_t, 6> eye{
      0xEF, 1, 0x20, (1 << 2) | (2 << 4) | (1 << 6) | 2, 0, 4};
  REQUIRE(VS::Event::CommandCameraLensOrShakeOpcode(look_at.data(), 0) == 0);
  REQUIRE(VS::Event::CommandCameraLensOrShakeOpcode(eye.data(), 0) == 0);
  VS::Event::UpdateEventCameraShakeEntry();
  CHECK(Event_camera->shake_active_count == 2);
  CHECK(Event_camera->shake_look_at_offset == Position3{.x = 0x40});
  CHECK(Event_camera->shake_eye_offset == Position3{.y = 0x20});
  CHECK(ScratchPosition(0xD8) == Position3{.x = 0x40});
  CHECK(ScratchPosition(0xE0) == Position3{.y = 0x20});
}

TEST_CASE("Live lens tween entry stores the value and the fraction",
          "[event][camera][lens]") {
  const LiveLensFixture fixture;
  VS::State::tickSpeed = 4;
  VS::Event::LensTween& tween = Event_camera->far_clip;
  tween.start_value = 0x100;
  tween.delta_value = 0x100;
  tween.elapsed_frames = 8;
  tween.duration_frames = 10;
  SetScratchWord(0x88, -1);
  VS::Event::UpdateEventLensTweenEntry(&tween);
  CHECK(tween.elapsed_frames == 10);
  CHECK(tween.duration_frames == 0);
  CHECK(tween.has_changed == 1);
  CHECK(ScratchWord(0x88) == 0x1000);
  CHECK(tween.value == 0x200);
}
