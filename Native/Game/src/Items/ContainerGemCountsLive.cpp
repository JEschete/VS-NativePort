#include "Items/ContainerGemCountsLive.hpp"

#include "Items/ContainerGemCounts.hpp"
#include "Items/ContainerIndexingLive.hpp"
#include "Menu/Container/ItemView.hpp"

namespace VS::Items {
// decomp: _getWeaponsubItemCount (src/MENU/MENUD.PRG/234.c:1959)
int CountLiveContainerWeaponGems(int weapon_slot) {
  auto* const owner = VS::Menu::container::containerData;
  LiveContainerWorld source(owner == nullptr ? nullptr : &owner->items);
  return CountContainerWeaponGems(weapon_slot, source);
}

// decomp: _getShieldsubItemCount (src/MENU/MENUD.PRG/234.c:1971)
int CountLiveContainerShieldGems(int shield_slot) {
  auto* const owner = VS::Menu::container::containerData;
  LiveContainerWorld source(owner == nullptr ? nullptr : &owner->items);
  return CountContainerShieldGems(shield_slot, source);
}
}  // namespace VS::Items
