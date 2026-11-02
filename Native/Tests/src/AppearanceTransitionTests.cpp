// Hand-maintained (tools/port/owned.csv).
#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Actors/AppearanceTransition.hpp"

namespace {
using VS::Actors::AppearanceTransition;
using VS::Actors::PalettePulse;
using VS::Actors::PaletteTransition;

class RecordingActor final : public VS::Actors::ActorAppearance {
 public:
  std::vector<std::string> calls;
  std::deque<bool> changes;
  AppearanceTransition transition;
  int movement_mode = 0;
  bool player = false;
  int settled_state = -1;

  bool ApplyPalettes(PaletteTransition mode, int speed) override {
    calls.push_back("apply " + std::to_string(static_cast<int>(mode)) + " " +
                    std::to_string(speed));
    if (changes.empty()) {
      return false;
    }
    const bool changed = changes.front();
    changes.pop_front();
    return changed;
  }
  void UploadPalettes(int state) override {
    calls.push_back("upload " + std::to_string(state));
  }
  void PublishState(int state) override {
    calls.push_back("publish " + std::to_string(state));
  }
  void ReleaseCarriedGimmick() override { calls.emplace_back("release"); }
  [[nodiscard]] int TransitionState() const override {
    return transition.state;
  }
  [[nodiscard]] AppearanceTransition LoadTransition() const override {
    return transition;
  }
  void StoreTransition(const AppearanceTransition& stored) override {
    transition = stored;
  }
  [[nodiscard]] int MovementMode() const override { return movement_mode; }
  [[nodiscard]] bool IsPlayer() const override { return player; }
  void SettleDeathMotion() override {
    calls.emplace_back("settle");
    if (settled_state >= 0) {
      transition.state = settled_state;
    }
  }
  void PlayFallingDeathAnimation() override { calls.emplace_back("fall"); }
};

std::vector<std::string> Calls(std::initializer_list<const char*> calls) {
  return {calls.begin(), calls.end()};
}

AppearanceTransition Transition(int state, int speed) {
  AppearanceTransition transition;
  transition.state = state;
  transition.speed = speed;
  return transition;
}

void Advance(RecordingActor& actor) {
  VS::Actors::AdvanceAppearanceTransition(actor.transition, actor);
}
}  // namespace

TEST_CASE("Palette pulse rises to the peak and falls back to zero",
          "[actors][appearance][palette-pulse]") {
  PalettePulse pulse{.level = 14, .flags = 1};
  CHECK(VS::Actors::AdvancePalettePulse(pulse, 2) == 16);
  CHECK(pulse.flags == 3);
  CHECK(VS::Actors::AdvancePalettePulse(pulse, 2) == 14);
  pulse = {.level = 2, .flags = 3};
  CHECK(VS::Actors::AdvancePalettePulse(pulse, 2) == 0);
  CHECK(pulse.flags == 1);
}

TEST_CASE("Palette pulse steps twice at double tick speed",
          "[actors][appearance][palette-pulse]") {
  PalettePulse pulse{.level = 14, .flags = 0};
  CHECK(VS::Actors::AdvancePalettePulse(pulse, 4) == 14);
  CHECK(pulse.flags == 2);
  CHECK(pulse.level == 14);
}

TEST_CASE("Palette pulse returns the untruncated level past zero",
          "[actors][appearance][palette-pulse]") {
  PalettePulse pulse{.level = 1, .flags = 2};
  CHECK(VS::Actors::AdvancePalettePulse(pulse, 2) == -1);
  CHECK(pulse.level == 255);
  CHECK(pulse.flags == 0);
}

TEST_CASE("Pulsed colors shift toward blue and clamp each channel",
          "[actors][appearance][palette-pulse]") {
  CHECK(VS::Actors::PulsePaletteColor(0x8000, 16) ==
        (0x8000 | 8 | (18 << 5) | (30 << 10)));
  CHECK(VS::Actors::PulsePaletteColor(0x7FFF, 16) == 0x7FFF);
  CHECK(VS::Actors::PulsePaletteColor(0x0003, 6) == ((3 << 5) | (6 << 10)));
  CHECK(VS::Actors::PulsePaletteColor(0x0003, -1) == 0x0003);
}

