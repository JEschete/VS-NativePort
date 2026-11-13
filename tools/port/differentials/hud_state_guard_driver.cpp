// Checked Live bindings with local storage and a bounded diagnostic sink.
// Field::SetLiveMinimapVisible itself is the production helper, not a stub.
#include <bit>
#include <csignal>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string_view>

#include "Battle/HudStateLive.hpp"
#include "Core/Log.hpp"
#include "Field/RoomStateLive.hpp"
extern "C" {
#include "Battle/Actor.h"
#include "Battle/Hud.h"
#include "Core/Diagnostics.h"
#include "Field/MapProgress.h"
#include "Field/Minimap.h"
#include "State/Progress.h"

char Battle_hudStatusIconsHidden{}, Battle_hudHidden{}, Battle_hudBattleMode{},
    Battle_battleModeIndicatorStep{}, Battle_shortcutHudRequested{},
    Battle_lowerScreenUiState{}, Field_isMinimapVisible{};
int Battle_hudSlideOffset{};
u_char Battle_shortcutHudAnimationStep{};
u_short Battle_battleModeIndicatorWobble{};
u_short Battle_upperStatusIconFlags[10]{};
Battle_Actor *Battle_characterState{};
// Linkage only for unused MapLive.cpp functions. No room/progress operation is
// exposed or exercised by this driver.
State_RoomVisitFlags State_roomVisitProgress{};
int Field_countableRoomMask[16]{};

void Core_Errorf(const char *channel, const char *format, ...) {
  std::fprintf(stderr, "%s: ", channel);
  std::va_list arguments;
  va_start(arguments, format);
  std::vfprintf(stderr, format, arguments);
  va_end(arguments);
  std::fputc('\n', stderr);
  std::fflush(stderr);
}
}

// COFF linking still requires unused MapLive.cpp dependencies. These services
// abort if reached; they supply no successful result or private implementation.
namespace VS::Field {
int LiveFindLocationId(int, int) {
  Core_Errorf("hud.guard", "Unexpected out-of-scope room lookup");
  std::abort();
}
} // namespace VS::Field

namespace VS::Core {
bool IsLogEnabled(LogLevel, std::string_view) {
  Core_Errorf("hud.guard", "Unexpected out-of-scope map log");
  std::abort();
}
void LogWrite(LogLevel, std::string_view, std::string_view) {
  Core_Errorf("hud.guard", "Unexpected out-of-scope map log");
  std::abort();
}
} // namespace VS::Core

