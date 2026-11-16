#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "Menu/Container/ContainerListTransfer.hpp"

namespace container_list_transfer_tests {
namespace transfer = VS::Menu::container;
namespace items = VS::Items;
using Category = items::ItemCategory;
using transfer::TransferFlag;
using transfer::TransferSound;
using transfer::TransferStorage;
using transfer::TransferText;

inline constexpr std::size_t kStorageCount = 5;
inline constexpr std::size_t kFlagCount = 8;
inline constexpr std::uint16_t kMenuTextBase = 1000;
inline constexpr std::uint16_t kItemHelpBase = 2000;
inline constexpr std::uint8_t kAnimationScale = 3;

struct Refused : std::runtime_error {
  using std::runtime_error::runtime_error;
};

struct Event {
  std::string name;
  std::int64_t first = 0;
  std::int64_t second = 0;
  friend bool operator==(const Event&, const Event&) = default;
};

struct DrawnText {
  std::string text;
  std::int32_t xy = 0;
  std::optional<std::uint32_t> color;
  transfer::TransferTextLayer layer{};
};

struct ActionMenu {
  std::int32_t count = 0;
  std::int32_t menu = 0;
  std::vector<TransferText> texts;
  std::vector<std::int32_t> types;
};

struct Storage final : items::ContainerWorld {
  items::ContainerStorage storage;
  std::uint16_t Capacity(Category category) override {
    return storage.Capacity(category);
  }
  items::ContainerRecord Read(Category category, std::size_t index) override {
    return storage.Read(category, index);
  }
  void Write(Category category, std::size_t index,
             const items::ContainerRecord& record) override {
    storage.Write(category, index, record);
  }
  [[noreturn]] void Refuse(std::string_view reason) override {
    throw Refused(std::string(reason));
  }
};

// A scripted TransferWorld: queries return the configured values and every
// side effect is recorded in order.
struct World final : transfer::TransferWorld {
  std::array<std::uint8_t, kFlagCount> flags{};
  std::uint8_t cursor_memory = 0;
  std::uint8_t token = 1;
  std::uint16_t direction = 0;
  std::uint32_t pressed = 0;
  std::uint32_t held = 0;
  std::uint32_t repeated = 0;
  std::uint16_t transferred_misc = 0;
  std::array<Storage, kStorageCount> storages{};
  std::array<std::uint16_t, transfer::kPendingTransferCount> pending{};
  std::size_t pending_limit = pending.size();
  std::function<void()> on_rebuild_pending;
  std::int32_t item_rows = 0;
  std::int32_t selected = -1;
  bool idle = true;
  std::int32_t closed_cursor = 0;
  std::int32_t highlighted = 0;
  std::int32_t action_result = -1;
  std::int32_t details_result = 0;
  std::int32_t confirmation_result = 0;
  std::int32_t discard_result = 0;
  std::int32_t sort_result = 0;
  std::int32_t list_result = 0;
  std::int32_t direction_result = 0;
  std::int32_t load_result = 0;
  bool unload_ready = false;
  std::int32_t copy_result = 0;
  std::int32_t capacity_error = 0;
  std::int16_t title_x = 0;
  std::uint8_t cursor_result = 0;
  std::vector<Event> events;
  std::vector<transfer::MenuItemPlacement> placements;
  std::vector<TransferText> messages;
  std::vector<DrawnText> texts;
  std::optional<ActionMenu> action_menu;
  std::vector<TransferSound> sounds;

  void Log(std::string name, std::int64_t first = 0, std::int64_t second = 0) {
    events.push_back({std::move(name), first, second});
  }
  [[nodiscard]] bool Logged(std::string_view name) const {
    for (const auto& event : events) {
      if (event.name == name) return true;
    }
    return false;
  }
  [[nodiscard]] std::size_t Count(std::string_view name) const {
    std::size_t count = 0;
    for (const auto& event : events) count += event.name == name ? 1 : 0;
    return count;
  }
  Storage& Of(TransferStorage storage) {
    return storages.at(static_cast<std::size_t>(storage));
  }
  std::uint8_t& FlagRef(TransferFlag flag) {
    return flags.at(static_cast<std::size_t>(flag));
  }

