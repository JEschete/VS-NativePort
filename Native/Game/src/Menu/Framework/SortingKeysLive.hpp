#pragma once

#include <cstdint>

#include "Items/ItemRecords.hpp"
#include "Menu/Framework/FrameworkLive.hpp"

namespace VS::Menu::Framework {
template <typename World>
class LiveSortingKeys : public World {
 public:
  std::int32_t WeaponValue(std::int32_t stat,
                           const Items::WeaponLoadout& weapon) override {
    return LiveWeaponSortValue(stat, &weapon);
  }
  std::int32_t ShieldValue(std::int32_t stat,
                           const Items::ShieldLoadout& shield) override {
    return LiveShieldSortValue(stat, &shield);
  }
  std::int32_t MiscValue(std::int32_t stat,
                         const Items::InventoryMisc& item) override {
    return LiveMiscSortValue(stat, &item);
  }
  std::int32_t EquipmentValue(std::int32_t stat,
                              const Items::EquipmentPart& part) override {
    return LiveEquipmentSortValue(stat, &part);
  }
};
}  // namespace VS::Menu::Framework
