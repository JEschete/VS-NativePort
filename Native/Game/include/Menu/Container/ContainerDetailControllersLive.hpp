#pragma once

namespace VS::Menu::container {
void DrawLiveCategoryIcons(int animation_step, int details_phase);
[[nodiscard]] int UpdateLiveWeaponDetails(int selection);
[[nodiscard]] int UpdateLiveBladeDetails(int selection);
[[nodiscard]] int UpdateLiveGripDetails(int selection);
[[nodiscard]] int UpdateLiveShieldDetails(int selection);
[[nodiscard]] int UpdateLiveArmorDetails(int selection);
[[nodiscard]] int UpdateLiveGemDetails(int selection);
[[nodiscard]] int UpdateLiveItemDetails(int selection);

// Cold session/test initialization only. Ordinary MENUD reload retains BSS.
void ResetLiveContainerDetailControllers();
}  // namespace VS::Menu::container
