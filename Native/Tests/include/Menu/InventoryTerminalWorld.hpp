#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

#include "Menu/Inventory/InventoryTerminal.hpp"
#include "Render/EncodedText.hpp"

namespace VS::tests::inventory_terminal {
inline auto LootText(std::uint8_t glyph = 1) {
  std::array<std::uint8_t, Menu::Inventory::kLootInventoryTextBytes> bytes{};
  bytes.fill(Render::kTextTerminator);
  for (std::size_t entry = 0;
       entry < Menu::Inventory::kLootInventoryTextEntries; ++entry) {
    bytes[entry * 2] = Menu::Inventory::kLootInventoryTextEntries;
    bytes[entry * 2 + 1] = 0;
  }
  bytes[Menu::Inventory::kLootInventoryTextEntries * 2] = glyph;
  return bytes;
}

namespace Inventory = Menu::Inventory;
namespace Framework = Menu::Framework;
using Category = Items::ItemCategory;
using TextId = Inventory::TerminalText;
using Sound = Inventory::TerminalSound;
enum class Call : std::uint8_t {
  kReady = 1,
  kInitInfo,
  kDiscardInfo,
  kDismissInfo,
  kInformation,
  kSound,
  kActions,
  kActionResult,
  kCreateActions,
  kActionInformation,
  kSort,
  kRow,
  kSetRow,
  kClear,
  kFlyOut,
  kFont,
  kDigit,
  kCursor,
  kClearItem,
  kRebuild,
  kParentWeapon,
  kParentShield,
  kFind,
  kActionFlags,
  kBeginRows,
  kEndRows,
  kPopulate,
  kFormatMisc,
  kCreateList,
  kRestore,
  kSelected,
  kStoreCursor,
  kDetails,
  kDiscard,
  kSortMenu,
  kLoadText,
  kUnloadText,
  kRenderScreen,
  kBaseStats,
  kActionState,
  kHud,
  kAshley,
  kNavigation,
  kEquipment,
  kBackgroundPriority,
  kCloseBackground,
  kClearCommand,
  kCount,
  kIcons,
  kStatus,
};

// Endpoint doubles only. State-machine decisions and transformations run in
// InventoryTerminal.cpp; the differential executes each original controller.
class World final : public Inventory::InventoryTerminalHost {
 public:
  static constexpr std::size_t kTextStride = 256;
  static constexpr std::size_t kTextKinds = 27;
  static constexpr std::int64_t kFormattedText = 100000;
  Inventory::TerminalRuntime runtime;
  Input::PadState input;
  Items::InventoryContents inventory;
  std::array<std::array<std::uint8_t, Inventory::kTerminalRows>, 7> order{};
  std::array<std::uint8_t, Inventory::kTerminalRows> misc_actions{};
  std::array<std::uint8_t, Inventory::kMiscActionMapEntries> action_map{};
  std::array<std::uint8_t, 256> unlocked{};
  std::array<std::int32_t, 256> action_flags{};
  std::array<Framework::MenuRow, Framework::kRowCount> rows{};
  std::array<std::int32_t, Inventory::kTerminalRows> formatted_flags{};
  std::array<std::uint8_t, kTextStride * kTextKinds> text_bank{};
  std::array<std::uint8_t, Inventory::kTerminalRows * 2> formatted_text{};
  std::array<int, 8> ready_values{};
  std::vector<std::int64_t> calls;
  std::size_t ready_position = 0;
  bool information_done = true;
  bool names_loaded = true;
  bool text_loaded = true;
  bool text_unloaded = true;
  bool tarnished = false;
  bool rows_active = false;
  bool fail_population = false;
  std::uint8_t intro = 3;
  std::int32_t action_result = -1;
  std::int32_t selected_result = -1;
  std::int32_t cursor_result = 0;
  std::int32_t details_result = 0;
  std::int32_t discard_result = 0;
  std::int32_t sort_result = 0;
  std::int32_t navigation_result = 0;
  std::int32_t equipment_result = 0;
  std::int32_t parent_weapon = 0;
  std::int32_t parent_shield = 0;
  std::int32_t find_result = 1;
  std::int32_t populate_count = 1;
  std::int32_t cursor_animation = 0;
  std::int32_t fade_step = 0;
  std::int32_t item_count = 1;
  std::array<std::int32_t, 11> animation{};
  // Optional callee-side panel edits expose stale Live snapshots.
  bool nested_navigation_changes_panels = false;

