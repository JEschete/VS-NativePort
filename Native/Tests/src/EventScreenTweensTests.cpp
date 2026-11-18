#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Core/Scratch.h"
#include "Effects/ScreenDistortion/RadialWarp.hpp"
#include "Event/Camera.hpp"
#include "Event/EventScreenTweens.hpp"
#include "Event/EventScreenTweensLive.hpp"
#include "Event/InterpolationCurveLive.hpp"
#include "Event/ScriptCamera.h"
#include "Event/ScriptCommand.hpp"
#include "Event/ScriptControlOpcodes.h"
#include "State/Timers.h"

namespace {
namespace ev = VS::Event;

// Records every port call in order as text; the reads return set values.
class RecordingScreen final : public ev::EventScreenEffectPort {
 public:
  void SetCaptureEnabled(std::int32_t enabled) override {
    calls.push_back("enabled " + std::to_string(enabled));
  }
  void SetDepth(std::int32_t depth) override {
    calls.push_back("depth " + std::to_string(depth));
  }
  void SetPreviousFrame(std::int32_t use_previous_frame) override {
    calls.push_back("previous " + std::to_string(use_previous_frame));
  }
  void SetScale(ev::EventScreenPair scale) override {
    calls.push_back("scale " + Pair(scale));
  }
  void SetTint(ev::EventScreenTint tint) override {
    calls.push_back("tint " + std::to_string(tint.red) + " " +
                    std::to_string(tint.green) + " " +
                    std::to_string(tint.blue));
  }
  void SetOffset(ev::EventScreenPair offset) override {
    calls.push_back("offset " + Pair(offset));
  }
  void SetBlendMode(std::int32_t blend_mode) override {
    calls.push_back("blend " + std::to_string(blend_mode));
  }
  ev::EventScreenPair ReadScale() override {
    calls.emplace_back("read scale");
    return scale;
  }
  ev::EventScreenTint ReadTint() override {
    calls.emplace_back("read tint");
    return tint;
  }
  ev::EventScreenPair ReadOffset() override {
    calls.emplace_back("read offset");
    return offset;
  }
  void SetRadialWarp(
      VS::Effects::ScreenDistortion::RadialWarpSettings settings) override {
    calls.push_back("warp " + std::to_string(settings.scale) + " " +
                    std::to_string(settings.growth));
  }

  std::vector<std::string> calls;
  ev::EventScreenPair scale{};
  ev::EventScreenTint tint{};
  ev::EventScreenPair offset{};

 private:
  static std::string Pair(ev::EventScreenPair pair) {
    return std::to_string(pair.x) + " " + std::to_string(pair.y);
  }
};

// Returns `fraction` and records each request and recorded fraction.
class FixedEasing final : public ev::EventTweenEasing {
 public:
  explicit FixedEasing(std::int32_t value) : fraction(value) {}

  std::int32_t EaseFraction(std::uint8_t mode, std::int16_t frame,
                            std::int16_t duration) override {
    calls.push_back("ease " + std::to_string(mode) + " " +
                    std::to_string(frame) + " " + std::to_string(duration));
    return fraction;
  }
  void RecordEasedFraction(std::int32_t value) override {
    calls.push_back("record " + std::to_string(value));
  }

  std::int32_t fraction;
  std::vector<std::string> calls;
};

std::optional<std::int32_t> Transform(std::vector<std::uint8_t> bytes,
                                      ev::EventScreenTweens& tweens,
                                      RecordingScreen& screen) {
  return ev::CommandScreenTransform(ev::ScriptCommand{bytes}, tweens, screen);
}

ev::EventAngleTween RolledFrom(std::int16_t angle, std::uint8_t control,
                               std::int16_t target) {
  ev::EventAngleTween camera{.angle = angle};
  ev::EventAngleTween screen{};
  const auto low = static_cast<std::uint8_t>(target & 0xFF);
  const auto high = static_cast<std::uint8_t>((target >> 8) & 0xFF);
  const std::vector<std::uint8_t> bytes{0xE2, low, high, control, 8};
  REQUIRE(ev::CommandTweenCameraRoll(ev::ScriptCommand{bytes}, camera, screen));
  CHECK(screen == ev::EventAngleTween{});
  return camera;
}
}  // namespace

