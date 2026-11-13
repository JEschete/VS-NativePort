#include <array>
#include <cstdint>
#include <functional>
#include <limits>
#include <span>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Battle/ActorVitals.hpp"
#include "Battle/HudState.hpp"

namespace {
namespace b = VS::Battle;

constexpr b::HudState kPeerState{
    .visibility = b::HudVisibilityCommand{b::HudVisibility::kHidden},
    .battle_mode = b::HudBattleModeCommand{b::HudBattleMode::kArmedActionMenu},
    .lower_screen =
        static_cast<std::uint8_t>(b::LowerScreenUi::kTimerAndMenuBackground),
    .shortcut = static_cast<std::uint8_t>(b::ShortcutHudRequest::kIdle),
};

enum class Service { kHud, kOverlay };
class HudWorld final : public b::GameplayHudWorld {
 public:
  b::HudState state = kPeerState;
  bool overlay = true;
  b::HudVisibilityCommand observed_visibility{};
  std::vector<Service> calls;
  std::function<void()> on_overlay;

  void SetHudVisibility(b::HudVisibility visibility) override {
    b::SetHudHidden(state, b::HudVisibilityCommand{visibility});
    calls.push_back(Service::kHud);
  }
  void SetFieldOverlayVisible(bool visible) override {
    observed_visibility = state.visibility;
    overlay = visible;
    calls.push_back(Service::kOverlay);
    if (on_overlay) on_overlay();
  }
};
}  // namespace

TEST_CASE("F00235 composes visibility in order and returns the full command",
          "[hud-state][F00235]") {
  const auto command =
      GENERATE(0, 1, -1, 256, -256, std::numeric_limits<std::int32_t>::min(),
               std::numeric_limits<std::int32_t>::max());
  HudWorld world;
  auto expected = kPeerState;
  expected.visibility = b::HudVisibilityCommand{
      command == 0 ? b::HudVisibility::kShown : b::HudVisibility::kHidden};
  CHECK(b::SetGameplayHudHidden(world, command) == command);
  CHECK(world.state == expected);
  CHECK(world.observed_visibility == expected.visibility);
  CHECK(world.overlay == (command == 0));
  CHECK(world.calls == std::vector{Service::kHud, Service::kOverlay});
}

TEST_CASE("F00235 leaves overlay-service mutations and re-entry intact",
          "[hud-state][F00235]") {
  HudWorld world;
  world.on_overlay = [&world] {
    world.state.visibility = b::HudVisibilityCommand{std::uint8_t{0x80}};
    world.state.lower_screen =
        static_cast<std::uint8_t>(b::LowerScreenUi::kMenuBackground);
  };
  CHECK(b::SetGameplayHudHidden(world, -1) == -1);
  CHECK(world.state.IsHidden());
  CHECK(world.state.visibility == b::HudVisibilityCommand{std::uint8_t{0x80}});
  CHECK(world.state.lower_screen ==
        static_cast<std::uint8_t>(b::LowerScreenUi::kMenuBackground));
  world.on_overlay = {};
  CHECK(b::SetGameplayHudHidden(world, 0) == 0);
  CHECK_FALSE(world.state.IsHidden());
  CHECK(world.overlay);
  CHECK(world.state.lower_screen ==
        static_cast<std::uint8_t>(b::LowerScreenUi::kMenuBackground));
}

TEST_CASE("F01356 maps unsigned high flags while ignoring unused status bits",
          "[hud-state][F01356]") {
  b::ActorVitalsState actor;
  actor.status.statuses = 0xF020003F;
  std::array<std::uint16_t, b::kHudMappedStatusCount> icons{};
  icons.front() = 0x8000;
  icons.back() = 0x0123;
  const auto result = b::StatusIconFlags(b::HudStatusView{actor.status}, icons);
  if (!result.has_value()) {
    FAIL("complete status table was refused");
    return;
  }
  CHECK(result.value() == 0x81230001);
  CHECK(actor.status.statuses == 0xF020003F);
}

TEST_CASE("F01356 requires only mapping entries for statuses actually present",
          "[hud-state][F01356]") {
  b::StatusLifecycleState status;
  status.statuses = 0x00200000;
  const std::array<std::uint16_t, 1> one_icon{0x8000};
  const auto mapped = b::StatusIconFlags(b::HudStatusView{status}, one_icon);
  if (!mapped.has_value()) {
    FAIL("first mapped status requires only its first entry");
    return;
  }
  CHECK(mapped.value() == 0x80000000);
  status.statuses = 0x00400000;
  CHECK_FALSE(
      b::StatusIconFlags(b::HudStatusView{status}, one_icon).has_value());
  status.statuses = 0x10000000;
  CHECK_FALSE(
      b::StatusIconFlags(b::HudStatusView{status}, one_icon).has_value());
  CHECK(status.statuses == 0x10000000);
}

TEST_CASE("F01356 leaves absent mappings lazy for direct and ignored statuses",
          "[hud-state][F01356]") {
  const auto statuses = GENERATE(std::uint32_t{0}, std::uint32_t{0xE000001F},
                                 std::uint32_t{0x001FFFE0});
  b::StatusLifecycleState status;
  status.statuses = statuses;
  const auto result = b::StatusIconFlags(b::HudStatusView{status}, {});
  if (!result.has_value()) {
    FAIL("unused status mapping must stay lazy");
    return;
  }
  CHECK(result.value() == (statuses == 0x001FFFE0 ? 0xFFFFU : 0U));
}

