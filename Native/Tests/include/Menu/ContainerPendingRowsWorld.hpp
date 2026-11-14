#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <functional>
#include <iterator>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "Menu/Container/ContainerPendingRows.hpp"
#include "Render/EncodedText.hpp"

namespace container_pending_rows_tests {
namespace rows = VS::Menu::container;
namespace items = VS::Items;
namespace framework = VS::Menu::Framework;
inline constexpr auto kRowCount = items::ContainerContents{}.misc.size();
inline constexpr std::uint16_t kMiscId = 323;
inline constexpr std::uint16_t kGemId = 261;
inline constexpr std::uint8_t kGripId = 97;
inline constexpr std::uint8_t kShieldId = 112;
inline constexpr std::uint8_t kArmorId = 145;
inline constexpr std::uint32_t kRestrictionMask = 0xFF01;
inline constexpr std::uint32_t kEquipped = 0xCA00;
inline constexpr std::uint32_t kAttached = 0xCC00;
inline constexpr std::uint32_t kPendingAttached = 0xCC01;
inline constexpr std::size_t kEncodedNameBytes = 24;
inline constexpr std::size_t kHandCategories = 10;
inline constexpr std::size_t kLastMiscSlot = kRowCount - 1;
inline constexpr std::size_t kLastGemSlot = 191;
inline constexpr std::size_t kLastPartSlot = 63;
inline constexpr std::uint16_t kLastMiscPendingToken = 0x700;
inline constexpr std::uint16_t kLastGemPendingToken = 0x5C0;
inline constexpr std::uint16_t kMiscPendingToken = 0x601;
inline constexpr std::uint16_t kBladePendingToken = 0x101;

inline std::vector<std::uint8_t> ReadText(const char* path) {
  std::ifstream stream(path, std::ios::binary);
  if (!stream) throw std::runtime_error("missing required original item text");
  return {std::istreambuf_iterator<char>(stream), {}};
}

struct RowTextHost final : framework::ItemTextHost {
  std::vector<std::uint8_t> help =
      ReadText(R"(Native\data\disc\MENU\ITEMHELP.BIN)");
  std::vector<std::uint8_t> names =
      ReadText(R"(Native\data\disc\MENU\ITEMNAME.BIN)");
  std::array<std::uint8_t, kHandCategories> hands{0, 0, 1, 0, 1, 0, 1, 1, 1, 0};
  VS::Render::EncodedTextArguments arguments{};
  std::function<void()> on_string;

  void Refuse(std::string_view reason) override {
    throw std::out_of_range(std::string(reason));
  }
  std::span<const std::uint8_t> HelpText() override { return help; }
  std::span<const std::uint8_t> NameText() override { return names; }
  std::span<const std::uint8_t> WeaponHands() override { return hands; }
  VS::Render::EncodedTextArguments TextArguments() override {
    return arguments;
  }
  void SetStringArgument(std::size_t slot,
                         std::span<const std::uint8_t> text) override {
    arguments.strings.at(slot) = text;
    if (on_string) on_string();
  }
  std::optional<items::WeaponLoadout> InventoryWeaponLoadout(
      const items::InventoryWeapon&) override {
    throw std::logic_error("container rows must build their own weapon stats");
  }
  std::optional<items::ShieldLoadout> InventoryShieldLoadout(
      const items::InventoryShield&) override {
    throw std::logic_error("container rows must build their own shield stats");
  }
};

struct Records final : items::ContainerWorld {
  items::ContainerStorage storage;
  std::function<void()> on_read;
  unsigned reads = 0;

