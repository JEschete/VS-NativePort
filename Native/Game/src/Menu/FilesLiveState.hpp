#pragma once
// Hand-maintained (tools/port/owned.csv).
// The file screens' storage other systems share: the container string table
// the module data loader fills, the memory-card text, the unsaved-data flag,
// the transfer counter, the select cursor and the title screen's buffer.
// FilesLive.cpp defines them and hands the typed screens (Files.hpp) a view of
// them.

#include "Legacy/Psx.h"

namespace VS::Menu::Files {
// decomp: _containerStrings (MENU/MENU7.PRG 260.c): halfword offsets, then
// the strings; ModuleData.cpp fills it from the disc string table.
inline constexpr int kContainerStringCount = 895;
extern u_short containerStrings[kContainerStringCount];
// decomp: _dataNotSaved (MENU/MENU7.PRG 260.c)
extern char dataNotSaved;
// decomp: _fileProgressCounter (MENU/MENU7.PRG 260.c)
extern int fileProgressCounter;
// decomp: _memoryCardMessage (MENU/MENU7.PRG 260.c): the text table's
// message, the native file menu's footer, or null.
extern char* memoryCardMessage;
// decomp: _selectCursorXy (MENU/MENU7.PRG 260.c)
extern int selectCursorXy;
// decomp: _textTable (MENU/MENU7.PRG 260.c): MCMAN.BIN's text once the menu
// resources load.
extern u_short* textTable;
// decomp: _frameBuf (TITLE/TITLE.PRG 22C.c)
extern char frameBuf;
}  // namespace VS::Menu::Files
