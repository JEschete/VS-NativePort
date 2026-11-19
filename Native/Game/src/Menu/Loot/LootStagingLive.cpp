#include "Menu/Loot/LootStagingLive.hpp"

#include <cstdint>

#include "Items/ItemRecordsLive.hpp"
#include "Menu/Loot/LootScreen.h"
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
  auto inventory = *Menu_Loot_inventory;
  const auto result = stage(convert(*source), inventory);
  *Menu_Loot_inventory = inventory;
  return result;
}
}  // namespace

int StageBlade(VS::Items::InventoryBlade* blade, int weapon_index) {
  return Apply(
      blade, "Menu::Loot::StageBlade",
      [](const auto& source) { return source; },
      [weapon_index](const auto& source, auto& inventory) {
        return VS::Menu::Loot::StageBlade(source, weapon_index, inventory);
      });
}

int StageGrip(VS::Items::InventoryGrip* grip, int weapon_index) {
  return Apply(
      grip, "Menu::Loot::StageGrip", [](const auto& source) { return source; },
      [weapon_index](const auto& source, auto& inventory) {
        return VS::Menu::Loot::StageGrip(source, weapon_index, inventory);
      });
}

int StageGem(VS::Items::InventoryGem* gem, int item_index) {
  return Apply(
      gem, "Menu::Loot::StageGem", [](const auto& source) { return source; },
      [item_index](const auto& source, auto& inventory) {
        return VS::Menu::Loot::StageGem(source, item_index, inventory);
      });
}

int StageWeapon(VS::Items::LootedWeapon* weapon) {
  return Apply(
      weapon, "Menu::Loot::StageWeapon",
      [](const auto& source) { return source; },
      [](const auto& source, auto& inventory) {
        return VS::Menu::Loot::StageWeapon(source, inventory);
      });
}

int StageArmor(VS::Items::InventoryArmor* armor) {
  return Apply(
      armor, "Menu::Loot::StageArmor",
      [](const auto& source) { return source; },
      [](const auto& source, auto& inventory) {
        return VS::Menu::Loot::StageArmor(source, inventory);
      });
}

int StageShield(VS::Items::LootedShield* shield) {
  return Apply(
      shield, "Menu::Loot::StageShield",
      [](const auto& source) { return source; },
      [](const auto& source, auto& inventory) {
        return VS::Menu::Loot::StageShield(source, inventory);
      });
}

int StageMiscItem(VS::Items::LootedItem* item) {
  return Apply(
      item, "Menu::Loot::StageMiscItem",
      [](const VS::Items::LootedItem& source) {
        return items::LootedItem{
            .item_id = source.item_id,
            .difficulty_flags =
                static_cast<std::uint8_t>(source.difficulty_flags),
            .count = static_cast<std::uint8_t>(source.count),
        };
      },
      [](const auto& source, auto& inventory) {
        return VS::Menu::Loot::StageMiscItem(source, inventory);
      });
}
}  // namespace VS::Menu::Loot::Live
