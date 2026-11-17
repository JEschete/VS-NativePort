#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

#include "Input/Pad.hpp"
#include "Items/ItemRecords.hpp"
#include "Menu/Framework/Framework.hpp"
#include "Menu/Inventory/ItemListRows.hpp"

namespace VS::Menu::Inventory {
using TerminalCategory = Items::ItemCategory;
inline constexpr std::size_t kTerminalRows = Items::kMiscSlots;
inline constexpr std::size_t kDiscardTemplateBytes = 12;
inline constexpr auto kDiscardInitialText = std::to_array<std::uint8_t>(
    {0xFA, 0x30, 0, 0, 0, 0xA1, 0, 0, 0, 0xE7, 0, 0});
inline constexpr std::size_t kInventoryActionSlots = 8;
inline constexpr std::size_t kLootInventoryTextBytes = 644;
inline constexpr std::size_t kLootInventoryTextEntries = 31;

struct LootInventoryText {
  std::array<std::uint8_t, kLootInventoryTextBytes> bytes{};
  bool loaded = false;
};
// MENUB's own encoded text table, never MENU3's similarly named global.
// Invalid input leaves the previous owned asset intact.
[[nodiscard]] bool BindLootInventoryText(LootInventoryText& asset,
                                         std::span<const std::uint8_t> bytes);

enum class TerminalSound : std::uint8_t {
  kSelect,
  kLeave,
  kChange,
  kInvalid,
  kDiscard,
};

// Names in the three owned text assets, not pointers into an overlay.
enum class TerminalText : std::uint8_t {
  kSortName,
  kSortDescription,
  kYesNo,
  kDiscardPrompt,
  kQuantityPrompt,
  kNoItems,
  kManage,
  kTarnished,
  kUse,
  kUseDescription,
  kCannotUse,
  kInsufficientMp,
  kUseProhibited,
  kSearch,
  kSearchDescription,
  kInvalidSearch,
  kDiscard,
  kDiscardDescription,
  kDiscardRestriction,
  kDiscardAll,
  kDiscardAllDescription,
  kCannotDiscard,
  kSort,
  kSortHint,
  kItemsTitle,
  kMiscTitle,
  kCategoryTitle,
};

struct InventoryCursorPosition {
  std::uint8_t row = 0;
  std::uint8_t page = 0;
};
struct InventoryCursorMemory {
  // 1..7 name the item categories; 0 is Manage.
  std::uint8_t category = 0;
  std::uint8_t reserved = 0;
  std::array<InventoryCursorPosition, Items::kItemCategoryCount> positions{};
};
struct TerminalPresentation {
  std::uint8_t secondary_title = 0;
  std::uint8_t category_icons = 0;
  std::uint8_t status_command = 0;
  std::uint8_t secondary_step = 0;
  std::uint8_t icons_step = 0;
  std::uint8_t status_step = 0;
  std::uint8_t icon_on_top = 0;
  std::int32_t ashley_layout = 0;
};
struct PendingItemUse {
  std::int16_t command = 0;
  std::int16_t kind = 0;
  std::uint16_t action = 0;
  std::uint16_t item = 0;
  std::uint8_t slot = 0;
};
struct TerminalRuntime {
  InventoryCursorMemory cursor;
  TerminalPresentation presentation;
  Framework::MenuControl control;
  PendingItemUse use;
  std::uint8_t cursor_memory_enabled = 0;
  std::uint8_t freeze_tabs = 0;
  std::int32_t restrictions = 0;
  std::int32_t hud_slide_offset = 0;
};

struct InventorySortState {
  std::uint8_t step = 0;
  TerminalCategory category = TerminalCategory::kWeapon;
};
struct InventoryDiscardEntry {
  TerminalCategory category = TerminalCategory::kWeapon;
  std::uint8_t one_based_slot = 0;
  // 0 single, 1 quantity, 2 whole stack.
  std::uint8_t kind = 0;
};
struct InventoryDiscardState {
  std::uint8_t step = 0;
  InventoryDiscardEntry item;
  std::uint8_t place = 0;
  std::uint8_t quantity = 0;
  std::uint8_t maximum = 0;
};
// Only these initialized data bytes reset on an ordinary overlay reload.
struct InventoryDiscardPresentation {
  std::array<std::uint8_t, kDiscardTemplateBytes> text = kDiscardInitialText;
  std::uint8_t cursor_animation = 0;
};
enum class InventoryAction : std::uint8_t {
  kNone,
  kUse,
  kSearch,
  kDiscard,
  kDiscardAll,
  kSort,
};
struct InventoryNavigationState {
  std::array<InventoryAction, kInventoryActionSlots> actions{};
  std::uint8_t step = 0;
  std::uint8_t no_items = 0;
  std::uint8_t ignore_cursor_memory = 0;
  std::int32_t selected_row = 0;
};
struct MiscShortcutState {
  std::array<std::uint8_t, kTerminalRows> actions{};
  std::int32_t selected_row = 0;
  std::uint8_t step = 0;
  std::uint8_t no_items = 0;
  std::uint8_t no_force_cursor_memory = 0;
};
struct InventoryScreenState {
  // Initialized module data; the countdown is BSS.
  std::uint8_t resume_without_cursor_reset = 0;
  std::uint8_t quit_countdown = 0;
};
struct TerminalRows {
  std::array<Framework::RowText, kTerminalRows> text{};
  std::array<std::int32_t, kTerminalRows> flags{};
};
struct ItemDetailRequest {
  TerminalCategory category;
  std::int32_t stored_cursor;
};

// A call's runtime is authoritative between service calls. Live bindings
// publish it before, and refresh it after, each external service so changes
// made by nested menus are never overwritten by an entry-time snapshot.
class InventoryTerminalHost {
 public:
  virtual ~InventoryTerminalHost() = default;
  [[nodiscard]] virtual TerminalRuntime& Runtime() = 0;
  [[nodiscard]] virtual Input::PadState Buttons() = 0;
  [[nodiscard]] virtual bool RowsReady() = 0;
  [[nodiscard]] virtual bool InformationDone() = 0;
  [[nodiscard]] virtual std::span<const std::uint8_t> Text(
      TerminalText text, std::int32_t index) = 0;
  [[nodiscard]] std::span<const std::uint8_t> Text(TerminalText text) {
    return Text(text, 0);
  }
  virtual void InitializeInformation() = 0;
  virtual void InitializeDiscardInformation() = 0;
  virtual void DismissInformation() = 0;
  virtual void SetInformation(std::span<const std::uint8_t> text) = 0;
  virtual void PlaySound(TerminalSound sound) = 0;
  virtual void UpdateActions() = 0;
  [[nodiscard]] virtual std::int32_t ActionResult() = 0;
  virtual void CreateActions(std::int32_t menu,
                             std::span<const Framework::RowText> rows,
                             std::span<const std::int32_t> flags) = 0;
  virtual void UpdateActionInformation() = 0;
  virtual void Sort(TerminalCategory category, std::int32_t stat) = 0;
  [[nodiscard]] virtual Framework::MenuRow& Row(std::int32_t row) = 0;
  [[nodiscard]] virtual Framework::MenuRow& SetRow(
      const Framework::RowPlacement& placement,
      std::span<const std::uint8_t> text) = 0;
  virtual void ClearExcept(std::int32_t row) = 0;
  virtual void FlyOutExcept(std::int32_t row) = 0;
  virtual void SetQuantityFont() = 0;
  virtual void DrawQuantityDigit(std::uint32_t digit, std::int32_t x) = 0;
  [[nodiscard]] virtual std::int32_t DrawQuantityCursor(std::uint8_t animation,
                                                        std::int32_t x,
                                                        std::int32_t y) = 0;

