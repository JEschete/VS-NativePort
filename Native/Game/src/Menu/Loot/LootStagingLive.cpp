#include "Menu/Loot/LootStagingLive.hpp"

#include <cstdint>

#include "Items/ItemRecordsLive.hpp"
extern "C" {
#include "Menu/Loot/LootScreen.h"
}
#include "Menu/Loot/LootStaging.hpp"

namespace VS::Menu::Loot::Live {
namespace {
namespace items = VS::Items;
namespace item_live = VS::Items::Live;

template <typename LegacySource, typename Convert, typename Stage>
int Apply(LegacySource* source, const char* operation, Convert&& convert,
          Stage&& stage) {
  if (source == nullptr || Menu_Loot_inventory == nullptr) {
    item_live::WarnNullRecord(operation);
    return 0;
  }
  auto inventory = item_live::Typed(*Menu_Loot_inventory);
  const auto result = stage(convert(*source), inventory);
  item_live::Commit(*Menu_Loot_inventory, inventory);
  return result;
}
}  // namespace

int StageBlade(Items_InventoryBlade* blade, int weapon_index) {
  return Apply(
      blade, "Menu::Loot::StageBlade",
      [](const auto& source) { return item_live::Typed(source); },
      [weapon_index](const auto& source, auto& inventory) {
        return VS::Menu::Loot::StageBlade(source, weapon_index, inventory);
      });
}

int StageGrip(Items_InventoryGrip* grip, int weapon_index) {
  return Apply(
      grip, "Menu::Loot::StageGrip",
      [](const auto& source) { return item_live::Typed(source); },
      [weapon_index](const auto& source, auto& inventory) {
        return VS::Menu::Loot::StageGrip(source, weapon_index, inventory);
      });
}

int StageGem(Items_InventoryGem* gem, int item_index) {
  return Apply(
      gem, "Menu::Loot::StageGem",
      [](const auto& source) { return item_live::Typed(source); },
      [item_index](const auto& source, auto& inventory) {
        return VS::Menu::Loot::StageGem(source, item_index, inventory);
      });
}

int StageWeapon(Items_LootedWeapon* weapon) {
  return Apply(
      weapon, "Menu::Loot::StageWeapon",
      [](const auto& source) { return item_live::Typed(source); },
      [](const auto& source, auto& inventory) {
        return VS::Menu::Loot::StageWeapon(source, inventory);
      });
}

int StageArmor(Items_InventoryArmor* armor) {
  return Apply(
      armor, "Menu::Loot::StageArmor",
      [](const auto& source) { return item_live::Typed(source); },
      [](const auto& source, auto& inventory) {
        return VS::Menu::Loot::StageArmor(source, inventory);
      });
}

int StageShield(Items_LootedShield* shield) {
  return Apply(
      shield, "Menu::Loot::StageShield",
      [](const auto& source) { return item_live::Typed(source); },
      [](const auto& source, auto& inventory) {
        return VS::Menu::Loot::StageShield(source, inventory);
      });
}

int StageMiscItem(Items_LootedItem* item) {
  return Apply(
      item, "Menu::Loot::StageMiscItem",
      [](const Items_LootedItem& source) {
        return items::LootedItem{
            .item_id = source.itemId,
            .difficulty_flags =
                static_cast<std::uint8_t>(source.difficultyFlags),
            .count = static_cast<std::uint8_t>(source.count),
        };
      },
      [](const auto& source, auto& inventory) {
        return VS::Menu::Loot::StageMiscItem(source, inventory);
      });
}
}  // namespace VS::Menu::Loot::Live
