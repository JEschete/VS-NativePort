#include "Boot/PlayerSettings.h"

#include <string.h>

#include "Input/Controller.h"

// Shared defaults from TITLE.PRG _initEnvironment,
// src/TITLE/TITLE.PRG/22C.c:4463. Title presentation and retained
// audio/vibration preferences stay with callers.
void Boot_InitializePlayerSettings(void) {
  enum { kInitialMenuFlags = 0x2D8, kInitialMapMode = 3 };
  memset(&Input_settings, 0, sizeof Input_settings);
  Input_settings.menuFlags = kInitialMenuFlags;
  Input_settings.information = 1;
  Input_settings.simpleMap = kInitialMapMode;
  Input_settings.containerEmpty = 1;
  Input_settings.abilityTimingDisplay = 1;
  Input_settings.cursorMemory = 1;
}
