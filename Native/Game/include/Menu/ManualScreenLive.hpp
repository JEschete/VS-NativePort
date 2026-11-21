#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstdint>

#include "Legacy/Psx.hpp"

// Bindings from the manual screen and help page cores (ManualScreen.hpp) to
// MENUE.PRG's statics (Menu/Manual.hpp), the menu framework, the HUD, the game
// heap, the disc queue, the display state, the buttons and the frame's
// ordering table, keeping the original call shapes. The help page statics
// are published before every call out, as the original's stores precede its
// calls, since the page, topic and illustration bindings read them. A null
// screen step, a help request or asset buffer that is missing or outside the
// game heap, an upload past its asset buffer and a missing ordering-table
// entry are refused with a throttled warning on the "menu.manual" channel.
namespace VS::Menu::manual {
struct ScreenState;

// decomp: MENUE.PRG func_80102E9C (src/MENU/MENUE.PRG/494.c:138).
void LiveClearMenuExceptRow8();
// decomp: MENUE.PRG menuReady (src/MENU/MENUE.PRG/494.c:140; was _menuReady).
void LivePollMenuReady();
// decomp: MENUE.PRG vs_menuE_exec (src/MENU/MENUE.PRG/494.c:148).
int LiveUpdateScreen(std::uint8_t* state);
// decomp: MENUE.PRG initMenuState (src/MENU/MENUE.PRG/494.c:225; was
// _initMenuState).
int LiveInitHelpState();
// decomp: MENUE.PRG showMenu (src/MENU/MENUE.PRG/494.c:251; was _showMenu).
int LiveUpdateHelpPages();

// Call where MENUE.PRG loads (VS::Boot::ResetMenueStatics): its reload zeroes
// vs_menuE_exec's statics.
void ResetLiveManualScreenStatics();
// The bound screen state, for the reset and tests.
[[nodiscard]] ScreenState& LiveManualScreenState() noexcept;
// Inputs the binding refused since start-up.
[[nodiscard]] std::uint64_t LiveManualScreenRefusalCount() noexcept;
}  // namespace VS::Menu::manual
