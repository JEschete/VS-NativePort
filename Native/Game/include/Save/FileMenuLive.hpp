#pragma once
// Hand-maintained (tools/port/owned.csv).

// Binding from the MENU7 data screen's and TITLE screen's menu globals, the
// controller and the native save files to the typed file pickers
// (FileMenu.cpp). Each runs one frame over the pressed (Circle/Cross) and
// repeated (Up/Down) buttons and returns the original tri-state: 0 while
// running, 1 once finished and -1 when left. Boot transitions remain in the
// original callers after these frames return.
#include <span>

namespace VS::Menu {
int UpdateLiveDataSaveMenu(int init_state);
int UpdateLiveDataLoadMenu(int init_fadeout);
int UpdateLiveLoadMenu(int init_fadeout);
int UpdateLiveSaveMenu(int init_state);
// The footer text the pickers point the memory-card message at, with its
// terminator.
[[nodiscard]] std::span<const char> LiveFileMenuFooter();
}  // namespace VS::Menu
