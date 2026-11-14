#include "Menu/MagicMenusLive.hpp"

#include "Menu/MagicMenus.hpp"

namespace VS::Menu::Magic {

int UpdateWarlockMenuLive(unsigned int init_shortcut_invoked) {
  return UpdateWarlockMenu(init_shortcut_invoked);
}

int UpdateShamanMenuLive(unsigned int init_shortcut_invoked) {
  return UpdateShamanMenu(init_shortcut_invoked);
}

int UpdateSorcererMenuLive(unsigned int init_shortcut_invoked) {
  return UpdateSorcererMenu(init_shortcut_invoked);
}

int UpdateEnchanterMenuLive(unsigned int init_shortcut_invoked) {
  return UpdateEnchanterMenu(init_shortcut_invoked);
}

}  // namespace VS::Menu::Magic
