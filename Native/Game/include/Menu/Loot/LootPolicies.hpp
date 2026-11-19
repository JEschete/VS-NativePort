#pragma once
// Hand-maintained (tools/port/owned.csv).
// The loot screen's (MENUB.PRG) gadget bag capacity panel, its visible loot
// rows, the previewed row's description and the leave and organize prompts.

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "Items/ItemRecords.hpp"
#include "Menu/Framework/Prompts.hpp"
#include "Menu/Loot/LootRows.hpp"

namespace VS::Menu::Framework {
struct MenuRow;
struct RowPlacement;
}  // namespace VS::Menu::Framework

namespace VS::Menu::Loot {  // NOLINT(readability-identifier-naming)
// The item categories the capacity panel lists (weapon .. misc).
inline constexpr std::size_t kListedCategories = 7;
// The menu rows of the visible loot rows and of the two prompt choices.
inline constexpr int kFirstLootMenuRow = 32;
inline constexpr int kFirstPromptRow = 30;
// Prompt cursor positions start at this cursor slot.
inline constexpr int kPromptCursorBase = 8;

// The panel's colors (vs_getRGB): unlit, dimmed, roomy and overflowing.
inline constexpr std::uint32_t kCapacityNeutralColor = 0x808080;
inline constexpr std::uint32_t kCapacityDimColor = 0x404040;
inline constexpr std::uint32_t kCapacityRoomyColor = 0x804020;
inline constexpr std::uint32_t kCapacityOverflowColor = 0x204080;

// The original's pad bits (PADRright | PADRdown, PADRdown, PADLup | PADLdown).
inline constexpr std::uint32_t kPromptConfirmButtons = 0x60;
inline constexpr std::uint32_t kPromptCancelButton = 0x40;
inline constexpr std::uint32_t kPromptToggleButtons = 0x5000;
// The low byte of the first pad's buttons.
inline constexpr std::uint32_t kPromptAnyFaceButtons = 0xFF;

// The panel texts: "GADGET BAG", "/", _itemCategoryHeaders[category], "GET",
// "ALL" and "END".
enum class CapacityLabel : std::uint8_t {
  kGadgetBag,
  kSlash,
  kCategory,
  kGet,
  kAll,
  kEnd,
};

// What _displayInventoryCapacities reads of the loot screen.
struct CapacityPanel {
  std::span<const LootRow> rows;
  LootCursor cursor{};
  // decomp: D_8010A6B7. 0 previews nothing, a value without a low nibble
  // previews every row, otherwise the 1-based visible row.
  std::uint8_t previewed{};
  // decomp: _controlsEnabled, the shift of the command colors.
  std::uint8_t controls_enabled{};
  // decomp: vs_mainMenu_inventoryItemCapacities; kListedCategories entries.
  std::span<const std::uint8_t> capacities;
};

class CapacityHost {
 public:
  CapacityHost() = default;
  CapacityHost(const CapacityHost&) = delete;
  CapacityHost& operator=(const CapacityHost&) = delete;
  CapacityHost(CapacityHost&&) = delete;
  CapacityHost& operator=(CapacityHost&&) = delete;
  virtual ~CapacityHost() = default;

