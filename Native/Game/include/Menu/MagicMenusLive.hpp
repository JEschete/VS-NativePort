#pragma once

#include "Legacy/Psx.h"

// NOLINTNEXTLINE(readability-identifier-naming): established subsystem API.
namespace VS::Menu::Magic {

int UpdateWarlockMenuLive(u_int init_shortcut_invoked);
int UpdateShamanMenuLive(u_int init_shortcut_invoked);
int UpdateSorcererMenuLive(u_int init_shortcut_invoked);
int UpdateEnchanterMenuLive(u_int init_shortcut_invoked);

}  // namespace VS::Menu::Magic
