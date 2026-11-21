#pragma once
// The item and weapon id bounds of each category; the BATTLE.PRG module image
// fills them (ModuleData.cpp).
#include <cstddef>
#include <cstdint>

#include "Items/Inventory.hpp"

namespace VS::Items {
// The module image reserves 48 bytes for the weapon table; the blade-model
// lookup reads its first 11 (ActorSpawning.hpp kWeaponCategoryBounds).
inline constexpr std::size_t kWeaponCategoryUpperIdBytes = 48;

// decomp: wepIdCategories (src/BATTLE/BATTLE.PRG/146C.c:296; was
// _wepIdCategories): the highest weapon id of each weapon category.
extern std::uint8_t weaponCategoryUpperIds[kWeaponCategoryUpperIdBytes];

// decomp: itemCategorySizes (src/BATTLE/BATTLE.PRG/146C.c:9634; was
// _itemCategorySizes): the exclusive upper item id of each inventory category.
extern std::uint32_t itemCategoryUpperIds[kItemIdCategoryCount];
}  // namespace VS::Items
