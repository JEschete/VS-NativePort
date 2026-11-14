#include <array>
#include <cstdint>
#include <cstdio>
#include <iostream>

#include "Battle/ActionCostsLive.hpp"
#include "Menu/Framework/FrameworkLive.hpp"
#include "Menu/MagicMenus.hpp"
#include "Menu/Sounds.hpp"
#include "Render/GameFontLive.hpp"
#include "Render/SpritesLive.hpp"

extern "C" {
#include "Battle/ActionTable.h"
#include "Input/ControllerLiveState.h"
#include "Menu/Framework/FrameworkLiveState.h"
#include "Menu/Framework/Menus.h"
#include "Menu/Magic.h"

Battle_ActionDefinition Battle_actionDefinitions[256]{};
State_Settings Input_settings{};
int Input_buttonRepeat{};
union Input_PressedButtonsView Input_buttonsPressed{};
char Menu_Magic_warlockSpellIds[20]{};
char Menu_Magic_shamanSpellIds[8]{};
char Menu_Magic_sorcererSpellIds[20]{};
char Menu_Magic_enchanterSpellIds[8]{};
u_char Menu_Magic_warlockLevelCursorBlink{};
char Menu_Magic_availableWarlockSpells[18]{};
char Menu_Magic_availableShamanSpells[6]{};
char Menu_Magic_availableSorcererSpells[18]{};
char Menu_Magic_availableEnchanterSpells[8]{};
u_short Menu_Magic_magicStrings[2462]{};
char Menu_Framework_cursorBrightnessAnimation[22012]{};
char Menu_Framework_shortcutInvoked{};
int Menu_Framework_actionRestrictions{};
Menu_Framework_MenuMemory Menu_Framework_menuMemory{};
char Menu_Framework_displayActionCost{};

void Menu_Magic_SetSpellMpCost(int) {}
void* Core_ScratchBase(void) {
  static std::array<std::uint32_t, 256> scratch{};
  return scratch.data();
}
u_long LegacyGpu_Handle(const void* pointer) {
  return static_cast<u_long>(reinterpret_cast<std::uintptr_t>(pointer) &
                             0xFFFFFFu);
}
}

namespace {
int g_menu = -1;
int g_offset = -1;
int g_calls = 0;
}

namespace VS::Menu::Framework {
void FlyOutLiveRowsAndHoistSelection(int row, int offset) {
  g_menu = row;
  g_offset = offset;
  ++g_calls;
}
int AreLiveMenuRowsIdle() { return 0; }
int LiveSelectedRow() { return -1; }
int LiveHighlightedRow() { return 0; }
void CreateLiveMenuRows(int, int, char*[], int[]) {}
void ClearLiveMenuExcept(int) {}
void DismissLiveInformationBox() {}
void SetLiveCommandLabel(int) {}
void FlyOutLiveRowRight(int) {}
}  // namespace VS::Menu::Framework

namespace VS::Battle {
int LiveActorActionRestrictionFlags(int, int) { return 0; }
}  // namespace VS::Battle

namespace VS::Menu {
void PlayLeaveSound() {}
void PlaySelectSound() {}
void PlayInvalidSound() {}
void PlayChangeSound() {}
}  // namespace VS::Menu

namespace VS::Render {
void DrawLiveCompactText(const char*, int, void*) {}
u_long* QueueLiveSpriteWithTexturePage(int, int, int, u_long*) {
  static std::array<u_long, 64> primitive{};
  return primitive.data() + 16;
}
}  // namespace VS::Render

int main(int argc, char**) {
  if (argc != 2) return 2;

  unsigned function = 0;
  unsigned init = 0;
  while (std::cin >> function >> init) {
    g_menu = -1;
    g_offset = -1;
    g_calls = 0;
    int result = 0;
    switch (function) {
      case 0:
        result = VS::Menu::Magic::UpdateWarlockMenu(init);
        break;
      case 1:
        result = VS::Menu::Magic::UpdateShamanMenu(init);
        break;
      case 2:
        result = VS::Menu::Magic::UpdateSorcererMenu(init);
        break;
      case 3:
        result = VS::Menu::Magic::UpdateEnchanterMenu(init);
        break;
      default:
        return 3;
    }
    std::printf("%d %d %d %d\n", result, g_calls, g_menu, g_offset);
  }
}
