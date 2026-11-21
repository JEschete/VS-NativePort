#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

#include "Items/ContainerIndexing.hpp"
#include "Menu/Framework/Framework.hpp"
#include "Menu/Framework/ItemRows.hpp"

// The container screen's list transfer: the direction prompt, the item list
// with its row actions, the screen controller and the weapon/shield rows of
// MENUD.PRG (234.c). Every side effect goes through TransferWorld; the
// controllers keep only their own step state.
namespace VS::Menu::container {

// A text is a halfword position in one of the two text tables the original
// addressed (MENUD's _menuText and ITEMHELP.BIN), never a pointer.
enum class TransferTextTable : std::uint8_t { kMenuText, kItemHelp };

struct TransferText {
  TransferTextTable table = TransferTextTable::kMenuText;
  std::uint32_t offset = 0;
  friend bool operator==(const TransferText&, const TransferText&) = default;
};

// The record owners the original reached through vs_menuD_containerData,
// D_80109A88 and the inventory storage block.
enum class TransferStorage : std::uint8_t {
  kActive,     // vs_menuD_containerData
  kOther,      // D_80109A88
  kStaged,     // storage->staged_transfer
  kPreview,    // storage->transfer_preview
  kContainer,  // storage->container
};

enum class TransferSound : std::uint8_t { kLeave, kInvalid, kSelect, kChange };

// MENUD's shared panel bytes (D_80109A7A-D_80109A81). Callees change them
// during a call, so they are read and written where the original did.
enum class TransferFlag : std::uint8_t {
  kBrowsingGadgetBag,
  kTitleVisible,
  kIconsVisible,
  kControlsMode,
  kTitleSlide,
  kIconsSlide,
  kControlsSlide,
  kDetailsPhase,
};

enum class TransferTextLayer : std::uint8_t { kDefault, kAboveMenu };

struct MenuItemPlacement {
  std::int32_t row = 0;
  std::int32_t x = 0;
  std::int32_t y = 0;
  std::int32_t width = 0;
  std::int32_t flags = 0;
  TransferText text{};
  friend bool operator==(const MenuItemPlacement&,
                         const MenuItemPlacement&) = default;
};

inline constexpr std::size_t kActionRowCapacity = 4;
inline constexpr std::size_t kPendingTransferCount = 64;

class TransferWorld {
 public:
  TransferWorld() = default;
  TransferWorld(const TransferWorld&) = delete;
  TransferWorld& operator=(const TransferWorld&) = delete;
  TransferWorld(TransferWorld&&) = delete;
  TransferWorld& operator=(TransferWorld&&) = delete;
  virtual ~TransferWorld() = default;
  [[noreturn]] virtual void Refuse(std::string_view reason) = 0;

  // Shared state and settings.
  [[nodiscard]] virtual std::uint8_t Flag(TransferFlag flag) = 0;
  virtual void SetFlag(TransferFlag flag, std::uint8_t value) = 0;
  [[nodiscard]] virtual std::uint8_t CursorMemorySetting() = 0;
  virtual void SetCursorMemorySetting(std::uint8_t value) = 0;
  virtual void SetContainerEmpty(bool empty) = 0;
  virtual void SetFreezeTabArrows(std::uint8_t value) = 0;
  [[nodiscard]] virtual std::uint8_t CategoryToken() = 0;
  virtual void SetCategoryToken(std::uint8_t token) = 0;
  virtual void ClearCategoryMemory() = 0;
  virtual void SetCategoryCursor(Items::ItemCategory category, std::uint8_t row,
                                 std::uint8_t page) = 0;
  [[nodiscard]] virtual std::uint16_t DirectionSelection() = 0;
  virtual void SetDirectionSelection(std::uint16_t selection) = 0;
  virtual void ReturnFromMenu(std::uint8_t return_state) = 0;
  [[nodiscard]] virtual std::uint32_t PressedButtons() = 0;
  [[nodiscard]] virtual std::uint32_t HeldButtons() = 0;
  [[nodiscard]] virtual std::uint32_t RepeatedButtons() = 0;
  [[nodiscard]] virtual std::uint16_t MenuTextEntry(std::size_t index) = 0;
  [[nodiscard]] virtual std::uint16_t ItemHelpEntry(std::size_t index) = 0;
  [[nodiscard]] virtual std::uint8_t RowAnimationStep(std::size_t index) = 0;
  [[nodiscard]] virtual std::uint8_t InventoryCapacity(
      Items::ItemCategory category) = 0;
  [[nodiscard]] virtual std::uint16_t ContainerCapacity(
      Items::ItemCategory category) = 0;
  [[nodiscard]] virtual std::uint16_t TransferredMiscCount() = 0;