  std::uint16_t Capacity(items::ItemCategory category) override {
    return storage.Capacity(category);
  }
  items::ContainerRecord Read(items::ItemCategory category,
                              std::size_t slot) override {
    auto result = storage.Read(category, slot);
    ++reads;
    if (on_read) on_read();
    return result;
  }
  void Write(items::ItemCategory category, std::size_t slot,
             const items::ContainerRecord& record) override {
    storage.Write(category, slot, record);
  }
  [[noreturn]] void Refuse(std::string_view reason) override {
    storage.Refuse(reason);
  }
};

struct StoredRow {
  framework::ItemRowText text{};
  rows::ItemRowStorage storage{};
  std::int32_t slot{};
  Records* records = nullptr;
  bool stored = false;
};

enum class Event : std::uint8_t { kPending, kStaging, kActive, kOrder, kStore };

struct World final : rows::ContainerItemRowsWorld {
  RowTextHost host;
  Records first;
  Records second;
  Records* active = &first;
  Records* staging = &second;
  Records* captured_active = nullptr;
  Records* captured_staging = nullptr;
  std::array<std::uint16_t, rows::kPendingTransferEntries> first_pending{};
  std::array<std::uint16_t, rows::kPendingTransferEntries> second_pending{};
  std::span<std::uint16_t> pending = first_pending;
  items::ContainerCapacities limits = items::kContainerCapacities;
  std::array<std::uint16_t, items::kContainerIndexEntries> order{};
  std::size_t order_limit = order.size();
  std::array<std::array<std::uint8_t, framework::kRowDescriptionBufferBytes>,
             kRowCount>
      descriptions{};
  std::array<std::uint32_t, kRowCount> types{};
  std::array<StoredRow, kRowCount> output{};
  std::function<void(Event)> callback;
  std::function<void(std::size_t)> on_store;
  std::size_t short_description = kRowCount;
  unsigned staging_queries = 0;
  unsigned active_queries = 0;
  unsigned order_queries = 0;

  void Call(Event event) {
    if (callback) callback(event);
  }
  std::span<std::uint16_t> PendingTransfers() override {
    Call(Event::kPending);
    return pending;
  }
  items::ContainerWorld& StagingRecords() override {
    ++staging_queries;
    Call(Event::kStaging);
    captured_staging = staging;
    return *staging;
  }
  std::uint16_t Capacity(items::ItemCategory category) override {
    return limits.at(static_cast<std::size_t>(category));
  }
  std::span<const std::uint16_t> CaptureOrder(
      items::ItemCategory category) override {
    ++order_queries;
    Call(Event::kOrder);
    active->storage.capacities = limits;
    const auto offset = items::ContainerCategoryIndices(category, *active);
    std::size_t end = 0;
    for (const auto capacity : items::kContainerCapacities) {
      end += capacity;
      if (end > offset) break;
    }
    end = std::min(end, std::min(order.size(), order_limit));
    if (offset >= end) return {};
    return std::span(order).subspan(offset, end - offset);
  }
  items::ContainerWorld& ActiveRecords() override {
    ++active_queries;
    Call(Event::kActive);
    captured_active = active;
    return *active;
  }
  framework::ItemTextHost& TextHost() override { return host; }
  std::span<std::uint8_t> Description(std::size_t row) override {
    auto buffer = std::span(descriptions.at(row));
    return row == short_description ? buffer.first(buffer.size() - 1) : buffer;
  }
  void StoreRow(const rows::ContainerRowOutput& value) override {
    output.at(value.row) = {
        .text = value.text,
        .storage = value.storage,
        .slot = value.slot,
        .records = value.storage == rows::ItemRowStorage::kStaged
                       ? captured_staging
                       : captured_active,
        .stored = true};
    types.at(value.row) = value.text.row_type;
    Call(Event::kStore);
    if (on_store) on_store(value.row);
  }
  void AddRowFlags(std::size_t row, std::uint32_t flags) override {
    types.at(row) |= flags;
  }
  void ReportItemStatus(items::ItemsStatus status) override {
    if (status != items::ItemsStatus::kOk) Refuse(items::Describe(status));
  }
  [[noreturn]] void Refuse(std::string_view reason) override {
    throw std::out_of_range(std::string(reason));
  }
  void Select(items::ItemCategory category,
              std::span<const std::uint16_t> tokens) {
    first.storage.capacities = limits;
    const auto offset = items::ContainerCategoryIndices(category, first);
    std::ranges::copy(tokens,
                      order.begin() + static_cast<std::ptrdiff_t>(offset));
  }
};

inline void Populate(Records& records) {
  auto& contents = records.storage.items;
  contents.weapons.front() = {.index = 1, .blade = 1, .grip = 1};
  contents.weapons.front().name[0] = 1;
  contents.weapons.front().name[1] =
      static_cast<char>(VS::Render::kTextTerminator);
  contents.blades.front() = {
      .id = 1, .category = 1, .type_bits = 1, .material = 1};
  contents.grips.front() = {.id = kGripId, .category = 1};
  contents.shields.front() = {
      .index = 1, .base = {.id = kShieldId, .category = 1, .material = 1}};
  contents.armor.front() = {.id = kArmorId, .category = 1, .material = 1};
  contents.gems.front().id = kGemId;
  contents.misc.front() = {.id = kMiscId, .count = 1};
}
}  // namespace container_pending_rows_tests
