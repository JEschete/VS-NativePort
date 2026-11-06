#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstdint>

#include "Menu/Rename.hpp"

// Binds the weapon naming screen (Rename.hpp) to the menu framework, the
// frame's ordering tables, the pad, vs_main_inventory, the item names and the
// battle string context, in the shape the framework's submenu dispatch calls.
namespace VS::Menu::Rename {
// MENU8.PRG vs_menu8_execRename (21A0.c:820) over the live State.
int UpdateLiveRename(std::uint8_t& step);
// MENU8.PRG _renderRenameMenu (21A0.c:133) over the live State.
void DrawLiveFrame(int x_offset);
// MENU8.PRG _highlightSelection (21A0.c:318).
void DrawLiveSelectionBox(const Box& box);
// The live State, for the frame's tests.
[[nodiscard]] State& LiveRenameState();
// Restores the MENU8.PRG naming statics a fresh load of the overlay brings
// back.
void ResetLiveRenameStatics();
}  // namespace VS::Menu::Rename
