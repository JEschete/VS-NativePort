#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>

#include "Items/ItemRecords.hpp"

namespace VS::Menu::sorting_test {
template <typename Base, typename Derived, typename Op, std::size_t N>
class KeyedWorld : public Base {
 public:
  std::array<std::int32_t, N> keys{};

  std::int32_t WeaponValue(std::int32_t stat,
                           const Items::WeaponLoadout& weapon) override {
    return Observer().Observe(Op::kWeapon, stat, keys[weapon.index]);
  }
  std::int32_t ShieldValue(std::int32_t stat,
                           const Items::ShieldLoadout& shield) override {
    return Observer().Observe(Op::kShield, stat, keys[shield.index]);
  }
  std::int32_t MiscValue(std::int32_t stat,
                         const Items::InventoryMisc& item) override {
    return Observer().Observe(Op::kMisc, stat, keys[item.index]);
  }
  std::int32_t EquipmentValue(std::int32_t stat,
                              const Items::EquipmentPart& part) override {
    return Observer().Observe(Op::kEquipment, stat, keys[part.index]);
  }
  [[noreturn]] void Refuse(std::string_view reason) override {
    throw std::runtime_error(std::string(reason));
  }

 private:
  friend Derived;
  KeyedWorld() = default;
  Derived& Observer() { return static_cast<Derived&>(*this); }
};

inline constexpr int kFirstWideCategory = 256;
inline constexpr auto kUnsupportedEquipmentCategories = std::to_array<int>({
    -1,
    static_cast<int>(Items::ItemCategory::kWeapon),
    static_cast<int>(Items::ItemCategory::kShield),
    static_cast<int>(Items::ItemCategory::kMisc),
    static_cast<int>(Items::ItemCategory::kManage),
    kFirstWideCategory,
    kFirstWideCategory + 1,
    std::numeric_limits<int>::min(),
    std::numeric_limits<int>::max(),
});
}  // namespace VS::Menu::sorting_test
