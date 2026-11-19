#pragma once

#include "Items/InventoryLive.hpp"
#include "Menu/EquipmentDetailControllersLiveFixture.hpp"
#include "Menu/Inventory/ItemDetailDispatchLive.hpp"

namespace item_detail_tests {
namespace inventory = VS::Menu::Inventory;
struct DispatchLiveFixture : equipment_detail_tests::LiveFixture {
  equipment_detail_tests::ValueRestore<
      decltype(VS::Menu::Framework::cursorColor)>
      saved_cursor{VS::Menu::Framework::cursorColor};

  DispatchLiveFixture() { inventory::ResetLiveItemDetailDispatch(); }
  ~DispatchLiveFixture() { inventory::ResetLiveItemDetailDispatch(); }

  void AddArmor(bool accessory = false) {
    VS::Items::InventoryArmor armor{
        .id = static_cast<std::uint8_t>(accessory ? 223 : 143),
        .category = static_cast<std::uint8_t>(accessory ? 7 : 1),
        .max_dp = 1000,
        .current_dp = 500,
        .strength = -7,
        .intelligence = 9,
        .material = 1};
    armor.classes[0] = -3;
    armor.affinities[0] = 4;
    VS::Items::inventory.armor[0] = armor;
    ++armor.id;
    VS::Items::inventory.armor[15] = armor;
    VS::Items::inventoryIndices.armor[0] = 16;
    VS::Items::inventoryIndices.armor[1] = 1;
  }
  void AddGems() {
    VS::Items::InventoryGem gem{.id = 261, .strength = 5};
    gem.classes[0] = -4;
    gem.affinities[0] = 7;
    VS::Items::inventory.gems[0] = gem;
    gem.id = 262;
    gem.strength = 9;
    VS::Items::inventory.gems[47] = gem;
    VS::Items::inventoryIndices.gems[0] = 48;
    VS::Items::inventoryIndices.gems[1] = 1;
  }
};
}  // namespace item_detail_tests
