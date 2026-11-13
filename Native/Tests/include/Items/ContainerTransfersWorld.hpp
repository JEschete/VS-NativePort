#pragma once

#include <algorithm>
#include <array>
#include <span>
#include <type_traits>
#include <variant>

#include "Items/ContainerTables.hpp"
#include "Items/ContainerTransfers.hpp"

namespace container_transfers_tests {
namespace items = VS::Items;

struct BagWorld final : items::GadgetBagWorld {
  items::ContainerStorage storage;
  items::InventoryContents inventory{};
  items::InventoryIndices indices{};
  items::CategoryCounts capacities = items::kCategoryCapacities;
  std::array<unsigned, items::kItemCategoryCount> order_calls{};
  unsigned clears = 0;

  items::ContainerWorld& Bag() override { return storage; }
  void ClearBagOrder() override {
    ++clears;
    storage.indices.fill(0);
  }
  std::span<std::uint16_t> BagOrder(items::ItemCategory category) override {
    ++order_calls[static_cast<std::size_t>(category)];
    const auto offset = items::ContainerCategoryIndices(category, storage);
    return std::span(storage.indices)
        .subspan(
            offset,
            items::kContainerCapacities[static_cast<std::size_t>(category)]);
  }
  std::uint8_t InventoryCapacity(items::ItemCategory category) override {
    return capacities[static_cast<std::size_t>(category)];
  }
  std::span<std::uint8_t> InventoryOrder(
      items::ItemCategory category) override {
    return items::Live::VisitContainerTable(
        indices, category,
        [](auto& table) -> std::span<std::uint8_t> { return table; });
  }
  items::ContainerRecord ReadInventory(items::ItemCategory category,
                                       std::size_t index) override {
    return items::Live::VisitContainerTable(
        inventory, category, [&](const auto& table) -> items::ContainerRecord {
          return table.at(index);
        });
  }
  void WriteInventory(items::ItemCategory category, std::size_t index,
                      const items::ContainerRecord& item) override {
    items::Live::VisitContainerTable(inventory, category, [&](auto& table) {
      using Record =
          typename std::remove_reference_t<decltype(table)>::value_type;
      table.at(index) = std::get<Record>(item);
    });
  }
};

inline items::ContainerTransfer Copy(items::ItemCategory category,
                                     std::int32_t parent = 0) {
  return {category, items::ContainerTransferMode::kCopy, parent};
}

inline items::ContainerTransfer Check(items::ItemCategory category) {
  return {category, items::ContainerTransferMode::kCheck, 0};
}
}  // namespace container_transfers_tests
