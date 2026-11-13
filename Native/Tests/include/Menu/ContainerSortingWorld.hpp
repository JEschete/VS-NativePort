#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

#include "Menu/Container/ContainerSorting.hpp"

namespace VS::Menu::container::sort_test {
enum class Operation : std::uint8_t {
  kSource,
  kCurrent,
  kOrder,
  kCapacity,
  kWeapon,
  kShield,
  kMisc,
  kEquipment
};
struct Event {
  Operation operation{};
  std::int32_t stat{};
  std::int32_t value{};
  std::uint32_t order_hash{};
  std::size_t owner{};
};
inline constexpr std::size_t kOwners = 2;
inline constexpr std::size_t kKeySlots = 256;

class World : public SortingWorld {
 public:
  std::array<Items::ContainerStorage, kOwners> owners;
  std::array<std::int32_t, kKeySlots> keys{};
  std::size_t owner = 0;
  std::uint8_t observed_material = 0;
  std::size_t trace_count = 0;
  std::span<std::uint16_t> captured_order;
  std::optional<std::size_t> order_extent;
  std::function<void(World&, Operation)> hook;
  std::vector<Event> events;

  Items::ContainerWorld& SourceRecords() override {
    auto& result = owners[owner];
    Observe(Operation::kSource);
    return result;
  }
  Items::ContainerWorld& CurrentRecords() override {
    auto& result = owners[owner];
    Observe(Operation::kCurrent);
    return result;
  }
  std::span<std::uint16_t> Order(Items::ItemCategory category) override {
    const auto offset =
        Items::ContainerCategoryIndices(category, owners[owner]);
    captured_order = std::span(owners[owner].indices).subspan(offset);
    if (order_extent.has_value()) {
      captured_order = captured_order.first(
          std::min(captured_order.size(), order_extent.value()));
    }
    auto result = captured_order;
    Observe(Operation::kOrder);
    return result;
  }
  std::uint16_t Capacity(Items::ItemCategory category) override {
    const auto result = owners[owner].Capacity(category);
    Observe(Operation::kCapacity);
    return result;
  }
  std::int32_t WeaponValue(std::int32_t stat,
                           const Items::WeaponLoadout& weapon) override {
    observed_material = weapon.blade.material;
    return Observe(Operation::kWeapon, stat, keys[weapon.index]);
  }
  std::int32_t ShieldValue(std::int32_t stat,
                           const Items::ShieldLoadout& shield) override {
    return Observe(Operation::kShield, stat, keys[shield.index]);
  }
  std::int32_t MiscValue(std::int32_t stat,
                         const Items::InventoryMisc& item) override {
    return Observe(Operation::kMisc, stat, keys[item.index]);
  }
  std::int32_t EquipmentValue(std::int32_t stat,
                              const Items::EquipmentPart& part) override {
    return Observe(Operation::kEquipment, stat, keys[part.index]);
  }
  [[noreturn]] void Refuse(std::string_view reason) override {
    throw std::runtime_error(std::string(reason));
  }
  std::int32_t Observe(Operation operation, std::int32_t stat = 0,
                       std::int32_t value = 0) {
    std::uint32_t hash = 0;
    const auto count = std::min(trace_count, captured_order.size());
    for (std::size_t row = 0; row < count; ++row) {
      hash += static_cast<std::uint32_t>(row + 1) * captured_order[row];
    }
    events.push_back({operation, stat, value, hash, owner});
    if (hook) hook(*this, operation);
    return value;
  }
};

inline void SetOrder(World& world, Items::ItemCategory category,
                     std::initializer_list<std::uint16_t> tokens) {
  auto order = world.Order(category);
  std::copy(tokens.begin(), tokens.end(), order.begin());
  world.events.clear();
}

inline void SetRecords(World& world) {
  for (auto& owner : world.owners) {
    for (std::size_t row = 0; row < owner.items.weapons.size(); ++row) {
      owner.items.weapons[row].index = static_cast<std::uint8_t>(row + 1);
      owner.items.shields[row].index = static_cast<std::uint8_t>(row + 1);
    }
    for (std::size_t row = 0; row < owner.items.gems.size(); ++row) {
      owner.items.gems[row].index = static_cast<std::int16_t>(row + 1);
    }
    for (std::size_t row = 0; row < owner.items.misc.size(); ++row) {
      owner.items.misc[row].index = static_cast<std::uint8_t>(row);
    }
    for (std::size_t row = 0; row < owner.items.blades.size(); ++row) {
      owner.items.blades[row].index = static_cast<std::uint8_t>(row + 1);
      owner.items.grips[row].index = static_cast<std::uint16_t>(row + 1);
      owner.items.armor[row].index = static_cast<std::uint8_t>(row + 1);
    }
  }
}
}  // namespace VS::Menu::container::sort_test
