#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>

#include "Items/ItemRecords.hpp"
#include "Menu/Framework/Framework.hpp"

// The inventory item lists' row materialization (MENU/items2.h, compiled into
// MENU3.PRG and MENUB.PRG): the parent links of blades, grips and gems, the
// list rows built from the display order, and the list restored after a
// sub-menu closes.
namespace VS::Menu::Inventory {
// Row flags the lists OR into a formatted row's type: an equipped item, and a
// part held by an assembled weapon or shield that is not equipped.
inline constexpr std::uint32_t kEquippedRowFlags = 0xCA00;
inline constexpr std::uint32_t kAssembledRowFlags = 0xCC00;
// vs_mainMenu_miscItemToActionMap covers misc items 323 .. 456.
inline constexpr std::uint16_t kFirstMiscActionItem = 323;
inline constexpr std::size_t kMiscActionMapEntries = 134;
// decomp: vs_mainMenu_miscItemToActionMap (src/MENU/MAINMENU.PRG/2D10.c:1571):
// the action each misc item uses, 0 for none.
using MiscItemActionMap = std::array<std::uint8_t, kMiscActionMapEntries>;
inline constexpr MiscItemActionMap kMiscItemToActionMap{
    0xE0, 0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xF3, 0xF4, 0xF5, 0xF6,
    0xE8, 0xE9, 0xEA, 0xEB, 0xEC, 0xED, 0xEF, 0xEE, 0xF0, 0xF2, 0xF7, 0xF8,
    0xF9, 0xFA, 0xFB, 0xFC, 0xF8, 0xF9, 0xFA, 0xFB, 0xFC, 0xF1, 0x00, 0xFE,
    0xFD, 0xFB, 0xF4, 0xFE, 0xFB, 0xF6, 0xF3, 0xF3, 0xFC, 0xFD, 0xFF, 0xFB,
    0xF5, 0xFE, 0xFD, 0xFF, 0xFD, 0xFD, 0xFF, 0xFD, 0xFE, 0xFE, 0xFF, 0xFE,
    0xFC, 0xFF, 0xFD, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69,
    0x6A, 0x00, 0x00, 0x00, 0x6E, 0x00, 0x00, 0x00, 0x72, 0x00, 0x00, 0x00,
    0x76, 0x00, 0x00, 0x00, 0x7A, 0x00, 0x00, 0x00, 0x7E, 0x00, 0x00, 0x00,
    0x82, 0x00, 0x00, 0x00, 0x5E, 0x5F, 0x60, 0x57, 0x58, 0x5A, 0x5B, 0x4D,
    0x3C, 0x36, 0x3D, 0x37, 0x3E, 0x38, 0x3F, 0x39, 0x4B, 0x4C, 0x52, 0x53,
    0x55, 0x4F, 0x50, 0x5C, 0x5D, 0x3A, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47,
    0x48, 0x49,
};
// miscActions (MENU3.PRG 16C.c:24) holds one action per misc row.
inline constexpr std::size_t kMiscActionRows = 64;
// Actions 106 .. 133 step through up to three unlocked successors.
inline constexpr std::uint32_t kFirstUpgradableAction = 106;
inline constexpr std::uint32_t kUpgradableActionCount = 28;
inline constexpr std::int32_t kActionUpgradeSteps = 3;
// The list rows the restore walks, and the cursor memory each category keeps.
inline constexpr std::int32_t kFirstItemListRow = 20;
inline constexpr std::int32_t kItemListCursorBase = 30;
inline constexpr std::uint8_t kItemListAnimationComplete = 16;

// MENU3.PRG defines _ITEMMENU and records each misc row's action; MENUB.PRG
// (the loot screen) compiles the same header without it.
enum class ItemRowsContext : std::uint8_t { kItemMenu, kLoot };

enum class ItemRowsRefusal : std::uint8_t {
  // An order byte the list would read lies outside the order table.
  kOrder,
  // A one-based order entry names a slot outside its inventory table.
  kSlot,
  // A misc item outside vs_mainMenu_miscItemToActionMap.
  kMiscItem,
  // A blade, grip or gem names a weapon outside the weapon table.
  kParentWeapon,
  // A gem names a shield outside the shield table.
  kParentShield,
  // More rows than the caller's text, type and description buffers hold.
  kRows,
  // More misc rows than _miscActions holds.
  kMiscRows,
  // The restored list cursor lies outside the menu memory.
  kCursorMemory,
  // A restored row is missing.
  kRow,
  // Advancing the list animation made no progress.
  kStalled,
};

// decomp: MENU3.PRG _getParentWeaponIndex (items2.h:11). The one-based weapon
// holding a blade or grip, or a gem set in a weapon; 0 for anything else.
// Returns nullopt when a category that reads a table names a slot outside it.
[[nodiscard]] std::optional<std::uint16_t> ParentWeaponIndex(
    std::int32_t category, std::int32_t index,
    const Items::InventoryContents& inventory);
// decomp: MENU3.PRG _getParentShieldIndex (items2.h:38). The one-based shield
// holding a gem (kShieldGemFlag set), or 0.
[[nodiscard]] std::optional<std::uint8_t> ParentShieldIndex(
    std::int32_t category, std::int32_t index,
    const Items::InventoryContents& inventory);

struct ItemRowsInput {
  Items::ItemCategory category = Items::ItemCategory::kWeapon;
  ItemRowsContext context = ItemRowsContext::kItemMenu;
  // The category's one-based display order; 0 ends the list.
  std::span<const std::uint8_t> order;
  std::uint8_t capacity = 0;
  std::span<const std::uint8_t> misc_action_map;
  // Rows the caller's menu text, row type and description buffers hold.
  std::size_t row_capacity = 0;
};

// The row buffers and formatters a list writes. Rows are list positions;
// slots are zero-based inventory table indices.
class ItemRowsWorld {
 public:
  ItemRowsWorld() = default;
  ItemRowsWorld(const ItemRowsWorld&) = delete;
  ItemRowsWorld& operator=(const ItemRowsWorld&) = delete;
  ItemRowsWorld(ItemRowsWorld&&) = delete;
  ItemRowsWorld& operator=(ItemRowsWorld&&) = delete;
  virtual ~ItemRowsWorld() = default;

