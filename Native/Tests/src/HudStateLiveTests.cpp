#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <iterator>
#include <span>

#include <catch2/catch_test_macros.hpp>

#include "Battle/HudStateLive.hpp"
extern "C" {
#include "Battle/AbilityState.h"
#include "Battle/Actor.h"
#include "Battle/Hud.h"
#include "Field/Minimap.h"
}

namespace {
namespace b = VS::Battle;

struct LiveHudFixture {
  b::HudState saved = b::ReadLiveHudState();
  char saved_icons = Battle_hudStatusIconsHidden;
  int saved_slide = Battle_hudSlideOffset;
  char saved_indicator = Battle_battleModeIndicatorStep;
  u_short saved_wobble = Battle_battleModeIndicatorWobble;
  u_char saved_animation = Battle_shortcutHudAnimationStep;
  char saved_minimap = Field_isMinimapVisible;
  Battle_Actor* saved_character = Battle_characterState;
  std::array<u_short, 10> saved_mapping =
      std::to_array(Battle_upperStatusIconFlags);

  LiveHudFixture() {
    Battle_hudStatusIconsHidden = static_cast<char>(0xA7);
    Battle_hudHidden = static_cast<char>(0x80);
    Battle_hudSlideOffset = 0x12345678;
    Battle_hudBattleMode = 3;
    Battle_battleModeIndicatorStep = static_cast<char>(0xCD);
    Battle_battleModeIndicatorWobble = 0xABCD;
    Battle_shortcutHudRequested = static_cast<char>(0xA9);
    Battle_shortcutHudAnimationStep = 0xD3;
    Battle_lowerScreenUiState = static_cast<char>(0xFF);
    Field_isMinimapVisible = static_cast<char>(0xAB);
    Battle_characterState = nullptr;
  }
  ~LiveHudFixture() {
    Battle_hudHidden = static_cast<char>(saved.visibility.value);
    Battle_hudBattleMode = static_cast<char>(saved.battle_mode.value);
    Battle_lowerScreenUiState = static_cast<char>(saved.lower_screen);
    Battle_shortcutHudRequested = static_cast<char>(saved.shortcut);
    Battle_hudStatusIconsHidden = saved_icons;
    Battle_hudSlideOffset = saved_slide;
    Battle_battleModeIndicatorStep = saved_indicator;
    Battle_battleModeIndicatorWobble = saved_wobble;
    Battle_shortcutHudAnimationStep = saved_animation;
    Field_isMinimapVisible = saved_minimap;
    Battle_characterState = saved_character;
    std::ranges::copy(saved_mapping,
                      std::span(Battle_upperStatusIconFlags).begin());
  }
};
}  // namespace

TEST_CASE(
    "F00235 Live composes the real minimap helper and preserves its command",
    "[hud-state-live][F00235]") {
  const LiveHudFixture fixture;
  CHECK(b::SetLiveGameplayHudHidden(256) == 256);
  CHECK(Battle_hudHidden == 1);
  CHECK(Field_isMinimapVisible == 0);
  CHECK(b::SetLiveGameplayHudHidden(0) == 0);
  CHECK(Battle_hudHidden == 0);
  CHECK(Field_isMinimapVisible == 1);
  CHECK(Battle_hudSlideOffset == 0x12345678);
  CHECK(Battle_lowerScreenUiState == static_cast<char>(0xFF));
}

