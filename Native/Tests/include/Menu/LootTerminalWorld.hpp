#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "Menu/Loot/LootTerminal.hpp"

namespace VS::tests::loot_terminal {
namespace loot = Menu::Loot;
namespace Framework = Menu::Framework;
using Category = Items::ItemCategory;
using TextId = loot::LootTerminalText;
using Sound = loot::LootTerminalSound;

enum class Call : std::uint8_t {
  kReady = 1,
  kRow,
  kSetRow,
  kInitInfo,
  kInformation,
  kDismissInfo,
  kFlyOut,
  kSound,
  kPromptCursor,
  kLootCursor,
  kBackground,
  kCommand,
  kHidePanels,
  kClearMenu,
  kField,
  kHud,
  kAshley,
  kInventory,
  kCount,
  kIcons,
  kButtonBackground,
  kButton,
  kStatusText,
  kRefresh,
  kDescription,
  kClaim,
  kKey,
  kLeave,
  kOrganize,
  kTransition,
  kDisassemble,
  kConsolidate,
  kBaseStats,
  kRebuild,
  kAcquire,
  kRelease,
  kStage,
  kClearRoomLoot,
  kLoadText,
  kUnloadText,
  kDismissText,
  kSelection,
  kCapacities,
  kLootRows,
};

// Endpoint doubles, not a second loot controller. Each tested decision and
// inventory transformation runs in LootTerminal.cpp.
class World final : public loot::LootTerminalHost {
 public:
  static constexpr std::size_t kTextStride = 16;
  static constexpr std::size_t kTextKinds = 7;
  static constexpr std::int32_t kItemNameToken = 10000;
  static constexpr std::int32_t kWeaponNameToken = 20000;
  loot::LootTerminalRuntime runtime;
  loot::LootTerminalWorkspace workspace;
  Items::InventoryContents ashley;
  Input::PadState input;
  std::array<Framework::MenuRow, Framework::kRowCount> menu_rows{};
  std::array<std::uint8_t, kTextStride * kTextKinds> texts{};
  std::array<std::int32_t, 4> ready_values{1, 1, 1, 1};
  std::array<std::int32_t, 11> animation{};
  std::array<std::int32_t, 7> counts{};
  std::array<std::uint8_t, 7 * Items::kMiscSlots> claim_results{};
  std::vector<std::int64_t> calls;
  std::size_t ready_position = 0;
  std::int32_t name_token = 0;
  std::uint8_t cursor_result = 0;
  std::int32_t inventory_result = 0;
  std::int32_t leave_result = 0;
  std::int32_t organize_result = 0;
  std::int32_t transition_result = 0;
  std::int32_t disassembly_result = 0;
  std::int32_t consolidation_result = 0;
  std::int32_t selection_result = 0;
  std::uint8_t staged_count = 1;
  std::uint8_t selection_preview = 0;
  bool information_done = true;
  bool text_loaded = true;
  bool text_unloaded = true;
  bool dismiss_pending = false;
  bool have_room_loot = false;
  bool active_workspace = true;
  bool backups_available = true;
  bool batch_available = false;
  bool change_inventory_ui = false;
  Menu::Inventory::TerminalPresentation nested_ui{};
  std::uint8_t nested_category = 0;
  std::optional<loot::LootCursor> refreshed_cursor;
  int paging_resets = 0;
  int advances = 0;
  int allocations = 0;
  int releases = 0;
  std::string refusal;

