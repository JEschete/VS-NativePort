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
#include "Menu/Inventory/InventoryTerminal.hpp"
#include "Menu/Loot/LootRows.hpp"

namespace VS::Menu::Loot {
using LootCategory = Items::ItemCategory;

struct LootTerminalWorkspace {
  Items::InventoryContents inventory;
  Items::InventoryContents inventory_backup;
  std::array<LootRow, kLootRowCapacity> rows{};
  std::array<LootRow, kLootRowCapacity> working_rows{};
};

struct LootTerminalRuntime {
  LootCursor cursor;
  Inventory::TerminalPresentation inventory_ui;
  std::uint8_t inventory_category = 0;
  std::uint8_t show_inventory = 0;
  std::uint8_t inventory_step = 0;
  std::uint8_t show_loot = 0;
  std::uint8_t loot_step = 0;
  std::uint8_t controls_enabled = 0;
  std::uint8_t previewed = 0;
  std::uint8_t claimed = 0;
  std::uint8_t information_shown = 0;
  std::uint8_t hud_hidden = 0;
  std::int32_t hud_slide_offset = 0;
  std::int16_t result_kind = 0;
};

struct InventoryTransitionState {
  std::uint8_t step = 0;
  std::uint8_t hud_was_hidden = 0;
};
struct DisassemblyState {
  std::uint8_t step = 0;
  std::uint8_t choice = 0;
  std::uint8_t saved_count = 0;
  std::uint8_t target_row = 0;
  LootCategory category = LootCategory::kWeapon;
  std::uint8_t cursor_animation = 0;
};
struct StackConsolidationState {
  std::uint16_t remaining = 0;
  std::uint16_t stack_room = 0;
  std::uint16_t item_id = 0;
  std::uint8_t step = 0;
  std::uint8_t choice = 0;
  std::uint8_t saved_count = 0;
  std::uint8_t cursor_animation = 0;
};
struct LootSelectionState {
  std::uint8_t step = 0;
  std::uint8_t removal_delay = 0;
  std::uint8_t cursor_animation = 0;
};

enum class LootTerminalText : std::uint8_t {
  kItemsTitle,
  kCategoryTitle,
  kYes,
  kNo,
  kDisassembleConfirm,
  kOrganizeConfirm,
  kEmptyChest,
};
enum class LootTerminalSound : std::uint8_t {
  kOpen,
  kSelect,
  kLeave,
  kChange,
  kInvalid,
  kClaim,
  kDisassemble,
};

// Runtime and workspace borrows remain stable for a call. Live publishes
// changes before services and refreshes them afterwards, including nested
// controllers. A missing backup must be refused before opening a preview.
class LootTerminalHost {
 public:
  virtual ~LootTerminalHost() = default;
  [[nodiscard]] virtual LootTerminalRuntime& Runtime() = 0;
  [[nodiscard]] virtual LootTerminalWorkspace& Workspace() = 0;
  virtual void RequireBackups(bool inventory) = 0;
  [[nodiscard]] virtual Items::InventoryContents AshleyInventory() = 0;
  [[nodiscard]] virtual Input::PadState Buttons() = 0;
  [[nodiscard]] virtual bool RowsReady() = 0;
  [[nodiscard]] virtual bool InformationDone() = 0;
  [[nodiscard]] std::span<const std::uint8_t> Text(LootTerminalText text) {
    return Text(text, 0);
  }
  [[nodiscard]] virtual std::span<const std::uint8_t> Text(
      LootTerminalText text, std::int32_t index) = 0;
  [[nodiscard]] virtual Framework::MenuRow& Row(std::int32_t row) = 0;
  [[nodiscard]] virtual Framework::MenuRow& SetRow(
      const Framework::RowPlacement& placement,
      std::span<const std::uint8_t> text) = 0;
  virtual void SetWeaponName(std::size_t slot) = 0;
  virtual void SetItemName(std::uint16_t id) = 0;
  virtual void InitializeInformation(bool caution) = 0;
  virtual void SetInformation(std::span<const std::uint8_t> text) = 0;
  virtual void DismissInformation() = 0;
  virtual void FlyOutRight(std::int32_t row) = 0;
  virtual void PlaySound(LootTerminalSound sound) = 0;
  [[nodiscard]] virtual std::uint8_t DrawPromptCursor(
      std::uint8_t animation, std::int32_t position) = 0;
  [[nodiscard]] virtual std::uint8_t DrawLootCursor(std::uint8_t animation,
                                                    std::int32_t x,
                                                    std::int32_t y) = 0;

