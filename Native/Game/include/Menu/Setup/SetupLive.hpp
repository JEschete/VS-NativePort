#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstdint>
#include <functional>

#include "Menu/Setup/SetupScreen.hpp"

// Binds the workshop menu (SetupScreen.hpp, Assembly.hpp, Combination.hpp) to
// the menu framework, the frame's ordering tables, the pad, Ashley's
// inventory and equipment, the item texts, the battle string context, the
// disc queue and the game heap, in the shape the framework's submenu dispatch
// calls. The inventory and its display order are read once a call and
// written back before each legacy call that reads them and when the call
// returns. Refusals are logged on the "menu.setup" channel.
namespace VS::Menu::Setup {
// MENUC.PRG vs_menuC_exec (168.c:4550) over the live State.
int UpdateLiveSetup(std::uint8_t& step);
// Runs `call` over the live State and host, as UpdateLiveSetup runs the
// menu (tests and tools).
int RunLiveSetup(const std::function<int(Context&)>& call);
// The live State, for the workshop's tests.
[[nodiscard]] State& LiveSetupState();
// The recipe file the live host decoded; null before one was read or after
// it was freed.
[[nodiscard]] const RecipeBook* LiveSetupRecipes();
// Restores the MENUC.PRG statics a fresh load of the overlay brings back:
// the cursor and blink animations and the staged gems. The recipe block the
// reload dropped is freed (the original lost it), unless its read is still
// in flight.
void ResetLiveSetupStatics();
}  // namespace VS::Menu::Setup
