#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Event/WeaponMode.hpp"

namespace {
namespace event = VS::Event;

class RecordingHost final : public event::WeaponModeHost {
 public:
  bool pending = false;
  bool skip = false;
  bool gate = true;
  bool held = false;
  std::vector<int> modes;
  std::vector<int> animations;
  std::vector<int> sounds;
  std::vector<bool> snaps;

  [[nodiscard]] bool TransitionPending() const override { return pending; }
  void SetTransitionPending(bool value) override { pending = value; }
  [[nodiscard]] bool SkipActive() const override { return skip; }
  [[nodiscard]] bool WeaponModeGateOpen() const override { return gate; }
  void SetBattleMode(int mode) override { modes.push_back(mode); }
  void PlayAnimation(int animation) override {
    animations.push_back(animation);
  }
  void PlayDrawSound(int sheathe) override { sounds.push_back(sheathe); }
  [[nodiscard]] bool AnimationHeld() const override { return held; }
  void SnapVisibility(bool drawn) override { snaps.push_back(drawn); }
};

constexpr event::WeaponModeCommand Command(int mode, int transition) {
  return {.mode = static_cast<std::uint8_t>(mode),
          .transition = static_cast<std::uint8_t>(transition)};
}
}  // namespace

TEST_CASE("Typed weapon mode advances without effects behind its story gate",
          "[event][weapon-mode][typed]") {
  RecordingHost host;
  host.gate = false;
  CHECK(event::ScriptSetAshleyBattleMode(Command(1, 1), host) ==
        event::ScriptStep::kAdvance);
  CHECK(host.modes.empty());
  CHECK(host.snaps.empty());
  CHECK_FALSE(host.pending);
}

TEST_CASE("Typed weapon mode snaps visibility when skipped or not animated",
          "[event][weapon-mode][typed]") {
  const int mode = GENERATE(0, 1);
  const bool skip = GENERATE(false, true);
  RecordingHost host;
  host.skip = skip;
  CHECK(event::ScriptSetAshleyBattleMode(Command(mode, skip ? 3 : 0), host) ==
        event::ScriptStep::kAdvance);
  CHECK(host.modes == std::vector<int>{mode});
  CHECK(host.animations.empty());
  CHECK(host.snaps == std::vector<bool>{mode != 0});
}

TEST_CASE("Typed weapon mode starts its transition and holds",
          "[event][weapon-mode][typed]") {
  const int mode = GENERATE(0, 1);
  const int variant = GENERATE(1, 2, 3);
  RecordingHost host;
  const auto command = Command(mode, variant);
  REQUIRE(event::ScriptSetAshleyBattleMode(command, host) ==
          event::ScriptStep::kHold);
  const int sheathe = mode ^ 1;
  CHECK(host.animations == std::vector<int>{((variant - 1) * 2) + sheathe +
                                            event::kTransitionAnimationBase});
  CHECK(host.sounds == std::vector<int>{sheathe});
  CHECK(host.pending);
  CHECK(event::ScriptSetAshleyBattleMode(command, host) ==
        event::ScriptStep::kHold);
  CHECK(host.animations.size() == 1);
}

TEST_CASE("Typed weapon mode idles once its transition animation ends",
          "[event][weapon-mode][typed]") {
  const int mode = GENERATE(0, 1);
  const int variant = GENERATE(1, 2, 3);
  RecordingHost host;
  const auto command = Command(mode, variant);
  REQUIRE(event::ScriptSetAshleyBattleMode(command, host) ==
          event::ScriptStep::kHold);
  host.held = true;
  CHECK(event::ScriptSetAshleyBattleMode(command, host) ==
        event::ScriptStep::kAdvance);
  CHECK(host.animations.back() == (mode != 0 ? event::kDrawnIdleAnimation
                                             : event::kSheathedIdleAnimation));
  CHECK_FALSE(host.pending);
  CHECK(host.modes == std::vector<int>{mode});
  CHECK(host.snaps.empty());
}

TEST_CASE("Typed weapon mode skip clears a pending transition and snaps",
          "[event][weapon-mode][typed]") {
  RecordingHost host;
  host.pending = true;
  host.skip = true;
  host.gate = false;
  CHECK(event::ScriptSetAshleyBattleMode(Command(1, 2), host) ==
        event::ScriptStep::kAdvance);
  CHECK_FALSE(host.pending);
  CHECK(host.modes.empty());
  CHECK(host.animations.empty());
  CHECK(host.snaps == std::vector<bool>{true});
}
