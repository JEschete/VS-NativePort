#pragma once
#include "Items/ItemRecords.hpp"
// Hand-maintained (tools/port/owned.csv).

// Binds the item rows (ItemRows.hpp) to their texts (ItemRowsLiveState.hpp),
// the inventory and container tables the menus show (vs_mainMenu_weapons ..
// vs_mainMenu_gems), the battle string context, vs_battle_stringBuf, the menu
// rows and the disc queue, in the shapes the copied menus call. A null record
// or text array, a reference outside the bound table and a table that is
// neither the inventory's nor the container's are refused and reported.
namespace VS::Menu::Framework {
struct MenuRow;
struct TextRef;
// Reuses the item formatters' resolver for caller-owned row buffers and names.
[[nodiscard]] char* ResolveLiveItemRowText(TextRef text, void* buffer,
                                           char* weapon_name);
// MAINMENU.PRG (2D10.c). The description buffer is
// kRowDescriptionBufferBytes long.
MenuRow* CreateLiveWeaponComponentRow(int row, int weapon_index, int slide);
MenuRow* CreateLiveShieldGemRow(int row, int shield_index, int slide);
void FormatLiveWeaponRow(VS::Items::WeaponLoadout* weapon, char* menu_text[],
                         int* row_type, void* buffer);
void FormatLiveInventoryWeaponRow(VS::Items::InventoryWeapon* weapon,
                                  char* menu_text[], int* row_type,
                                  void* buffer);
void FormatLiveBladeRow(VS::Items::InventoryBlade* blade, char* menu_text[],
                        int* row_type, void* buffer);
void FormatLiveGripRow(VS::Items::InventoryGrip* grip, char* menu_text[],
                       int* row_type, void* buffer);
void FormatLiveShieldRow(VS::Items::ShieldLoadout* shield, char* menu_text[],
                         int* row_type, void* buffer);
void FormatLiveInventoryShieldRow(VS::Items::InventoryShield* shield,
                                  char* menu_text[], int* row_type,
                                  void* buffer);
void FormatLiveArmorRow(VS::Items::ArmorLoadout* armor, char* menu_text[],
                        int* row_type, void* buffer);
void FormatLiveInventoryArmorRow(VS::Items::InventoryArmor* armor,
                                 char* menu_text[], int* row_type,
                                 void* buffer);
void FormatLiveGemRow(VS::Items::InventoryGem* gem, char* menu_text[],
                      int* row_type, void* buffer);
void FormatLiveMiscItemRow(VS::Items::InventoryMisc* misc, char* menu_text[],
                           int* row_type, void* buffer);
int LoadLiveItemText(int load);
int UnloadLiveItemTextWhenReady();

// MAINMENU.PRG reloads restore the module image values: no text block (the
// block itself is not freed) and the hand table.
void ResetLiveItemRowsStatics();
}  // namespace VS::Menu::Framework
