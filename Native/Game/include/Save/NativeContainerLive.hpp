#pragma once
// Hand-maintained (tools/port/owned.csv).

namespace VS::Menu::Files {

// Runs the container save submenu over the live menu framework, inventory and
// native save picker. The state is the framework's submenu byte; returns 1
// once the submenu has finished and reset it.
int UpdateLiveContainerSave(char& state);

}  // namespace VS::Menu::Files