TEST_CASE("Screen capture opcode forwards its enable byte",
          "[event][camera][screen-tween]") {
  RecordingScreen screen;
  const std::vector<std::uint8_t> bytes{0xE1, 0xFE};
  CHECK(ev::CommandSetScreenCapture(ev::ScriptCommand{bytes}, screen));
  CHECK(screen.calls == std::vector<std::string>{"enabled 254"});
  const std::vector<std::uint8_t> short_bytes{0xE1};
  CHECK_FALSE(
      ev::CommandSetScreenCapture(ev::ScriptCommand{short_bytes}, screen));
  CHECK(screen.calls.size() == 1);
}

TEST_CASE("Roll opcode sets an immediate angle on the selected roll",
          "[event][camera][screen-tween]") {
  ev::EventAngleTween camera{.easing = 3, .angle = 5, .start = 9};
  ev::EventAngleTween screen{};
  const std::vector<std::uint8_t> bytes{0xE3, 0x34, 0xF2, 0x51, 0};
  REQUIRE(ev::CommandTweenCameraRoll(ev::ScriptCommand{bytes}, camera, screen));
  CHECK(camera == ev::EventAngleTween{.easing = 3, .angle = 5, .start = 9});
  CHECK(screen == ev::EventAngleTween{
                      .changed = 1,
                      .angle = static_cast<std::int16_t>(0xF234 - 0x10000)});
}

TEST_CASE("Roll opcode takes the shorter way unless the direction forces one",
          "[event][camera][screen-tween]") {
  // 0x100 from 0xF00 is -0xE00, or 0x200 the shorter way round.
  const ev::EventAngleTween shorter = RolledFrom(0xF00, 0x15, 0x100);
  CHECK(shorter == ev::EventAngleTween{.easing = 5,
                                       .angle = 0xF00,
                                       .start = 0xF00,
                                       .delta = 0x200,
                                       .duration = 8});
  CHECK(RolledFrom(0xF00, 0x05, 0x100).delta == -0xE00);
  CHECK(RolledFrom(0x100, 0x2A, 0xF00).delta == 0xE00);
  CHECK(RolledFrom(0x100, 0x1A, 0xF00).delta == -0x200);
  // Exactly half a turn is kept as it is.
  CHECK(RolledFrom(0, 0x10, 0x800).delta == 0x800);
  CHECK(RolledFrom(0x800, 0x10, 0).delta == -0x800);
}

TEST_CASE("Roll opcode refuses another opcode or a short command",
          "[event][camera][screen-tween]") {
  ev::EventAngleTween camera{.angle = 7};
  ev::EventAngleTween screen{.angle = 9};
  const std::vector<std::uint8_t> other{0xE4, 1, 2, 3, 0};
  CHECK_FALSE(
      ev::CommandTweenCameraRoll(ev::ScriptCommand{other}, camera, screen));
  const std::vector<std::uint8_t> short_bytes{0xE2, 1, 2, 3};
  CHECK_FALSE(ev::CommandTweenCameraRoll(ev::ScriptCommand{short_bytes}, camera,
                                         screen));
  CHECK(camera == ev::EventAngleTween{.angle = 7});
  CHECK(screen == ev::EventAngleTween{.angle = 9});
}

