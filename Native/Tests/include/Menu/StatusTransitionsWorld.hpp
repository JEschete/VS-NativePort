#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "Items/ItemRecords.hpp"
#include "Menu/Framework/Framework.hpp"
#include "Menu/Framework/ItemRows.hpp"
#include "Menu/Status/DetailTransitions.hpp"
#include "Menu/StatusComponentsWorld.hpp"

namespace vs_test::status_transitions {
namespace Framework = VS::Menu::Framework;
namespace Items = VS::Items;
namespace status = VS::Menu::Status;

inline constexpr std::size_t kStatusBytes = 3520;
inline constexpr std::size_t kOriginalRowBytes = 64;
inline constexpr std::size_t kRowPrefixBytes = 60;
inline constexpr std::uint32_t kStatusAddress = 0x80107448;
inline constexpr std::uint32_t kSubTextAddress = 0x80460000;
inline constexpr std::array<char, 1> kSubText{'x'};

enum class Event : std::uint32_t {
  kFlyRight = 1,
  kCreate = 2,
  kDeactivate = 3,
  kStatPages = 4,
  kPrimary = 5,
  kLeave = 6,
  kClear = 7,
  kFlyLeft = 8,
  kBars = 9,
};

inline std::array<std::uint8_t, kOriginalRowBytes> RowBytes(
    const Framework::MenuRow& row) {
  static_assert(offsetof(Framework::MenuRow, text) == 28);
  const auto native = std::as_bytes(std::span(&row, 1));
  std::array<std::uint8_t, kOriginalRowBytes> bytes{};
  for (std::size_t index = 0; index < kRowPrefixBytes; ++index) {
    bytes[index] = std::to_integer<std::uint8_t>(native[index]);
  }
  const auto pointer = row.sub_text == nullptr ? 0U : kSubTextAddress;
  for (std::size_t index = 0; index < sizeof(pointer); ++index) {
    bytes[kRowPrefixBytes + index] =
        static_cast<std::uint8_t>(pointer >> (index * 8));
  }
  return bytes;
}

class World final : public Framework::ItemRowsHost,
                    public status::DetailTransitionHost {
 public:
  explicit World(std::uint32_t seed = 0) : text(seed) {
    status_text.fill(status_components::kEnd);
    for (std::size_t entry = 0; entry < 12; ++entry) {
      const auto offset = 64 + entry * 8;
      status_text[entry * 2] = static_cast<std::uint8_t>(offset / 2);
      status_text[entry * 2 + 1] = 0;
      status_text[offset] = static_cast<std::uint8_t>(1 + (seed + entry) % 220);
      status_text[offset + 1] = static_cast<std::uint8_t>(1 + entry);
      status_text[offset + 2] = status_components::kEnd;
    }
    for (std::size_t index = 0; index < rows.size(); ++index) {
      // Only the pointer widens; compare the original 60-byte value prefix
      // and the normalized borrowed sub-text pointer separately.
      auto bytes = std::as_writable_bytes(std::span(&rows[index], 1));
      for (std::size_t byte = 0; byte < kRowPrefixBytes; ++byte) {
        bytes[byte] =
            static_cast<std::byte>((seed + index * 13 + byte * 7) & 255);
      }
      rows[index].sub_text = kSubText.data();
    }
  }

  status_components::World text;
  std::array<Framework::MenuRow, Framework::kRowCount> rows{};
  std::array<std::uint8_t, kStatusBytes> status_text{};
  std::vector<std::uint32_t> events;
  std::size_t row_extent = Framework::kRowCount;
  std::size_t status_extent = kStatusBytes;
  bool refuse_creation = false;

  void Refuse(std::string_view reason) override { text.Refuse(reason); }
  std::span<Framework::MenuRow> Rows() override {
    return std::span(rows).first(row_extent);
  }
  std::span<const std::uint8_t> StatusText() override {
    return std::span(status_text).first(status_extent);
  }
  std::span<const std::uint8_t> HelpText() override { return text.HelpText(); }
  std::span<const std::uint8_t> NameText() override { return text.NameText(); }
  std::span<const std::uint8_t> WeaponHands() override {
    return text.WeaponHands();
  }
  VS::Render::EncodedTextArguments TextArguments() override {
    return text.TextArguments();
  }
  void SetStringArgument(std::size_t slot,
                         std::span<const std::uint8_t> value) override {
    text.SetStringArgument(slot, value);
  }
  std::span<std::uint8_t> StringBuffer() override { return text.buffer; }
  std::optional<Items::WeaponLoadout> InventoryWeaponLoadout(
      const Items::InventoryWeapon& weapon) override {
    return text.InventoryWeaponLoadout(weapon);
  }
  std::optional<Items::ShieldLoadout> InventoryShieldLoadout(
      const Items::InventoryShield& shield) override {
    return text.InventoryShieldLoadout(shield);
  }
  std::optional<Items::InventoryWeapon> Weapon(int) override {
    return UnexpectedInventory<Items::InventoryWeapon>();
  }
  std::optional<Items::InventoryBlade> Blade(int) override {
    return UnexpectedInventory<Items::InventoryBlade>();
  }
  std::optional<Items::InventoryGrip> Grip(int) override {
    return UnexpectedInventory<Items::InventoryGrip>();
  }
  std::optional<Items::InventoryShield> Shield(int) override {
    return UnexpectedInventory<Items::InventoryShield>();
  }
  std::optional<Items::InventoryArmor> Armor(int) override {
    return UnexpectedInventory<Items::InventoryArmor>();
  }
  std::optional<Items::InventoryGem> Gem(int) override {
    return UnexpectedInventory<Items::InventoryGem>();
  }

  Framework::MenuRow* CreateTitle(
      const Framework::RowPlacement& placement,
      std::span<const std::uint8_t> value) override {
    const auto offset = value.data() - status_text.data();
    return Create(placement,
                  kStatusAddress + static_cast<std::uint32_t>(offset), value);
  }
  Framework::MenuRow* SetMenuItem(const Framework::RowPlacement& placement,
                                  Framework::TextRef value) override {
    const auto storage = value.source == Framework::TextSource::kHelp
                             ? text.HelpText()
                             : text.NameText();
    return Create(placement, text.Address(value),
                  storage.subspan(value.offset));
  }
  void DeactivateRow(int row) override {
    EventArgument(Event::kDeactivate, row);
  }
  void FlyOutRight(int row) override { EventArgument(Event::kFlyRight, row); }
  void SetStatPages(int command) override {
    EventArgument(Event::kStatPages, command);
  }
  void AnimatePrimaryStats(int command) override {
    EventArgument(Event::kPrimary, command);
  }
  void PlayLeaveSound() override {
    events.push_back(static_cast<std::uint32_t>(Event::kLeave));
  }
  void ClearRowsExcept(int row) override { EventArgument(Event::kClear, row); }
  void FlyOutLeft(int row) override { EventArgument(Event::kFlyLeft, row); }
  void AnimateDpPpBars(int command) override {
    EventArgument(Event::kBars, command);
  }

 private:
  template <typename Record>
  std::optional<Record> UnexpectedInventory() {
    Refuse("status transition unexpectedly looked up inventory ownership");
    return std::nullopt;
  }

  void EventArgument(Event kind, int argument) {
    events.insert(events.end(), {static_cast<std::uint32_t>(kind),
                                 static_cast<std::uint32_t>(argument)});
  }

  Framework::MenuRow* Create(const Framework::RowPlacement& placement,
                             std::uint32_t address,
                             std::span<const std::uint8_t> value) {
    const auto terminator = std::ranges::find(value, status_components::kEnd);
    if (terminator == value.end()) {
      Refuse("test row text has no terminator");
      return nullptr;
    }
    const auto length =
        static_cast<std::size_t>(terminator - value.begin()) + 1;
    events.insert(events.end(),
                  {static_cast<std::uint32_t>(Event::kCreate),
                   static_cast<std::uint32_t>(placement.row),
                   static_cast<std::uint32_t>(placement.x),
                   static_cast<std::uint32_t>(placement.y),
                   static_cast<std::uint32_t>(placement.w),
                   static_cast<std::uint32_t>(placement.background_width),
                   address, static_cast<std::uint32_t>(length)});
    const auto contents = value.first(length);
    events.insert(events.end(), contents.begin(), contents.end());
    events.insert(events.end(), text.buffer.begin(), text.buffer.end());
    for (const auto argument : text.arguments.strings) {
      events.push_back(text.Address(argument));
    }
    if (refuse_creation || placement.row < 0 ||
        static_cast<std::size_t>(placement.row) >= row_extent) {
      return nullptr;
    }
    return &rows[static_cast<std::size_t>(placement.row)];
  }
};

inline Items::WeaponLoadout Weapon() {
  auto weapon = status_components::Weapon();
  weapon.grip.gem_slots = 3;
  return weapon;
}

inline Items::ShieldLoadout Shield() {
  Items::ShieldLoadout shield{};
  shield.base.gem_slots = 3;
  shield.gems[0].id = 261;
  shield.gems[2].id = 322;
  return shield;
}
}  // namespace vs_test::status_transitions
