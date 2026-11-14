#pragma once
// Hand-maintained (tools/port/owned.csv).

#include "Legacy/Psx.h"

namespace VS::Menu::break_arts {
void SetHpCostLive(int action_id);
void DrawNextUnlockPointsLive(int x_offset, int weapon_category, int row);
int UpdateArtListLive(int type_cursor_memory);
int UpdateWeaponCategoriesLive(int initialize);
void CreateHeaderLive();
int UpdateLive(u_char* step);
void ResetLiveStatics();
}  // namespace VS::Menu::break_arts
