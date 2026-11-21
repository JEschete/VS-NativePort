#include "Items/ContainerGemCountsLive.hpp"

#include "Items/ContainerGemCounts.hpp"
#include "Items/ContainerIndexingLive.hpp"
#include "Menu/Container/ItemView.hpp"

namespace VS::Items {
// decomp: getWeaponsubItemCount (src/MENU/MENUD.PRG/234.c:1960; was
// _getWeaponsubItemCount)
int CountLiveContainerWeaponGems(int weapon_slot) {
  auto* const owner = VS::Menu::container::containerData;
  LiveContainerWorld source(owner == nullptr ? nullptr : &owner->items);
  return CountContainerWeaponGems(weapon_slot, source);
}

// decomp: getShieldsubItemCount (src/MENU/MENUD.PRG/234.c:1972; was
// _getShieldsubItemCount)
int CountLiveContainerShieldGems(int shield_slot) {
  auto* const owner = VS::Menu::container::containerData;
  LiveContainerWorld source(owner == nullptr ? nullptr : &owner->items);
  return CountContainerShieldGems(shield_slot, source);
}
}  // namespace VS::Items
