#pragma once
// Hand-maintained (tools/port/owned.csv).

struct Items_ArmorLoadout;
struct Items_InventoryArmor;
struct Items_InventoryBlade;
struct Items_InventoryGem;
struct Items_InventoryGrip;
struct Items_InventoryMisc;
struct Items_InventoryShield;
struct Items_InventoryWeapon;
struct Items_ShieldLoadout;
struct Items_WeaponLoadout;
struct Menu_Framework_MenuRow;

// Binds the item rows (ItemRows.hpp) to their texts (ItemRowsLiveState.h),
// the inventory and container tables the menus show (vs_mainMenu_weapons ..
// vs_mainMenu_gems), the battle string context, vs_battle_stringBuf, the menu
// rows and the disc queue, in the shapes the copied menus call. A null record
// or text array, a reference outside the bound table and a table that is
// neither the inventory's nor the container's are refused and reported.
namespace VS::Menu::Framework {
struct TextRef;
// Reuses the item formatters' resolver for caller-owned row buffers and names.
[[nodiscard]] char* ResolveLiveItemRowText(TextRef text, void* buffer,
                                           char* weapon_name);
// MAINMENU.PRG (2D10.c). The description buffer is
// kRowDescriptionBufferBytes long.
Menu_Framework_MenuRow* CreateLiveWeaponComponentRow(int row, int weapon_index,
                                                     int slide);
Menu_Framework_MenuRow* CreateLiveShieldGemRow(int row, int shield_index,
                                               int slide);
void FormatLiveWeaponRow(Items_WeaponLoadout* weapon, char* menu_text[],
                         int* row_type, void* buffer);
void FormatLiveInventoryWeaponRow(Items_InventoryWeapon* weapon,
                                  char* menu_text[], int* row_type,
                                  void* buffer);
void FormatLiveBladeRow(Items_InventoryBlade* blade, char* menu_text[],
                        int* row_type, void* buffer);
void FormatLiveGripRow(Items_InventoryGrip* grip, char* menu_text[],
                       int* row_type, void* buffer);
void FormatLiveShieldRow(Items_ShieldLoadout* shield, char* menu_text[],
                         int* row_type, void* buffer);
void FormatLiveInventoryShieldRow(Items_InventoryShield* shield,
                                  char* menu_text[], int* row_type,
                                  void* buffer);
void FormatLiveArmorRow(Items_ArmorLoadout* armor, char* menu_text[],
                        int* row_type, void* buffer);
void FormatLiveInventoryArmorRow(Items_InventoryArmor* armor, char* menu_text[],
                                 int* row_type, void* buffer);
void FormatLiveGemRow(Items_InventoryGem* gem, char* menu_text[], int* row_type,
                      void* buffer);
void FormatLiveMiscItemRow(Items_InventoryMisc* misc, char* menu_text[],
                           int* row_type, void* buffer);
int LoadLiveItemText(int load);
int UnloadLiveItemTextWhenReady();

// MAINMENU.PRG reloads restore the module image values: no text block (the
// block itself is not freed) and the hand table.
void ResetLiveItemRowsStatics();
}  // namespace VS::Menu::Framework
