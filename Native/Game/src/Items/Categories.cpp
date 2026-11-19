// Storage for the category bound tables (Categories.hpp).
#include "Items/Categories.hpp"

namespace VS::Items {
// decomp: _wepIdCategories (_wepIdCategories@BATTLE/BATTLE.PRG)
std::uint8_t weaponCategoryUpperIds[kWeaponCategoryUpperIdBytes];

// decomp: _itemCategorySizes (_itemCategorySizes@BATTLE/BATTLE.PRG)
std::uint32_t itemCategoryUpperIds[kItemIdCategoryCount];
}  // namespace VS::Items
