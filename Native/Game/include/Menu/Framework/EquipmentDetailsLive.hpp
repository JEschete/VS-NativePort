#pragma once
// Hand-maintained (tools/port/owned.csv).

// Binds the equipment details (EquipmentDetails.hpp) to the variables the
// copied menus share (EquipmentDetailsLiveState.h), the item tables and texts
// (ItemRowsLive.hpp), Ashley's equipment, the battle actors, the controller,
// the menu rows and the frame's ordering tables, in the shapes the copied
// menus call. Each entry loads the shared variables into the typed state,
// runs the typed function and stores them back. A reference outside the bound
// table, a missing actor or loadout and a draw outside the frame's table are
// refused and reported.
namespace VS::Menu::Framework {
// MAINMENU.PRG 2D10.c: the stats of the 1-based item of each table.
void ResetLiveEquipmentStats();
void SetLiveWeaponStats(int weapon_index);
void SetLiveBladeStats(int blade_index);
void SetLiveGripStats(int grip_index);
void SetLiveShieldStats(int shield_index);
void SetLiveArmorStats(int armor_index);
void SetLiveGemStats(int gem_index);
void ResetLiveItemDetailCursor();
void UpdateLiveItemDetails();

// MAINMENU.PRG C48.c.
void DrawLiveEquipmentStatPage(int x_offset);
void SetLiveEnabledStatPages(int enabled);
void UpdateLiveEquipmentStatPages(int command);
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
void SetLiveRangeAndRisk(int range, int risk, int unused, int shown);
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
void SetLivePrimaryStats(int strength, int intelligence, int agility,
                         int known);
void SetLiveActorComparisonStats(int actor);
void UpdateLivePrimaryStatPanel(int animate);
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
void SetLiveDpPp(int current_dp, int max_dp, int current_pp, int max_pp);
void UpdateLiveDpPpBars(int flags);

// MAINMENU.PRG reloads restore the module image values of the screen's
// statics (the DP/PP bars, the cursor and the panels' slides); the shared
// variables are not reset.
void ResetLiveEquipmentDetailsStatics();
}  // namespace VS::Menu::Framework