  World() {
    claim_results.fill(1);
    for (std::size_t step = 0; step < animation.size(); ++step) {
      animation[step] = static_cast<std::int32_t>(step * 16);
    }
  }
  template <typename... Values>
  void Record(Call call, Values... values) {
    const std::array<std::int64_t, sizeof...(values) + 2> record{
        static_cast<std::int64_t>(call), sizeof...(values),
        static_cast<std::int64_t>(values)...};
    calls.insert(calls.end(), record.begin(), record.end());
  }
  std::int64_t TextToken(std::span<const std::uint8_t> text) const {
    for (std::size_t offset = 0; offset < texts.size(); ++offset) {
      if (text.data() == &texts[offset]) {
        return static_cast<std::int64_t>(offset);
      }
    }
    return -1;
  }
  loot::LootTerminalRuntime& Runtime() override { return runtime; }
  loot::LootTerminalWorkspace& Workspace() override {
    if (!active_workspace) Refuse("missing test loot workspace");
    return workspace;
  }
  void RequireBackups(bool) override {
    if (!backups_available) Refuse("missing test loot backup");
  }
  Items::InventoryContents AshleyInventory() override { return ashley; }
  Input::PadState Buttons() override { return input; }
  bool RowsReady() override {
    Record(Call::kReady);
    return ready_values[std::min(ready_position++, ready_values.size() - 1)] !=
           0;
  }
  bool InformationDone() override { return information_done; }
  std::span<const std::uint8_t> Text(TextId text, std::int32_t index) override {
    const auto offset = static_cast<std::size_t>(text) * kTextStride + index;
    if (offset >= texts.size()) Refuse("text outside fixture");
    return std::span(texts).subspan(offset, 1);
  }
  Framework::MenuRow& Row(std::int32_t row) override {
    if (row < 0 || static_cast<std::size_t>(row) >= menu_rows.size()) {
      Refuse("row outside fixture");
    }
    Record(Call::kRow, row);
    return menu_rows[static_cast<std::size_t>(row)];
  }
  Framework::MenuRow& SetRow(const Framework::RowPlacement& placement,
                             std::span<const std::uint8_t> text) override {
    if (placement.row < 0 ||
        static_cast<std::size_t>(placement.row) >= menu_rows.size()) {
      Refuse("row placement outside fixture");
    }
    Record(Call::kSetRow, placement.row, placement.x, placement.y, placement.w,
           placement.background_width, TextToken(text));
    return menu_rows[static_cast<std::size_t>(placement.row)];
  }
  void SetWeaponName(std::size_t slot) override {
    name_token = kWeaponNameToken + static_cast<std::int32_t>(slot);
  }
  void SetItemName(std::uint16_t id) override {
    name_token = kItemNameToken + id;
  }
  void InitializeInformation(bool caution) override {
    Record(Call::kInitInfo, caution);
  }
  void SetInformation(std::span<const std::uint8_t> text) override {
    Record(Call::kInformation, TextToken(text));
  }
  void DismissInformation() override { Record(Call::kDismissInfo); }
  void FlyOutRight(std::int32_t row) override { Record(Call::kFlyOut, row); }
  void PlaySound(Sound sound) override { Record(Call::kSound, sound); }
  std::uint8_t DrawPromptCursor(std::uint8_t animation_step,
                                std::int32_t position) override {
    Record(Call::kPromptCursor, animation_step, position);
    return cursor_result;
  }
  std::uint8_t DrawLootCursor(std::uint8_t animation_step, std::int32_t x,
                              std::int32_t y) override {
    Record(Call::kLootCursor, animation_step, Framework::PackXy(x, y));
    return cursor_result;
  }
  void SetBackgroundFade(bool enabled) override {
    Record(Call::kBackground, enabled);
  }
  void SetMenuCommand(bool enabled) override {
    Record(Call::kCommand, enabled ? 2 : 0);
  }
  void HideInventoryPanels() override { Record(Call::kHidePanels, 0); }
  void ClearMenu() override { Record(Call::kClearMenu, Framework::kRowCount); }
  void SetFieldRenderingEnabled(bool enabled) override {
    Record(Call::kField, enabled);
  }
  void SetHudHidden(bool hidden) override {
    Record(Call::kHud, hidden);
    runtime.hud_hidden = hidden ? 1 : 0;
  }
  void RenderAshley(Framework::AshleyRenderMode mode) override {
    Record(Call::kAshley, mode);
  }
  std::int32_t LootInventory(bool initialize) override {
    Record(Call::kInventory, initialize);
    if (change_inventory_ui) {
      runtime.inventory_ui = nested_ui;
      runtime.inventory_category = nested_category;
    }
    return inventory_result;
  }
  std::int32_t AnimationOffset(std::size_t step) override {
    if (step >= animation.size()) Refuse("animation outside fixture");
    return animation[step];
  }
  std::uint8_t Capacity(Category category) override {
    const auto index = static_cast<std::size_t>(category);
    if (index >= Items::kCategoryCapacities.size()) {
      Refuse("category outside fixture");
    }
    return Items::kCategoryCapacities[index];
  }
  std::int32_t CountItems(Category category) override {
    const auto index = static_cast<std::size_t>(category);
    if (index >= counts.size()) Refuse("item count outside fixture");
    Record(Call::kCount, category);
    return counts[index];
  }
  void DrawCategoryIcons(std::uint8_t step, std::uint8_t on_top) override {
    Record(Call::kIcons, step, on_top);
  }
  void DrawStatusCommand(std::int32_t offset, std::uint8_t state) override {
    constexpr std::uint32_t kStatusColor = 0x202020;
    Record(Call::kButtonBackground, 16 - offset, 38, 88, 10);
    Record(Call::kButton, 1, 8 - offset, 36, 0);
    Record(Call::kStatusText, Framework::PackXy(28 - offset, 38),
           kStatusColor << (state & 31U));
  }
  void RefreshClaimableRows() override {
    Record(Call::kRefresh);
    if (refreshed_cursor) runtime.cursor = *refreshed_cursor;
  }
  void ShowDescription(std::size_t row) override {
    Record(Call::kDescription, row);
  }
  bool ClaimItem(const loot::LootRow& row) override {
    Record(Call::kClaim, row.category, row.item_index);
    const auto offset = row.category * Items::kMiscSlots + row.item_index;
    if (offset >= claim_results.size()) Refuse("claim outside fixture");
    return claim_results[offset] != 0;
  }
  void RecordKey(std::uint16_t id) override { Record(Call::kKey, id); }
  std::int32_t LeaveMenu(bool initialize) override {
    Record(Call::kLeave, initialize);
    return leave_result;
  }
  std::int32_t OrganizeMenu(std::optional<std::size_t> row) override {
    Record(Call::kOrganize, row ? static_cast<int>(*row + 1) : 0);
    return organize_result;
  }
  std::int32_t InventoryTransition(std::optional<Category> category) override {
    Record(Call::kTransition, category ? static_cast<int>(*category) + 1 : 0);
    return transition_result;
  }
  std::int32_t Disassemble(std::optional<std::size_t> row) override {
    Record(Call::kDisassemble, row ? static_cast<int>(*row + 1) : 0);
    return disassembly_result;
  }
  std::int32_t Consolidate(std::optional<std::size_t> row) override {
    Record(Call::kConsolidate, row ? static_cast<int>(*row + 1) : 0);
    return consolidation_result;
  }
  bool AdvanceBatch() override {
    ++advances;
    if (batch_available) {
      runtime.cursor = {staged_count, 0, 0};
      batch_available = false;
      return true;
    }
    return false;
  }
  void PopulateBaseStats(std::int32_t actor) override {
    Record(Call::kBaseStats, actor);
  }
  void RebuildInventory(Category category) override {
    Record(Call::kRebuild, category);
  }
  void AcquireWorkspace() override {
    if (active_workspace) Refuse("duplicate test workspace acquisition");
    Record(Call::kAcquire);
    active_workspace = true;
    ++allocations;
  }
  void ReleaseWorkspace() override {
    if (!active_workspace) Refuse("duplicate test workspace release");
    Record(Call::kRelease);
    active_workspace = false;
    ++releases;
  }
  void ResetPaging() override { ++paging_resets; }
  void BuildStagedInventory() override {
    Record(Call::kStage, have_room_loot);
    runtime.cursor.count = staged_count;
  }
  bool HaveRoomLoot() override { return have_room_loot; }
  void ClearRoomLoot() override { Record(Call::kClearRoomLoot); }
  bool LoadItemText(bool initialize) override {
    Record(Call::kLoadText, initialize);
    return text_loaded;
  }
  bool UnloadItemText() override {
    Record(Call::kUnloadText);
    return text_unloaded;
  }
  bool DismissScreenText() override {
    Record(Call::kDismissText, 7);
    return dismiss_pending;
  }
  std::int32_t Selection(bool initialize) override {
    Record(Call::kSelection, initialize);
    runtime.previewed = selection_preview;
    return selection_result;
  }
  void RenderCapacities(std::int32_t x) override {
    Record(Call::kCapacities, x);
  }
  void RenderLoot(std::int32_t x) override { Record(Call::kLootRows, x); }
  [[noreturn]] void Refuse(std::string_view reason) override {
    refusal = reason;
    throw std::runtime_error(refusal);
  }
};
}  // namespace VS::tests::loot_terminal
