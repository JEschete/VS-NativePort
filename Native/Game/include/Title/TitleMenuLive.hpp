#pragma once
// Hand-maintained (tools/port/owned.csv).

#include "Title/TitleMenu.hpp"

// Binds the title (TitleMenu.hpp) to the game: the pad and player settings,
// the intro movie, VRAM and the display environments, the low end of the
// heap, the menu sounds, the TITLE.PRG module images, the file screens
// (Menu/FilesLive.hpp, Save/FileMenuLive.hpp) and the state a new game
// starts from.
namespace VS::Title {
// TITLE.PRG vs_title_exec (22C.c:4491) over the live game: the selection.
int RunLive();
// TITLE.PRG _initGameData (22C.c:4698) over the live game state.
void InitializeLiveNewGameData();
// TITLE.PRG _setTitleExitFlags (22C.c:4741) over the live state flags and
// entry door.
void PrepareLiveExit(int cause);

// The live menu items, for the title's tests.
[[nodiscard]] State& LiveTitleState();
// The live host (stateless between calls), for the title's tests.
[[nodiscard]] Host& LiveTitleHost();
}  // namespace VS::Title
