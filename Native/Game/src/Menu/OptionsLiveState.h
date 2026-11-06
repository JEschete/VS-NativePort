#pragma once
// Hand-maintained (tools/port/owned.csv).
// The options screen's string table in the shape the module data loader fills.
// OptionsLive.cpp defines it and hands the typed screen (Options.hpp) a view
// of it.

#include "Legacy/Psx.h"

#ifdef __cplusplus
extern "C" {
#endif

// decomp: _menuStrings (MENU/MENU8.PRG 88.c): halfword offsets, then the
// strings; ModuleData.cpp fills it from the disc string table.
enum { kMenuOptionsMenuStringCount = 1070 };
extern u_short Menu_Options_menuStrings[kMenuOptionsMenuStringCount];

#ifdef __cplusplus
}
#endif
