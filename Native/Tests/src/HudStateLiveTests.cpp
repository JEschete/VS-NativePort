#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <iterator>
#include <span>

#include <catch2/catch_test_macros.hpp>

#include "Battle/AbilityState.hpp"
#include "Battle/Actor.hpp"
#include "Battle/Hud.hpp"
#include "Battle/HudStateLive.hpp"
#include "Field/Minimap.hpp"

using VS::Battle::LimbStats;

namespace {
namespace b = VS::Battle;

struct LiveHudFixture {
  b::HudState saved = b::ReadLiveHudState();
  char saved_icons = VS::Battle::hudStatusIconsHidden;
  int saved_slide = VS::Battle::hudSlideOffset;
  char saved_indicator = VS::Battle::battleModeIndicatorStep;
  std::uint16_t saved_wobble = VS::Battle::battleModeIndicatorWobble;
  std::uint8_t saved_animation = VS::Battle::shortcutHudAnimationStep;
  std::uint8_t saved_minimap = VS::Field::isMinimapVisible;
  VS::Battle::Actor* saved_character = VS::Battle::characterState;
  std::array<std::uint16_t, 10> saved_mapping =
      VS::Battle::upperStatusIconFlags;

  LiveHudFixture() {
    VS::Battle::hudStatusIconsHidden = static_cast<char>(0xA7);
    VS::Battle::hudHidden = static_cast<char>(0x80);
    VS::Battle::hudSlideOffset = 0x12345678;
    VS::Battle::hudBattleMode = 3;
    VS::Battle::battleModeIndicatorStep = static_cast<char>(0xCD);
    VS::Battle::battleModeIndicatorWobble = 0xABCD;
    VS::Battle::shortcutHudRequested = static_cast<char>(0xA9);
    VS::Battle::shortcutHudAnimationStep = 0xD3;
    VS::Battle::lowerScreenUiState = static_cast<char>(0xFF);
    VS::Field::isMinimapVisible = 0xAB;
    VS::Battle::characterState = nullptr;
  }
  ~LiveHudFixture() {
    VS::Battle::hudHidden = static_cast<char>(saved.visibility.value);
    VS::Battle::hudBattleMode = static_cast<char>(saved.battle_mode.value);
    VS::Battle::lowerScreenUiState = static_cast<char>(saved.lower_screen);
    VS::Battle::shortcutHudRequested = static_cast<char>(saved.shortcut);
    VS::Battle::hudStatusIconsHidden = saved_icons;
    VS::Battle::hudSlideOffset = saved_slide;
    VS::Battle::battleModeIndicatorStep = saved_indicator;
    VS::Battle::battleModeIndicatorWobble = saved_wobble;
    VS::Battle::shortcutHudAnimationStep = saved_animation;
    VS::Field::isMinimapVisible = saved_minimap;
    VS::Battle::characterState = saved_character;
    std::ranges::copy(saved_mapping,
                      std::span(VS::Battle::upperStatusIconFlags).begin());
  }
};
}  // namespace

TEST_CASE(
    "F00235 Live composes the real minimap helper and preserves its command",
    "[hud-state-live][F00235]") {
  const LiveHudFixture fixture;
  CHECK(b::SetLiveGameplayHudHidden(256) == 256);
  CHECK(VS::Battle::hudHidden == 1);
  CHECK(VS::Field::isMinimapVisible == 0);
  CHECK(b::SetLiveGameplayHudHidden(0) == 0);
  CHECK(VS::Battle::hudHidden == 0);
  CHECK(VS::Field::isMinimapVisible == 1);
  CHECK(VS::Battle::hudSlideOffset == 0x12345678);
  CHECK(VS::Battle::lowerScreenUiState == static_cast<char>(0xFF));
}

TEST_CASE(
    "F01356 Live reads its supplied stats and only the eight mapped entries",
    "[hud-state-live][F01356]") {
  const LiveHudFixture fixture;
  VS::Battle::ActorStats first{}, second{};
  std::fill(std::begin(VS::Battle::upperStatusIconFlags),
            std::end(VS::Battle::upperStatusIconFlags), 0xFFFF);
  std::fill_n(std::begin(VS::Battle::upperStatusIconFlags),
              b::kHudMappedStatusCount, 0);
  VS::Battle::upperStatusIconFlags[0] = 0x8000;
  VS::Battle::upperStatusIconFlags[7] = 3;
  first.statuses = 0x10200020;
  second.statuses = std::bit_cast<int>(std::uint32_t{0xE000001F});
  CHECK(std::bit_cast<std::uint32_t>(b::LiveStatusIconFlags(&first)) ==
        0x80030001);
  CHECK(b::LiveStatusIconFlags(&second) == 0);
  CHECK(first.statuses == 0x10200020);
  CHECK(VS::Battle::upperStatusIconFlags[8] == 0xFFFF);
  CHECK(VS::Battle::shortcutHudRequested == static_cast<char>(0xA9));
}

