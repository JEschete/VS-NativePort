#include "Menu/MagicMenusLive.hpp"

#include "Menu/MagicMenus.hpp"

namespace VS::Menu::Magic {

int UpdateWarlockMenuLive(u_int init_shortcut_invoked) {
  return UpdateWarlockMenu(init_shortcut_invoked);
}

int UpdateShamanMenuLive(u_int init_shortcut_invoked) {
  return UpdateShamanMenu(init_shortcut_invoked);
}

int UpdateSorcererMenuLive(u_int init_shortcut_invoked) {
  return UpdateSorcererMenu(init_shortcut_invoked);
}

int UpdateEnchanterMenuLive(u_int init_shortcut_invoked) {
  return UpdateEnchanterMenu(init_shortcut_invoked);
}

}  // namespace VS::Menu::Magic