  [[nodiscard]] virtual Items::InventoryContents Inventory() = 0;
  [[nodiscard]] virtual std::span<std::uint8_t> Order(
      TerminalCategory category) = 0;
  virtual void SetMiscCount(std::size_t slot, std::uint8_t count) = 0;
  virtual void ClearItem(TerminalCategory category,
                         std::uint8_t one_based_slot) = 0;
  virtual void RebuildInventory(TerminalCategory category) = 0;
  [[nodiscard]] virtual std::int32_t ParentWeapon(TerminalCategory category,
                                                  std::int32_t slot) = 0;
  [[nodiscard]] virtual std::int32_t ParentShield(TerminalCategory category,
                                                  std::int32_t slot) = 0;
  [[nodiscard]] virtual std::int32_t FindItem(TerminalCategory category,
                                              std::int32_t slot) = 0;
  [[nodiscard]] virtual std::uint8_t MiscAction(std::size_t row) = 0;
  [[nodiscard]] virtual std::span<const std::uint8_t> MiscActionMap() = 0;
  [[nodiscard]] virtual bool ActionUnlocked(std::uint32_t action) = 0;
  [[nodiscard]] virtual std::int32_t ActionFlags(std::uint8_t action) = 0;
  [[nodiscard]] virtual std::uint8_t IntroState() = 0;
  [[nodiscard]] virtual bool EquipmentTarnished() = 0;

