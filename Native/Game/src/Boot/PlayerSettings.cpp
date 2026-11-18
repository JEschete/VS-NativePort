#include "Boot/PlayerSettings.hpp"

#include <string.h>

#include "Input/ControllerLiveState.hpp"

// Shared defaults from TITLE.PRG _initEnvironment,
// src/TITLE/TITLE.PRG/22C.c:4463. Title presentation and retained
// audio/vibration preferences stay with callers.
void VS::Boot::InitializePlayerSettings(void) {
  enum { kInitialMenuFlags = 0x2D8, kInitialMapMode = 3 };
  memset(&VS::Input::settings, 0, sizeof VS::Input::settings);
  VS::Input::settings.menuFlags = kInitialMenuFlags;
  VS::Input::settings.information = 1;
  VS::Input::settings.simpleMap = kInitialMapMode;
  VS::Input::settings.containerEmpty = 1;
  VS::Input::settings.abilityTimingDisplay = 1;
  VS::Input::settings.cursorMemory = 1;
}
