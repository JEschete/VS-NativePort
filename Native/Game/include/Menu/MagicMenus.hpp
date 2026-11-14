#pragma once

#include "Legacy/Psx.h"

// NOLINTNEXTLINE(readability-identifier-naming): established subsystem API.
namespace VS::Menu::Magic {

// decomp: _warlockMagicMenu (MENU0.PRG/84.c:164)
int UpdateWarlockMenu(u_int init_shortcut_invoked);

// decomp: _shamanMagicMenu (MENU0.PRG/84.c:512)
int UpdateShamanMenu(u_int init_shortcut_invoked);

// decomp: _sorcererMagicMenu (MENU0.PRG/84.c:613)
int UpdateSorcererMenu(u_int init_shortcut_invoked);

// decomp: _enchanterMagicMenu (MENU0.PRG/84.c:713)
int UpdateEnchanterMenu(u_int init_shortcut_invoked);

}  // namespace VS::Menu::Magic