  // Native-owned buffers, released on every return/refusal. Their lifetime
  // covers the formatter borrows and CreateList, not another frame.
  virtual void BeginRows() = 0;
  virtual void EndRows() noexcept = 0;
  [[nodiscard]] virtual std::int32_t PopulateRows(ItemRowsContext context,
                                                  TerminalCategory category,
                                                  TerminalRows& rows) = 0;
  virtual void FormatMisc(std::size_t slot, std::size_t row,
                          TerminalRows& rows) = 0;
  virtual void CreateList(std::int32_t menu,
                          std::span<const Framework::RowText> rows,
                          std::span<const std::int32_t> flags) = 0;
  virtual void RestoreRows(TerminalCategory category) = 0;
  [[nodiscard]] virtual std::int32_t SelectedRow() = 0;
  [[nodiscard]] virtual std::int32_t StoreCursor() = 0;
  [[nodiscard]] virtual std::int32_t Details(
      std::optional<ItemDetailRequest> request) = 0;
  [[nodiscard]] virtual std::int32_t Discard(
      std::optional<InventoryDiscardEntry> entry) = 0;
  [[nodiscard]] virtual std::int32_t SortMenu(
      std::optional<TerminalCategory> category) = 0;

  [[nodiscard]] virtual bool ItemNamesLoaded() = 0;
  [[nodiscard]] virtual bool LoadItemText(bool initialize) = 0;
  [[nodiscard]] virtual bool UnloadItemText() = 0;
  virtual void RenderScreen() = 0;
  virtual void PopulateBaseStats(std::int32_t actor) = 0;
  virtual void SetFieldRenderingEnabled(bool enabled) = 0;
  virtual void SetHudHidden(bool hidden) = 0;
  virtual void RenderAshley(std::int32_t mode) = 0;
  [[nodiscard]] virtual std::int32_t Navigation(std::int32_t initialize) = 0;
  [[nodiscard]] virtual std::int32_t EquipmentMenu(bool initialize) = 0;
  virtual void SetBackgroundPriority(std::int32_t offset,
                                     std::int32_t brightness) = 0;
  virtual void CloseBackground() = 0;
  virtual void ClearCommand() = 0;
  [[nodiscard]] virtual std::int32_t BackgroundFadeStep() = 0;
  [[nodiscard]] virtual std::int32_t AnimationOffset(std::size_t step) = 0;
  [[nodiscard]] virtual std::uint8_t Capacity(TerminalCategory category) = 0;
  [[nodiscard]] virtual std::int32_t CountItems(TerminalCategory category) = 0;
  virtual void DrawCategoryIcons(std::uint8_t step,
                                 std::uint8_t selected_on_top) = 0;
  virtual void DrawStatusCommand(std::int32_t offset, std::uint8_t state) = 0;
  [[noreturn]] virtual void Refuse(std::string_view reason) = 0;
};

// Nonzero legacy initialization arguments are decoded only in Live.
// Refusals name the invalid record/row/text through host.Refuse.
[[nodiscard]] std::int32_t UpdateSortMenu(
    InventorySortState& state, InventoryTerminalHost& host,
    std::optional<TerminalCategory> entry = std::nullopt);
[[nodiscard]] std::int32_t UpdateDiscardMenu(
    InventoryDiscardState& state, InventoryDiscardPresentation& presentation,
    InventoryTerminalHost& host,
    std::optional<InventoryDiscardEntry> entry = std::nullopt);
[[nodiscard]] std::int32_t UpdateItemList(InventoryNavigationState& state,
                                          InventoryTerminalHost& host,
                                          ItemRowsContext context,
                                          std::int32_t initialize = 0);
void UpdateMiscItemShortcut(MiscShortcutState& state,
                            InventoryTerminalHost& host,
                            std::int32_t initialize = 0);
[[nodiscard]] std::int32_t UpdateInventoryScreen(InventoryScreenState& state,
                                                 std::uint8_t& menu_step,
                                                 InventoryTerminalHost& host);
}  // namespace VS::Menu::Inventory
