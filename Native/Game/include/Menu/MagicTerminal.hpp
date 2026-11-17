#pragma once

#include <cstdint>
#include <optional>

#include "Menu/MagicMenus.hpp"

// NOLINTNEXTLINE(readability-identifier-naming): established subsystem API.
namespace VS::Menu::Magic {
// decomp: vs_menu0_exec (src/MENU/MENU0.PRG/15AC.c:166).
// The caller owns the state byte. Nested menu writes remain visible.
std::optional<int> Update(std::uint8_t& state, MenuHost& host);
}  // namespace VS::Menu::Magic
