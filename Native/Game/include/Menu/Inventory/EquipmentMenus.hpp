#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "Menu/Framework/Framework.hpp"

// The equipment menu (MENU3.PRG 16C.c:343-1233): the four slot menus that
// list a category's items and equip the chosen one, the handler table that
// picks the slot menu for a top-menu row, and the top menu itself.
namespace VS::Menu::Inventory {
// _dispatchEquipMenu's handler table, in its order (16C.c:1030).
enum class EquipmentMenu : std::uint8_t {
  kArmor,
  kWeapon,
  kShield,
  kAccessory
};
inline constexpr std::size_t kEquipmentMenus = 4;

// The inventory tables the slot menus list.
enum class EquipmentTable : std::uint8_t { kWeapons, kShields, kArmor };
inline constexpr std::int32_t kEquipmentWeaponSlots = 8;
inline constexpr std::int32_t kEquipmentShieldSlots = 8;
inline constexpr std::int32_t kEquipmentBladeSlots = 16;
inline constexpr std::int32_t kEquipmentArmorSlots = 16;
// The choice list's "none" row and one row per listed item.
inline constexpr std::size_t kEquipmentChoiceRows = 17;
// The top menu's rows: weapon, shield, five armor parts and the accessory.
inline constexpr std::size_t kEquipmentTopRows = 8;
// The armor menu's body parts (vs_main_equipArmor's part + 1); 7 is the
// accessory's limb.
inline constexpr std::uint8_t kFirstBodyPart = 1;
inline constexpr std::uint8_t kLastBodyPart = 7;
inline constexpr std::uint8_t kAccessoryLimb = 7;
// vs_mainMenu_weaponHands' categories and the bare-handed category.
inline constexpr std::uint8_t kEquipmentWeaponCategories = 10;
inline constexpr std::uint8_t kUnarmedCategory = 10;
// _menuText's halfwords (items0.h).
inline constexpr std::size_t kEquipmentMenuTextEntries = 472;

// A slot menu's result once it closes: the chosen row + 1 (1 unequips),
// kEquipmentMenuBack after Cross, kEquipmentMenuQuit after Triangle; 0
// while the menu is open.
inline constexpr std::int32_t kEquipmentMenuUnequip = 1;
inline constexpr std::int32_t kEquipmentMenuBack = -1;
inline constexpr std::int32_t kEquipmentMenuQuit = -2;

// The slot menus' state byte (16C.c:343-1025).
enum class EquipmentSlotStep : std::uint8_t {
  kInit,
  kHandleInput,
  kLoadObject,
  kWaitMenuReady,
};
// _equipMenu's state byte (16C.c:1054).
enum class EquipmentMenuStep : std::uint8_t {
  kInit,
  kInitMenu,
  kTopMenu,
  kQuit,
  kInitSubMenu,
  kSubMenu,
};

// Each menu's statics. `step` keeps the raw byte: a value outside the steps
// leaves the menu idle, as the original's switch does.
struct AccessoryMenuState {
  std::int32_t selected = 0;
  std::uint8_t step = 0;
  // The 1-based armor slot equipped on entry, or 0.
  std::uint8_t equipped_accessory = 0;
};
struct WeaponMenuState {
  std::int32_t selected = 0;
  std::uint8_t step = 0;
  // _loadObjectData's arguments for the chosen weapon.
  std::uint8_t category = 0;
  std::uint8_t model_id = 0;
  std::uint8_t material = 0;
};
struct ShieldMenuState {
  std::int32_t selected = 0;
  std::uint8_t step = 0;
  std::uint8_t model_id = 0;
  std::uint8_t material = 0;
};
struct ArmorMenuState {
  std::int32_t selected = 0;
  std::uint8_t step = 0;
  // The 1-based armor slot equipped on entry, or 0.
  std::uint8_t armor_index = 0;
  std::uint8_t body_part = 0;
};
struct EquipmentMenuState {
  std::int32_t selected_row = 0;
  std::uint8_t step = 0;
};

// An inventory blade's preview model (category, wepId, material's low byte).
struct BladeModel {
  std::uint8_t category = 0;
  std::uint8_t model_id = 0;
  std::uint8_t material = 0;
};
// A shield's preview model (base.wepId, base.material's low byte).
struct ShieldModel {
  std::uint8_t model_id = 0;
  std::uint8_t material = 0;
};
// The blade Ashley holds (vs_battle_characterState->stats).
struct HeldBlade {
  std::uint16_t id = 0;
  std::uint8_t category = 0;
};

// The inventory, menu rows, panels and equipment calls the menus make. Slots
// are 0-based; the menus check them against the table sizes above before
// asking.
class EquipmentMenusHost {
 public:
  virtual ~EquipmentMenusHost() = default;

