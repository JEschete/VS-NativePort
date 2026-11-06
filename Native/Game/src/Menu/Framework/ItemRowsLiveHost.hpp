#pragma once
// Hand-maintained (tools/port/owned.csv).
// The item rows' Live host: ItemRowsLive.cpp implements it and the equipment
// details' Live binding (EquipmentDetailsLive.cpp) reuses it.

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

#include "Items/ItemRecords.hpp"
#include "Menu/Framework/ItemRows.hpp"
#include "Render/EncodedText.hpp"

namespace VS::Menu::Framework {
// ItemRowsHost over the item texts (ItemRowsLiveState.h), the tables
// vs_mainMenu_weapons .. vs_mainMenu_gems point at (Ashley's inventory or the
// open container), vs_battle_stringContext, vs_battle_stringBuf and the menu
// rows. Refusals are logged on the "menu.framework" channel.
class LiveItemRowsHost final : public ItemRowsHost {
 public:
  void Refuse(std::string_view what) override;
  [[nodiscard]] std::span<const std::uint8_t> HelpText() override;
  [[nodiscard]] std::span<const std::uint8_t> NameText() override;
  [[nodiscard]] std::span<const std::uint8_t> WeaponHands() override;
  [[nodiscard]] Render::EncodedTextArguments TextArguments() override;
  void SetStringArgument(std::size_t slot,
                         std::span<const std::uint8_t> text) override;
  [[nodiscard]] std::optional<Items::WeaponLoadout> InventoryWeaponLoadout(
      const Items::InventoryWeapon& weapon) override;
  [[nodiscard]] std::optional<Items::ShieldLoadout> InventoryShieldLoadout(
      const Items::InventoryShield& shield) override;
  [[nodiscard]] std::optional<Items::InventoryWeapon> Weapon(
      int reference) override;
  [[nodiscard]] std::optional<Items::InventoryBlade> Blade(
      int reference) override;
  [[nodiscard]] std::optional<Items::InventoryGrip> Grip(
      int reference) override;
  [[nodiscard]] std::optional<Items::InventoryShield> Shield(
      int reference) override;
  [[nodiscard]] std::optional<Items::InventoryArmor> Armor(
      int reference) override;
  [[nodiscard]] std::optional<Items::InventoryGem> Gem(int reference) override;
  [[nodiscard]] std::span<std::uint8_t> StringBuffer() override;
  void DeactivateRow(int row) override;
  MenuRow* SetMenuItem(const RowPlacement& placement, TextRef text) override;

  // The bytes `text` names: the empty text for kEmpty, nothing for kNone and
  // kWeaponName (the caller holds the weapon).
  [[nodiscard]] std::span<const std::uint8_t> TextBytes(TextRef text);
};
}  // namespace VS::Menu::Framework
