#pragma once

#include "Menu/EquipmentDetailControllersLiveFixture.hpp"
#include "Menu/Inventory/ItemDetailDispatchLive.hpp"

namespace item_detail_tests {
namespace inventory = VS::Menu::Inventory;
struct DispatchLiveFixture : equipment_detail_tests::LiveFixture {
  equipment_detail_tests::ValueRestore<decltype(Menu_Framework_cursorColor)>
      saved_cursor{Menu_Framework_cursorColor};

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
    VS::Items::Live::Commit(Audio_Seq_unusedInventory.armor[0], armor);
    ++armor.id;
    VS::Items::Live::Commit(Audio_Seq_unusedInventory.armor[15], armor);
    Audio_Seq_unusedInventoryIndices.armor[0] = 16;
    Audio_Seq_unusedInventoryIndices.armor[1] = 1;
  }
  void AddGems() {
    VS::Items::InventoryGem gem{.id = 261, .strength = 5};
    gem.classes[0] = -4;
    gem.affinities[0] = 7;
    VS::Items::Live::Commit(Audio_Seq_unusedInventory.gems[0], gem);
    gem.id = 262;
    gem.strength = 9;
    VS::Items::Live::Commit(Audio_Seq_unusedInventory.gems[47], gem);
    Audio_Seq_unusedInventoryIndices.gems[0] = 48;
    Audio_Seq_unusedInventoryIndices.gems[1] = 1;
  }
};
}  // namespace item_detail_tests