TEST_CASE(
    "Palette pulse skips transparent colors and stops at the shorter span",
    "[actors][appearance][palette-pulse]") {
  const std::array<std::uint16_t, 3> source{0, 0x0010, 0x0020};
  std::array<std::uint16_t, 2> destination{0x1111, 0x2222};
  PalettePulse pulse{.level = 0, .flags = 0};
  CHECK(VS::Actors::UpdatePalettePulse(pulse, 2, source, destination) == 2);
  CHECK(destination[0] == 0x1111);
  CHECK(destination[1] == VS::Actors::PulsePaletteColor(0x0010, 2));
}

TEST_CASE("RGB steps clamp at the target and wrap as bytes",
          "[actors][appearance]") {
  std::array<std::uint8_t, 3> rgb{100, 5, 50};
  const std::array<std::uint8_t, 3> tint{90, 3, 50};
  CHECK(VS::Actors::StepRgbTowardTarget(rgb, tint, -8));
  CHECK(rgb == std::array<std::uint8_t, 3>{92, 253, 50});
  CHECK(VS::Actors::StepRgbTowardTarget(rgb, tint, -8));
  CHECK(rgb == std::array<std::uint8_t, 3>{90, 245, 50});
  rgb = {10, 20, 30};
  CHECK_FALSE(VS::Actors::StepRgbTowardTarget(
      rgb, std::array<std::uint8_t, 3>{10, 20, 30}, 8));
  CHECK(VS::Actors::StepRgbTowardTarget(
      rgb, std::array<std::uint8_t, 3>{12, 20, 30}, 8));
  CHECK(rgb[0] == 12);
}

TEST_CASE("Palette states upload and advance once their palettes settle",
          "[actors][appearance]") {
  const auto [state, mode] =
      GENERATE(std::pair{2, 0}, std::pair{7, 1}, std::pair{9, 2},
               std::pair{12, 5}, std::pair{14, 6});
  const bool changing = GENERATE(false, true);
  RecordingActor actor;
  actor.transition = Transition(state, 1);
  actor.changes = {changing};
  Advance(actor);
  std::vector<std::string> expected{"apply " + std::to_string(mode) + " 1",
                                    "upload " + std::to_string(state)};
  if (!changing) {
    expected.push_back("publish " + std::to_string(state + 1));
  }
  CHECK(actor.calls == expected);
  CHECK(actor.transition.state == (changing ? state : state + 1));
}

TEST_CASE("Palette states set their color and fade before applying",
          "[actors][appearance]") {
  RecordingActor actor;
  actor.transition = Transition(9, 1);
  Advance(actor);
  CHECK(actor.transition.colors.rgb ==
        std::array<std::uint8_t, 3>{255, 255, 255});
  actor.transition = Transition(14, 1);
  Advance(actor);
  CHECK(actor.transition.colors.fade_level == 64);
  actor.transition = Transition(7, 3);
  actor.transition.colors.fade_level = 60;
  Advance(actor);
  CHECK(actor.transition.colors.fade_level == 64);
}

TEST_CASE("Death darkening ends white with the delay counter cleared",
          "[actors][appearance]") {
  RecordingActor actor;
  actor.transition = Transition(3, 1);
  actor.transition.colors.delay = 9;
  actor.changes = {false, true};
  Advance(actor);
  CHECK(actor.calls ==
        Calls({"apply 1 1", "apply 2 1", "upload 3", "publish 4"}));
  CHECK(actor.transition.colors.rgb == std::array<std::uint8_t, 3>{64, 64, 64});
  CHECK(actor.transition.colors.delay == 0);
}

