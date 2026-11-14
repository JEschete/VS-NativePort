#pragma once

#include <cstdint>
#include <string_view>

#include "Items/ItemRecords.hpp"

namespace VS::Menu::Framework {
class SortingKeys {
 public:
  SortingKeys() = default;
  SortingKeys(const SortingKeys&) = delete;
  SortingKeys& operator=(const SortingKeys&) = delete;
  SortingKeys(SortingKeys&&) = delete;
  SortingKeys& operator=(SortingKeys&&) = delete;
  virtual ~SortingKeys() = default;

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
}  // namespace VS::Menu::Framework
