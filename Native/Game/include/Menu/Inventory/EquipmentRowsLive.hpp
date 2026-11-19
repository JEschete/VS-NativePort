#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstdint>

#include "Legacy/Psx.h"

// Bindings from the equipment choice list and preview model reload
// (EquipmentRows.hpp) to MENU3.PRG's list globals (Inventory.h), the game
// heap, the menu rows, the controller, the equipment stat panel, the preview
// actor and the asset queue, keeping the original call shapes. A row count
// outside the list's capacity, a list text outside a live game heap payload,
// a cursor or scroll position outside the list, an accessory outside the
// bound armor table and a missing preview actor or Ashley model slot are
// refused with a throttled warning.
namespace VS::Menu::Inventory {
// decomp: MENU3.PRG _populateMenuRows (16C.c:39). menu_text holds a name and
// a description string (or NULL) per row.
void LiveCreateEquipmentRows(int count, u_short** menu_text, int* row_types);
// decomp: MENU3.PRG _getSelectedItemRow (16C.c:101).
int LiveSelectedEquipmentRow();
// decomp: MENU3.PRG _processEquipSubMenu (16C.c:111).
void UpdateLiveEquipmentChoices(int has_sfx);
// decomp: MENU3.PRG _populateUiAccessory (16C.c:311).
void SetLiveAccessoryPreviewStats(int accessory_id);
// decomp: MENU3.PRG _loadObjectData (68C4.c:12). A refusal returns 0, as a
// finished reload does, so the caller stops waiting.
int ReloadLiveEquipmentModels(int category, int wep_id, int material);
// decomp: MENU3.PRG func_80109320 (68C4.c:87). Returns 1 for a two-handed
// weapon category.
int LiveIsTwoHandedWeapon(int category);
// decomp: MENU3.PRG _populateDataSlot (68C4.c:100).
void QueueLiveEquipmentModel(int wep_id, int material);
// MENU3.PRG reloads restore _processEquipSubMenu's cursor static
// (VS::Boot::ResetMenu3Statics).
void ResetEquipmentRowsLive();
// Refusals logged since start-up.
[[nodiscard]] std::uint64_t LiveEquipmentRowsRefusalCount();
}  // namespace VS::Menu::Inventory
