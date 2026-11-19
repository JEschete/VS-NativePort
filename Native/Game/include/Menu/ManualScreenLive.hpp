#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstdint>

#include "Legacy/Psx.h"

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

// decomp: MENUE.PRG func_80102E9C (494.c:132).
void LiveClearMenuExceptRow8();
// decomp: MENUE.PRG _menuReady (494.c:134).
void LivePollMenuReady();
// decomp: MENUE.PRG vs_menuE_exec (494.c:142).
int LiveUpdateScreen(u_char* state);
// decomp: MENUE.PRG _initMenuState (494.c:219).
int LiveInitHelpState();
// decomp: MENUE.PRG _showMenu (494.c:245).
int LiveUpdateHelpPages();

// Call where MENUE.PRG loads (VS::Boot::ResetMenueStatics): its reload zeroes
// vs_menuE_exec's statics.
void ResetLiveManualScreenStatics();
// The bound screen state, for the reset and tests.
[[nodiscard]] ScreenState& LiveManualScreenState() noexcept;
// Inputs the binding refused since start-up.
[[nodiscard]] std::uint64_t LiveManualScreenRefusalCount() noexcept;
}  // namespace VS::Menu::manual