  // `category` names the header of kCategory and is 0 for the other labels.
  virtual void DrawLabel(CapacityLabel label, std::uint32_t xy) = 0;
  virtual void DrawColoredLabel(CapacityLabel label, std::uint8_t category,
                                std::uint32_t xy, std::uint32_t color) = 0;
  // The items of `category` Ashley carries.
  [[nodiscard]] virtual std::int32_t CountItems(std::uint8_t category) = 0;
  // The items of `category` claiming `row` would add.
  [[nodiscard]] virtual std::int32_t CountContained(std::uint8_t category,
                                                    const LootRow& row) = 0;
  virtual void DrawInteger(std::int32_t value, std::uint32_t xy,
                           std::uint32_t color) = 0;
  virtual void DrawButtonBackground(std::int32_t x, std::int32_t y,
                                    std::int32_t w, std::int32_t h) = 0;
  virtual void DrawButton(Framework::ButtonIcon button, std::int32_t x,
                          std::int32_t y) = 0;
};

// Draws the capacity panel; false (nothing drawn) when the previewed row or
// the row count lies outside `rows`.
[[nodiscard]] bool RenderBagCapacities(std::int32_t edge_x,
                                       const CapacityPanel& panel,
                                       CapacityHost& host);

// What _displayCurrentLoot reads and updates.
struct LootRowsView {
  // The whole loot list; the visible rows' slide steps advance.
  std::span<LootRow> rows;
  LootCursor cursor{};
  // decomp: _gradientStateIndex, the offset the menu rows were last drawn at.
  std::uint8_t gradient_state_index{};
};

class LootRowsHost {
 public:
  LootRowsHost() = default;
  LootRowsHost(const LootRowsHost&) = delete;
  LootRowsHost& operator=(const LootRowsHost&) = delete;
  LootRowsHost(LootRowsHost&&) = delete;
  LootRowsHost& operator=(LootRowsHost&&) = delete;
  virtual ~LootRowsHost() = default;

  [[nodiscard]] virtual Framework::MenuRow& Row(int row) = 0;
  virtual Framework::MenuRow& SetRow(const Framework::RowPlacement& placement,
                                     std::span<const std::uint8_t> text) = 0;
  // decomp: vs_mainMenu_itemNames[id].
  [[nodiscard]] virtual std::span<const std::uint8_t> ItemName(
      std::uint16_t id) = 0;
};

// Lays out the visible loot rows. A row naming a slot outside its table (or a
// weapon without a blade) keeps the parts it cannot read and reports
// kSlotOutOfRange; a list window past `rows` stops at the last row.
[[nodiscard]] Items::ItemsStatus RenderLootRows(
    std::int32_t x, LootRowsView& view,
    const Items::InventoryContents& inventory, LootRowsHost& host);

class DescriptionHost {
 public:
  DescriptionHost() = default;
  DescriptionHost(const DescriptionHost&) = delete;
  DescriptionHost& operator=(const DescriptionHost&) = delete;
  DescriptionHost(DescriptionHost&&) = delete;
  DescriptionHost& operator=(DescriptionHost&&) = delete;
  virtual ~DescriptionHost() = default;

  // Saves Ashley's inventory to _inventoryBackup.
  virtual void BackupInventory() = 0;
  // Copies the staged inventory over Ashley's.
  virtual void LoadStagedInventory() = 0;
  // Formats slot `index` of `category` in Ashley's (now staged) inventory and
  // returns the description text.
  [[nodiscard]] virtual std::span<const std::uint8_t> FormatDescription(
      Items::ItemCategory category, std::uint8_t index) = 0;
  virtual void ShowInformation(std::span<const std::uint8_t> message) = 0;
  // Restores Ashley's inventory from _inventoryBackup.
  virtual void RestoreInventory() = 0;
};

// Shows the description of a loot row; refuses (and does nothing) for a row
// outside the item categories or naming a slot outside its table.
[[nodiscard]] Items::ItemsStatus ShowLootDescription(const LootRow& row,
                                                     DescriptionHost& host);

// The prompt texts (MENUB.PRG menuText).
enum class PromptText : std::uint8_t {
  kOptionYes,
  kOptionNo,
  kItemsRemaining,
  kInvalidDiscard,
  kDiscardAllConfirm,
  kDisassemble,
  kOrganize,
  kOrganizeInventory,
  kDisassembleInfo,
  kOrganizeInfo,
  kOrganizeInventoryInfo,
};

struct PromptInput {
  // decomp: vs_main_buttonsPressed.all
  std::uint32_t pressed{};
  // decomp: vs_main_buttonRepeat
  std::uint32_t repeat{};
  // decomp: vs_main_settings.information
  bool information_shown{};
};

class PromptHost {
 public:
  PromptHost() = default;
  PromptHost(const PromptHost&) = delete;
  PromptHost& operator=(const PromptHost&) = delete;
  PromptHost(PromptHost&&) = delete;
  PromptHost& operator=(PromptHost&&) = delete;
  virtual ~PromptHost() = default;

