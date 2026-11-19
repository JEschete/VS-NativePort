#pragma once
#include "Items/ItemRecords.hpp"

namespace VS::Menu::Status {  // NOLINT(readability-identifier-naming)
// Borrowed MAINMENU item text and BATTLE arguments, string buffer and rows.
// These entries own no allocations or statics: neither MENU4 reload nor cold
// reset needs a new reset hook, and neither may release another module's
// storage.
void FormatLiveBladeDescription(VS::Items::EquipmentPart* blade,
                                char* menu_text[], int* row_types);
void FormatLiveGripDescription(VS::Items::EquipmentPart* grip,
                               char* menu_text[], int* row_types);
void CopyLiveGemDescription(VS::Items::EquipmentPart* gem, char* menu_text[],
                            int* row_types);
char* SelectLiveWeaponComponent(int row, VS::Items::WeaponLoadout* weapon);
char* SelectLiveShieldComponent(int row, VS::Items::ShieldLoadout* shield);
char* LiveArmorDescription(VS::Items::ArmorLoadout* armor);
char* LiveAccessoryDescription(VS::Items::AccessorySpawnLoadout* accessory);
int IsLiveDetailElementSelectable(int element, int flags);
}  // namespace VS::Menu::Status