  [[nodiscard]] virtual bool ActionUnlocked(std::uint32_t action) = 0;
  // Formats the slot's name pair, row type and description into `row`.
  virtual void FormatRow(Items::ItemCategory category, std::size_t slot,
                         std::size_t row) = 0;
  virtual void AddRowFlags(std::size_t row, std::uint32_t flags) = 0;
  virtual void StoreMiscAction(std::size_t row, std::uint8_t action) = 0;
};

// decomp: MENU3.PRG _populateItemRows (items2.h:60). Returns the row count. A
// refusal writes nothing.
[[nodiscard]] std::expected<std::int32_t, ItemRowsRefusal> BuildItemRows(
    const ItemRowsInput& input, const Items::InventoryContents& inventory,
    ItemRowsWorld& world);

class ItemRowsRestoreWorld {
 public:
  ItemRowsRestoreWorld() = default;
  ItemRowsRestoreWorld(const ItemRowsRestoreWorld&) = delete;
  ItemRowsRestoreWorld& operator=(const ItemRowsRestoreWorld&) = delete;
  ItemRowsRestoreWorld(ItemRowsRestoreWorld&&) = delete;
  ItemRowsRestoreWorld& operator=(ItemRowsRestoreWorld&&) = delete;
  virtual ~ItemRowsRestoreWorld() = default;

  [[nodiscard]] virtual std::uint8_t ListAnimationRow() = 0;
  virtual void AdvanceListRows() = 0;
  // Null when the menu has no such row.
  [[nodiscard]] virtual Framework::MenuRow* Row(std::int32_t index) = 0;
  [[nodiscard]] virtual std::span<const std::uint8_t> CursorMemory() = 0;
};

// decomp: MENU3.PRG _restoreItemMenu (items2.h:175). Finishes the list
// animation, settles sliding rows and reselects the remembered row.
[[nodiscard]] std::expected<void, ItemRowsRefusal> RestoreItemRows(
    ItemRowsRestoreWorld& world, std::int32_t category);
}  // namespace VS::Menu::Inventory
