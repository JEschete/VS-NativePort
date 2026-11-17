#pragma once

#include <cstdint>

// NOLINTNEXTLINE(readability-identifier-naming): established subsystem API.
namespace VS::Menu::Magic {

int UpdateWarlockMenuLive(unsigned int init_shortcut_invoked);
int UpdateShamanMenuLive(unsigned int init_shortcut_invoked);
int UpdateSorcererMenuLive(unsigned int init_shortcut_invoked);
int UpdateEnchanterMenuLive(unsigned int init_shortcut_invoked);

// Restores the complete school owner, including lists and rank blink, on a
// cold start or MENU0 reload. Does not reset framework cursor memories.
void ResetMagicMenusLiveState();
[[nodiscard]] std::uint64_t RefusedMagicMenuCount();

}  // namespace VS::Menu::Magic
