#pragma once

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
// _miscActions (MENU3.PRG 16C.c) holds one action per misc row.
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