TEST_CASE("Angle tween advances by half the tick speed and stops at the end",
          "[event][camera][screen-tween]") {
  FixedEasing easing(0x800);
  ev::EventAngleTween tween{.easing = 0xF2,
                            .angle = 1,
                            .start = 0x100,
                            .delta = -0x300,
                            .elapsed = 6,
                            .duration = 9};
  ev::UpdateEventAngleTween(tween, 5, easing);
  CHECK(tween == ev::EventAngleTween{.changed = 1,
                                     .easing = 0xF2,
                                     .angle = -0x80,
                                     .start = 0x100,
                                     .delta = -0x300,
                                     .elapsed = 8,
                                     .duration = 9});
  CHECK(easing.calls == std::vector<std::string>{"ease 242 8 9"});
  ev::UpdateEventAngleTween(tween, 4, easing);
  CHECK(tween.elapsed == 10);
  CHECK(tween.duration == 0);
  ev::EventAngleTween idle{.angle = 3, .duration = -1};
  ev::UpdateEventAngleTween(idle, 4, easing);
  CHECK(idle == ev::EventAngleTween{.angle = 3, .duration = -1});
  CHECK(easing.calls.size() == 2);
}

TEST_CASE("Screen transform starts scale tint and offset tweens",
          "[event][camera][screen-tween]") {
  RecordingScreen screen;
  screen.scale = {.x = 0x12345, .y = 0x100};
  screen.tint = {.red = 0x80, .green = 0x10, .blue = 0xFF};
  screen.offset = {.x = -3, .y = 4};
  ev::EventScreenTweens tweens{};
  CHECK(Transform({0xE4, 0x20, 0x01, 0x07, 9}, tweens, screen) == 0);
  CHECK(tweens.scale == ev::EventScreenTween{.easing = 7,
                                             .start = {0x2345, 0x100, 0},
                                             .delta = {-0x1345, -0x80, 0},
                                             .duration = 9});
  CHECK(Transform({0xE5, 0x00, 0x90, 0x7F, 0x83, 4}, tweens, screen) == 0);
  CHECK(tweens.tint == ev::EventScreenTween{.easing = 0x83,
                                            .start = {0x80, 0x10, 0xFF},
                                            .delta = {-0x80, 0x80, -0x80},
                                            .duration = 4});
  CHECK(Transform({0xE6, 0xFE, 0x05, 0x02, 2}, tweens, screen) == 0);
  CHECK(tweens.translation == ev::EventScreenTween{.easing = 2,
                                                   .start = {-3, 4, 0},
                                                   .delta = {1, 1, 0},
                                                   .duration = 2});
  CHECK(screen.calls ==
        std::vector<std::string>{"read scale", "read tint", "read offset"});
}

TEST_CASE("Screen transform applies immediate scale tint and offset",
          "[event][camera][screen-tween]") {
  RecordingScreen screen;
  ev::EventScreenTweens tweens{};
  tweens.scale.elapsed = 5;
  CHECK(Transform({0xE4, 0xFF, 0x20, 0x07, 0}, tweens, screen) == 0);
  CHECK(Transform({0xE5, 0x01, 0x02, 0x03, 0x04, 0}, tweens, screen) == 0);
  CHECK(Transform({0xE6, 0x80, 0x7F, 0x02, 0}, tweens, screen) == 0);
  CHECK(tweens == ev::EventScreenTweens{.scale = {.elapsed = 5}});
  CHECK(screen.calls == std::vector<std::string>{"scale 32640 4096",
                                                 "tint 1 2 3",
                                                 "offset -128 127"});
}

TEST_CASE("Screen transform radial warp waits and tweens from its current warp",
          "[event][camera][screen-tween]") {
  RecordingScreen screen;
  ev::EventScreenTweens tweens{};
  CHECK(Transform({0xED, 0xFF, 0xF0, 0x03, 0}, tweens, screen) == 0);
  CHECK(tweens.radial_warp.current[0] == 0x3FC0);
  CHECK(tweens.radial_warp.current[1] == -0x10);
  CHECK(Transform({0xED, 0x01, 0x10, 0x04, 6}, tweens, screen) == 0);
  CHECK(tweens.radial_warp ==
        ev::EventScreenTween{.easing = 4,
                             .current = {0x3FC0, -0x10, 0},
                             .start = {0x3FC0, -0x10, 0},
                             .delta = {0x40 - 0x3FC0, 0x20, 0},
                             .duration = 6});
  const ev::EventScreenTweens running = tweens;
  CHECK(Transform({0xED, 0x02, 0x00, 0x00, 0}, tweens, screen) == 1);
  CHECK(tweens == running);
  CHECK(screen.calls == std::vector<std::string>{"warp 16320 -16"});
}

