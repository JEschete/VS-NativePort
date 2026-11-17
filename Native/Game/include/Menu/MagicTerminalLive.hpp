#pragma once

#include <cstdint>

// NOLINTNEXTLINE(readability-identifier-naming): established subsystem API.
namespace VS::Menu::Magic {
// decomp: vs_menu0_exec (src/MENU/MENU0.PRG/15AC.c:166).
int UpdateLive(std::uint8_t* state);
}  // namespace VS::Menu::Magic
