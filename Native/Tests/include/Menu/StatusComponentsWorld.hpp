#pragma once

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "Items/ItemRecords.hpp"
#include "Menu/Framework/Framework.hpp"
#include "Menu/Framework/ItemRows.hpp"
#include "Menu/Status/ComponentDescriptions.hpp"
#include "Render/EncodedText.hpp"

namespace vs_test::status_components {
namespace Framework = VS::Menu::Framework;
namespace status = VS::Menu::Status;
namespace Items = VS::Items;
namespace Render = VS::Render;

inline constexpr std::uint32_t kHelpAddress = 0x80500000;
inline constexpr std::uint32_t kNamesAddress = 0x80600000;
inline constexpr std::uint32_t kBufferAddress = 0x80420000;
inline constexpr std::uint32_t kItemAddress = 0x80410000;
inline constexpr std::size_t kEntryCount = 960;
inline constexpr std::size_t kEntryBase = 2048;
inline constexpr std::size_t kEntryStride = 16;
inline constexpr std::uint8_t kEnd = Render::kTextTerminator;

class World final : public Framework::ItemTextHost,
                    public status::ComponentDisplay {
 public:
  explicit World(std::uint32_t seed = 0) { Initialize(seed); }
  std::array<std::uint8_t, Framework::kItemHelpBytes> help{};
  std::array<std::uint8_t, Framework::kItemNamesBytes> names{};
  std::array<std::uint8_t, 10> hands{};
  std::array<std::uint8_t, Framework::kRowDescriptionBufferBytes> buffer{};
  Render::EncodedTextArguments arguments{};
  std::array<std::uint8_t, Framework::kRowCount> selected{};
  std::vector<std::string> refusals;
  std::vector<std::uint32_t> events;
  std::vector<std::size_t> string_slots;
  const void* origin = nullptr;
  bool rows_available = true;
  std::size_t help_extent = Framework::kItemHelpBytes;
  std::size_t names_extent = Framework::kItemNamesBytes;

  status::ComponentContext Context() { return {*this, buffer, *this}; }
  void Refuse(std::string_view reason) override {
    refusals.emplace_back(reason);
  }
  std::span<const std::uint8_t> HelpText() override {
    return std::span(help).first(help_extent);
  }
  std::span<const std::uint8_t> NameText() override {
    return std::span(names).first(names_extent);
  }
  std::span<const std::uint8_t> WeaponHands() override { return hands; }
  Render::EncodedTextArguments TextArguments() override { return arguments; }
  void SetStringArgument(std::size_t slot,
                         std::span<const std::uint8_t> text) override {
    arguments.strings.at(slot) = text;
    string_slots.push_back(slot);
  }
  std::optional<Items::WeaponLoadout> InventoryWeaponLoadout(
      const Items::InventoryWeapon&) override {
    Refuse("unexpected inventory weapon lookup");
    return std::nullopt;
  }
  std::optional<Items::ShieldLoadout> InventoryShieldLoadout(
      const Items::InventoryShield&) override {
    Refuse("unexpected inventory shield lookup");
    return std::nullopt;
  }

  void ShowBladeStats(const Items::WeaponLoadout& weapon) override {
    Record(1, weapon);
  }
  void ShowGripStats(const Items::EquipmentPart& grip) override {
    Record(2, grip);
  }
  void ShowGemStats(const Items::EquipmentPart& gem) override {
    Record(3, gem);
  }
  void HideComponentBars() override { events.insert(events.end(), {4, 8}); }
  void ClearComponentStats() override { events.push_back(5); }
  bool SelectRow(int row) override {
    events.insert(events.end(), {6, static_cast<std::uint32_t>(row)});
    if (!rows_available || row < 0 ||
        static_cast<std::size_t>(row) >= selected.size()) {
      return false;
    }
    selected[static_cast<std::size_t>(row)] = 1;
    return true;
  }

  void Entry(std::size_t index, std::size_t halfword) {
    help.at(index * 2) = static_cast<std::uint8_t>(halfword);
    help.at(index * 2 + 1) = static_cast<std::uint8_t>(halfword >> 8);
  }
  std::span<const std::uint8_t> EntryText(std::size_t index) const {
    return std::span(help).subspan(kEntryBase + index * kEntryStride);
  }
  std::uint32_t Address(std::span<const std::uint8_t> value) const {
    if (value.empty()) {
      return 0;
    }
    const auto pointer = reinterpret_cast<std::uintptr_t>(value.data());
    const auto in = [pointer](auto& storage, std::uint32_t base) {
      const auto begin = reinterpret_cast<std::uintptr_t>(storage.data());
      return pointer >= begin && pointer - begin < storage.size()
                 ? base + static_cast<std::uint32_t>(pointer - begin)
                 : 0U;
    };
    if (const auto address = in(help, kHelpAddress)) {
      return address;
    }
    if (const auto address = in(names, kNamesAddress)) {
      return address;
    }
    return in(buffer, kBufferAddress);
  }
  std::uint32_t Address(status::ComponentDescription value) const {
    if (!value) {
      return 0;
    }
    const auto offset = static_cast<std::uint32_t>(value->offset);
    switch (value->source) {
      case Framework::TextSource::kHelp:
        return kHelpAddress + offset;
      case Framework::TextSource::kNames:
        return kNamesAddress + offset;
      case Framework::TextSource::kBuffer:
        return kBufferAddress + offset;
      default:
        return 0;
    }
  }

 private:
  void Initialize(std::uint32_t seed) {
    help.fill(kEnd);
    names.fill(kEnd);
    for (std::size_t entry = 0; entry < kEntryCount; ++entry) {
      const auto start = kEntryBase + entry * kEntryStride;
      Entry(entry, start / 2);
      for (std::size_t byte = 0; byte < kEntryStride; ++byte) {
        help[start + byte] =
            static_cast<std::uint8_t>(1 + (seed + entry * 13 + byte * 7) % 220);
      }
      help[start + 3 + entry % 4] = kEnd;
    }
    for (std::size_t row = 0; row < names.size() / Framework::kItemNameRowBytes;
         ++row) {
      const auto start = row * Framework::kItemNameRowBytes;
      names[start] = static_cast<std::uint8_t>(1 + (row + seed) % 220);
      names[start + 1] = static_cast<std::uint8_t>(1 + row / 220);
    }
    Template(Framework::item_text::kWeaponDescTemplateOffset,
             {0xFF, 9, 0x10, 0xFF, 8, 0x11, 0xFF, 7, 0x12, 0xFF, 6, kEnd});
    Template(Framework::item_text::kGripTemplateOffset, {0x19, 0xFF, 5, kEnd});
    Template(Framework::item_text::kClassTemplateOffset,
             {0xFF, 0, 0x13, 0xFF, 1, kEnd});
    Template(Framework::item_text::kArmorTemplateOffset,
             {0xFF, 8, 0x14, 0xFF, 9, kEnd});
    Template(Framework::item_text::kNoGemsOffset, {0x31, 0x32, kEnd});
    for (std::size_t index = 0; index < hands.size(); ++index) {
      hands[index] = static_cast<std::uint8_t>(seed + index * 29);
      arguments.strings[index] = EntryText(900 + index);
    }
    for (std::size_t index = 0; index < buffer.size(); ++index) {
      buffer[index] = static_cast<std::uint8_t>(seed + index * 3);
    }
    for (std::size_t index = 0; index < selected.size(); ++index) {
      selected[index] = static_cast<std::uint8_t>(seed + index * 5);
    }
  }
  void Template(std::size_t halfword,
                std::initializer_list<std::uint8_t> bytes) {
    std::ranges::copy(bytes,
                      help.begin() + static_cast<std::ptrdiff_t>(
                                         halfword * sizeof(std::uint16_t)));
  }
  template <typename RecordType>
  void Record(std::uint32_t kind, const RecordType& record) {
    const auto pointer = reinterpret_cast<std::uintptr_t>(&record);
    const auto base = reinterpret_cast<std::uintptr_t>(origin);
    const auto address =
        kItemAddress + static_cast<std::uint32_t>(pointer - base);
    const auto bytes =
        std::bit_cast<std::array<std::uint8_t, sizeof(RecordType)>>(record);
    events.insert(events.end(),
                  {kind, address, static_cast<std::uint32_t>(bytes.size())});
    events.insert(events.end(), bytes.begin(), bytes.end());
    events.insert(events.end(), buffer.begin(), buffer.end());
    for (const auto argument : arguments.strings) {
      events.push_back(Address(argument));
    }
  }
};

inline Items::EquipmentPart Blade() {
  return {.id = 17, .category = 9, .damage_type = 3, .material = 6};
}
inline Items::WeaponLoadout Weapon() {
  Items::WeaponLoadout weapon{};
  weapon.blade = Blade();
  weapon.grip.id = 101;
  weapon.grip.category = 4;
  weapon.gems[0].id = 261;
  weapon.gems[2].id = 322;
  return weapon;
}
}  // namespace vs_test::status_components
