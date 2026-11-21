#pragma once

#include <cstdint>

// The prompt glyphs the menus draw from the interface texture: the command
// label above the menu and the controller-button icons.
namespace VS::Menu::Framework {
// decomp: vs_mainMenu_menuCommands (src/MENU/MAINMENU.PRG/58EC.h:48), the
// label vs_mainMenu_setMenuCommand (src/MENU/MAINMENU.PRG/58EC.c:412) shows
// above the menu.
enum class MenuCommand : std::uint8_t {
  kNone,
  kCommand,
  kMenu,
  kNum,
  kWarning,
  kHelp,
  kInfo,
};

// decomp: vs_mainMenu_buttonIDs (src/MENU/MAINMENU.PRG/58EC.h:92), the icon
// vs_mainmenu_renderButton (src/MENU/MAINMENU.PRG/58EC.c:494) cuts from the
// interface texture.
enum class ButtonIcon : std::uint8_t {
  kTriangle,
  kSquare,
  kCircle,
  kCross,
  kL1,
  kR1,
  kL2,
  kR2,
};
}  // namespace VS::Menu::Framework