  // Record owners.
  [[nodiscard]] virtual Items::ContainerWorld& Records(
      TransferStorage storage) = 0;
  [[nodiscard]] virtual std::span<std::uint16_t> Indices(
      TransferStorage storage) = 0;
  [[nodiscard]] virtual std::span<std::uint16_t> PendingTransfers() = 0;
  virtual void SelectStorages(bool browsing_gadget_bag) = 0;
  virtual void ClearStagedTransfers() = 0;
  virtual void PublishActiveRecords() = 0;
  virtual void StoreContainerSnapshot() = 0;
  virtual void SnapshotGadgetBag() = 0;
  virtual void CommitGadgetBag() = 0;
  virtual void RebuildCategoryIndices(TransferStorage storage,
                                      Items::ItemCategory category) = 0;
  virtual void RebuildPendingTransfers() = 0;
  virtual void PreviewTransferredMisc() = 0;
  virtual void RefreshMiscTransferCount() = 0;
  [[nodiscard]] virtual std::int32_t CopyItem(std::int32_t flags,
                                              TransferStorage target,
                                              std::int32_t slot,
                                              TransferStorage source) = 0;
  [[nodiscard]] virtual std::int32_t TransferCapacityError(
      Items::ItemCategory category, std::int32_t slot) = 0;
  virtual void StageItemTransfer(Items::ItemCategory category,
                                 std::int32_t slot) = 0;
  virtual void UndoPendingTransfer(std::int32_t row) = 0;

  // Menu rows.
  virtual void AcquireItemRows() = 0;
  [[nodiscard]] virtual std::int32_t BuildItemRows(
      Items::ItemCategory category) = 0;
  virtual void MarkItemRow(std::size_t row, std::uint32_t flags) = 0;
  virtual void CreateItemMenuRows(std::int32_t count,
                                  std::int32_t row_info) = 0;
  virtual void ReleaseItemRows() = 0;
  virtual void RestoreItemRows(Items::ItemCategory category) = 0;
  virtual void SetMenuItem(const MenuItemPlacement& placement) = 0;
  virtual void SetRowSelected(std::int32_t row, bool selected) = 0;
  virtual void ClearRowOutsetIcon(std::int32_t row) = 0;
  [[nodiscard]] virtual std::int16_t RowX(std::int32_t row) = 0;
  virtual void SetRowX(std::int32_t row, std::int16_t x) = 0;
  virtual void SlideRow(std::int32_t row, Framework::RowState state,
                        std::int16_t target) = 0;
  virtual void SetRowCounts(std::int32_t row, std::uint16_t count,
                            std::uint16_t max) = 0;
  virtual void ClearMenuExcept(std::int32_t row) = 0;
  virtual void FlyOutRowsLeftExcept(std::int32_t row) = 0;
  [[nodiscard]] virtual std::int32_t SelectedRow() = 0;
  [[nodiscard]] virtual bool RowsIdle() = 0;
  [[nodiscard]] virtual std::int32_t CloseRowsAndRememberCursor() = 0;
  virtual void CreateActionRows(std::int32_t count, std::int32_t menu,
                                std::span<const TransferText> texts,
                                std::span<const std::int32_t> types) = 0;
  [[nodiscard]] virtual std::int32_t HighlightedActionRow() = 0;
  virtual void ShowActionInformation(std::int32_t row) = 0;
  virtual void UpdateActionMenu() = 0;
  [[nodiscard]] virtual std::int32_t ActionSelectionResult() = 0;