TEST_CASE("F01357 preserves strict quarter and three-quarter HP boundaries",
          "[hud-state][F01357]") {
  using c = b::HudLimbCondition;
  const auto sample = GENERATE(std::pair{std::int16_t{1}, c::kCritical},
                               std::pair{std::int16_t{2}, c::kDamaged},
                               std::pair{std::int16_t{99}, c::kDamaged},
                               std::pair{std::int16_t{100}, c::kWounded},
                               std::pair{std::int16_t{299}, c::kWounded},
                               std::pair{std::int16_t{300}, c::kGood},
                               std::pair{std::int16_t{399}, c::kGood},
                               std::pair{std::int16_t{400}, c::kExcellent},
                               std::pair{std::int16_t{401}, c::kExcellent});
  const b::StatusLimb limb{.hp = sample.first, .max_hp = 400};
  CHECK(b::LimbCondition(b::HudLimbHealthView{limb}) == sample.second);
  CHECK(limb.hp == sample.first);
}

TEST_CASE("F01357 supports signed extremes and zero or negative maximum HP",
          "[hud-state][F01357]") {
  using c = b::HudLimbCondition;
  const auto sample = GENERATE(
      std::array<std::int16_t, 2>{-32768, -32768},
      std::array<std::int16_t, 2>{-1, 32767}, std::array<std::int16_t, 2>{1, 0},
      std::array<std::int16_t, 2>{2, 0}, std::array<std::int16_t, 2>{2, -32768},
      std::array<std::int16_t, 2>{32767, 32767});
  const b::StatusLimb limb{.hp = sample[0], .max_hp = sample[1]};
  CHECK(b::LimbCondition(b::HudLimbHealthView{limb}) ==
        (sample[0] < 2 ? c::kCritical : c::kExcellent));
}

TEST_CASE("F01357 retains the ordered comparisons for very small limbs",
          "[hud-state][F01357]") {
  CHECK(b::LimbCondition({2, 3}) == b::HudLimbCondition::kWounded);
  CHECK(b::LimbCondition({2, 2}) == b::HudLimbCondition::kExcellent);
  CHECK(b::LimbCondition({2, 32767}) == b::HudLimbCondition::kDamaged);
}

TEST_CASE("F01359 requests shortcut opening without replacing other HUD policy",
          "[hud-state][F01359]") {
  auto state = kPeerState;
  auto expected = state;
  expected.shortcut = static_cast<std::uint8_t>(b::ShortcutHudRequest::kOpen);
  b::ShowShortcutHud(state);
  CHECK(state == expected);
  b::ShowShortcutHud(state);
  CHECK(state == expected);
  CHECK(state.SuppressesLimbDisplay());
  CHECK_FALSE(state.ShowsPuzzleTimer());
}

TEST_CASE("F01382 replaces raised-menu state with the puzzle-timer policy",
          "[hud-state][F01382]") {
  auto state = kPeerState;
  auto expected = state;
  expected.lower_screen =
      static_cast<std::uint8_t>(b::LowerScreenUi::kPuzzleTimer);
  b::ShowPuzzleTimer(state);
  CHECK(state == expected);
  CHECK(state.ShowsPuzzleTimer());
  CHECK_FALSE(state.SuppressesLimbDisplay());
  b::ShowPuzzleTimer(state);
  CHECK(state == expected);
}

TEST_CASE("F01383 clears the entire lower-screen command on re-entry",
          "[hud-state][F01383]") {
  auto state = kPeerState;
  state.lower_screen = 0xFF;
  auto expected = state;
  expected.lower_screen = static_cast<std::uint8_t>(b::LowerScreenUi::kIdle);
  b::HidePuzzleTimer(state);
  CHECK(state == expected);
  CHECK_FALSE(state.ShowsPuzzleTimer());
  CHECK_FALSE(state.SuppressesLimbDisplay());
  b::ShowPuzzleTimer(state);
  b::HidePuzzleTimer(state);
  CHECK(state == expected);
}

TEST_CASE("F01385 keeps noncanonical visibility bytes and unrelated HUD peers",
          "[hud-state][F01385]") {
  const auto value = GENERATE(std::uint8_t{0}, std::uint8_t{1},
                              std::uint8_t{0x80}, std::uint8_t{0xFF});
  const b::HudVisibilityCommand visibility{value};
  auto state = kPeerState;
  auto expected = state;
  expected.visibility = visibility;
  b::SetHudHidden(state, visibility);
  CHECK(state == expected);
  CHECK(state.IsHidden() == (value != 0));
}

TEST_CASE("F01386 distinguishes weapon limb mode from the exact indicator mode",
          "[hud-state][F01386]") {
  const auto command = GENERATE(0, 1, 2, 3, 0x80, 0x81, 0xFF);
  auto state = kPeerState;
  auto expected = state;
  expected.battle_mode =
      b::HudBattleModeCommand{static_cast<std::uint8_t>(command)};
  b::SetHudBattleMode(state, expected.battle_mode);
  CHECK(state == expected);
  CHECK(state.UsesWeaponLimbDisplay() == ((command & 1) != 0));
  CHECK(state.ShowsBattleModeIndicator() == (command == 1));
}