  [[noreturn]] void Refuse(std::string_view reason) override {
    throw Refused(std::string(reason));
  }
  std::uint8_t Flag(TransferFlag flag) override { return FlagRef(flag); }
  void SetFlag(TransferFlag flag, std::uint8_t value) override {
    FlagRef(flag) = value;
  }
  std::uint8_t CursorMemorySetting() override { return cursor_memory; }
  void SetCursorMemorySetting(std::uint8_t value) override {
    Log("cursor-memory", value);
    cursor_memory = value;
  }
  void SetContainerEmpty(bool empty) override {
    Log("container-empty", empty ? 1 : 0);
  }
  void SetFreezeTabArrows(std::uint8_t value) override {
    Log("freeze-tabs", value);
  }
  std::uint8_t CategoryToken() override { return token; }
  void SetCategoryToken(std::uint8_t value) override {
    Log("token", value);
    token = value;
  }
  void ClearCategoryMemory() override { Log("clear-category-memory"); }
  void SetCategoryCursor(Category category, std::uint8_t row,
                         std::uint8_t page) override {
    Log("category-cursor", static_cast<std::int64_t>(category),
        row * 0x100 + page);
  }
  std::uint16_t DirectionSelection() override { return direction; }
  void SetDirectionSelection(std::uint16_t value) override {
    Log("direction", value);
    direction = value;
  }
  void ReturnFromMenu(std::uint8_t state) override { Log("return", state); }
  std::uint32_t PressedButtons() override { return pressed; }
  std::uint32_t HeldButtons() override { return held; }
  std::uint32_t RepeatedButtons() override { return repeated; }
  std::uint16_t MenuTextEntry(std::size_t index) override {
    return static_cast<std::uint16_t>(kMenuTextBase + index);
  }
  std::uint16_t ItemHelpEntry(std::size_t index) override {
    return static_cast<std::uint16_t>(kItemHelpBase + index);
  }
  std::uint8_t RowAnimationStep(std::size_t index) override {
    return static_cast<std::uint8_t>(index * kAnimationScale);
  }
  std::uint8_t InventoryCapacity(Category category) override {
    return static_cast<std::uint8_t>(static_cast<int>(category) + 10);
  }
  std::uint16_t ContainerCapacity(Category category) override {
    return static_cast<std::uint16_t>(static_cast<int>(category) + 100);
  }
  std::uint16_t TransferredMiscCount() override { return transferred_misc; }

  items::ContainerWorld& Records(TransferStorage storage) override {
    return Of(storage);
  }
  std::span<std::uint16_t> Indices(TransferStorage storage) override {
    return Of(storage).storage.indices;
  }
  std::span<std::uint16_t> PendingTransfers() override {
    return std::span(pending).first(pending_limit);
  }
  void SelectStorages(bool browsing) override {
    Log("select-storages", browsing ? 1 : 0);
  }
  void ClearStagedTransfers() override { Log("clear-staged"); }
  void PublishActiveRecords() override { Log("publish"); }
  void StoreContainerSnapshot() override { Log("store-snapshot"); }
  void SnapshotGadgetBag() override { Log("snapshot-bag"); }
  void CommitGadgetBag() override { Log("commit-bag"); }
  void RebuildCategoryIndices(TransferStorage storage,
                              Category category) override {
    Log("rebuild-indices", static_cast<std::int64_t>(storage),
        static_cast<std::int64_t>(category));
  }
  void RebuildPendingTransfers() override {
    Log("rebuild-pending");
    if (on_rebuild_pending) on_rebuild_pending();
  }
  void PreviewTransferredMisc() override { Log("preview-misc"); }
  void RefreshMiscTransferCount() override { Log("refresh-misc"); }
  std::int32_t CopyItem(std::int32_t flags_value, TransferStorage target,
                        std::int32_t slot, TransferStorage source) override {
    Log("copy", flags_value, slot);
    Log("copy-owners", static_cast<std::int64_t>(target),
        static_cast<std::int64_t>(source));
    return copy_result;
  }
  std::int32_t TransferCapacityError(Category category,
                                     std::int32_t slot) override {
    Log("capacity-error", static_cast<std::int64_t>(category), slot);
    return capacity_error;
  }
  void StageItemTransfer(Category category, std::int32_t slot) override {
    Log("stage", static_cast<std::int64_t>(category), slot);
  }
  void UndoPendingTransfer(std::int32_t row) override { Log("undo", row); }

