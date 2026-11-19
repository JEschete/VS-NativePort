#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstdint>

#include "Menu/BreakArts.hpp"

namespace VS::Menu::break_arts {
// decomp: _strings (_strings@src/MENU/MENU1.PRG/30.c): MENU1.PRG's string
// table, halfword offsets then the encoded strings; ModuleData.cpp loads it
// from the module image.
extern std::uint16_t strings[kMenuStringHalfwords];

void SetHpCostLive(int action_id);
void DrawNextUnlockPointsLive(int x_offset, int weapon_category, int row);
int UpdateArtListLive(int type_cursor_memory);
int UpdateWeaponCategoriesLive(int initialize);
void CreateHeaderLive();
int UpdateLive(std::uint8_t* step);
void ResetLiveStatics();
}  // namespace VS::Menu::break_arts
