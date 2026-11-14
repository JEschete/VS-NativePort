#include "Menu/Inventory/InventorySortingLive.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <span>
#include <string_view>

#include "Items/ItemRecordsLive.hpp"
#include "Menu/Framework/SortingKeysLive.hpp"
#include "Menu/Inventory/InventorySorting.hpp"

extern "C" {
#include "Audio/Seq/Channel.h"
#include "Core/Diagnostics.h"
#include "Items/InventoryLiveState.h"
#include "Menu/Inventory/ItemList.h"
}

namespace VS::Menu::Inventory {
namespace {
[[noreturn]] void Refuse(std::string_view reason) {
  Core_Errorf("menu.inventory.sorting", "Inventory sorting refused: %.*s",
              static_cast<int>(reason.size()), reason.data());
  std::abort();
}

auto OrderTables() {
  auto& order = Audio_Seq_unusedInventoryIndices;
  return std::array<std::span<std::uint8_t>,
                    static_cast<std::size_t>(Items::ItemCategory::kManage)>{
      order.weapons, order.blades, order.grips, order.shields,
      order.armor,   order.gems,   order.misc};
}

std::span<std::uint8_t> KnownOrder(std::uint8_t* pointer) {
  if (pointer == nullptr) Refuse("missing inventory equipment order");
  for (auto table : OrderTables()) {
    for (std::size_t offset = 0; offset < table.size(); ++offset) {
      if (pointer == &table[offset]) return table.subspan(offset);
    }
  }
  Refuse("equipment order is outside known inventory index arrays");
}

bool OverlapsInventory(const Items_EquipmentPartStats* target) {
  const auto output = reinterpret_cast<std::uintptr_t>(target);
  const auto source =
      reinterpret_cast<std::uintptr_t>(&Audio_Seq_unusedInventory);
  return output <= source ? source - output < sizeof(*target)
                          : output - source < sizeof(Audio_Seq_unusedInventory);
}

class LiveSortingWorld final : public Framework::LiveSortingKeys<SortingWorld> {
 public:
  Items::InventoryContents CurrentInventory() override {
    return Items::Live::Typed(Audio_Seq_unusedInventory);
  }
  std::span<std::uint8_t> FixedOrder(Items::ItemCategory category) override {
    return OrderTables()[static_cast<std::size_t>(category)];
  }
  std::span<std::uint8_t> EquipmentOrder(
      Items::ItemCategory category) override {
    return KnownOrder(
        Menu_Inventory_inventoryIndices[static_cast<std::size_t>(category)]);
  }
  std::uint8_t Capacity(Items::ItemCategory category) override {
    return Items_Inventory_inventoryItemCapacities[static_cast<std::size_t>(
        category)];
  }
  [[noreturn]] void Refuse(std::string_view reason) override {
    Inventory::Refuse(reason);
  }
};
}  // namespace

// decomp: _sortWeapons (MENU/items1.h:731).
void SortLiveWeapons(int stat) {
  LiveSortingWorld world;
  SortWeapons(world, stat);
}

// decomp: _sortShields (MENU/items1.h:827).
void SortLiveShields(int stat) {
  LiveSortingWorld world;
  SortShields(world, stat);
}

// decomp: _sortMisc (MENU/items1.h:899).
void SortLiveMisc(int stat) {
  LiveSortingWorld world;
  SortMisc(world, stat);
}

// decomp: _copyEquipment (MENU/items1.h:994).
void CopyLiveEquipmentStats(Items_EquipmentPartStats* target, int category,
                            int index) {
  if (!HasEquipmentProjection(category)) return;
  if (target == nullptr) Refuse("missing inventory projection output");
  if (OverlapsInventory(target)) {
    Refuse("equipment output overlaps inventory storage");
  }
  LiveSortingWorld world;
  auto output = Items::Live::Typed(*target);
  CopyEquipmentStats(output, category, index, world);
  Items::Live::Commit(*target, output);
}

// decomp: _sortEquipment (MENU/items1.h:1017).
void SortLiveEquipment(int category, int stat) {
  LiveSortingWorld world;
  SortEquipment(world, category, stat);
}
}  // namespace VS::Menu::Inventory
