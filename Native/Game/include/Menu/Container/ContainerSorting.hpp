#pragma once

#include <cstdint>
#include <span>
#include <string_view>

#include "Items/ContainerIndexing.hpp"

namespace VS::Menu::container {
class SortingWorld {
 public:
  SortingWorld() = default;
  SortingWorld(const SortingWorld&) = delete;
  SortingWorld& operator=(const SortingWorld&) = delete;
  SortingWorld(SortingWorld&&) = delete;
  SortingWorld& operator=(SortingWorld&&) = delete;
  virtual ~SortingWorld() = default;

  [[nodiscard]] virtual Items::ContainerWorld& SourceRecords() = 0;
  [[nodiscard]] virtual Items::ContainerWorld& CurrentRecords() = 0;
  [[nodiscard]] virtual std::span<std::uint16_t> Order(
      Items::ItemCategory category) = 0;
  [[nodiscard]] virtual std::uint16_t Capacity(
      Items::ItemCategory category) = 0;
  [[nodiscard]] virtual std::int32_t WeaponValue(
      std::int32_t stat, const Items::WeaponLoadout& weapon) = 0;
  [[nodiscard]] virtual std::int32_t ShieldValue(
      std::int32_t stat, const Items::ShieldLoadout& shield) = 0;
  [[nodiscard]] virtual std::int32_t MiscValue(
      std::int32_t stat, const Items::InventoryMisc& item) = 0;
  [[nodiscard]] virtual std::int32_t EquipmentValue(
      std::int32_t stat, const Items::EquipmentPart& part) = 0;
  [[noreturn]] virtual void Refuse(std::string_view reason) = 0;
};

void SortWeapons(SortingWorld& world, std::int32_t stat);
void SortShields(SortingWorld& world, std::int32_t stat);
void SortMisc(SortingWorld& world, std::int32_t stat);
[[nodiscard]] bool HasEquipmentProjection(std::int32_t category);
// Unsupported categories are a no-op, including an absent source. The
// source and output are distinct typed objects; raw byte overlaps are not
// this API's ownership contract.
void CopyEquipmentStats(Items::EquipmentPart& target, std::int32_t category,
                        const Items::ContainerRecord* source,
                        Items::ContainerWorld& records);
void SortEquipment(SortingWorld& world, std::int32_t category,
                   std::int32_t stat);
}  // namespace VS::Menu::container
