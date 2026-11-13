#pragma once

namespace VS::Menu::Inventory {
void DrawLiveItemCategoryIcons(int animation_step, int highlight_selected);
int UpdateLiveArmorDetails(int item_info);
int UpdateLiveGemDetails(int item_info);
int UpdateLiveItemDetails(int item_info);
// Explicit cold-session/test reset only; ordinary overlay loads retain BSS.
void ResetLiveItemDetailDispatch();
}  // namespace VS::Menu::Inventory