TEST_CASE("Screen transform restores and captures the screen in order",
          "[event][camera][screen-tween]") {
  RecordingScreen screen;
  ev::EventScreenTweens tweens{};
  CHECK(Transform({0xBF, 0, 0}, tweens, screen) == 0);
  CHECK(screen.calls == std::vector<std::string>{"blend 0", "offset 0 0",
                                                 "scale 4096 4096",
                                                 "tint 128 128 128", "depth 0",
                                                 "previous 0", "enabled 0"});
  screen.calls.clear();
  CHECK(Transform({0xBF, 0xFE, 0xFF}, tweens, screen) == 0);
  CHECK(screen.calls ==
        std::vector<std::string>{"enabled 1", "blend 0", "offset 0 2",
                                 "scale 4096 4096", "previous 1",
                                 "tint 144 144 144", "depth -2"});
}

TEST_CASE("Screen transform forwards depth previous frame and blend mode",
          "[event][camera][screen-tween]") {
  RecordingScreen screen;
  ev::EventScreenTweens tweens{};
  CHECK(Transform({0xB9, 0x00, 0x80}, tweens, screen) == 0);
  CHECK(Transform({0xBA, 0xFF}, tweens, screen) == 0);
  CHECK(Transform({0xE7, 0x02}, tweens, screen) == 0);
  CHECK(screen.calls ==
        std::vector<std::string>{"depth -32768", "previous 255", "blend 2"});
}

TEST_CASE("Screen transform refuses another opcode or a short command",
          "[event][camera][screen-tween]") {
  RecordingScreen screen;
  ev::EventScreenTweens tweens{};
  CHECK_FALSE(Transform({0xBB, 1, 2, 3, 4, 5}, tweens, screen).has_value());
  CHECK_FALSE(Transform({0xE5, 1, 2, 3, 4}, tweens, screen).has_value());
  CHECK_FALSE(Transform({}, tweens, screen).has_value());
  CHECK_FALSE(ev::ScreenTransformLength(0xE8).has_value());
  CHECK(tweens == ev::EventScreenTweens{});
  CHECK(screen.calls.empty());
}

TEST_CASE("Screen tween advances all three values and records the fraction",
          "[event][camera][screen-tween]") {
  FixedEasing easing(0x400);
  ev::EventScreenTween tween{.easing = 0x81,
                             .current = {9, 9, 9},
                             .start = {0x10, -0x7FFF, 0},
                             .delta = {0x40, -0x8000, 0x7FFF},
                             .elapsed = 2,
                             .duration = 6};
  const std::optional<std::int32_t> eased =
      ev::UpdateEventTripleTween(tween, 9, easing);
  REQUIRE(eased.has_value());
  CHECK(eased.value_or(0) == 0x400);
  CHECK(tween.current == std::array<std::int16_t, 3>{0x20, 0x6001, 0x1FFF});
  CHECK(tween.elapsed == 6);
  CHECK(tween.duration == 0);
  CHECK(easing.calls ==
        std::vector<std::string>{"ease 129 6 6", "record 1024"});
  CHECK_FALSE(ev::UpdateEventTripleTween(tween, 9, easing).has_value());
  CHECK(easing.calls.size() == 2);
}