  World() {
    ready_values.fill(1);
    for (std::size_t category = 0; category < order.size(); ++category) {
      for (std::size_t slot = 0; slot < Items::kCategoryCapacities[category];
           ++slot) {
        order[category][slot] = static_cast<std::uint8_t>(slot + 1U);
      }
    }
    for (std::size_t index = 0; index < inventory.misc.size(); ++index) {
      inventory.misc[index].id = Inventory::kFirstMiscActionItem;
      inventory.misc[index].count = 10;
    }
    for (std::size_t step = 0; step < animation.size(); ++step) {
      animation[step] = static_cast<int>(step * 32);
    }
  }

  template <typename... Values>
  void Record(Call call, Values... values) {
    calls.push_back(static_cast<std::int64_t>(call));
    calls.push_back(sizeof...(values));
    (calls.push_back(static_cast<std::int64_t>(values)), ...);
  }
  std::int64_t TextToken(std::span<const std::uint8_t> text) const {
    for (std::size_t index = 0; index < text_bank.size(); ++index) {
      if (text.data() == &text_bank[index]) {
        return static_cast<std::int64_t>(index);
      }
    }
    for (std::size_t index = 0; index < formatted_text.size(); ++index) {
      if (text.data() == &formatted_text[index]) {
        return kFormattedText + static_cast<std::int64_t>(index);
      }
    }
    return -1;
  }
  void RecordRows(Call call, std::int32_t menu,
                  std::span<const Framework::RowText> text,
                  std::span<const std::int32_t> flags) {
    Record(call, menu, text.size(), runtime.cursor_memory_enabled);
    for (std::size_t row = 0; row < text.size(); ++row) {
      calls.push_back(TextToken(text[row].name));
      calls.push_back(TextToken(text[row].description));
      calls.push_back(flags[row]);
    }
  }
  Inventory::TerminalRuntime& Runtime() override { return runtime; }
  Input::PadState Buttons() override { return input; }
  bool RowsReady() override {
    Record(Call::kReady);
    const auto index = std::min(ready_position++, ready_values.size() - 1);
    return ready_values[index] != 0;
  }
  bool InformationDone() override { return information_done; }
  std::span<const std::uint8_t> Text(TextId text, std::int32_t index) override {
    if (index < 0 || index >= static_cast<int>(kTextStride)) {
      Refuse("text index");
    }
    return std::span(text_bank).subspan(
        static_cast<std::size_t>(text) * kTextStride +
            static_cast<std::size_t>(index),
        1);
  }
  void InitializeInformation() override { Record(Call::kInitInfo); }
  void InitializeDiscardInformation() override { Record(Call::kDiscardInfo); }
  void DismissInformation() override { Record(Call::kDismissInfo); }
  void SetInformation(std::span<const std::uint8_t> text) override {
    Record(Call::kInformation, TextToken(text));
  }
  void PlaySound(Sound sound) override { Record(Call::kSound, sound); }
  void UpdateActions() override { Record(Call::kActions); }
  std::int32_t ActionResult() override {
    Record(Call::kActionResult);
    return action_result;
  }
  void CreateActions(std::int32_t menu,
                     std::span<const Framework::RowText> text,
                     std::span<const std::int32_t> flags) override {
    RecordRows(Call::kCreateActions, menu, text, flags);
  }
  void UpdateActionInformation() override { Record(Call::kActionInformation); }
  void Sort(Category category, std::int32_t stat) override {
    Record(Call::kSort, category, stat);
  }
  Framework::MenuRow& Row(std::int32_t row) override {
    if (row < 0 || static_cast<std::size_t>(row) >= rows.size()) Refuse("row");
    Record(Call::kRow, row);
    return rows[static_cast<std::size_t>(row)];
  }
  Framework::MenuRow& SetRow(const Framework::RowPlacement& placement,
                             std::span<const std::uint8_t> text) override {
    const auto token = TextToken(text);
    Record(Call::kSetRow, placement.row, placement.x, placement.y, placement.w,
           placement.background_width, token);
    if (token == -1) {
      for (const auto value : text) calls.push_back(value);
    }
    if (placement.row < 0 ||
        static_cast<std::size_t>(placement.row) >= rows.size()) {
      Refuse("row");
    }
    auto& row = rows[static_cast<std::size_t>(placement.row)];
    row.x = static_cast<std::int16_t>(placement.x);
    row.y = static_cast<std::int16_t>(placement.y);
    return row;
  }
  void ClearExcept(std::int32_t row) override { Record(Call::kClear, row); }
  void FlyOutExcept(std::int32_t row) override { Record(Call::kFlyOut, row); }
  void SetQuantityFont() override { Record(Call::kFont); }
  void DrawQuantityDigit(std::uint32_t digit, std::int32_t x) override {
    Record(Call::kDigit, digit, x);
  }
  std::int32_t DrawQuantityCursor(std::uint8_t animation_value, std::int32_t x,
                                  std::int32_t y) override {
    Record(Call::kCursor, animation_value, x, y);
    return cursor_animation;
  }
  Items::InventoryContents Inventory() override { return inventory; }
  std::span<std::uint8_t> Order(Category category) override {
    const auto index = static_cast<std::size_t>(category);
    if (index >= order.size()) Refuse("order category");
    return std::span(order[index]).first(Items::kCategoryCapacities[index]);
  }
  void SetMiscCount(std::size_t slot, std::uint8_t count) override {
    inventory.misc.at(slot).count = count;
  }
  void ClearItem(Category category, std::uint8_t slot) override {
    Record(Call::kClearItem, category, slot);
  }
  void RebuildInventory(Category category) override {
    Record(Call::kRebuild, category);
  }
  std::int32_t ParentWeapon(Category category, std::int32_t slot) override {
    Record(Call::kParentWeapon, category, slot);
    return parent_weapon;
  }
  std::int32_t ParentShield(Category category, std::int32_t slot) override {
    Record(Call::kParentShield, category, slot);
    return parent_shield;
  }
  std::int32_t FindItem(Category category, std::int32_t slot) override {
    Record(Call::kFind, category, slot);
    return find_result;
  }
  std::uint8_t MiscAction(std::size_t row) override {
    return misc_actions.at(row);
  }
  std::span<const std::uint8_t> MiscActionMap() override { return action_map; }
  bool ActionUnlocked(std::uint32_t action) override {
    return unlocked.at(action) != 0;
  }
  std::int32_t ActionFlags(std::uint8_t action) override {
    Record(Call::kActionFlags, action);
    return action_flags[action];
  }
  std::uint8_t IntroState() override { return intro; }
  bool EquipmentTarnished() override { return tarnished; }
  void BeginRows() override {
    if (rows_active) Refuse("row workspace reentry");
    rows_active = true;
    Record(Call::kBeginRows);
  }
  void EndRows() noexcept override {
    rows_active = false;
    try {
      Record(Call::kEndRows);
    } catch (...) {
      std::terminate();
    }
  }
  void Format(std::size_t row, Inventory::TerminalRows& result) {
    result.text[row] = {std::span(formatted_text).subspan(row * 2, 1),
                        std::span(formatted_text).subspan(row * 2 + 1, 1)};
    result.flags[row] = formatted_flags[row];
  }
  std::int32_t PopulateRows(Inventory::ItemRowsContext context,
                            Category category,
                            Inventory::TerminalRows& result) override {
    Record(Call::kPopulate, context, category);
    if (fail_population) Refuse("population failure");
    if (populate_count < 0 ||
        populate_count > static_cast<int>(Inventory::kTerminalRows)) {
      return populate_count;
    }
    for (int row = 0; row < populate_count; ++row) {
      Format(static_cast<std::size_t>(row), result);
    }
    return populate_count;
  }
  void FormatMisc(std::size_t slot, std::size_t row,
                  Inventory::TerminalRows& result) override {
    Record(Call::kFormatMisc, slot, row);
    Format(row, result);
  }
  void CreateList(std::int32_t menu, std::span<const Framework::RowText> text,
                  std::span<const std::int32_t> flags) override {
    RecordRows(Call::kCreateList, menu, text, flags);
  }
  void RestoreRows(Category category) override {
    Record(Call::kRestore, category);
  }
  std::int32_t SelectedRow() override {
    Record(Call::kSelected);
    return selected_result;
  }
  std::int32_t StoreCursor() override {
    Record(Call::kStoreCursor);
    return cursor_result;
  }
  std::int32_t Details(
      std::optional<Inventory::ItemDetailRequest> request) override {
    Record(Call::kDetails, request ? static_cast<int>(request->category) : -1,
           request ? request->stored_cursor : 0);
    return details_result;
  }
  std::int32_t Discard(
      std::optional<Inventory::InventoryDiscardEntry> entry) override {
    Record(Call::kDiscard, entry ? static_cast<int>(entry->category) : -1,
           entry ? entry->one_based_slot : 0, entry ? entry->kind : 0);
    return discard_result;
  }
  std::int32_t SortMenu(std::optional<Category> category) override {
    Record(Call::kSortMenu, category ? static_cast<int>(*category) : -1);
    return sort_result;
  }
  bool ItemNamesLoaded() override { return names_loaded; }
  bool LoadItemText(bool initialize) override {
    Record(Call::kLoadText, initialize);
    return text_loaded;
  }
  bool UnloadItemText() override {
    Record(Call::kUnloadText);
    return text_unloaded;
  }
  void RenderScreen() override { Record(Call::kRenderScreen); }
  void PopulateBaseStats(std::int32_t actor) override {
    Record(Call::kBaseStats, actor);
  }
  void SetFieldRenderingEnabled(bool enabled) override {
    Record(Call::kActionState, enabled);
  }
  void SetHudHidden(bool hidden) override { Record(Call::kHud, hidden); }
  void RenderAshley(Framework::AshleyRenderMode mode) override {
    Record(Call::kAshley, mode);
  }
  std::int32_t Navigation(std::int32_t initialize) override {
    Record(Call::kNavigation, initialize);
    if (nested_navigation_changes_panels) {
      runtime.presentation.status_command = 2;
    }
    return navigation_result;
  }
  std::int32_t EquipmentMenu(bool initialize) override {
    Record(Call::kEquipment, initialize);
    return equipment_result;
  }
  void SetBackgroundPriority(std::int32_t offset,
                             std::int32_t brightness) override {
    Record(Call::kBackgroundPriority, offset, brightness);
  }
  void CloseBackground() override { Record(Call::kCloseBackground); }
  void ClearCommand() override { Record(Call::kClearCommand); }
  std::int32_t BackgroundFadeStep() override { return fade_step; }
  std::int32_t AnimationOffset(std::size_t step) override {
    return animation.at(step);
  }
  std::uint8_t Capacity(Category category) override {
    return Items::kCategoryCapacities.at(static_cast<std::size_t>(category));
  }
  std::int32_t CountItems(Category category) override {
    Record(Call::kCount, category);
    return item_count;
  }
  void DrawCategoryIcons(std::uint8_t step, std::uint8_t selected) override {
    Record(Call::kIcons, step, selected);
  }
  void DrawStatusCommand(std::int32_t offset, std::uint8_t state) override {
    Record(Call::kStatus, offset, state);
  }
  [[noreturn]] void Refuse(std::string_view reason) override {
    throw std::runtime_error(std::string(reason));
  }
};
}  // namespace VS::tests::inventory_terminal
