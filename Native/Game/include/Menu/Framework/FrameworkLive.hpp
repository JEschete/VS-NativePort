#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstdint>
#include <span>

#include "Legacy/Psx.h"

extern "C" {
#include "Menu/Framework/ActionSelection.h"
}

struct Menu_Framework_MenuRow;
struct Items_LootListNode;
struct Items_BattleLoot;
struct Items_WeaponLoadout;
struct Items_ShieldLoadout;
struct Items_InventoryMisc;
struct Items_EquipmentPartStats;

// Binds the menu framework (Framework.hpp) to its variables
// (FrameworkLiveState.h), the frame ordering tables, the disc queue, the text
// boxes, the battle interface and the submenus, in the shapes the copied
// menus and the battle flow call. Each entry runs the typed function over the
// live State and writes the State back before it returns.
namespace VS::Menu::Framework {
struct MenuRow;
struct RowPlacement;

// BATTLE.PRG (5BF94.c).
int LoadLiveMenuOverlay(int packed_menu_state);
Menu_Framework_MenuRow* LiveMenuItem(int row);
Menu_Framework_MenuRow* SetLiveMenuItem(int row, int x, int y, int w,
                                        int background_width, char* text);
// SetLiveMenuItem for typed hosts: the row builder trusts its text for
// kTrustedEncodedTextBytes, so it reads a terminator-padded copy of `text`.
MenuRow& SetLiveMenuItemText(const RowPlacement& placement,
                             std::span<const std::uint8_t> text);
// LiveMenuItem for typed hosts.
MenuRow& LiveMenuRow(int row);
int LiveGradientStopColor(u_int gradient, u_int color_index, int brightness);
void DrawLiveMenuItem(Menu_Framework_MenuRow* row);
int UpdateLiveMainMenuSelection(int mode);
void UpdateLiveMenuOverlay();
void InitLiveGameInterface(void* interface_texture);
void ResumeLiveAfterActionSelection();
int LiveActionRestrictionFlags(int mode);
int OpenLiveGameMenu(int menu_state, int mode);
int OpenLiveShortcutMenu(int shortcut);
void ReleaseLiveMenuRows();
void OpenLiveLootListMenu(Items_LootListNode* lists);
void OpenLiveLootEntryMenu(Items_BattleLoot* entry);
void OpenLiveChestStorageMenu(int location);
void OpenLiveAbilityUnlockMenu(int art, int ability);
void OpenLiveChallengeMenu();
Menu_Framework_ActionSelection* LiveActionSelectionState();
int DrawLiveMainMenuCursor(int step, int position);

// MAINMENU.PRG (58EC.c, C48.c).
void CreateLiveActionRows(int row_count, int sub_menu_id, char* menu_text[],
                          int row_types[]);
int LiveHighlightedActionRow();
int LiveActionSelectionResult();
Menu_Framework_MenuRow* CreateLiveActionRow(int row, int x);
void UpdateLiveActionMenu();
void UpdateLiveInformationVisibility(int source_row, int action_menu_state);
void SetLiveCommandLabel(int action);
void DrawLiveCommandLabel();
void SetLiveBackgroundFade(int fade_state);
void DeactivateLiveRow(int row);
void DismissLiveInformationBox();
void InitializeLiveInformationBox();
void SetLiveInformationMessage(char* message);
// A null `before` links into the overlay ordering table.
void DrawLiveButtonIcon(int button_id, int x, int y, u_long* before);
int DrawLiveCursor(u_int step, int position);
int DrawLiveColoredInteger(int value, int xy, int color, u_long* before);
int DrawLiveComparedInteger(int value, int xy, int threshold, u_long* before);
void DrawLiveNeutralInteger(int value, int xy, u_long* before);
void DrawLiveFeatheredRowIcon(int direction, int xy, int uv_clut,
                              u_long* before);
void DrawLiveMaterialIndicator(int material, int xy);
void DrawLiveEquipmentStateIcon(int icon, int xy);
void DrawLiveRowIcon(int icon, int x, int y);
void SetLiveActionCostText(int index, char const* text, int x_offset,
                           int disabled);
void DrawLiveActionCostPanel();
void DrawLiveButtonPromptBackground(int x, int y, int w, int h);
void SetLiveBackgroundPriority(int ot_offset, int brightness);
void LoadLiveBackgroundTexture(u_int* buffer);
void CreateLiveMenuRows(int row_count, int row_info, char* strings[],
                        int row_types[]);
int CloseLiveRowsAndRememberCursor();
int LiveHighlightedRow();
int LiveSelectedRow();
Menu_Framework_MenuRow* CreateLiveListRow(int row, int x);
void UpdateLiveRowSelection();
void DrawLiveBackground(int brightness);
int DrawLiveFeatheredRowCharacter(u_int character, int x,
                                  Menu_Framework_MenuRow* row, u_long* before);
void DrawLiveTabNavigation(int mode);
void DrawLiveScrollableRow(Menu_Framework_MenuRow* row);
void UpdateAndDrawLiveRows();
void UpdateLiveScreen();
int LiveNextFreeActorAssetRequest();
void FlyOutLiveRowLeft(int row);
void FlyOutLiveRowsLeftExcept(int row);
void FlyOutLiveRowRight(int row);
void ClearLiveMenuExcept(int row);
void FlyOutLiveRowsAndHoistSelection(int row, int offset);
int AreLiveMenuRowsIdle();
int LiveQuitDelay();
int ReturnLiveFromSubmenu(int packed_menu_state);
void UpdateLiveMainMenu(int packed_menu_state);

// The MENU*.PRG copies.
void RenderLiveAshley(int mode);
void ContinueLiveItemDetailsTransition(int row);
void UpdateLiveItemDetailsSelection(int row, char* text[], u_int row_type,
                                    int page);
int LiveWeaponSortValue(int stat, Items_WeaponLoadout* weapon);
int LiveShieldSortValue(int stat, Items_ShieldLoadout* shield);
int LiveMiscSortValue(int stat, Items_InventoryMisc* misc);
int LiveEquipmentSortValue(int stat, Items_EquipmentPartStats* equipment);
void SetLiveMenuVisible(int display);
void ClearLiveMenuExceptTitle();
void BuildLiveYawMatrix(MATRIX* matrix, std::int16_t angle);
void BuildLivePitchMatrix(MATRIX* matrix, std::int16_t angle);
void ShowLiveSecondaryTitle(char* text);

// Restores the MAINMENU.PRG statics a fresh load of the overlay brings back
// (the module image values) and drops both item lists' pointers.
void ResetLiveMainMenuStatics();
}  // namespace VS::Menu::Framework
