// Hand-maintained (tools/port/owned.csv).
#include "Battle/HudStateLive.hpp"

#include <bit>
#include <cstdint>
#include <cstdlib>
#include <iterator>
#include <span>

#include "Battle/Actor.h"
#include "Battle/Hud.hpp"
#include "Core/Diagnostics.hpp"
#include "Field/MapLive.hpp"

namespace VS::Battle {
namespace {
[[noreturn]] void Refuse(const char* message) {
  VS::Core::Errorf("battle.hud", "%s", message);
  std::abort();
}

class LiveGameplayHudWorld final : public GameplayHudWorld {
 public:
  void SetHudVisibility(HudVisibility visibility) override {
    SetLiveHudHidden(static_cast<std::uint8_t>(visibility));
  }
  void SetFieldOverlayVisible(bool visible) override {
    Field::SetLiveMinimapVisible(visible ? 1 : 0);
  }
};
}  // namespace

HudState ReadLiveHudState() {
  return {
      .visibility = HudVisibilityCommand{static_cast<std::uint8_t>(
          VS::Battle::hudHidden)},
      .battle_mode = HudBattleModeCommand{static_cast<std::uint8_t>(
          VS::Battle::hudBattleMode)},
      .lower_screen = static_cast<std::uint8_t>(VS::Battle::lowerScreenUiState),
      .shortcut = static_cast<std::uint8_t>(VS::Battle::shortcutHudRequested),
  };
}

// decomp: F00235 func_8007BD8C (src/BATTLE/BATTLE.PRG/146C.c:8561).
int SetLiveGameplayHudHidden(int command) {
  LiveGameplayHudWorld world;
  return SetGameplayHudHidden(world, command);
}

// decomp: F01356 vs_battle_getStatusFlags (src/BATTLE/BATTLE.PRG/5BF94.c:3158).
int LiveStatusIconFlags(Battle_ActorStats* stats) {
  if (stats == nullptr) Refuse("Status icon flags require actor stats");
  const HudStatusView status{std::bit_cast<std::uint32_t>(stats->statuses)};
  static_assert(std::size(VS::Battle::upperStatusIconFlags) >=
                kHudMappedStatusCount);
  const auto icons =
      StatusIconFlags(status, std::span(VS::Battle::upperStatusIconFlags)
                                  .first<kHudMappedStatusCount>());
  if (!icons.has_value()) {
    Refuse("Status icon flags require their used mapping");
  }
  return std::bit_cast<std::int32_t>(icons.value());
}

// decomp: F01357 vs_battle_getLimbStatus (src/BATTLE/BATTLE.PRG/5BF94.c:3174).
int LiveLimbCondition(Battle_LimbStats* limb) {
  if (limb == nullptr) Refuse("Limb condition requires limb stats");
  const HudLimbHealthView status{limb->hp, limb->maxHp};
  return static_cast<int>(LimbCondition(status));
}

// decomp: F01359 func_800C9F88 (src/BATTLE/BATTLE.PRG/5BF94.c:3239).
void ShowLiveShortcutHud() {
  HudState state;
  ShowShortcutHud(state);
  VS::Battle::shortcutHudRequested = static_cast<char>(state.shortcut);
}

// decomp: F01382 func_800CB550 (src/BATTLE/BATTLE.PRG/5BF94.c:3838).
void ShowLivePuzzleTimer() {
  HudState state;
  ShowPuzzleTimer(state);
  VS::Battle::lowerScreenUiState = static_cast<char>(state.lower_screen);
}

// decomp: F01383 func_800CB560 (src/BATTLE/BATTLE.PRG/5BF94.c:3840).
void HideLivePuzzleTimer() {
  HudState state;
  HidePuzzleTimer(state);
  VS::Battle::lowerScreenUiState = static_cast<char>(state.lower_screen);
}

// decomp: F01385 func_800CB654 (src/BATTLE/BATTLE.PRG/5BF94.c:3867).
void SetLiveHudHidden(int command) {
  HudState state;
  SetHudHidden(state, HudVisibilityCommand{static_cast<std::uint8_t>(command)});
  VS::Battle::hudHidden = static_cast<char>(state.visibility.value);
}

// decomp: F01386 func_800CB660 (src/BATTLE/BATTLE.PRG/5BF94.c:3869).
void SetLiveHudBattleMode(int command) {
  HudState state;
  SetHudBattleMode(state,
                   HudBattleModeCommand{static_cast<std::uint8_t>(command)});
  VS::Battle::hudBattleMode = static_cast<char>(state.battle_mode.value);
}
}  // namespace VS::Battle