TEST_CASE(
    "F01356 Live reads its supplied stats and only the eight mapped entries",
    "[hud-state-live][F01356]") {
  const LiveHudFixture fixture;
  Battle_ActorStats first{}, second{};
  std::fill(std::begin(Battle_upperStatusIconFlags),
            std::end(Battle_upperStatusIconFlags), 0xFFFF);
  std::fill_n(std::begin(Battle_upperStatusIconFlags), b::kHudMappedStatusCount,
              0);
  Battle_upperStatusIconFlags[0] = 0x8000;
  Battle_upperStatusIconFlags[7] = 3;
  first.statuses = 0x10200020;
  second.statuses = std::bit_cast<int>(std::uint32_t{0xE000001F});
  CHECK(std::bit_cast<std::uint32_t>(b::LiveStatusIconFlags(&first)) ==
        0x80030001);
  CHECK(b::LiveStatusIconFlags(&second) == 0);
  CHECK(first.statuses == 0x10200020);
  CHECK(Battle_upperStatusIconFlags[8] == 0xFFFF);
  CHECK(Battle_shortcutHudRequested == static_cast<char>(0xA9));
}

TEST_CASE(
    "F01357 Live classifies signed limb HP without unrelated actor owners",
    "[hud-state-live][F01357]") {
  const LiveHudFixture fixture;
  Battle_LimbStats limb{};
  limb.hp = 100;
  limb.maxHp = 400;
  CHECK(b::LiveLimbCondition(&limb) ==
        static_cast<int>(b::HudLimbCondition::kWounded));
  limb.hp = 300;
  CHECK(b::LiveLimbCondition(&limb) ==
        static_cast<int>(b::HudLimbCondition::kGood));
  limb.hp = 2;
  limb.maxHp = 0;
  CHECK(b::LiveLimbCondition(&limb) ==
        static_cast<int>(b::HudLimbCondition::kExcellent));
  limb.hp = -32768;
  CHECK(b::LiveLimbCondition(&limb) ==
        static_cast<int>(b::HudLimbCondition::kCritical));
  CHECK(limb.maxHp == 0);
}

TEST_CASE("F01359 Live requests opening without resetting animation or peers",
          "[hud-state-live][F01359]") {
  const LiveHudFixture fixture;
  auto expected = b::ReadLiveHudState();
  expected.shortcut = static_cast<std::uint8_t>(b::ShortcutHudRequest::kOpen);
  b::ShowLiveShortcutHud();
  CHECK(b::ReadLiveHudState() == expected);
  CHECK(Battle_shortcutHudAnimationStep == 0xD3);
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
  CHECK(Battle_lowerScreenUiState == 1);
  CHECK(Battle_battleModeIndicatorWobble == 0xABCD);
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
  CHECK(Battle_shortcutHudAnimationStep == 0xD3);
}

TEST_CASE("F01385 Live stores the verbatim low byte rather than a boolean",
          "[hud-state-live][F01385]") {
  const LiveHudFixture fixture;
  auto expected = b::ReadLiveHudState();
  expected.visibility = b::HudVisibilityCommand{b::HudVisibility::kShown};
  b::SetLiveHudHidden(256);
  CHECK(b::ReadLiveHudState() == expected);
  b::SetLiveHudHidden(-1);
  CHECK(static_cast<std::uint8_t>(Battle_hudHidden) == 0xFF);
  CHECK(Battle_hudStatusIconsHidden == static_cast<char>(0xA7));
  CHECK(Battle_hudSlideOffset == 0x12345678);
}

TEST_CASE("F01386 Live preserves the mode byte and indicator animation peers",
          "[hud-state-live][F01386]") {
  const LiveHudFixture fixture;
  b::SetLiveHudBattleMode(0x101);
  CHECK(b::ReadLiveHudState().ShowsBattleModeIndicator());
  b::SetLiveHudBattleMode(0x103);
  CHECK(Battle_hudBattleMode == 3);
  CHECK_FALSE(b::ReadLiveHudState().ShowsBattleModeIndicator());
  CHECK(b::ReadLiveHudState().UsesWeaponLimbDisplay());
  CHECK(Battle_battleModeIndicatorStep == static_cast<char>(0xCD));
  CHECK(Battle_battleModeIndicatorWobble == 0xABCD);
  b::SetLiveHudBattleMode(0x100);
  CHECK(Battle_hudBattleMode == 0);
}
