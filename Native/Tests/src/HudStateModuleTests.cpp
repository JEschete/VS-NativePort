#include <array>
#include <bit>
#include <cstdint>
#include <span>
#include <type_traits>

#include <catch2/catch_test_macros.hpp>

#include "Battle/Actor.h"
#include "Battle/Hud.hpp"
#include "Battle/HudStateLive.hpp"
#include "Content/ModuleData.hpp"
#include "Field/Minimap.hpp"

namespace {
namespace b = VS::Battle;

template <typename Function>
Function ModuleEntry(unsigned address, Function expected) {
  const auto symbols = VS::Content::ModuleSymbols();
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
  const char hidden = VS::Battle::hudHidden;
  const std::uint8_t minimap = VS::Field::isMinimapVisible;
  const auto command = ModuleEntry(0x8007BD8C, &b::SetLiveGameplayHudHidden);
  if (command == nullptr) return;
  CHECK(command(-256) == -256);
  CHECK(VS::Battle::hudHidden == 1);
  CHECK(VS::Field::isMinimapVisible == 0);
  VS::Battle::hudHidden = hidden;
  VS::Field::isMinimapVisible = minimap;
}

TEST_CASE("F01356 production module preserves the signed status-mask ABI",
          "[hud-state-module][F01356]") {
  const auto flags = ModuleEntry(0x800C9E08, &b::LiveStatusIconFlags);
  if (flags == nullptr) return;
  Battle_ActorStats stats{};
  stats.statuses = 0x20;
  CHECK(flags(&stats) == 1);
  const u_short saved = VS::Battle::upperStatusIconFlags[0];
  VS::Battle::upperStatusIconFlags[0] = 0x8000;
  stats.statuses = 0x00200000;
  CHECK(std::bit_cast<std::uint32_t>(flags(&stats)) == 0x80000000);
  VS::Battle::upperStatusIconFlags[0] = saved;
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
  const char saved = VS::Battle::shortcutHudRequested;
  const u_char animation = VS::Battle::shortcutHudAnimationStep;
  show();
  CHECK(VS::Battle::shortcutHudRequested == 1);
  CHECK(VS::Battle::shortcutHudAnimationStep == animation);
  VS::Battle::shortcutHudRequested = saved;
}

TEST_CASE("F01382 production module enables the exact lower-screen timer state",
          "[hud-state-module][F01382]") {
  const auto show = ModuleEntry(0x800CB550, &b::ShowLivePuzzleTimer);
  if (show == nullptr) return;
  const char saved = VS::Battle::lowerScreenUiState;
  VS::Battle::lowerScreenUiState = 3;
  show();
  CHECK(VS::Battle::lowerScreenUiState == 1);
  VS::Battle::lowerScreenUiState = saved;
}

TEST_CASE("F01383 production module disables the whole timer-state byte",
          "[hud-state-module][F01383]") {
  const auto hide = ModuleEntry(0x800CB560, &b::HideLivePuzzleTimer);
  if (hide == nullptr) return;
  const char saved = VS::Battle::lowerScreenUiState;
  VS::Battle::lowerScreenUiState = 3;
  hide();
  CHECK(VS::Battle::lowerScreenUiState == 0);
  VS::Battle::lowerScreenUiState = saved;
}

TEST_CASE("F01385 production module accepts noncanonical visibility bytes",
          "[hud-state-module][F01385]") {
  const auto hide = ModuleEntry(0x800CB654, &b::SetLiveHudHidden);
  if (hide == nullptr) return;
  const char saved = VS::Battle::hudHidden;
  hide(256);
  CHECK(VS::Battle::hudHidden == 0);
  hide(-1);
  CHECK(static_cast<std::uint8_t>(VS::Battle::hudHidden) == 0xFF);
  VS::Battle::hudHidden = saved;
}

TEST_CASE("F01386 production module preserves all low-byte mode bits",
          "[hud-state-module][F01386]") {
  const auto mode = ModuleEntry(0x800CB660, &b::SetLiveHudBattleMode);
  if (mode == nullptr) return;
  const char saved = VS::Battle::hudBattleMode;
  mode(0x103);
  CHECK(VS::Battle::hudBattleMode == 3);
  VS::Battle::hudBattleMode = saved;
}
