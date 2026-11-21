#pragma once

#include <cstdint>
#include <optional>
#include <span>

#include "Items/ItemRecords.hpp"
#include "Menu/Framework/ItemRows.hpp"

// MENU4.PRG status components borrow MAINMENU item text and BATTLE string
// arguments, description buffer and rows. They own no persistent state.
namespace VS::Menu::Status {  // NOLINT(readability-identifier-naming)
class ComponentDisplay {
 public:
  virtual ~ComponentDisplay() = default;
  virtual void ShowBladeStats(const Items::WeaponLoadout& weapon) = 0;
  virtual void ShowGripStats(const Items::EquipmentPart& grip) = 0;
  virtual void ShowGemStats(const Items::EquipmentPart& gem) = 0;
  virtual void HideComponentBars() = 0;
  virtual void ClearComponentStats() = 0;
  // Selects an existing menu row; false when its workspace is absent.
  [[nodiscard]] virtual bool SelectRow(int row) = 0;
};

struct ComponentContext {
  Framework::ItemTextHost& text;
  std::span<std::uint8_t> buffer;
  ComponentDisplay& display;
};

using ComponentDescription = std::optional<Framework::TextRef>;

// Each refusal is reported through ItemTextHost::Refuse. Formatting changes
// only the description, never the caller's name or row-type output.
// decomp: initBladeDescription (src/MENU/MENU4.PRG/120.c:234; was
// _initBladeDescription)
[[nodiscard]] ComponentDescription FormatBladeDescription(
    Framework::ItemTextHost& text, const Items::EquipmentPart& blade,
    std::span<std::uint8_t> buffer);
// decomp: initGripInfo (src/MENU/MENU4.PRG/120.c:260; was _initGripInfo)
[[nodiscard]] ComponentDescription FormatGripDescription(
    Framework::ItemTextHost& text, const Items::EquipmentPart& grip,
    std::span<std::uint8_t> buffer);
// decomp: initGemInfo (src/MENU/MENU4.PRG/120.c:280; was _initGemInfo)
[[nodiscard]] ComponentDescription CopyGemDescription(
    Framework::ItemTextHost& text, const Items::EquipmentPart& gem,
    std::span<std::uint8_t> buffer);
// decomp: drawWeaponInfoRow (src/MENU/MENU4.PRG/120.c:297; was
// _drawWeaponInfoRow)
[[nodiscard]] ComponentDescription SelectWeaponComponent(
    ComponentContext& context, int row, const Items::WeaponLoadout& weapon);
// decomp: drawShieldInfoRow (src/MENU/MENU4.PRG/120.c:343; was
// _drawShieldInfoRow)
[[nodiscard]] ComponentDescription SelectShieldComponent(
    ComponentContext& context, int row, const Items::ShieldLoadout& shield);
// decomp: drawArmorInfoRow (src/MENU/MENU4.PRG/120.c:372; was
// _drawArmorInfoRow)
[[nodiscard]] ComponentDescription ArmorDescription(
    Framework::ItemTextHost& text, const Items::ArmorLoadout& armor,
    std::span<std::uint8_t> buffer);
// decomp: drawAccessoryInfoRow (src/MENU/MENU4.PRG/120.c:387; was
// _drawAccessoryInfoRow)
[[nodiscard]] ComponentDescription AccessoryDescription(
    Framework::ItemTextHost& text,
    const Items::AccessorySpawnLoadout& accessory,
    std::span<std::uint8_t> buffer);
// decomp: equipmentDetailElementSelectable (src/MENU/MENU4.PRG/120.c:405; was
// _equipmentDetailElementSelectable)
[[nodiscard]] bool IsDetailElementSelectable(int element, std::uint32_t flags,
                                             std::uint8_t stat_page);
}  // namespace VS::Menu::Status