  virtual Framework::MenuRow& SetRow(const Framework::RowPlacement& placement,
                                     PromptText text) = 0;
  [[nodiscard]] virtual Framework::MenuRow& Row(int row) = 0;
  // vs_mainMenu_initInformationTextBox(1).
  virtual void InitInformationTextBox() = 0;
  virtual void InitInformationBox() = 0;
  virtual void ShowInformation(PromptText text) = 0;
  [[nodiscard]] virtual bool RowsIdle() = 0;
  virtual void FlyOutRight(int row) = 0;
  virtual void DismissInformationBox() = 0;
  virtual void PlayLeaveSound() = 0;
  virtual void PlayChangeSound() = 0;
  virtual void PlaySelectSound() = 0;
  // The discard confirmation (vs_main_playSfxDefault(0x7E, 0x1C)).
  virtual void PlayDiscardSound() = 0;
  [[nodiscard]] virtual std::uint8_t DrawCursor(std::uint8_t step,
                                                int position) = 0;
  // vs_battle_stringContext.strings[0] = vs_mainMenu_itemNames[item_id].
  virtual void SetStringItem(std::uint16_t item_id) = 0;
};

// _discardItems' statics.
struct LeaveConfirmation {
  std::uint8_t state{};
  // 0 for "yes", 1 for "no".
  std::uint8_t choice{};
  // The key item that cannot be left behind; kept across openings.
  std::uint16_t kept_item{};
  std::uint8_t cursor_step{};
  friend bool operator==(const LeaveConfirmation&,
                         const LeaveConfirmation&) = default;
};

// What opening the leave confirmation reads.
struct LeaveContext {
  std::span<const LootRow> rows;
  std::uint8_t count{};
  // decomp: _lootClaimed
  bool claimed{};
  // decomp: vs_menuB_lootEntry != NULL
  bool single_entry{};
};

// Opens the leave confirmation; kSlotOutOfRange (the scan skips the row) for
// a misc row outside the table or a count past `rows`.
[[nodiscard]] Items::ItemsStatus InitLeaveConfirmation(
    LeaveConfirmation& prompt, const LeaveContext& context,
    const Items::InventoryContents& inventory);
// One frame of the leave confirmation: 0 while open, 1 (claimed) or 2 to
// leave the rest, 1 or 2 directly for an empty or single-entry list, -1 to
// stay. nullopt for a choice outside "yes" and "no".
[[nodiscard]] std::optional<std::int32_t> UpdateLeaveConfirmation(
    LeaveConfirmation& prompt, bool claimed, const PromptInput& input,
    PromptHost& host);

// _organizeInventory's statics.
struct OrganizeMenu {
  std::uint8_t state{};
  std::uint8_t category{};
  // 0 for the item's action, 1 for "organize inventory".
  std::uint8_t selected_row{};
  // 0 none, 1 disassemble, 2 organize the stack.
  std::uint8_t action{};
  std::uint8_t cursor_step{};
  friend bool operator==(const OrganizeMenu&, const OrganizeMenu&) = default;
};

// Opens the organize prompt for `row`; refuses (and does nothing) for a misc
// row outside the table.
[[nodiscard]] Items::ItemsStatus InitOrganizeMenu(
    OrganizeMenu& menu, const LootRow& row,
    const Items::InventoryContents& inventory, PromptHost& host);
// One frame of the organize prompt: 0 while open, -1 to cancel, 1 to
// organize the inventory, 2 to disassemble, 3 to organize the stack. nullopt
// for a selected row outside the two choices.
[[nodiscard]] std::optional<std::int32_t> UpdateOrganizeMenu(
    OrganizeMenu& menu, const PromptInput& input, PromptHost& host);
}  // namespace VS::Menu::Loot
