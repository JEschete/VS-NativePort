#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstdint>

// Binds the options screen (Options.hpp) to the menu framework,
// vs_main_settings, the state flags, the sound output and the vibration, in
// the shape the framework's submenu dispatch calls.
namespace VS::Menu::Options {
// MENU8.PRG vs_menu8_exec (88.c:551) over the live options State.
int UpdateLiveOptions(std::uint8_t& step);
// Restores the MENU8.PRG options statics a fresh load of the overlay brings
// back.
void ResetLiveOptionsStatics();
}  // namespace VS::Menu::Options