TEST_CASE(
    "F01357 Live classifies signed limb HP without unrelated actor owners",
    "[hud-state-live][F01357]") {
  const LiveHudFixture fixture;
  LimbStats limb{};
  limb.hp = 100;
  limb.max_hp = 400;
  CHECK(b::LiveLimbCondition(&limb) ==
        static_cast<int>(b::HudLimbCondition::kWounded));
  limb.hp = 300;
  CHECK(b::LiveLimbCondition(&limb) ==
        static_cast<int>(b::HudLimbCondition::kGood));
  limb.hp = 2;
  limb.max_hp = 0;
  CHECK(b::LiveLimbCondition(&limb) ==
        static_cast<int>(b::HudLimbCondition::kExcellent));
  limb.hp = -32768;
  CHECK(b::LiveLimbCondition(&limb) ==
        static_cast<int>(b::HudLimbCondition::kCritical));
  CHECK(limb.max_hp == 0);
}

TEST_CASE("F01359 Live requests opening without resetting animation or peers",
          "[hud-state-live][F01359]") {
  const LiveHudFixture fixture;
  auto expected = b::ReadLiveHudState();
  expected.shortcut = static_cast<std::uint8_t>(b::ShortcutHudRequest::kOpen);
  b::ShowLiveShortcutHud();
  CHECK(b::ReadLiveHudState() == expected);
  CHECK(VS::Battle::shortcutHudAnimationStep == 0xD3);
  b::ShowLiveShortcutHud();
  CHECK(b::ReadLiveHudState() == expected);
}

TEST_CASE("F01382 Live overwrites the lower-screen menu bit and no HUD peers",
          "[hud-state-live][F01382]") {
  const LiveHudFixture fixture;
  auto expected = b::ReadLiveHudState();
  expected.lower_screen =
      static_cast<std::uint8_t>(b::LowerScreenUi::kPuzzleTimer);
  b::ShowLivePuzzleTimer();
  CHECK(b::ReadLiveHudState() == expected);
  CHECK(VS::Battle::lowerScreenUiState == 1);
  CHECK(VS::Battle::battleModeIndicatorWobble == 0xABCD);
  CHECK(b::ReadLiveHudState().ShowsPuzzleTimer());
}

TEST_CASE(
    "F01383 Live clears the entire lower-screen byte across repeated entry",
    "[hud-state-live][F01383]") {
  const LiveHudFixture fixture;
  auto expected = b::ReadLiveHudState();
  expected.lower_screen = static_cast<std::uint8_t>(b::LowerScreenUi::kIdle);
  b::HideLivePuzzleTimer();
  CHECK(b::ReadLiveHudState() == expected);
  b::ShowLivePuzzleTimer();
  b::HideLivePuzzleTimer();
  CHECK(b::ReadLiveHudState() == expected);
  CHECK(VS::Battle::shortcutHudAnimationStep == 0xD3);
}

TEST_CASE("F01385 Live stores the verbatim low byte rather than a boolean",
          "[hud-state-live][F01385]") {
  const LiveHudFixture fixture;
  auto expected = b::ReadLiveHudState();
  expected.visibility = b::HudVisibilityCommand{b::HudVisibility::kShown};
  b::SetLiveHudHidden(256);
  CHECK(b::ReadLiveHudState() == expected);
  b::SetLiveHudHidden(-1);
  CHECK(static_cast<std::uint8_t>(VS::Battle::hudHidden) == 0xFF);
  CHECK(VS::Battle::hudStatusIconsHidden == static_cast<char>(0xA7));
  CHECK(VS::Battle::hudSlideOffset == 0x12345678);
}

TEST_CASE("F01386 Live preserves the mode byte and indicator animation peers",
          "[hud-state-live][F01386]") {
  const LiveHudFixture fixture;
  b::SetLiveHudBattleMode(0x101);
  CHECK(b::ReadLiveHudState().ShowsBattleModeIndicator());
  b::SetLiveHudBattleMode(0x103);
  CHECK(VS::Battle::hudBattleMode == 3);
  CHECK_FALSE(b::ReadLiveHudState().ShowsBattleModeIndicator());
  CHECK(b::ReadLiveHudState().UsesWeaponLimbDisplay());
  CHECK(VS::Battle::battleModeIndicatorStep == static_cast<char>(0xCD));
  CHECK(VS::Battle::battleModeIndicatorWobble == 0xABCD);
  b::SetLiveHudBattleMode(0x100);
  CHECK(VS::Battle::hudBattleMode == 0);
}
