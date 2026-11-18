#pragma once
// Hand-maintained (tools/port/owned.csv).
// The naming screen's string table in the shape the module data loader fills.
// RenameLive.cpp defines it and hands the typed screen (Rename.hpp) a view of
// it.

#include "Legacy/Psx.h"

namespace VS::Menu::Rename {

// decomp: _renderRenameMenuStrings (MENU/MENU8.PRG 21A0.c): halfword offsets,
// the strings, then the character grid; ModuleData.cpp fills it from the disc
// string table.
inline constexpr int kMenuStringCount = 208;
extern u_short renderRenameMenuStrings[kMenuStringCount];

}  // namespace VS::Menu::Rename
