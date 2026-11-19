// The MAINMENU.PRG item list statics.
#include "Menu/Inventory/ItemList.hpp"

#include <array>
#include <cstdint>

#include "Items/InventoryLive.hpp"

namespace VS::Menu::Inventory {
namespace {
constexpr CategoryOrderTable InitialCategoryOrders() noexcept {
  auto& order = Items::inventoryIndices;
  return {order.weapons.data(), order.blades.data(), order.grips.data(),
          order.shields.data(), order.armor.data(),  order.gems.data(),
          order.misc.data()};
}
}  // namespace

// decomp: vs_mainMenu_freezeTabArrows
// (vs_mainMenu_freezeTabArrows@MENU/MAINMENU.PRG)
constinit std::uint8_t freezeTabArrows = 0;

// decomp: vs_mainMenu_inventoryIndices
// (vs_mainMenu_inventoryIndices@MENU/MAINMENU.PRG)
constinit CategoryOrderTable categoryOrders = InitialCategoryOrders();

void ResetItemListStatics() {
  freezeTabArrows = 0;
  categoryOrders = InitialCategoryOrders();
}
}  // namespace VS::Menu::Inventory