  // Information box, sounds, dialogs and text.
  virtual void InitializeInformationBox() = 0;
  virtual void DismissInformationBox() = 0;
  virtual void SetInformationMessage(TransferText text) = 0;
  virtual void SetPanelVisibility(std::uint8_t visibility) = 0;
  virtual void PlaySound(TransferSound sound) = 0;
  [[nodiscard]] virtual std::int32_t LoadItemText(std::int32_t load) = 0;
  [[nodiscard]] virtual bool UnloadItemTextWhenReady() = 0;
  [[nodiscard]] virtual std::int32_t UpdateItemDetails(
      std::int32_t selection) = 0;
  [[nodiscard]] virtual std::int32_t UpdateTransferConfirmation(
      std::int32_t initialize) = 0;
  [[nodiscard]] virtual std::int32_t UpdateDiscardMenu(
      std::int32_t selection) = 0;
  [[nodiscard]] virtual std::int32_t UpdateSortMenu(std::int32_t selection) = 0;
  [[nodiscard]] virtual std::int32_t UpdateItemList(std::int32_t direction) = 0;
  [[nodiscard]] virtual std::int32_t UpdateTransferDirection(
      std::int32_t initialize) = 0;

  // Drawing.
  virtual void DrawCompactText(std::string_view text, std::int32_t xy,
                               std::optional<std::uint32_t> color,
                               TransferTextLayer layer) = 0;
  [[nodiscard]] virtual std::uint8_t DrawCursor(std::uint8_t animation,
                                                std::int32_t position) = 0;
  virtual void DrawStorageCapacities() = 0;
  virtual void DrawCategoryIcons(std::uint8_t step, std::uint8_t phase) = 0;
  virtual void DrawButtonPromptBackground(std::int32_t x, std::int32_t y,
                                          std::int32_t width,
                                          std::int32_t height) = 0;
  virtual void DrawButtonIcon(Framework::ButtonIcon button, std::int32_t x,
                              std::int32_t y) = 0;
  virtual void DrawCapacityCount(std::int32_t limit, std::int32_t xy,
                                 std::int32_t count) = 0;
};

// decomp: func_80107490's statics (src/MENU/MENUD.PRG/234.c:2395-2399) and
// D_80109A60.
struct ItemListState {
  std::uint8_t step = 0;
  bool category_empty = false;
  bool first_entry = false;
  std::int32_t selected_row = 0;
  std::array<std::uint8_t, 2 * kActionRowCapacity> action_ids{};
};

// decomp: func_801089BC's statics (D_80109A2C, D_80109A2D, D_80109A2E,
// D_80109A70).
struct TransferDirectionState {
  std::uint8_t step = 0;
  std::uint8_t ok_blink = 0;
  std::uint8_t cursor_animation = 0;
  bool cursor_reset_pending = true;
};

// decomp: vs_menuD_exec's statics (D_80109A74, D_80109A78, D_80109A79).
struct ScreenState {
  std::uint32_t result = 0;
  std::uint8_t reopen_delay = 0;
  std::uint8_t capacity_slide = 0;
};

// The equipment rows' stat build status and formatted text.
struct WeaponRow {
  Framework::ItemRowText text{};
  Items::ItemsStatus status{};
};

// decomp: MENUD.PRG setWeaponUi (src/MENU/MENUD.PRG/234.c:811; was
// _setWeaponUi)
[[nodiscard]] WeaponRow BuildWeaponRow(Framework::ItemTextHost& host,
                                       const Items::InventoryWeapon& weapon,
                                       Items::ContainerWorld& records,
                                       std::span<std::uint8_t> buffer);
// decomp: MENUD.PRG setShieldUi (src/MENU/MENUD.PRG/234.c:821; was
// _setShieldUi)
[[nodiscard]] Framework::ItemRowText BuildShieldRow(
    Framework::ItemTextHost& host, const Items::InventoryShield& shield,
    Items::ContainerWorld& records, std::span<std::uint8_t> buffer);
// decomp: MENUD.PRG func_80107490 (src/MENU/MENUD.PRG/234.c:2394)
[[nodiscard]] std::int32_t UpdateItemList(std::int32_t direction,
                                          ItemListState& state,
                                          TransferWorld& world);
// decomp: MENUD.PRG func_801089BC (src/MENU/MENUD.PRG/234.c:3040)
[[nodiscard]] std::int32_t UpdateTransferDirection(
    std::int32_t initialize, TransferDirectionState& state,
    TransferWorld& world);
// decomp: MENUD.PRG vs_menuD_exec (src/MENU/MENUD.PRG/234.c:3133)
[[nodiscard]] std::int32_t UpdateScreen(std::uint8_t& step, ScreenState& state,
                                        TransferWorld& world);
}  // namespace VS::Menu::container
