#pragma once

#include <cstdint>
#include <span>
#include <string_view>

#include "Items/ItemRecords.hpp"
#include "Menu/Framework/SortingKeys.hpp"

namespace VS::Menu::Inventory {
class SortingWorld : public Framework::SortingKeys {
 public:
  SortingWorld() = default;
  SortingWorld(const SortingWorld&) = delete;
  SortingWorld& operator=(const SortingWorld&) = delete;
  SortingWorld(SortingWorld&&) = delete;
  SortingWorld& operator=(SortingWorld&&) = delete;
  ~SortingWorld() override = default;

  [[nodiscard]] virtual Items::InventoryContents CurrentInventory() = 0;
  [[nodiscard]] virtual std::span<std::uint8_t> FixedOrder(
      Items::ItemCategory category) = 0;
  [[nodiscard]] virtual std::span<std::uint8_t> EquipmentOrder(
      Items::ItemCategory category) = 0;
  [[nodiscard]] virtual std::uint8_t Capacity(Items::ItemCategory category) = 0;
};

void SortWeapons(SortingWorld& world, std::int32_t stat);
void SortShields(SortingWorld& world, std::int32_t stat);
void SortMisc(SortingWorld& world, std::int32_t stat);
[[nodiscard]] bool HasEquipmentProjection(std::int32_t category);
// Actual callers supply a separate stack output. Unsupported categories
// leave it untouched and do not read inventory or validate the index.
void CopyEquipmentStats(Items::EquipmentPart& target, std::int32_t category,
                        std::int32_t index, SortingWorld& world);
void SortEquipment(SortingWorld& world, std::int32_t category,
                   std::int32_t stat);
}  // namespace VS::Menu::Inventory
