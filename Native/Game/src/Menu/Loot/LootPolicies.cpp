// Hand-maintained (tools/port/owned.csv).
#include "Menu/Loot/LootPolicies.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "Items/ItemRecords.hpp"
#include "Menu/Framework/Framework.hpp"
#include "Menu/Loot/LootRows.hpp"

namespace VS::Menu::Loot {
namespace {
using Framework::MenuRow;
using Framework::RowPlacement;
using Framework::RowState;
using Items::ItemCategory;
using Items::ItemsStatus;

// The preview nibble of D_8010A6B7.
constexpr std::uint8_t kPreviewRowMask = 0xF;
// MIPS sllv shifts by the low five bits.
constexpr std::uint32_t kShiftMask = 31;
// Colors whose top component is lit (0x808080) take the room colors.
constexpr int kLitColorShift = 23;
constexpr int kPanelLeft = 176;
constexpr int kTitleY = 12;
constexpr int kFirstCategoryY = 24;
constexpr int kCategoryStepY = 12;
constexpr int kSlashX = 272;
constexpr int kHeaderX = 192;
constexpr int kCapacityX = 296;
constexpr int kButtonBackgroundX = 184;
constexpr int kGetRowY = 112;
constexpr int kEndRowY = 128;
constexpr int kGetBackgroundW = 112;
constexpr int kEndBackgroundW = 64;
constexpr int kButtonBackgroundH = 10;
constexpr int kCommandX = 196;
constexpr int kAllX = 252;
constexpr int kCircleX = 176;
constexpr int kSquareX = 232;
constexpr int kButtonIconY = 110;
constexpr int kEndIconY = 126;

constexpr int kRowLeft = 24;
constexpr int kRowTop = 50;
constexpr int kRowStepY = 16;
constexpr int kRowWidth = 152;
constexpr int kSlideStepX = 48;
constexpr std::uint8_t kSlideDone = 4;
constexpr std::uint8_t kRowScrollsUp = 1;
constexpr std::uint8_t kRowScrollsDown = 2;
constexpr std::uint8_t kChosenRowState = 2;
constexpr std::uint8_t kChosenRowBackground = 0x18;
constexpr std::uint8_t kFirstIndentedRowState = 3;
constexpr int kIndentStepX = 7;
constexpr std::uint8_t kGripIconBase = 10;
constexpr std::uint8_t kShieldIcon = 15;
constexpr std::uint8_t kArmorIconBase = 0xE;
constexpr std::uint8_t kGemIcon = 22;
// _displayCurrentLoot's 256-byte gradientStates, indexed up to 255 + 7.
constexpr std::size_t kGradientSlots = 256 + kVisibleLootRows;

constexpr int kPromptX = 0x140;
constexpr int kPromptFirstY = 0x92;
constexpr int kPromptSecondY = 0xA2;
constexpr int kPromptWidth = 0x7E;
constexpr std::int16_t kPromptTargetX = 0xC2;
// The leave prompt waits for a choice in this state.
constexpr std::uint8_t kLeaveChoiceState = 3;
constexpr std::uint8_t kLeaveEmptyState = 4;
constexpr std::uint8_t kLeaveSingleEntryState = 5;
constexpr std::uint8_t kLeaveKeyItemState = 6;
constexpr std::uint8_t kLeaveResultBase = 3;
constexpr std::uint8_t kNoChoice = 1;
constexpr std::int32_t kStay = -1;
constexpr std::int32_t kLeaveClaimed = 1;
constexpr std::int32_t kLeaveUnclaimed = 2;
constexpr std::uint8_t kDisassembleAction = 1;
constexpr std::uint8_t kOrganizeStackAction = 2;
constexpr std::uint8_t kOrganizeInventoryRow = 1;

constexpr int kYShift = 16;

[[nodiscard]] constexpr std::uint32_t Xy(std::int32_t x, std::int32_t y) {
  return static_cast<std::uint32_t>(x) |
         (static_cast<std::uint32_t>(y) << kYShift);
}

[[nodiscard]] constexpr std::uint32_t DimShifted(std::uint32_t shift) {
  return kCapacityDimColor << (shift & kShiftMask);
}

[[nodiscard]] bool InTable(ItemCategory category, std::uint8_t index) {
  return index <
         Items::kCategoryCapacities.at(static_cast<std::size_t>(category));
}

// The preview: nullopt for none, a null row for every row, else the row.
struct Preview {
  bool shown{};
  const LootRow* row{};
  // The "ALL" command is lit unless a previewed row's list has no room.
  std::uint32_t all_rows_state{1};
};

[[nodiscard]] std::optional<Preview> ReadPreview(const CapacityPanel& panel) {
  Preview preview;
  if (panel.previewed == 0) return preview;
  preview.shown = true;
  if ((panel.previewed & kPreviewRowMask) == 0) return preview;
  const std::size_t index =
      static_cast<std::size_t>(panel.cursor.offset) + panel.previewed - 1;
  if (index >= panel.rows.size() || panel.cursor.count > panel.rows.size()) {
    return std::nullopt;
  }
  preview.row = &panel.rows[index];
  preview.all_rows_state = 0;
  for (const auto& row : panel.rows.first(panel.cursor.count)) {
    preview.all_rows_state += row.state;
  }
  return preview;
}

void DrawCategory(std::int32_t edge_x, const CapacityPanel& panel,
                  const Preview& preview, std::uint8_t category,
                  CapacityHost& host) {
  std::uint32_t color = kCapacityNeutralColor;
  const std::int32_t capacity = panel.capacities[category];
  const std::int32_t count = host.CountItems(category);
  if (preview.shown) {
    const bool lit = preview.row == nullptr ||
                     host.CountContained(category, *preview.row) != 0;
    color = DimShifted(lit ? 1 : 0);
  }
  const std::int32_t y = kFirstCategoryY + (category * kCategoryStepY);
  host.DrawColoredLabel(CapacityLabel::kSlash, category,
                        Xy(edge_x + kSlashX, y), color);
  host.DrawColoredLabel(CapacityLabel::kCategory, category,
                        Xy(edge_x + kHeaderX, y), color);
  host.DrawInteger(capacity, Xy(edge_x + kCapacityX, y), color);
  if ((color >> kLitColorShift) != 0) {
    if (count * 2 < capacity) color = kCapacityRoomyColor;
    if (preview.row != nullptr &&
        capacity < host.CountContained(category, *preview.row) + count) {
      color = kCapacityOverflowColor;
    }
  }
  host.DrawInteger(count, Xy(edge_x + kSlashX, y), color);
}

void DrawCommands(std::int32_t edge_x, const CapacityPanel& panel,
                  const Preview& preview, CapacityHost& host) {
  host.DrawButtonBackground(edge_x + kButtonBackgroundX, kGetRowY,
                            kGetBackgroundW, kButtonBackgroundH);
  host.DrawButtonBackground(edge_x + kButtonBackgroundX, kEndRowY,
                            kEndBackgroundW, kButtonBackgroundH);
  host.DrawColoredLabel(CapacityLabel::kGet, 0,
                        Xy(edge_x + kCommandX, kGetRowY),
                        DimShifted(panel.controls_enabled));
  host.DrawButton(PromptButton::kCircle, edge_x + kCircleX, kButtonIconY);
  const std::uint32_t all_color = preview.all_rows_state != 0
                                      ? DimShifted(panel.controls_enabled & 1U)
                                      : kCapacityDimColor;
  host.DrawColoredLabel(CapacityLabel::kAll, 0, Xy(edge_x + kAllX, kGetRowY),
                        all_color);
  host.DrawButton(PromptButton::kSquare, edge_x + kSquareX, kButtonIconY);
  host.DrawColoredLabel(CapacityLabel::kEnd, 0,
                        Xy(edge_x + kCommandX, kEndRowY),
                        DimShifted(panel.controls_enabled));
  host.DrawButton(PromptButton::kCross, edge_x + kCircleX, kEndIconY);
}

[[nodiscard]] std::span<const std::uint8_t> WeaponName(
    const Items::InventoryWeapon& weapon) {
  const auto bytes = std::as_bytes(std::span(weapon.name));
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
  return {reinterpret_cast<const std::uint8_t*>(bytes.data()), bytes.size()};
}

// The row the item's name and look fill in; false when its slot is outside
// its table (the menu row keeps its old look).
bool FillItemRow(const LootRow& loot, const RowPlacement& placement,
                 const Items::InventoryContents& inventory, LootRowsHost& host,
                 MenuRow*& row) {
  const auto category = static_cast<ItemCategory>(loot.category);
  const std::uint8_t index = loot.item_index;
  if (loot.category >= kListedCategories || !InTable(category, index)) {
    return false;
  }
  switch (category) {
    case ItemCategory::kWeapon: {
      const auto& weapon = inventory.weapons.at(index);
      row = &host.SetRow(placement, WeaponName(weapon));
      // decomp: the blade is read at blades[blade - 1]; blade 0 is not one.
      if (weapon.blade == 0 || weapon.blade > inventory.blades.size()) {
        return false;
      }
      const auto& blade = inventory.blades.at(weapon.blade - 1U);
      row->row_icon = blade.category;
      row->material = static_cast<std::uint8_t>(blade.material);
      return true;
    }
    case ItemCategory::kBlade: {
      const auto& blade = inventory.blades.at(index);
      row = &host.SetRow(placement, host.ItemName(blade.id));
      row->row_icon = blade.category;
      row->material = static_cast<std::uint8_t>(blade.material);
      return true;
    }
    case ItemCategory::kGrip: {
      const auto& grip = inventory.grips.at(index);
      row = &host.SetRow(placement, host.ItemName(grip.id));
      row->row_icon = static_cast<std::uint8_t>(grip.category + kGripIconBase);
      return true;
    }
    case ItemCategory::kShield: {
      const auto& shield = inventory.shields.at(index);
      row = &host.SetRow(placement, host.ItemName(shield.base.id));
      row->row_icon = kShieldIcon;
      row->material = static_cast<std::uint8_t>(shield.base.material);
      return true;
    }
    case ItemCategory::kArmor: {
      const auto& armor = inventory.armor.at(index);
      row = &host.SetRow(placement, host.ItemName(armor.id));
      row->row_icon =
          static_cast<std::uint8_t>(armor.category + kArmorIconBase);
      row->material = static_cast<std::uint8_t>(armor.material);
      return true;
    }
    case ItemCategory::kGem: {
      const auto& gem = inventory.gems.at(index);
      row = &host.SetRow(placement, host.ItemName(gem.id));
      row->row_icon = kGemIcon;
      return true;
    }
    case ItemCategory::kMisc: {
      const auto& misc = inventory.misc.at(index);
      row = &host.SetRow(placement, host.ItemName(misc.id));
      row->count = misc.count;
      row->font_color = misc.id < kFirstKeyItemId ? 0 : 1;
      return true;
    }
    case ItemCategory::kManage:
      break;
  }
  return false;
}

void SlideRow(LootRow& loot, MenuRow& row) {
  if (loot.slide_step == 0) return;
  row.x = static_cast<std::int16_t>(row.x - (loot.slide_step * kSlideStepX));
  ++loot.slide_step;
  if (loot.slide_step == kSlideDone) row.gradient_state = 0;
}

void MarkRow(int visible_row, LootRow& loot, const LootCursor& cursor,
             std::uint8_t gradient, MenuRow& row) {
  row.gradient_state = gradient;
  row.unselectable = loot.state == 0 ? 1 : 0;
  SlideRow(loot, row);
  if (visible_row == 0 && cursor.offset != 0) row.is_scrollable = kRowScrollsUp;
  if (visible_row == kVisibleLootRows - 1 &&
      cursor.offset != cursor.count - kVisibleLootRows) {
    row.is_scrollable = kRowScrollsDown;
  }
  if (loot.state == kChosenRowState) {
    row.background_width = kChosenRowBackground;
  }
  if (loot.state > kChosenRowState) {
    row.x = static_cast<std::int16_t>(
        row.x + ((loot.state - kFirstIndentedRowState) * kIndentStepX));
  }
}

[[nodiscard]] RowPlacement PromptPlacement(int row, int y) {
  return {.row = row,
          .x = kPromptX,
          .y = y,
          .w = kPromptWidth,
          .background_width = 0};
}

void SlidePromptRow(PromptHost& host, int row, int y, PromptText text) {
  MenuRow& slid = host.SetRow(PromptPlacement(row, y), text);
  slid.state = RowState::kSlideX;
  slid.target0 = kPromptTargetX;
}

void SelectPromptRow(PromptHost& host, std::uint8_t selected) {
  host.Row(kFirstPromptRow + selected).selected = 1;
  host.Row(kFirstPromptRow + 1 - selected).selected = 0;
}

void FlyOutPromptRows(PromptHost& host) {
  host.FlyOutRight(kFirstPromptRow);
  host.FlyOutRight(kFirstPromptRow + 1);
}

std::int32_t ConfirmLeave(LeaveConfirmation& prompt, bool claimed,
                          const PromptInput& input, PromptHost& host) {
  FlyOutPromptRows(host);
  if (!input.information_shown) host.DismissInformationBox();
  if (prompt.choice == 0 && (input.pressed & kPromptCancelButton) == 0) {
    host.PlayDiscardSound();
    return claimed ? kLeaveClaimed : kLeaveUnclaimed;
  }
  host.PlayLeaveSound();
  return kStay;
}

std::optional<std::int32_t> ChooseLeave(LeaveConfirmation& prompt, bool claimed,
                                        const PromptInput& input,
                                        PromptHost& host) {
  if (prompt.choice > kNoChoice) return std::nullopt;
  SelectPromptRow(host, prompt.choice);
  if ((input.pressed & kPromptConfirmButtons) != 0) {
    return ConfirmLeave(prompt, claimed, input, host);
  }
  if ((input.repeat & kPromptToggleButtons) != 0) {
    host.PlayChangeSound();
    prompt.choice = static_cast<std::uint8_t>(kNoChoice - prompt.choice);
  }
  prompt.cursor_step =
      host.DrawCursor(prompt.cursor_step, kPromptCursorBase + prompt.choice);
  return 0;
}

[[nodiscard]] PromptText OrganizeInformation(const OrganizeMenu& menu) {
  if (menu.selected_row != 0) return PromptText::kOrganizeInventoryInfo;
  return menu.action == kDisassembleAction ? PromptText::kDisassembleInfo
                                           : PromptText::kOrganizeInfo;
}

std::optional<std::int32_t> ChooseOrganize(OrganizeMenu& menu,
                                           const PromptInput& input,
                                           PromptHost& host) {
  if (menu.selected_row > kOrganizeInventoryRow) return std::nullopt;
  SelectPromptRow(host, menu.selected_row);
  if ((input.pressed & kPromptConfirmButtons) != 0) {
    FlyOutPromptRows(host);
    if ((input.pressed & kPromptCancelButton) != 0) {
      host.PlayLeaveSound();
      return kStay;
    }
    host.PlaySelectSound();
    if (menu.selected_row == 0) {
      menu.selected_row = static_cast<std::uint8_t>(menu.action + 1);
    }
    return menu.selected_row;
  }
  if (menu.action != 0 && (input.repeat & kPromptToggleButtons) != 0) {
    host.PlayChangeSound();
    menu.selected_row =
        static_cast<std::uint8_t>(kOrganizeInventoryRow - menu.selected_row);
  }
  host.ShowInformation(OrganizeInformation(menu));
  menu.cursor_step =
      host.DrawCursor(menu.cursor_step, kPromptCursorBase + menu.selected_row);
  return 0;
}
}  // namespace

// decomp: MENUB.PRG _displayInventoryCapacities
// (src/MENU/MENUB.PRG/260.c:1015), 8010837C
bool RenderBagCapacities(std::int32_t edge_x, const CapacityPanel& panel,
                         CapacityHost& host) {
  if (panel.capacities.size() < kListedCategories) return false;
  const auto preview = ReadPreview(panel);
  if (!preview) return false;
  host.DrawLabel(CapacityLabel::kGadgetBag, Xy(edge_x + kPanelLeft, kTitleY));
  for (std::uint8_t category = 0; category < kListedCategories; ++category) {
    DrawCategory(edge_x, panel, *preview, category, host);
  }
  DrawCommands(edge_x, panel, *preview, host);
  return true;
}

// decomp: MENUB.PRG _displayCurrentLoot (src/MENU/MENUB.PRG/260.c:1192),
// 80108938
ItemsStatus RenderLootRows(std::int32_t x, LootRowsView& view,
                           const Items::InventoryContents& inventory,
                           LootRowsHost& host) {
  std::array<std::uint8_t, kGradientSlots> gradients{};
  for (int i = 0; i < kVisibleLootRows; ++i) {
    MenuRow& row = host.Row(kFirstLootMenuRow + i);
    gradients.at(static_cast<std::size_t>(i) + view.gradient_state_index) =
        row.gradient_state;
    row.state = RowState::kInactive;
  }
  const int count = std::min<int>(view.cursor.count, kVisibleLootRows);
  ItemsStatus status = ItemsStatus::kOk;
  for (int i = 0; i < count; ++i) {
    const std::size_t at = static_cast<std::size_t>(view.cursor.offset) + i;
    if (at >= view.rows.size()) {
      status = ItemsStatus::kSlotOutOfRange;
      break;
    }
    LootRow& loot = view.rows[at];
    MenuRow* row = &host.Row(kFirstLootMenuRow + i);
    const RowPlacement placement{.row = kFirstLootMenuRow + i,
                                 .x = kRowLeft - x,
                                 .y = kRowTop + (i * kRowStepY),
                                 .w = kRowWidth,
                                 .background_width = 0};
    if (!FillItemRow(loot, placement, inventory, host, row) &&
        loot.category < kListedCategories) {
      status = ItemsStatus::kSlotOutOfRange;
    }
    MarkRow(i, loot, view.cursor, gradients.at(at), *row);
  }
  view.gradient_state_index = view.cursor.offset;
  return status;
}

// decomp: MENUB.PRG _populateLootItem (src/MENU/MENUB.PRG/260.c:1319),
// 80108E78
ItemsStatus ShowLootDescription(const LootRow& row, DescriptionHost& host) {
  if (row.category >= kListedCategories) {
    return ItemsStatus::kCategoryOutOfRange;
  }
  const auto category = static_cast<ItemCategory>(row.category);
  if (!InTable(category, row.item_index)) return ItemsStatus::kSlotOutOfRange;
  host.BackupInventory();
  host.LoadStagedInventory();
  host.ShowInformation(host.FormatDescription(category, row.item_index));
  host.RestoreInventory();
  return ItemsStatus::kOk;
}

// decomp: MENUB.PRG _discardItems (src/MENU/MENUB.PRG/260.c:1377), 801090A4
// (the init != 0 half)
ItemsStatus InitLeaveConfirmation(LeaveConfirmation& prompt,
                                  const LeaveContext& context,
                                  const Items::InventoryContents& inventory) {
  prompt.choice = kNoChoice;
  prompt.state = 0;
  if (context.count == 0) {
    prompt.state = kLeaveEmptyState;
    return ItemsStatus::kOk;
  }
  if (!context.claimed && context.single_entry) {
    prompt.state = kLeaveSingleEntryState;
    return ItemsStatus::kOk;
  }
  ItemsStatus status = ItemsStatus::kOk;
  const std::size_t count =
      std::min<std::size_t>(context.count, context.rows.size());
  if (count < context.count) status = ItemsStatus::kSlotOutOfRange;
  for (const auto& row : context.rows.first(count)) {
    if (row.category != static_cast<std::uint8_t>(ItemCategory::kMisc)) {
      continue;
    }
    if (!InTable(ItemCategory::kMisc, row.item_index)) {
      status = ItemsStatus::kSlotOutOfRange;
      continue;
    }
    const std::uint16_t id = inventory.misc.at(row.item_index).id;
    if (id >= kFirstKeyItemId) {
      prompt.kept_item = id;
      prompt.state = kLeaveKeyItemState;
    }
  }
  return status;
}

// decomp: MENUB.PRG _discardItems (src/MENU/MENUB.PRG/260.c:1377), 801090A4
// (the per-frame half)
std::optional<std::int32_t> UpdateLeaveConfirmation(LeaveConfirmation& prompt,
                                                    bool claimed,
                                                    const PromptInput& input,
                                                    PromptHost& host) {
  switch (prompt.state) {
    case 0:
      SlidePromptRow(host, kFirstPromptRow, kPromptFirstY,
                     PromptText::kOptionYes);
      prompt.state = 1;
      break;
    case 1:
      SlidePromptRow(host, kFirstPromptRow + 1, kPromptSecondY,
                     PromptText::kOptionNo);
      host.InitInformationTextBox();
      host.ShowInformation(claimed ? PromptText::kItemsRemaining
                                   : PromptText::kDiscardAllConfirm);
      prompt.state = 2;
      break;
    case 2:
      if (host.RowsIdle()) ++prompt.state;
      break;
    case kLeaveChoiceState:
      return ChooseLeave(prompt, claimed, input, host);
    case kLeaveEmptyState:
    case kLeaveSingleEntryState:
      return prompt.state - kLeaveResultBase;
    case kLeaveKeyItemState:
      host.SetStringItem(prompt.kept_item);
      host.InitInformationTextBox();
      host.ShowInformation(PromptText::kInvalidDiscard);
      prompt.state = kLeaveKeyItemState + 1;
      [[fallthrough]];
    case kLeaveKeyItemState + 1:
      if ((input.pressed & kPromptAnyFaceButtons) != 0) {
        host.PlayLeaveSound();
        return kStay;
      }
      break;
    default:
      break;
  }
  return 0;
}

// decomp: MENUB.PRG _organizeInventory (src/MENU/MENUB.PRG/260.c:1510),
// 80109444 (the loot != NULL half)
ItemsStatus InitOrganizeMenu(OrganizeMenu& menu, const LootRow& row,
                             const Items::InventoryContents& inventory,
                             PromptHost& host) {
  const bool misc =
      row.category == static_cast<std::uint8_t>(ItemCategory::kMisc);
  if (misc && !InTable(ItemCategory::kMisc, row.item_index)) {
    return ItemsStatus::kSlotOutOfRange;
  }
  host.InitInformationBox();
  menu.category = row.category;
  menu.state = 0;
  menu.selected_row = 0;
  menu.action = 0;
  if (row.category == static_cast<std::uint8_t>(ItemCategory::kWeapon) ||
      row.category == static_cast<std::uint8_t>(ItemCategory::kShield)) {
    menu.action = kDisassembleAction;
  } else if (misc && inventory.misc.at(row.item_index).id < kFirstKeyItemId) {
    menu.action = kOrganizeStackAction;
  } else {
    menu.selected_row = kOrganizeInventoryRow;
  }
  if (menu.action != 0) {
    SlidePromptRow(host, kFirstPromptRow, kPromptFirstY,
                   menu.action == kDisassembleAction ? PromptText::kDisassemble
                                                     : PromptText::kOrganize);
  }
  return ItemsStatus::kOk;
}

// decomp: MENUB.PRG _organizeInventory (src/MENU/MENUB.PRG/260.c:1510),
// 80109444 (the per-frame half)
std::optional<std::int32_t> UpdateOrganizeMenu(OrganizeMenu& menu,
                                               const PromptInput& input,
                                               PromptHost& host) {
  switch (menu.state) {
    case 0:
      SlidePromptRow(host, kFirstPromptRow + 1, kPromptSecondY,
                     PromptText::kOrganizeInventory);
      menu.state = 1;
      break;
    case 1:
      if (host.RowsIdle()) ++menu.state;
      break;
    case 2:
      return ChooseOrganize(menu, input, host);
    default:
      break;
  }
  return 0;
}
}  // namespace VS::Menu::Loot
