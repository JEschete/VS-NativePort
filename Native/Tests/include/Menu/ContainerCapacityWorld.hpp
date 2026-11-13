#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "Items/ContainerGemCounts.hpp"
#include "Items/ContainerIndexing.hpp"
#include "Items/ContainerTables.hpp"
#include "Menu/Container/ContainerCapacity.hpp"

namespace container_capacity_tests {
namespace items = VS::Items;
namespace capacity = VS::Menu::container;
namespace framework = VS::Menu::Framework;

struct DrawEvent {
  bool number{};
  framework::NumberText value{};
  std::string text;
};

struct World final : capacity::StorageCapacityWorld,
                     capacity::CapacityTransferWorld {
  items::ContainerStorage source;
  items::ContainerStorage destination;
  items::ContainerStorage staged;
  items::ContainerStorage bag;
  items::ContainerWorld* active = &source;
  items::ContainerWorld* receiving = &destination;
  items::ContainerWorld* staging = &staged;
  items::ContainerWorld* summary_bag = &bag;
  items::ContainerWorld* summary_container = &destination;
  std::array<std::uint16_t, capacity::kPendingTransferEntries> pending{};
  std::size_t pending_extent = pending.size();
  items::ContainerCapacities limits = items::kContainerCapacities;
  items::ContainerCapacities bag_limits{};
  std::vector<DrawEvent> draws;
  std::vector<items::ItemCategory> checked_limits;
  std::function<void()> number_callback;
  std::function<void()> active_callback;
  std::function<void()> staging_callback;
  std::function<void()> receiving_callback;
  std::function<void()> limit_callback;
  unsigned active_queries{};
  unsigned staging_queries{};
  unsigned receiving_queries{};

  World() { std::ranges::copy(items::kCategoryCapacities, bag_limits.begin()); }
  void DrawNumber(const framework::NumberText& number) override {
    draws.push_back({.number = true, .value = number, .text = {}});
    if (number_callback) number_callback();
  }
  void DrawLabel(std::string_view text, std::uint32_t position,
                 std::uint32_t color) override {
    draws.push_back({.number = false,
                     .value = {.position = position, .color = color},
                     .text = std::string(text)});
  }
  std::int32_t Occupied(capacity::StorageArea area,
                        items::ItemCategory category) override {
    return items::CountContainerItems(category,
                                      area == capacity::StorageArea::kGadgetBag
                                          ? *summary_bag
                                          : *summary_container);
  }
  std::uint16_t Limit(capacity::StorageArea area,
                      items::ItemCategory category) override {
    const auto index = static_cast<std::size_t>(category);
    return area == capacity::StorageArea::kGadgetBag ? bag_limits.at(index)
                                                     : limits.at(index);
  }
  items::ContainerWorld& Active() override {
    ++active_queries;
    if (active_callback) active_callback();
    return *active;
  }
  items::ContainerWorld& Receiving() override {
    ++receiving_queries;
    if (receiving_callback) receiving_callback();
    return *receiving;
  }
  items::ContainerWorld& Staging() override {
    ++staging_queries;
    if (staging_callback) staging_callback();
    return *staging;
  }
  std::span<const std::uint16_t> PendingTransfers() override {
    return std::span(pending).first(pending_extent);
  }
  std::uint16_t ReceivingLimit(items::ItemCategory category) override {
    checked_limits.push_back(category);
    if (limit_callback) limit_callback();
    return limits.at(static_cast<std::size_t>(category));
  }
  [[noreturn]] void Refuse(std::string_view reason) override {
    throw std::out_of_range(std::string(reason));
  }
};

inline void Fill(items::ContainerStorage& storage, items::ItemCategory category,
                 std::size_t count) {
  items::Live::VisitContainerTable(storage.items, category, [&](auto& table) {
    for (std::size_t slot = 0; slot < count; ++slot) {
      auto& record = table.at(slot);
      if constexpr (requires { record.blade; }) {
        record.blade = 1;
      } else if constexpr (requires { record.base; }) {
        record.base.id = 1;
      } else {
        record.id = 1;
      }
    }
  });
}

inline void ArmedWeapon(items::ContainerStorage& storage) {
  constexpr std::uint8_t kLastPart = 64;
  constexpr std::uint8_t kLastGem = 192;
  constexpr std::uint8_t kThreeSlots = 3;
  storage.items.weapons.back() = {
      .blade = kLastPart, .grip = kLastPart, .gems = {kLastGem, 0, 1, 0}};
  storage.items.grips.back() = {.id = 1, .gem_slots = kThreeSlots};
  storage.items.blades.back().id = 1;
  storage.items.gems.back().id = 1;
  storage.items.gems.front().id = 2;
}
}  // namespace container_capacity_tests