  // vs_mainMenu_countItems(category, NULL).
  [[nodiscard]] virtual std::int32_t CountItems(std::int32_t category) = 0;
  // The 1-based slot the table's index list holds at `position`, 0 for none.
  [[nodiscard]] virtual std::uint8_t ListedSlot(EquipmentTable table,
                                                std::int32_t position) = 0;
  // A weapon's or shield's isEquipped byte.
  [[nodiscard]] virtual bool IsEquipped(EquipmentTable table,
                                        std::int32_t slot) = 0;
  virtual void MarkEquipped(EquipmentTable table, std::int32_t slot) = 0;
  [[nodiscard]] virtual std::uint8_t ArmorLimb(std::int32_t slot) = 0;
  [[nodiscard]] virtual std::uint8_t ArmorCategory(std::int32_t slot) = 0;
  virtual void SetArmorLimb(std::int32_t slot, std::uint8_t limb) = 0;
  // A weapon's 1-based blade slot.
  [[nodiscard]] virtual std::uint8_t WeaponBlade(std::int32_t slot) = 0;
  [[nodiscard]] virtual BladeModel Blade(std::int32_t slot) = 0;
  [[nodiscard]] virtual ShieldModel Shield(std::int32_t slot) = 0;
  // nullopt without Ashley's battle stats.
  [[nodiscard]] virtual std::optional<HeldBlade> HeldWeapon() = 0;
  // vs_mainMenu_weaponHands[category - 1] for a category in 1..10.
  [[nodiscard]] virtual bool IsTwoHanded(std::uint8_t category) = 0;

  // vs_mainMenu_rowsIdle.
  [[nodiscard]] virtual bool RowsIdle() = 0;
  // A row's name and description become _menuText at the two halfword
  // offsets (the menus check them against kEquipmentMenuTextEntries).
  virtual void SetRowText(std::int32_t row, std::uint16_t name,
                          std::uint16_t description) = 0;
  // _menuText[index] for an index below kEquipmentMenuTextEntries.
  [[nodiscard]] virtual std::uint16_t MenuTextEntry(std::size_t index) = 0;
  // Formats the slot into row `row`'s texts and 96-byte description buffer
  // (vs_mainMenu_setUi*); returns the row's flags.
  [[nodiscard]] virtual std::int32_t FormatRow(EquipmentTable table,
                                               std::int32_t slot,
                                               std::int32_t row) = 0;
  // _populateMenuRows over the first flags.size() rows.
  virtual void CreateChoiceRows(std::span<const std::int32_t> flags) = 0;
  // _processEquipSubMenu.
  virtual void UpdateChoices(std::int32_t has_sfx) = 0;
  // _getSelectedItemRow.
  [[nodiscard]] virtual std::int32_t SelectedChoice() = 0;
  // _equippedItem: the highlighted row's 1-based slot, 0 for "none".
  [[nodiscard]] virtual std::uint8_t HighlightedItem() = 0;

  // The stat preview of a 1-based slot or 0: vs_mainMenu_setArmorStats,
  // setWeaponStats, setShieldStats or _populateUiAccessory.
  virtual void PreviewItem(EquipmentMenu menu, std::int32_t item) = 0;
  virtual void UpdateStatPages(std::int32_t command) = 0;
  virtual void UpdateDpPpBars(std::int32_t flags) = 0;
  virtual void UpdatePrimaryStatPanel(std::int32_t animate) = 0;
  virtual void ClearMenuExcept(std::int32_t row) = 0;
  virtual void FlyOutRowLeft(std::int32_t row) = 0;

  virtual void UnequipWeapon() = 0;
  virtual void UnequipShield() = 0;
  virtual void EquipWeapon(std::int32_t slot) = 0;
  virtual void EquipShield(std::int32_t slot) = 0;
  // vs_main_equipArmor(part, armor or NULL).
  virtual void EquipArmor(std::int32_t part,
                          std::optional<std::int32_t> slot) = 0;
  virtual void EquipAccessory(std::optional<std::int32_t> slot) = 0;
  // _loadObjectData; true while the preview reload continues.
  [[nodiscard]] virtual bool ReloadModels(std::int32_t category,
                                          std::int32_t model_id,
                                          std::int32_t material) = 0;