  virtual void SetBackgroundFade(bool enabled) = 0;
  virtual void SetMenuCommand(bool enabled) = 0;
  virtual void HideInventoryPanels() = 0;
  virtual void ClearMenu() = 0;
  virtual void SetFieldRenderingEnabled(bool enabled) = 0;
  virtual void SetHudHidden(bool hidden) = 0;
  virtual void RenderAshley(Framework::AshleyRenderMode mode) = 0;
  // Always MENUB's controller, never MENU3's _ITEMMENU specialization.
  [[nodiscard]] virtual std::int32_t LootInventory(bool initialize) = 0;
  [[nodiscard]] virtual std::int32_t AnimationOffset(std::size_t step) = 0;
  [[nodiscard]] virtual std::uint8_t Capacity(LootCategory category) = 0;
  [[nodiscard]] virtual std::int32_t CountItems(LootCategory category) = 0;
  virtual void DrawCategoryIcons(std::uint8_t step, std::uint8_t on_top) = 0;
  virtual void DrawStatusCommand(std::int32_t offset, std::uint8_t state) = 0;

  virtual void RefreshClaimableRows() = 0;
  virtual void ShowDescription(std::size_t row) = 0;
  [[nodiscard]] virtual bool ClaimItem(const LootRow& row) = 0;
  virtual void RecordKey(std::uint16_t id) = 0;
  [[nodiscard]] virtual std::int32_t LeaveMenu(bool initialize) = 0;
  [[nodiscard]] virtual std::int32_t OrganizeMenu(
      std::optional<std::size_t> row) = 0;
  [[nodiscard]] virtual std::int32_t InventoryTransition(
      std::optional<LootCategory> category) = 0;
  [[nodiscard]] virtual std::int32_t Disassemble(
      std::optional<std::size_t> row) = 0;
  [[nodiscard]] virtual std::int32_t Consolidate(
      std::optional<std::size_t> row) = 0;
  // Existing #128 batch continuation, separate from original single-batch
  // parity. False when no deferred drops remain.
  [[nodiscard]] virtual bool AdvanceBatch() = 0;

  virtual void PopulateBaseStats(std::int32_t actor) = 0;
  virtual void RebuildInventory(LootCategory category) = 0;
  virtual void AcquireWorkspace() = 0;
  virtual void ReleaseWorkspace() = 0;
  virtual void ResetPaging() = 0;
  virtual void BuildStagedInventory() = 0;
  [[nodiscard]] virtual bool HaveRoomLoot() = 0;
  virtual void ClearRoomLoot() = 0;
  [[nodiscard]] virtual bool LoadItemText(bool initialize) = 0;
  [[nodiscard]] virtual bool UnloadItemText() = 0;
  [[nodiscard]] virtual bool DismissScreenText() = 0;
  [[nodiscard]] virtual std::int32_t Selection(bool initialize) = 0;
  virtual void RenderCapacities(std::int32_t x) = 0;
  virtual void RenderLoot(std::int32_t x) = 0;
  [[noreturn]] virtual void Refuse(std::string_view reason) = 0;
};

// Optional entries are zero-based semantic categories/rows. Only Live
// decodes the original nonzero initialization arguments.
[[nodiscard]] std::int32_t UpdateInventoryTransition(
    InventoryTransitionState& state, LootTerminalHost& host,
    std::optional<LootCategory> category = std::nullopt);
[[nodiscard]] std::int32_t UpdateDisassembly(
    DisassemblyState& state, LootTerminalHost& host,
    std::optional<std::size_t> row = std::nullopt);
[[nodiscard]] std::int32_t UpdateStackConsolidation(
    StackConsolidationState& state, LootTerminalHost& host,
    std::optional<std::size_t> row = std::nullopt);
[[nodiscard]] std::int32_t UpdateLootSelection(LootSelectionState& state,
                                               LootTerminalHost& host,
                                               bool initialize = false);
[[nodiscard]] std::int32_t UpdateLootScreen(std::uint8_t& step,
                                            LootTerminalHost& host);
}  // namespace VS::Menu::Loot
