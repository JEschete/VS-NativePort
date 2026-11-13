#include <array>
#include <bit>
#include <cstdint>
#include <span>
#include <type_traits>

#include <catch2/catch_test_macros.hpp>

#include "Battle/HudStateLive.hpp"
#include "Content/ModuleData.h"
extern "C" {
#include "Battle/Actor.h"
#include "Battle/Hud.h"
#include "Field/Minimap.h"
}

namespace {
namespace b = VS::Battle;

template <typename Function>
Function ModuleEntry(unsigned address, Function expected) {
  const auto symbols =
      std::span(Content_moduleSymbols, Content_moduleSymbolCount);
  void* const entry =
      VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG", address);
  if (entry == nullptr || entry != reinterpret_cast<void*>(expected)) {
    FAIL("HUD module entry did not resolve to the registered native signature");
    return nullptr;
  }
  return reinterpret_cast<Function>(entry);
}

static_assert(
    std::is_same_v<decltype(&b::SetLiveGameplayHudHidden), int (*)(int)>);
static_assert(std::is_same_v<decltype(&b::LiveStatusIconFlags),
                             int (*)(Battle_ActorStats*)>);
static_assert(std::is_same_v<decltype(&b::LiveLimbCondition),
                             int (*)(Battle_LimbStats*)>);
}  // namespace

TEST_CASE("F00235 production module returns a full-word visibility command",
          "[hud-state-module][F00235]") {
  const char hidden = Battle_hudHidden;
  const char minimap = Field_isMinimapVisible;
  const auto command = ModuleEntry(0x8007BD8C, &b::SetLiveGameplayHudHidden);
  if (command == nullptr) return;
  CHECK(command(-256) == -256);
  CHECK(Battle_hudHidden == 1);
  CHECK(Field_isMinimapVisible == 0);
  Battle_hudHidden = hidden;
  Field_isMinimapVisible = minimap;
}

TEST_CASE("F01356 production module preserves the signed status-mask ABI",
          "[hud-state-module][F01356]") {
  const auto flags = ModuleEntry(0x800C9E08, &b::LiveStatusIconFlags);
  if (flags == nullptr) return;
  Battle_ActorStats stats{};
  stats.statuses = 0x20;
  CHECK(flags(&stats) == 1);
  const u_short saved = Battle_upperStatusIconFlags[0];
  Battle_upperStatusIconFlags[0] = 0x8000;
  stats.statuses = 0x00200000;
  CHECK(std::bit_cast<std::uint32_t>(flags(&stats)) == 0x80000000);
  Battle_upperStatusIconFlags[0] = saved;
}

TEST_CASE("F01357 production module retains the limb-pointer ABI",
          "[hud-state-module][F01357]") {
  const auto condition = ModuleEntry(0x800C9E5C, &b::LiveLimbCondition);
  if (condition == nullptr) return;
  Battle_LimbStats limb{};
  limb.hp = 300;
  limb.maxHp = 400;
  CHECK(condition(&limb) == 3);
}

TEST_CASE("F01359 production module requests the shortcut without resetting it",
          "[hud-state-module][F01359]") {
  const auto show = ModuleEntry(0x800C9F88, &b::ShowLiveShortcutHud);
  if (show == nullptr) return;
  const char saved = Battle_shortcutHudRequested;
  const u_char animation = Battle_shortcutHudAnimationStep;
  show();
  CHECK(Battle_shortcutHudRequested == 1);
  CHECK(Battle_shortcutHudAnimationStep == animation);
  Battle_shortcutHudRequested = saved;
}

TEST_CASE("F01382 production module enables the exact lower-screen timer state",
          "[hud-state-module][F01382]") {
  const auto show = ModuleEntry(0x800CB550, &b::ShowLivePuzzleTimer);
  if (show == nullptr) return;
  const char saved = Battle_lowerScreenUiState;
  Battle_lowerScreenUiState = 3;
  show();
  CHECK(Battle_lowerScreenUiState == 1);
  Battle_lowerScreenUiState = saved;
}

TEST_CASE("F01383 production module disables the whole timer-state byte",
          "[hud-state-module][F01383]") {
  const auto hide = ModuleEntry(0x800CB560, &b::HideLivePuzzleTimer);
  if (hide == nullptr) return;
  const char saved = Battle_lowerScreenUiState;
  Battle_lowerScreenUiState = 3;
  hide();
  CHECK(Battle_lowerScreenUiState == 0);
  Battle_lowerScreenUiState = saved;
}

TEST_CASE("F01385 production module accepts noncanonical visibility bytes",
          "[hud-state-module][F01385]") {
  const auto hide = ModuleEntry(0x800CB654, &b::SetLiveHudHidden);
  if (hide == nullptr) return;
  const char saved = Battle_hudHidden;
  hide(256);
  CHECK(Battle_hudHidden == 0);
  hide(-1);
  CHECK(static_cast<std::uint8_t>(Battle_hudHidden) == 0xFF);
  Battle_hudHidden = saved;
}

TEST_CASE("F01386 production module preserves all low-byte mode bits",
          "[hud-state-module][F01386]") {
  const auto mode = ModuleEntry(0x800CB660, &b::SetLiveHudBattleMode);
  if (mode == nullptr) return;
  const char saved = Battle_hudBattleMode;
  mode(0x103);
  CHECK(Battle_hudBattleMode == 3);
  Battle_hudBattleMode = saved;
}