  // vs_battle_setMenuItem with _menuText at a halfword offset.
  [[nodiscard]] virtual Framework::MenuRow& SetMenuItem(
      const Framework::RowPlacement& placement, std::uint16_t text) = 0;
  // vs_mainmenu_setMenuRows over the top menu's rows.
  virtual void CreateTopRows(
      std::span<const std::int32_t, kEquipmentTopRows> flags,
      std::int32_t row_info) = 0;
  [[nodiscard]] virtual bool SquarePressed() = 0;
  virtual void PlayInvalidSound() = 0;
  [[nodiscard]] virtual std::int32_t SelectedRow() = 0;
  [[nodiscard]] virtual std::int32_t HighlightedRow() = 0;
  virtual void StartTransition(std::int32_t row) = 0;
  virtual void ContinueTransition(std::int32_t row) = 0;
  // _ashleyRenderState, _itemCategoryIconsEnabled, _statusCommandState and
  // _selectedItemCategoryIconOnTop.
  virtual void SetPreviewState(std::int32_t state) = 0;
  virtual void SetCategoryIconsEnabled(std::uint8_t enabled) = 0;
  virtual void SetStatusCommandState(std::uint8_t state) = 0;
  virtual void SetCategoryIconOnTop(std::int32_t icon) = 0;
  // _dispatchEquipMenu.
  [[nodiscard]] virtual std::int32_t Dispatch(std::int32_t row) = 0;
};

// Each slot menu is called with a nonzero argument to open (the armor menu's
// is the body part) and with 0 each frame after; it returns 0 until it
// closes. nullopt is a refusal: a count, listed slot or chosen item outside
// its table, a body part outside 1..7, a weapon whose blade or category lies
// outside the tables, or a shield without Ashley's stats. A refusal on entry
// changes nothing; one after the list's input leaves the menu waiting.

// decomp: MENU3.PRG equipAccessoryMenu (src/MENU/MENU3.PRG/16C.c:338; was
// _equipAccessoryMenu)
[[nodiscard]] std::optional<std::int32_t> UpdateAccessoryMenu(
    AccessoryMenuState& state, EquipmentMenusHost& host,
    std::int32_t initialize);
// decomp: MENU3.PRG equipWeaponMenu (src/MENU/MENU3.PRG/16C.c:498; was
// _equipWeaponMenu)
[[nodiscard]] std::optional<std::int32_t> UpdateWeaponMenu(
    WeaponMenuState& state, EquipmentMenusHost& host, std::int32_t initialize);
// decomp: MENU3.PRG equipShieldMenu (src/MENU/MENU3.PRG/16C.c:678; was
// _equipShieldMenu)
[[nodiscard]] std::optional<std::int32_t> UpdateShieldMenu(
    ShieldMenuState& state, EquipmentMenusHost& host, std::int32_t initialize);
// decomp: MENU3.PRG equipArmorMenu (src/MENU/MENU3.PRG/16C.c:851; was
// _equipArmorMenu)
[[nodiscard]] std::optional<std::int32_t> UpdateArmorMenu(
    ArmorMenuState& state, EquipmentMenusHost& host, std::int32_t body_part);

// _dispatchEquipMenu's handlers, called with the top-menu row (the armor
// part for armor) to open and with 0 each frame after.
using EquipmentMenuHandler = int (*)(int);
using EquipmentMenuHandlers = std::array<EquipmentMenuHandler, kEquipmentMenus>;

// decomp: MENU3.PRG dispatchEquipMenu (src/MENU/MENU3.PRG/16C.c:1023; was
// _dispatchEquipMenu) Row 1 opens the weapon menu, 2 the shield's, 3-7 the
// armor's (part 1-5) and 8 the accessory's; 0 updates the open one through
// `handlers`. nullopt (nothing changed) for a row outside 0..8 or an unbound
// handler.
[[nodiscard]] std::optional<std::int32_t> DispatchEquipmentMenu(
    std::uint8_t& current, const EquipmentMenuHandlers& handlers,
    std::int32_t row);

// decomp: MENU3.PRG equipMenu (src/MENU/MENU3.PRG/16C.c:1049; was _equipMenu)
// Returns 0 until the menu closes, then the top menu's selection
// (kEquipmentMenuBack or kEquipmentMenuQuit). nullopt is a refusal: a
// highlighted row outside the menu or a _menuText offset outside the table.
[[nodiscard]] std::optional<std::int32_t> UpdateEquipmentMenu(
    EquipmentMenuState& state, EquipmentMenusHost& host,
    std::int32_t initialize);
}  // namespace VS::Menu::Inventory
