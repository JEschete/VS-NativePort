#pragma once
// Hand-maintained (tools/port/owned.csv).
// The options screen's string table in the shape the module data loader fills.
// OptionsLive.cpp defines it and hands the typed screen (Options.hpp) a view
// of it.

#include "Legacy/Psx.h"

namespace VS::Menu::Options {

// decomp: _menuStrings (MENU/MENU8.PRG 88.c): halfword offsets, then the
// strings; ModuleData.cpp fills it from the disc string table.
inline constexpr int kMenuStringCount = 1070;
extern u_short menuStrings[kMenuStringCount];

}  // namespace VS::Menu::Options
