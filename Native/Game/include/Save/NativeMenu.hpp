#pragma once

#include <filesystem>

struct Items_ContainerData;

namespace VS::Game::Save {

void ConfigureDirectory(std::filesystem::path root);
void ResetFileMenuSession();
void ShutdownFileMenus();

// Runs one frame of the container-file picker; init begins it. Returns 0
// while open, 1 once a file is selected and -1 when cancelled.
int SelectContainer(int init);
// 1 when native saves exist (or cannot be read), so TITLE focuses Continue.
int InitialTitleSelection();
// Draws the active native file screen when it belongs to the title (true) or
// the in-game (false) menu.
void DrawFileOverlay(bool title);
// The selected file's container storage; aborts without a selection.
Items_ContainerData* ContainerData();
void ReleaseContainer();
// TITLE calls only while VS::Boot::isClearSavePending still identifies the
// ending. Reset/return to title/shutdown release the owned staging copy.
void StageClearGame();

}  // namespace VS::Game::Save