#ifndef VS_HUD_LIVE_STORAGE_ONLY
namespace {
namespace b = VS::Battle;

void Seed() {
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
  Battle_upperStatusIconFlags[0] = 0x8000;
  Battle_upperStatusIconFlags[7] = 3;
  Battle_upperStatusIconFlags[8] = 0xFFFF;
  Battle_upperStatusIconFlags[9] = 0xFFFF;
}

bool Probe(std::string_view selected) {
  if (selected == "status-null") {
    b::LiveStatusIconFlags(nullptr);
    return false;
  }
  if (selected == "limb-null") {
    b::LiveLimbCondition(nullptr);
    return false;
  }
  if (selected == "gameplay-composed") {
    const bool hidden = b::SetLiveGameplayHudHidden(256) == 256 &&
                        Battle_hudHidden == 1 && Field_isMinimapVisible == 0;
    return hidden && b::SetLiveGameplayHudHidden(0) == 0 &&
           Battle_hudHidden == 0 && Field_isMinimapVisible == 1 &&
           Battle_hudSlideOffset == 0x12345678;
  }
  if (selected == "visibility-byte") {
    b::SetLiveHudHidden(256);
    const bool shown = Battle_hudHidden == 0;
    b::SetLiveHudHidden(-1);
    return shown && static_cast<std::uint8_t>(Battle_hudHidden) == 0xFF &&
           Battle_hudStatusIconsHidden == static_cast<char>(0xA7) &&
           Battle_hudSlideOffset == 0x12345678;
  }
  if (selected == "mode-byte") {
    b::SetLiveHudBattleMode(257);
    const bool indicator = b::ReadLiveHudState().ShowsBattleModeIndicator();
    b::SetLiveHudBattleMode(259);
    return indicator && Battle_hudBattleMode == 3 &&
           b::ReadLiveHudState().UsesWeaponLimbDisplay() &&
           !b::ReadLiveHudState().ShowsBattleModeIndicator() &&
           Battle_battleModeIndicatorStep == static_cast<char>(0xCD) &&
           Battle_battleModeIndicatorWobble == 0xABCD;
  }
  if (selected == "shortcut-reentry") {
    b::ShowLiveShortcutHud();
    b::ShowLiveShortcutHud();
    return Battle_shortcutHudRequested == 1 &&
           Battle_shortcutHudAnimationStep == 0xD3 &&
           Battle_lowerScreenUiState == static_cast<char>(0xFF);
  }
  if (selected == "timer-enable") {
    Battle_lowerScreenUiState = 3;
    b::ShowLivePuzzleTimer();
    return Battle_lowerScreenUiState == 1 &&
           b::ReadLiveHudState().ShowsPuzzleTimer() &&
           !b::ReadLiveHudState().SuppressesLimbDisplay() &&
           Battle_hudBattleMode == 3;
  }
  if (selected == "timer-disable") {
    b::HideLivePuzzleTimer();
    const bool hidden = Battle_lowerScreenUiState == 0;
    b::ShowLivePuzzleTimer();
    b::HideLivePuzzleTimer();
    return hidden && Battle_lowerScreenUiState == 0 &&
           Battle_shortcutHudAnimationStep == 0xD3;
  }
  if (selected == "status-current-owner") {
    Battle_ActorStats first{}, second{};
    first.statuses = 0x10200020;
    second.statuses = std::bit_cast<int>(std::uint32_t{0xE000001F});
    return std::bit_cast<std::uint32_t>(b::LiveStatusIconFlags(&first)) ==
               0x80030001 &&
           b::LiveStatusIconFlags(&second) == 0 &&
           Battle_upperStatusIconFlags[8] == 0xFFFF &&
           Battle_shortcutHudRequested == static_cast<char>(0xA9);
  }
  if (selected == "limb-signed") {
    Battle_LimbStats limb{};
    limb.hp = -32768;
    limb.maxHp = -32768;
    const bool critical = b::LiveLimbCondition(&limb) == 0;
    limb.hp = 2;
    limb.maxHp = 0;
    const bool excellent = b::LiveLimbCondition(&limb) == 4;
    limb.hp = 100;
    limb.maxHp = 400;
    return critical && excellent && b::LiveLimbCondition(&limb) == 2;
  }
  if (selected == "fresh-state") {
    Battle_hudHidden = 0;
    Battle_hudBattleMode = static_cast<char>(0x81);
    Battle_lowerScreenUiState = 3;
    const auto state = b::ReadLiveHudState();
    return !state.IsHidden() && state.UsesWeaponLimbDisplay() &&
           !state.ShowsBattleModeIndicator() && !state.ShowsPuzzleTimer() &&
           state.SuppressesLimbDisplay();
  }
  if (selected == "unused-status-table") {
    b::StatusLifecycleState status;
    status.statuses = 0xE000001F;
    const auto icons = b::StatusIconFlags(b::HudStatusView{status}, {});
    if (!icons.has_value())
      return false;
    return icons.value() == 0;
  }
  if (selected == "used-status-table-refused") {
    b::StatusLifecycleState status;
    status.statuses = 0x00200000;
    return !b::StatusIconFlags(b::HudStatusView{status}, {}).has_value();
  }
  std::fprintf(stderr, "unknown checked HUD probe\n");
  return false;
}
} // namespace

int main(int argc, char **argv) {
  if (argc != 2)
    return 64;
  // The actual Live abort is exercised without invoking Windows crash UI.
  std::signal(SIGABRT, [](int) { std::_Exit(85); });
  Seed();
  const bool passed = Probe(argv[1]);
  std::puts(passed ? "PASS" : "FAIL");
  return passed ? 0 : 1;
}
#endif
