#pragma once
// Hand-maintained (tools/port/owned.csv).
// The file screens' storage other systems share: the container string table
// the module data loader fills, the memory-card text, the unsaved-data flag,
// the transfer counter, the select cursor and the title screen's buffer.
// FilesLive.cpp defines them and hands the typed screens (Files.hpp) a view of
// them.

#include "Legacy/Psx.h"

#ifdef __cplusplus
extern "C" {
#endif

// decomp: _containerStrings (MENU/MENU7.PRG 260.c): halfword offsets, then
// the strings; ModuleData.cpp fills it from the disc string table.
enum { kMenuFilesContainerStringCount = 895 };
extern u_short Menu_Files_containerStrings[kMenuFilesContainerStringCount];
// decomp: _dataNotSaved (MENU/MENU7.PRG 260.c)
extern char Menu_Files_dataNotSaved;
// decomp: _fileProgressCounter (MENU/MENU7.PRG 260.c)
extern int Menu_Files_fileProgressCounter;
// decomp: _memoryCardMessage (MENU/MENU7.PRG 260.c): the text table's
// message, the native file menu's footer, or null.
extern char* Menu_Files_memoryCardMessage;
// decomp: _selectCursorXy (MENU/MENU7.PRG 260.c)
extern int Menu_Files_selectCursorXy;
// decomp: _textTable (MENU/MENU7.PRG 260.c): MCMAN.BIN's text once the menu
// resources load.
extern u_short* Menu_Files_textTable;
// decomp: _frameBuf (TITLE/TITLE.PRG 22C.c)
extern char Menu_Files_frameBuf;

#ifdef __cplusplus
}
#endif
