#pragma once

#include <cstdint>

#include "Items/ItemRecordsLive.hpp"
#include "Menu/Framework/FrameworkLive.hpp"

namespace VS::Menu::Framework {
template <typename World>
class LiveSortingKeys : public World {
 public:
  std::int32_t WeaponValue(std::int32_t stat,
                           const Items::WeaponLoadout& weapon) override {
    Items_WeaponLoadout legacy{};
    Items::Live::Commit(legacy, weapon);
    return LiveWeaponSortValue(stat, &legacy);
  }
  std::int32_t ShieldValue(std::int32_t stat,
                           const Items::ShieldLoadout& shield) override {
    Items_ShieldLoadout legacy{};
    Items::Live::Commit(legacy, shield);
    return LiveShieldSortValue(stat, &legacy);
  }
  std::int32_t MiscValue(std::int32_t stat,
                         const Items::InventoryMisc& item) override {
    Items_InventoryMisc legacy{};
    Items::Live::Commit(legacy, item);
    return LiveMiscSortValue(stat, &legacy);
  }
  std::int32_t EquipmentValue(std::int32_t stat,
                              const Items::EquipmentPart& part) override {
    Items_EquipmentPartStats legacy{};
    Items::Live::Commit(legacy, part);
    return LiveEquipmentSortValue(stat, &legacy);
  }
};
}  // namespace VS::Menu::Framework
