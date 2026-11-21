#pragma once
// Hand-maintained (tools/port/owned.csv).
// The options screen's string table in the shape the module data loader fills.
// OptionsLive.cpp defines it and hands the typed screen (Options.hpp) a view
// of it.

#include "Legacy/Psx.hpp"

namespace VS::Menu::Options {

// decomp: menuStringTable (MENU/MENU8.PRG src/MENU/MENU8.PRG/88.c:12; was
// _menuStrings): halfword offsets, then the strings; ModuleData.cpp fills it
// from the disc string table.
inline constexpr int kMenuStringCount = 1070;
extern std::uint16_t menuStrings[kMenuStringCount];

}  // namespace VS::Menu::Options
