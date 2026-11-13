#pragma once

namespace VS::Menu::Inventory {
int UpdateLiveWeaponDetails(int item_info);
int UpdateLiveBladeDetails(int item_info);
int UpdateLiveGripDetails(int item_info);
int UpdateLiveShieldDetails(int item_info);
// Explicit cold-session/test reset, not an ordinary overlay-reload hook.
void ResetLiveEquipmentDetailControllers();
}  // namespace VS::Menu::Inventory