  void AcquireItemRows() override { Log("acquire-rows"); }
  std::int32_t BuildItemRows(Category category) override {
    Log("build-rows", static_cast<std::int64_t>(category));
    return item_rows;
  }
  void MarkItemRow(std::size_t row, std::uint32_t value) override {
    Log("mark-row", static_cast<std::int64_t>(row), value);
  }
  void CreateItemMenuRows(std::int32_t count, std::int32_t info) override {
    Log("create-rows", count, info);
  }
  void ReleaseItemRows() override { Log("release-rows"); }
  void RestoreItemRows(Category category) override {
    Log("restore-rows", static_cast<std::int64_t>(category));
  }
  void SetMenuItem(const transfer::MenuItemPlacement& placement) override {
    placements.push_back(placement);
  }
  void SetRowSelected(std::int32_t row, bool value) override {
    Log("row-selected", row, value ? 1 : 0);
  }
  void ClearRowOutsetIcon(std::int32_t row) override { Log("clear-icon", row); }
  std::int16_t RowX(std::int32_t) override { return title_x; }
  void SetRowX(std::int32_t row, std::int16_t x) override {
    Log("row-x", row, x);
  }
  void SlideRow(std::int32_t row, std::uint8_t state,
                std::int16_t target) override {
    Log("slide", row, state * 0x10000 + target);
  }
  void SetRowCounts(std::int32_t row, std::uint16_t count,
                    std::uint16_t max) override {
    Log("counts", row, count * 0x10000 + max);
  }
  void ClearMenuExcept(std::int32_t row) override { Log("clear-menu", row); }
  void FlyOutRowsLeftExcept(std::int32_t row) override { Log("fly-out", row); }
  std::int32_t SelectedRow() override { return selected; }
  bool RowsIdle() override { return idle; }
  std::int32_t CloseRowsAndRememberCursor() override {
    Log("close-rows");
    return closed_cursor;
  }
  void CreateActionRows(std::int32_t count, std::int32_t menu,
                        std::span<const TransferText> values,
                        std::span<const std::int32_t> types) override {
    action_menu = ActionMenu{count,
                             menu,
                             {values.begin(), values.end()},
                             {types.begin(), types.end()}};
  }
  std::int32_t HighlightedActionRow() override { return highlighted; }
  void ShowActionInformation(std::int32_t row) override {
    Log("action-info", row);
  }
  void UpdateActionMenu() override { Log("action-update"); }
  std::int32_t ActionSelectionResult() override { return action_result; }

  void InitializeInformationBox() override { Log("info-init"); }
  void DismissInformationBox() override { Log("info-dismiss"); }
  void SetInformationMessage(TransferText text) override {
    messages.push_back(text);
  }
  void SetPanelVisibility(std::uint8_t value) override { Log("panel", value); }
  void PlaySound(TransferSound sound) override { sounds.push_back(sound); }
  std::int32_t LoadItemText(std::int32_t load) override {
    Log("load-text", load);
    return load_result;
  }
  bool UnloadItemTextWhenReady() override {
    Log("unload-text");
    return unload_ready;
  }
  std::int32_t UpdateItemDetails(std::int32_t selection) override {
    Log("details", selection);
    return details_result;
  }
  std::int32_t UpdateTransferConfirmation(std::int32_t initialize) override {
    Log("confirmation", initialize);
    return confirmation_result;
  }
  std::int32_t UpdateDiscardMenu(std::int32_t selection) override {
    Log("discard", selection);
    return discard_result;
  }
  std::int32_t UpdateSortMenu(std::int32_t selection) override {
    Log("sort", selection);
    return sort_result;
  }
  std::int32_t UpdateItemList(std::int32_t value) override {
    Log("item-list", value);
    return list_result;
  }
  std::int32_t UpdateTransferDirection(std::int32_t initialize) override {
    Log("direction-menu", initialize);
    return direction_result;
  }

  void DrawCompactText(std::string_view text, std::int32_t xy,
                       std::optional<std::uint32_t> color,
                       transfer::TransferTextLayer layer) override {
    texts.push_back({std::string(text), xy, color, layer});
  }
  std::uint8_t DrawCursor(std::uint8_t animation,
                          std::int32_t position) override {
    Log("cursor", animation, position);
    return cursor_result;
  }
  void DrawStorageCapacities() override { Log("draw-capacities"); }
  void DrawCategoryIcons(std::uint8_t step, std::uint8_t phase) override {
    Log("icons", step, phase);
  }
  void DrawButtonPromptBackground(std::int32_t x, std::int32_t y, std::int32_t,
                                  std::int32_t) override {
    Log("prompt", x, y);
  }
  void DrawButtonIcon(std::int32_t button, std::int32_t x,
                      std::int32_t) override {
    Log("button", button, x);
  }
  void DrawCapacityCount(std::int32_t limit, std::int32_t,
                         std::int32_t count) override {
    Log("capacity", limit, count);
  }
};
}  // namespace container_list_transfer_tests