TEST_CASE("Disappearance drops carried gimmicks and fades to black",
          "[actors][appearance]") {
  RecordingActor actor;
  actor.transition = Transition(4, 2);
  actor.transition.colors.rgb = {12, 40, 40};
  actor.transition.colors.fade_level = 58;
  Advance(actor);
  CHECK(actor.calls == Calls({"release", "upload 4"}));
  CHECK(actor.transition.colors.rgb == std::array<std::uint8_t, 3>{4, 4, 4});
  CHECK(actor.transition.colors.fade_level == 62);
  actor.calls.clear();
  Advance(actor);
  CHECK(actor.calls == Calls({"release", "upload 4", "publish 5"}));
  CHECK(actor.transition.colors.rgb == std::array<std::uint8_t, 3>{0, 0, 0});
  CHECK(actor.transition.colors.fade_level == 64);
}

TEST_CASE("Disappearance raises a negative fade level as a signed byte",
          "[actors][appearance]") {
  RecordingActor actor;
  actor.transition = Transition(4, 1);
  actor.transition.colors.fade_level = 0xFE;
  Advance(actor);
  CHECK(actor.transition.colors.fade_level == 0);
}

TEST_CASE("Appearance delay holds for fifteen frames", "[actors][appearance]") {
  RecordingActor actor;
  actor.transition = Transition(6, 1);
  actor.transition.colors.delay = 13;
  Advance(actor);
  CHECK(actor.calls == Calls({"apply 3 1", "upload 6"}));
  CHECK(actor.transition.colors.delay == 14);
  actor.calls.clear();
  Advance(actor);
  CHECK(actor.calls == Calls({"apply 3 1", "upload 6", "publish 7"}));
  CHECK(actor.transition.colors.fade_level == 4);
}

TEST_CASE("Restore returns the fade and color to the saved tint",
          "[actors][appearance]") {
  RecordingActor actor;
  actor.transition = Transition(10, 1);
  actor.transition.colors = {.rgb = {255, 255, 255},
                             .delay = 0,
                             .fade_level = 64,
                             .fade_target = 60,
                             .tint = {128, 250, 255}};
  Advance(actor);
  CHECK(actor.calls == Calls({"apply 4 1", "upload 10"}));
  CHECK(actor.transition.colors.fade_level == 62);
  CHECK(actor.transition.colors.rgb ==
        std::array<std::uint8_t, 3>{247, 250, 255});
  actor.transition.colors.fade_level = 60;
  actor.transition.colors.rgb = actor.transition.colors.tint;
  actor.calls.clear();
  Advance(actor);
  CHECK(actor.calls == Calls({"apply 4 1", "upload 10", "publish 11"}));
  Advance(actor);
  CHECK(actor.transition.state == 0);
}

TEST_CASE("Unlisted transition states change nothing", "[actors][appearance]") {
  const int state = GENERATE(0, 1, 5, 8, 13, 15);
  RecordingActor actor;
  actor.transition = Transition(state, 1);
  Advance(actor);
  CHECK(actor.calls.empty());
  CHECK(actor.transition.state == state);
}

TEST_CASE("Dying actors settle their motion before the delay state",
          "[actors][appearance]") {
  const int state = GENERATE(2, 6);
  const int mode = GENERATE(0, 1, 4);
  const bool player = GENERATE(false, true);
  RecordingActor actor;
  actor.transition = Transition(state, 1);
  actor.movement_mode = mode;
  actor.player = player;
  VS::Actors::UpdateActorAppearanceTransition(actor);
  std::vector<std::string> expected;
  if (state < 6 && mode != 0) {
    expected.emplace_back("settle");
    if (mode >= 4 && !player) {
      expected.emplace_back("fall");
    }
  }
  expected.push_back(state == 2 ? "apply 0 1" : "apply 3 1");
  REQUIRE(actor.calls.size() >= expected.size());
  CHECK(std::vector(actor.calls.begin(),
                    actor.calls.begin() + static_cast<std::ptrdiff_t>(
                                              expected.size())) == expected);
}

TEST_CASE("Actor transitions load their state after the death motion",
          "[actors][appearance]") {
  RecordingActor actor;
  actor.transition = Transition(2, 1);
  actor.movement_mode = 1;
  actor.settled_state = 11;
  VS::Actors::UpdateActorAppearanceTransition(actor);
  CHECK(actor.calls == Calls({"settle"}));
  CHECK(actor.transition.state == 0);
}
