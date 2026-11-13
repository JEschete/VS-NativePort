#include "Items/ContainerGemCountsLive.hpp"

#include "Items/ContainerGemCounts.hpp"
#include "Items/ContainerIndexingLive.hpp"

extern "C" {
#include "Menu/Container/ItemView.h"
}

namespace VS::Items {
// decomp: _getWeaponsubItemCount (src/MENU/MENUD.PRG/234.c:1959)
int CountLiveContainerWeaponGems(int weapon_slot) {
  auto* const owner = Menu_Container_containerData;
  LiveContainerWorld source(owner == nullptr ? nullptr : &owner->items);
  return CountContainerWeaponGems(weapon_slot, source);
}

// decomp: _getShieldsubItemCount (src/MENU/MENUD.PRG/234.c:1971)
int CountLiveContainerShieldGems(int shield_slot) {
  auto* const owner = Menu_Container_containerData;
  LiveContainerWorld source(owner == nullptr ? nullptr : &owner->items);
  return CountContainerShieldGems(shield_slot, source);
}
}  // namespace VS::Items