TEST_CASE("Screen tween keeps running when it steps past its duration",
          "[event][camera][screen-tween]") {
  FixedEasing easing(0x1000);
  ev::EventScreenTween tween{.elapsed = 6, .duration = 7};
  REQUIRE(ev::UpdateEventTripleTween(tween, 4, easing).has_value());
  CHECK(tween.elapsed == 8);
  CHECK(tween.duration == 7);
  tween.elapsed = 0x7FFF;
  REQUIRE(ev::UpdateEventTripleTween(tween, 2, easing).has_value());
  CHECK(tween.elapsed == -0x8000);
}

TEST_CASE("Screen tweens apply each running tween in order",
          "[event][camera][screen-tween]") {
  FixedEasing easing(0x1000);
  RecordingScreen screen;
  ev::EventScreenTweens tweens{
      .scale = {.start = {0x1000, -2, 0}, .duration = 3},
      .tint = {.start = {0x1FF, 0x80, -1}, .duration = 3},
      .translation = {},
      .radial_warp = {.start = {-0x40, 5, 0}, .duration = 3}};
  ev::UpdateEventScreenTransforms(tweens, 2, easing, screen);
  CHECK(screen.calls == std::vector<std::string>{"scale 4096 -2",
                                                 "tint 255 128 255",
                                                 "warp 65472 5"});
  CHECK(easing.calls.size() == 6);
  CHECK(tweens.translation == ev::EventScreenTween{});
}

TEST_CASE("Live screen tween entries skip a missing camera or tween",
          "[event][camera][screen-tween]") {
  VS::Event::CameraState* const saved = Event_camera;
  Event_camera = nullptr;
  const std::uint64_t before = ev::RefusedEventScreenTweenCount();
  std::array<std::uint8_t, 5> instruction{0xE4, 1, 2, 3, 0};
  CHECK(Event_CommandScreenTransformOpcode(instruction.data(), 0) == 0);
  instruction[0] = 0xE2;
  CHECK(Event_CommandTweenCameraRollOpcode(instruction.data(), 0) == 0);
  ev::UpdateEventScreenTransformsEntry();
  ev::UpdateEventAngleTweenEntry(nullptr);
  CHECK(ev::UpdateEventTripleTweenEntry(nullptr) == 0);
  CHECK(ev::RefusedEventScreenTweenCount() == before + 5);
  Event_camera = saved;
}

TEST_CASE("Live screen tween entries keep the workspace layout",
          "[event][camera][screen-tween]") {
  VS::Event::CameraState* const saved = Event_camera;
  const int saved_tick = State_tickSpeed;
  VS::Event::CameraState camera{};
  Event_camera = &camera;
  camera.captured_screen_roll.angle = 0x700;
  std::array<std::uint8_t, 5> roll{0xE3, 0x00, 0x01, 0x2F, 4};
  CHECK(Event_CommandTweenCameraRollOpcode(roll.data(), 0) == 0);
  CHECK(camera.captured_screen_roll.delta_angle == -0x600 + 0x1000);
  CHECK(camera.captured_screen_roll.easing_mode == 0xF);
  State_tickSpeed = 4;
  camera.screen_tint.reserved08 = 0x1234;
  camera.screen_tint.reserved1a = 0x5678;
  camera.screen_tint.start_value0 = 0x40;
  camera.screen_tint.duration_frames = 2;
  CHECK(ev::UpdateEventTripleTweenEntry(&camera.screen_tint) == 1);
  CHECK(camera.screen_tint.reserved08 == 0x1234);
  CHECK(camera.screen_tint.reserved1a == 0x5678);
  CHECK(camera.screen_tint.duration_frames == 0);
  std::int32_t recorded = 0;
  std::memcpy(&recorded, static_cast<std::byte*>(Core_ScratchBase()) + 0x88,
              sizeof(recorded));
  CHECK(recorded == VS::Event::EvaluateInterpolationCurveEntry(0, 2, 2));
  State_tickSpeed = saved_tick;
  Event_camera = saved;
}
