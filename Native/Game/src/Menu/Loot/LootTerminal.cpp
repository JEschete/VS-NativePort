#include "Menu/Loot/LootTerminal.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace VS::Menu::Loot {
namespace {
constexpr std::uint8_t kAnimationSteps = 10;
constexpr std::uint8_t kIconSteps = 4;
constexpr std::uint8_t kPreviewChangeStep = 6;
constexpr std::uint8_t kPreviewAll = 128;
constexpr std::int32_t kSlidePixels = 32;
constexpr std::int32_t kTitleRow = 3;
constexpr std::int32_t kYesRow = 30;
constexpr std::int32_t kNoRow = 31;
constexpr std::int32_t kFirstLootRow = 32;
constexpr std::int32_t kMenuRight = 320;
constexpr std::int32_t kTitleX = 180;
constexpr std::int32_t kTitleY = 18;
constexpr std::int32_t kCategoryY = 34;
constexpr std::int32_t kTitleWidth = 126;
constexpr std::int32_t kCategoryWidth = 140;
constexpr std::int32_t kTitleStyle = 8;
constexpr std::int32_t kPromptX = 194;
constexpr std::int32_t kYesY = 146;
constexpr std::int32_t kNoY = 162;
constexpr std::int32_t kPromptCursorBase = 8;
constexpr std::int32_t kLootCursorX = 10;
constexpr std::int32_t kLootCursorY = 42;
constexpr std::int32_t kRowHeight = 16;
constexpr std::int32_t kHiddenHudOffset = 0x200000;
constexpr std::int32_t kComparisonActor = 16;
constexpr std::uint16_t kStackMaximum = 100;
constexpr std::uint32_t kFaceButtons = 255;
constexpr std::uint32_t kConfirm = 0x20;
constexpr std::uint32_t kCancel = 0x40;
constexpr std::uint32_t kTakeAll = 0x80;
constexpr std::uint32_t kUp = 0x1000;
constexpr std::uint32_t kDown = 0x4000;
constexpr std::uint8_t kClaimDelay = 3;
constexpr std::uint8_t kSelectionStateMask = 7;
constexpr std::uint8_t kPreviewRowMask = 15;
constexpr std::uint8_t kDisassemblyRoot = 2;
constexpr std::uint8_t kDetachedShield = 3;
constexpr std::uint8_t kDetachedPart = 4;
constexpr std::uint8_t kDetachedGem = 5;
constexpr std::uint8_t kWeaponPartsWithRoot = 3;

enum TransitionStep : std::uint8_t {
  kTransitionBegin,
  kTransitionHideHud,
  kTransitionInventory,
  kTransitionReturn,
};
enum ScreenStep : std::uint8_t {
  kScreenInitialize,
  kScreenLoadText,
  kScreenWaitRows,
  kScreenSelection,
  kScreenEmpty,
  kScreenTeardown,
};
enum OrganizationChoice : std::int32_t {
  kOrganizeInventory = 1,
  kDisassembleItem,
  kConsolidateStack,
};

enum PromptStep : std::uint8_t {
  kPreview,
  kShowYes,
  kShowNo,
  kWaitRows,
  kChoose,
  kApply,
  kRestore,
  kWaitExit,
};
enum SelectionStep : std::uint8_t {
  kInitialize,
  kInput,
  kLeave,
  kOrganize,
  kInventory,
  kDisassemble,
  kConsolidate,
  kWaitInput,
  kWaitPanels,
  kRemoveClaimed,
};

void RequireCategory(LootCategory category, LootTerminalHost& host) {
  if (category >= LootCategory::kManage) {
    host.Refuse("loot category outside the seven item tables");
  }
}
void RequireCount(std::size_t count, LootTerminalHost& host) {
  if (count > kLootRowCapacity) host.Refuse("loot count exceeds owned rows");
}
void RequireItem(const LootRow& row, LootTerminalHost& host) {
  const auto category = static_cast<LootCategory>(row.category);
  RequireCategory(category, host);
  if (row.item_index >= Items::kCategoryCapacities[row.category]) {
    host.Refuse("loot item index outside its category table");
  }
}
void RequireRows(std::span<const LootRow> rows, std::size_t count,
                 LootTerminalHost& host) {
  RequireCount(count, host);
  for (const auto& row : rows.first(count)) RequireItem(row, host);
}
std::size_t SelectedRow(LootTerminalHost& host) {
  const auto& cursor = host.Runtime().cursor;
  RequireCount(cursor.count, host);
  const auto index = std::size_t{cursor.selected} + cursor.offset;
  if (cursor.selected >= kVisibleLootRows || index >= cursor.count) {
    host.Refuse("selected loot row outside the visible list");
  }
  RequireItem(host.Workspace().rows[index], host);
  return index;
}
void EraseRow(std::span<LootRow> rows, std::uint8_t& count, std::size_t index) {
  --count;
  for (; index < count; ++index) rows[index] = rows[index + 1];
}
void ShowPromptRow(LootTerminalHost& host, bool yes) {
  auto& row = host.SetRow(
      {.row = yes ? kYesRow : kNoRow,
       .x = kMenuRight,
       .y = yes ? kYesY : kNoY,
       .w = kTitleWidth,
       .background_width = 0},
      host.Text(yes ? LootTerminalText::kYes : LootTerminalText::kNo));
  row.state = Framework::RowState::kSlideX;
  row.target0 = kPromptX;
}
void PollPrompt(std::uint8_t& step, std::uint8_t& choice,
                std::uint8_t& animation, LootTerminalHost& host,
                LootTerminalSound apply_sound) {
  if (choice > 1) host.Refuse("loot confirmation choice is not yes or no");
  host.Row(kYesRow + choice).selected = 1;
  host.Row(kNoRow - choice).selected = 0;
  const auto buttons = host.Buttons();
  if ((buttons.pressed & (kConfirm | kCancel)) != 0) {
    host.FlyOutRight(kYesRow);
    host.FlyOutRight(kNoRow);
    host.Runtime().show_loot = 0;
    const bool cancel = choice != 0 || (buttons.pressed & kCancel) != 0;
    host.PlaySound(cancel ? LootTerminalSound::kLeave : apply_sound);
    step = cancel ? kRestore : kApply;
    if (host.Runtime().information_shown == 0) host.DismissInformation();
    return;
  }
  if ((buttons.repeat & (kUp | kDown)) != 0) {
    host.PlaySound(LootTerminalSound::kChange);
    choice = 1 - choice;
  }
  animation = host.DrawPromptCursor(animation, choice + kPromptCursorBase);
}
std::int32_t PanelOffset(std::uint8_t enabled, std::uint8_t& step,
                         LootTerminalHost& host) {
  if (step > kAnimationSteps) host.Refuse("loot panel animation outside curve");
  if (enabled != 0) {
    if (step < kAnimationSteps) ++step;
    return host.AnimationOffset(kAnimationSteps - step);
  }
  if (step != 0) --step;
  return (kAnimationSteps - step) * kSlidePixels;
}
void RenderInventoryPanels(LootTerminalHost& host) {
  auto& ui = host.Runtime().inventory_ui;
  const auto secondary =
      PanelOffset(ui.secondary_title, ui.secondary_step, host);
  if (ui.secondary_step != 0 && ui.icon_on_top == 0) {
    const auto category = static_cast<LootCategory>(
        (host.Runtime().inventory_category - 1U) & kSelectionStateMask);
    auto& row = host.SetRow({.row = kNoRow,
                             .x = secondary + kTitleX,
                             .y = kCategoryY,
                             .w = kCategoryWidth,
                             .background_width = kTitleStyle},
                            host.Text(LootTerminalText::kCategoryTitle,
                                      static_cast<std::int32_t>(category)));
    row.selected = 1;
    if (category != LootCategory::kManage) {
      row.max = host.Capacity(category);
      row.count = static_cast<std::uint16_t>(host.CountItems(category));
    }
  }
  host.RenderAshley(Framework::AshleyRenderMode::kRender);
  if (ui.icons_step > kIconSteps) {
    host.Refuse("loot icon animation outside curve");
  }
  if (ui.category_icons != 0) {
    if (ui.icons_step < kIconSteps) ++ui.icons_step;
  } else if (ui.icons_step != 0) {
    --ui.icons_step;
  }
  host.DrawCategoryIcons(ui.icons_step, ui.icon_on_top);
  const auto status = PanelOffset(ui.status_command, ui.status_step, host);
  if (ui.status_step != 0) host.DrawStatusCommand(status, ui.status_command);
}
void BeginInventoryView(InventoryTransitionState& state,
                        LootTerminalHost& host) {
  host.HideInventoryPanels();
  auto& runtime = host.Runtime();
  runtime.inventory_ui.secondary_step = 0;
  runtime.inventory_ui.icons_step = 0;
  runtime.inventory_ui.status_step = 0;
  runtime.inventory_ui.icon_on_top = 0;
  if (!host.RowsReady()) return;
  host.SetFieldRenderingEnabled(false);
  state.hud_was_hidden = runtime.hud_hidden;
  if (state.hud_was_hidden == 0) {
    host.SetHudHidden(true);
    runtime.hud_slide_offset = kHiddenHudOffset;
  }
  host.RenderAshley(Framework::AshleyRenderMode::kInit);
  static_cast<void>(host.LootInventory(true));
  state.step = kTransitionInventory;
}
void EndInventoryView(InventoryTransitionState& state, LootTerminalHost& host) {
  if (host.LootInventory(false) == 0) return;
  host.HideInventoryPanels();
  host.ClearMenu();
  host.SetFieldRenderingEnabled(true);
  if (state.hud_was_hidden == 0) {
    host.SetHudHidden(false);
    host.Runtime().hud_slide_offset = 0;
  }
  host.RenderAshley(Framework::AshleyRenderMode::kRestore);
  state.step = kTransitionReturn;
  if (host.Runtime().information_shown == 0) host.DismissInformation();
}
std::span<std::uint8_t> AttachedGems(Items::InventoryContents& inventory,
                                     LootCategory category, std::size_t slot,
                                     LootTerminalHost& host) {
  RequireCategory(category, host);
  if (slot >= Items::kCategoryCapacities[static_cast<std::size_t>(category)]) {
    host.Refuse("disassembly target outside inventory");
  }
  std::span<std::uint8_t> gems;
  std::size_t slots = 0;
  if (category == LootCategory::kWeapon) {
    auto& weapon = inventory.weapons[slot];
    auto* blade = Items::ReferencedSlot(inventory.blades, weapon.blade);
    auto* grip = Items::ReferencedSlot(inventory.grips, weapon.grip);
    if (blade == nullptr || grip == nullptr) {
      host.Refuse("disassembly weapon has a missing blade or grip");
    }
    slots = grip->gem_slots;
    gems = weapon.gems;
  } else if (category == LootCategory::kShield) {
    auto& shield = inventory.shields[slot];
    slots = shield.base.gem_slots;
    gems = shield.gems;
  } else {
    host.Refuse("only weapons and shields can be disassembled");
  }
  if (slots > gems.size()) host.Refuse("gem count exceeds disassembly owner");
  gems = gems.first(slots);
  for (auto gem : gems) {
    if (gem != 0 && Items::ReferencedSlot(inventory.gems, gem) == nullptr) {
      host.Refuse("disassembly gem reference outside inventory");
    }
  }
  return gems;
}
std::size_t DisassemblySlot(const DisassemblyState& state,
                            LootTerminalHost& host) {
  RequireCount(state.saved_count, host);
  if (state.target_row >= state.saved_count) {
    host.Refuse("disassembly target outside saved loot rows");
  }
  const auto& row = host.Workspace().working_rows[state.target_row];
  if (row.category != static_cast<std::uint8_t>(state.category)) {
    host.Refuse("disassembly target category changed during preview");
  }
  RequireItem(row, host);
  return row.item_index;
}
void PreviewDisassembly(const DisassemblyState& state, LootTerminalHost& host) {
  auto& workspace = host.Workspace();
  auto& runtime = host.Runtime();
  const auto slot = DisassemblySlot(state, host);
  const auto gems =
      AttachedGems(workspace.inventory, state.category, slot, host);
  workspace.rows.fill({});
  workspace.rows[0] = {static_cast<std::uint8_t>(state.category),
                       static_cast<std::uint8_t>(slot), 0, kDisassemblyRoot};
  if (state.category == LootCategory::kWeapon) {
    const auto& weapon = workspace.inventory.weapons[slot];
    workspace.rows[1] = {static_cast<std::uint8_t>(LootCategory::kBlade),
                         static_cast<std::uint8_t>(weapon.blade - 1U), 0,
                         kDetachedPart};
    workspace.rows[2] = {static_cast<std::uint8_t>(LootCategory::kGrip),
                         static_cast<std::uint8_t>(weapon.grip - 1U), 0,
                         kDetachedPart};
    runtime.cursor.count = kWeaponPartsWithRoot;
  } else {
    workspace.rows[1] = {static_cast<std::uint8_t>(LootCategory::kShield),
                         static_cast<std::uint8_t>(slot), 0, kDetachedShield};
    runtime.cursor.count = 2;
  }
  for (auto gem : gems) {
    if (gem != 0) {
      workspace.rows[runtime.cursor.count++] = {
          static_cast<std::uint8_t>(LootCategory::kGem),
          static_cast<std::uint8_t>(gem - 1U), 0, kDetachedGem};
    }
  }
  runtime.loot_step = kAnimationSteps;
  runtime.show_loot = 1;
}
void ApplyDisassembly(DisassemblyState& state, LootTerminalHost& host) {
  auto& workspace = host.Workspace();
  const auto count = host.Runtime().cursor.count;
  const auto slot = DisassemblySlot(state, host);
  auto gems = AttachedGems(workspace.inventory, state.category, slot, host);
  if (count < 2 || std::size_t{state.saved_count} + count - 1 >
                       workspace.working_rows.size()) {
    host.Refuse("disassembly component rows exceed the backup");
  }
  RequireRows(workspace.rows, count, host);
  std::ranges::copy(std::span(workspace.rows).subspan(1, count - 1),
                    workspace.working_rows.begin() + state.saved_count);
  if (state.category == LootCategory::kWeapon) {
    const auto& weapon = workspace.inventory.weapons[slot];
    workspace.inventory.blades[weapon.blade - 1U].assembled_weapon_index = 0;
    workspace.inventory.grips[weapon.grip - 1U].assembled_weapon_index = 0;
  }
  for (auto& gem : gems) {
    if (gem == 0) continue;
    workspace.inventory.gems[gem - 1U].set_item_index = 0;
    gem = 0;
  }
  state.saved_count = static_cast<std::uint8_t>(state.saved_count + count - 1U);
  EraseRow(workspace.working_rows, state.saved_count, state.target_row);
}
void RestoreDisassembly(DisassemblyState& state, LootTerminalHost& host) {
  RequireCount(state.saved_count, host);
  host.Runtime().cursor.count = state.saved_count;
  host.Workspace().rows = host.Workspace().working_rows;
  host.RefreshClaimableRows();
  host.Runtime().loot_step = kAnimationSteps;
  host.Runtime().show_loot = 1;
  state.step = kWaitExit;
}
void RemoveMatchingStacks(std::span<LootRow> rows, std::uint8_t& count,
                          Items::InventoryContents& inventory, std::uint16_t id,
                          std::uint16_t& total, LootTerminalHost& host) {
  RequireRows(rows, count, host);
  for (std::size_t row = 0; row < count;) {
    const auto entry = rows[row];
    if (entry.category == static_cast<std::uint8_t>(LootCategory::kMisc) &&
        inventory.misc[entry.item_index].id == id) {
      auto& item = inventory.misc[entry.item_index];
      if (item.count > kStackMaximum) {
        host.Refuse("loot stack exceeds 100 items");
      }
      total = static_cast<std::uint16_t>(total + item.count);
      item = {};
      EraseRow(rows, count, row);
    } else {
      ++row;
    }
  }
}
std::size_t EmptyMiscSlot(Items::InventoryContents& inventory) {
  const auto found =
      std::ranges::find(inventory.misc, 0, &Items::InventoryMisc::id);
  return static_cast<std::size_t>(found - inventory.misc.begin());
}
void PreviewConsolidation(StackConsolidationState& state,
                          LootTerminalHost& host) {
  auto workspace = host.Workspace();
  auto count = host.Runtime().cursor.count;
  std::uint16_t room = 0;
  for (const auto& item : host.AshleyInventory().misc) {
    if (item.id != state.item_id) continue;
    if (item.count > kStackMaximum) {
      host.Refuse("Ashley stack exceeds 100 items");
    }
    room = static_cast<std::uint16_t>(room + kStackMaximum - item.count);
  }
  std::uint16_t remaining = 0;
  RemoveMatchingStacks(workspace.rows, count, workspace.inventory,
                       state.item_id, remaining, host);
  count = 0;
  while (remaining != 0) {
    const auto slot = EmptyMiscSlot(workspace.inventory);
    if (slot == Items::kMiscSlots) {
      host.Refuse("consolidation preview has no free misc slot");
    }
    auto& row = workspace.rows[count++];
    row = {static_cast<std::uint8_t>(LootCategory::kMisc),
           static_cast<std::uint8_t>(slot), 0,
           static_cast<std::uint8_t>(room != 0)};
    // #412: existing-stack capacity is not a supply of loot.
    const auto amount = std::min(
        remaining, room != 0 ? std::min(room, kStackMaximum) : kStackMaximum);
    auto& item = workspace.inventory.misc[slot];
    item.id = state.item_id;
    item.count = static_cast<std::uint8_t>(amount);
    remaining = static_cast<std::uint16_t>(remaining - amount);
    if (room != 0) room = static_cast<std::uint16_t>(room - amount);
  }
  host.Workspace() = workspace;
  state.remaining = remaining;
  state.stack_room = room;
  host.Runtime().cursor.count = count;
  host.Runtime().loot_step = kAnimationSteps;
  host.Runtime().show_loot = 1;
}
void ApplyConsolidation(StackConsolidationState& state,
                        LootTerminalHost& host) {
  auto workspace = host.Workspace();
  auto count = state.saved_count;
  std::uint16_t ignored_total = 0;
  RemoveMatchingStacks(workspace.working_rows, count,
                       workspace.inventory_backup, state.item_id, ignored_total,
                       host);
  const auto preview_count = host.Runtime().cursor.count;
  RequireRows(workspace.rows, preview_count, host);
  for (std::size_t row = 0; row < preview_count; ++row) {
    const auto& preview = workspace.rows[row];
    if (preview.category != static_cast<std::uint8_t>(LootCategory::kMisc)) {
      host.Refuse("consolidation preview contains a non-misc row");
    }
    const auto slot = EmptyMiscSlot(workspace.inventory_backup);
    if (slot == Items::kMiscSlots || count == workspace.working_rows.size()) {
      host.Refuse("consolidation result exceeds the saved inventory");
    }
    workspace.working_rows[count++] = {
        static_cast<std::uint8_t>(LootCategory::kMisc),
        static_cast<std::uint8_t>(slot), 0, preview.state};
    auto& item = workspace.inventory_backup.misc[slot];
    item.id = state.item_id;
    item.count = workspace.inventory.misc[preview.item_index].count;
  }
  host.Workspace() = workspace;
  state.saved_count = count;
}
void NavigateLoot(LootCursor& cursor, const Input::PadState& buttons) {
  const auto last = cursor.count - 1U;
  if ((buttons.repeat & kUp) != 0) {
    if (cursor.count <= kVisibleLootRows) {
      cursor.selected = static_cast<std::uint8_t>(
          cursor.selected == 0 ? last : cursor.selected - 1U);
    } else if (cursor.offset == 0) {
      if (cursor.selected != 0) {
        --cursor.selected;
      } else if ((buttons.pressed & kUp) != 0) {
        cursor.selected = kVisibleLootRows - 1;
        cursor.offset = cursor.count - kVisibleLootRows;
      }
    } else if (cursor.selected == 1) {
      --cursor.offset;
    } else {
      --cursor.selected;
    }
  }
  if ((buttons.repeat & kDown) == 0) return;
  if (cursor.count <= kVisibleLootRows) {
    cursor.selected = static_cast<std::uint8_t>(
        cursor.selected == last ? 0 : cursor.selected + 1U);
  } else if (cursor.offset == cursor.count - kVisibleLootRows) {
    if (cursor.selected != kVisibleLootRows - 1) {
      ++cursor.selected;
    } else if ((buttons.pressed & kDown) != 0) {
      cursor.selected = 0;
      cursor.offset = 0;
    }
  } else if (cursor.selected == kVisibleLootRows - 2) {
    ++cursor.offset;
  } else {
    ++cursor.selected;
  }
}
void RecordKey(const LootRow& row, LootTerminalHost& host) {
  if (row.category != static_cast<std::uint8_t>(LootCategory::kMisc)) return;
  RequireItem(row, host);
  const auto id = host.Workspace().inventory.misc[row.item_index].id;
  if (id >= kFirstKeyItemId) host.RecordKey(id);
}
bool ClaimAll(LootTerminalHost& host) {
  auto& runtime = host.Runtime();
  auto& rows = host.Workspace().rows;
  RequireRows(rows, runtime.cursor.count, host);
  const auto previous = runtime.claimed;
  for (std::size_t index = 0; index < runtime.cursor.count; ++index) {
    const auto row = rows[index];
    if (!host.ClaimItem(row)) continue;
    RecordKey(row, host);
    rows[index].slide_step = 1;
    ++runtime.claimed;
  }
  if (previous == runtime.claimed) {
    host.PlaySound(LootTerminalSound::kInvalid);
    return false;
  }
  host.PlaySound(LootTerminalSound::kClaim);
  return true;
}
void ProcessLootInput(LootSelectionState& state, LootTerminalHost& host) {
  auto& runtime = host.Runtime();
  runtime.controls_enabled = 1;
  const auto buttons = host.Buttons();
  if ((buttons.pressed & kCancel) != 0) {
    host.PlaySound(LootTerminalSound::kLeave);
    runtime.controls_enabled = 0;
    static_cast<void>(host.LeaveMenu(true));
    state.step = kLeave;
    return;
  }
  if ((buttons.pressed & kTakeAll) != 0 && ClaimAll(host)) {
    state.step = kRemoveClaimed;
    return;
  }
  const auto index = SelectedRow(host);
  const auto row = host.Workspace().rows[index];
  if ((buttons.pressed & kConfirm) == 0) {
    NavigateLoot(runtime.cursor, buttons);
    const auto selected = SelectedRow(host);
    if (selected != index) host.PlaySound(LootTerminalSound::kChange);
    host.ShowDescription(selected);
    state.cursor_animation = host.DrawLootCursor(
        state.cursor_animation, kLootCursorX,
        runtime.cursor.selected * kRowHeight + kLootCursorY);
  } else if (row.state == 0) {
    host.PlaySound(LootTerminalSound::kSelect);
    runtime.controls_enabled = 0;
    static_cast<void>(host.OrganizeMenu(index));
    state.step = kOrganize;
  } else {
    host.PlaySound(LootTerminalSound::kClaim);
    RecordKey(row, host);
    static_cast<void>(host.ClaimItem(row));
    host.Workspace().rows[index].slide_step = 1;
    state.removal_delay = kClaimDelay;
    ++runtime.claimed;
    state.step = kRemoveClaimed;
  }
}
void ProcessOrganization(LootSelectionState& state, LootTerminalHost& host) {
  const auto result = host.OrganizeMenu(std::nullopt);
  if (result == 0) return;
  if (result < 0) {
    state.step = kInitialize;
    return;
  }
  const auto row = SelectedRow(host);
  switch (result) {
    case kOrganizeInventory:
      static_cast<void>(host.InventoryTransition(
          static_cast<LootCategory>(host.Workspace().rows[row].category)));
      state.step = kInventory;
      break;
    case kDisassembleItem:
      static_cast<void>(host.Disassemble(row));
      state.step = kDisassemble;
      break;
    case kConsolidateStack:
      static_cast<void>(host.Consolidate(row));
      state.step = kConsolidate;
      break;
    default:
      host.Refuse("organization returned an unknown loot action");
  }
}
void RemoveClaimed(LootSelectionState& state, LootTerminalHost& host) {
  if (state.removal_delay != 0) {
    --state.removal_delay;
    return;
  }
  auto& runtime = host.Runtime();
  auto& rows = host.Workspace().rows;
  RequireCount(runtime.cursor.count, host);
  for (std::size_t index = 0; index < runtime.cursor.count;) {
    if (rows[index].slide_step != 0) {
      EraseRow(rows, runtime.cursor.count, index);
    } else {
      ++index;
    }
  }
  host.RefreshClaimableRows();
  if (runtime.cursor.count == 0) {
    runtime.controls_enabled = 0;
    static_cast<void>(host.LeaveMenu(true));
    state.step = kLeave;
  } else {
    state.step = kInitialize;
  }
}
void RenderScreen(LootTerminalHost& host) {
  auto& runtime = host.Runtime();
  const auto advance = [&](std::uint8_t shown, std::uint8_t& step) {
    if (step > kAnimationSteps) {
      host.Refuse("loot screen animation outside curve");
    }
    if (shown != 0) {
      if (step != 0) --step;
      return host.AnimationOffset(step);
    }
    if (step < kAnimationSteps) ++step;
    return step * kSlidePixels;
  };
  const auto inventory =
      advance(runtime.show_inventory, runtime.inventory_step);
  if (runtime.inventory_step < kAnimationSteps) {
    host.RenderCapacities(inventory);
  }
  const auto loot = advance(runtime.show_loot, runtime.loot_step);
  if (runtime.loot_step < kAnimationSteps) {
    host.RenderLoot(loot);
    if ((runtime.previewed & kPreviewRowMask) != 0) {
      if (runtime.previewed > kVisibleLootRows) {
        host.Refuse("previewed loot row outside the menu rows");
      }
      host.Row(runtime.previewed + kFirstLootRow - 1).selected = 1;
    }
  }
}
void InitializeScreen(std::uint8_t& step, LootTerminalHost& host) {
  host.PlaySound(LootTerminalSound::kOpen);
  host.PopulateBaseStats(kComparisonActor);
  for (std::size_t index = 0;
       index < static_cast<std::size_t>(LootCategory::kManage); ++index) {
    host.RebuildInventory(static_cast<LootCategory>(index));
  }
  host.AcquireWorkspace();
  auto& runtime = host.Runtime();
  runtime.claimed = 0;
  runtime.cursor.count = 0;
  runtime.cursor.offset = 0;
  static_cast<void>(host.LoadItemText(true));
  host.BuildStagedInventory();
  runtime.controls_enabled = 0;
  runtime.show_inventory = 1;
  runtime.inventory_step = kAnimationSteps;
  runtime.show_loot = 0;
  runtime.loot_step = kAnimationSteps;
  step = kScreenLoadText;
}
void LoadScreenText(std::uint8_t& step, LootTerminalHost& host) {
  if (!host.LoadItemText(false)) return;
  host.Runtime().show_loot = 1;
  if (host.Runtime().cursor.count == 0) {
    host.InitializeInformation(true);
    host.SetInformation(host.Text(LootTerminalText::kEmptyChest));
    step = kScreenEmpty;
  } else {
    host.InitializeInformation(false);
    step = kScreenWaitRows;
  }
}
}  // namespace

// decomp: topLevelMenuTransition (src/MENU/MENUB.PRG/260.c:64; was
// _topLevelMenuTransition), original MENUB.PRG [0x801066CC, 0x80106B80).
std::int32_t UpdateInventoryTransition(InventoryTransitionState& state,
                                       LootTerminalHost& host,
                                       std::optional<LootCategory> category) {
  if (category) {
    RequireCategory(*category, host);
    host.Runtime().inventory_category =
        static_cast<std::uint8_t>(static_cast<std::uint8_t>(*category) + 1U);
    host.Runtime().show_inventory = 0;
    host.Runtime().show_loot = 0;
    state.step = kTransitionBegin;
    return 0;
  }
  switch (state.step) {
    case kTransitionBegin:
      if (host.RowsReady()) {
        host.SetBackgroundFade(true);
        host.SetMenuCommand(true);
        auto& row = host.SetRow({.row = kTitleRow,
                                 .x = kMenuRight,
                                 .y = kTitleY,
                                 .w = kTitleWidth,
                                 .background_width = kTitleStyle},
                                host.Text(LootTerminalText::kItemsTitle));
        row.state = Framework::RowState::kSlideX;
        row.target0 = kTitleX;
        row.selected = 1;
        state.step = kTransitionHideHud;
      }
      break;
    case kTransitionHideHud:
      BeginInventoryView(state, host);
      break;
    case kTransitionInventory:
      EndInventoryView(state, host);
      break;
    case kTransitionReturn:
      host.SetBackgroundFade(false);
      host.SetMenuCommand(false);
      if (host.RowsReady()) {
        host.Runtime().show_inventory = 1;
        host.Runtime().show_loot = 1;
        return 1;
      }
      break;
    default:
      host.Refuse("unknown loot inventory-transition state");
  }
  RenderInventoryPanels(host);
  return 0;
}

// decomp: disassembleItem (src/MENU/MENUB.PRG/260.c:235; was _disassembleItem),
// original MENUB.PRG [0x80106B80, 0x801073E0).
std::int32_t UpdateDisassembly(DisassemblyState& state, LootTerminalHost& host,
                               std::optional<std::size_t> row) {
  host.RequireBackups(false);
  auto& workspace = host.Workspace();
  auto& runtime = host.Runtime();
  if (row) {
    RequireCount(runtime.cursor.count, host);
    if (*row >= runtime.cursor.count) {
      host.Refuse("disassembly row outside loot");
    }
    const auto target = workspace.rows[*row];
    RequireItem(target, host);
    const auto category = static_cast<LootCategory>(target.category);
    static_cast<void>(
        AttachedGems(workspace.inventory, category, target.item_index, host));
    workspace.working_rows = workspace.rows;
    runtime.show_loot = 0;
    state.saved_count = runtime.cursor.count;
    state.target_row = static_cast<std::uint8_t>(*row);
    state.category = category;
    runtime.cursor.selected = 0;
    runtime.cursor.offset = 0;
    state.choice = 0;
    state.step = kPreview;
    return 0;
  }
  switch (state.step) {
    case kPreview:
      if (runtime.loot_step == kPreviewChangeStep) {
        PreviewDisassembly(state, host);
        state.step = kShowYes;
      }
      break;
    case kShowYes:
      ShowPromptRow(host, true);
      state.step = kShowNo;
      break;
    case kShowNo: {
      const auto slot = DisassemblySlot(state, host);
      ShowPromptRow(host, false);
      if (state.category == LootCategory::kWeapon) {
        host.SetWeaponName(slot);
      } else {
        host.SetItemName(workspace.inventory.shields[slot].base.id);
      }
      host.InitializeInformation(true);
      host.SetInformation(host.Text(LootTerminalText::kDisassembleConfirm));
      state.step = kWaitRows;
      break;
    }
    case kWaitRows:
      if (host.RowsReady()) state.step = kChoose;
      break;
    case kChoose:
      PollPrompt(state.step, state.choice, state.cursor_animation, host,
                 LootTerminalSound::kDisassemble);
      break;
    case kApply:
      if (runtime.loot_step != kPreviewChangeStep) break;
      ApplyDisassembly(state, host);
      [[fallthrough]];
    case kRestore:
      if (runtime.loot_step == kPreviewChangeStep) {
        RestoreDisassembly(state, host);
      }
      break;
    case kWaitExit:
      if (runtime.loot_step == 0) return -1;
      break;
    default:
      host.Refuse("unknown loot disassembly state");
  }
  return 0;
}

// decomp: consolidateMiscItems (src/MENU/MENUB.PRG/260.c:461; was
// _consolidateMiscItems), original MENUB.PRG [0x801073E0, 0x80107C54).
std::int32_t UpdateStackConsolidation(StackConsolidationState& state,
                                      LootTerminalHost& host,
                                      std::optional<std::size_t> row) {
  host.RequireBackups(true);
  auto& workspace = host.Workspace();
  auto& runtime = host.Runtime();
  if (row) {
    RequireCount(runtime.cursor.count, host);
    if (*row >= runtime.cursor.count) {
      host.Refuse("consolidation row outside loot");
    }
    const auto target = workspace.rows[*row];
    RequireItem(target, host);
    if (target.category != static_cast<std::uint8_t>(LootCategory::kMisc)) {
      host.Refuse("only misc loot can be consolidated");
    }
    const auto id = workspace.inventory.misc[target.item_index].id;
    if (id == 0 || id >= kFirstKeyItemId) {
      host.Refuse("consolidation requires an ordinary nonempty misc item");
    }
    workspace.working_rows = workspace.rows;
    workspace.inventory_backup = workspace.inventory;
    runtime.show_loot = 0;
    state.saved_count = runtime.cursor.count;
    state.item_id = id;
    runtime.cursor.selected = 0;
    runtime.cursor.offset = 0;
    state.choice = 0;
    state.step = kPreview;
    return 0;
  }
  switch (state.step) {
    case kPreview:
      if (runtime.loot_step == kPreviewChangeStep) {
        PreviewConsolidation(state, host);
        state.step = kShowYes;
      }
      break;
    case kShowYes:
      ShowPromptRow(host, true);
      state.step = kShowNo;
      break;
    case kShowNo:
      ShowPromptRow(host, false);
      host.SetItemName(state.item_id);
      host.InitializeInformation(true);
      host.SetInformation(host.Text(LootTerminalText::kOrganizeConfirm));
      state.step = kWaitRows;
      break;
    case kWaitRows:
      if (host.RowsReady()) state.step = kChoose;
      break;
    case kChoose:
      PollPrompt(state.step, state.choice, state.cursor_animation, host,
                 LootTerminalSound::kSelect);
      break;
    case kApply:
      if (runtime.loot_step != kPreviewChangeStep) break;
      ApplyConsolidation(state, host);
      [[fallthrough]];
    case kRestore:
      if (runtime.loot_step == kPreviewChangeStep) {
        RequireCount(state.saved_count, host);
        runtime.cursor.count = state.saved_count;
        workspace.rows = workspace.working_rows;
        workspace.inventory = workspace.inventory_backup;
        runtime.loot_step = kAnimationSteps;
        runtime.show_loot = 1;
        state.step = kWaitExit;
      }
      break;
    case kWaitExit:
      if (runtime.loot_step == 0) return -1;
      break;
    default:
      host.Refuse("unknown loot consolidation state");
  }
  return 0;
}

// decomp: processLootMenu (src/MENU/MENUB.PRG/260.c:1625; was
// _processLootMenu), original MENUB.PRG [0x80109750, 0x80109E90).
std::int32_t UpdateLootSelection(LootSelectionState& state,
                                 LootTerminalHost& host, bool initialize) {
  auto& runtime = host.Runtime();
  if (initialize) {
    runtime.cursor.selected = 0;
    state.step = kInitialize;
    return 0;
  }
  switch (state.step) {
    case kInitialize:
      if (!host.RowsReady()) break;
      host.InitializeInformation(false);
      state.step = kInput;
      [[fallthrough]];
    case kInput:
      ProcessLootInput(state, host);
      break;
    case kLeave: {
      const auto result = host.LeaveMenu(false);
      if (result < 0) {
        state.step = kWaitPanels;
      } else if (result != 0) {
        if (!host.AdvanceBatch()) return result;
        runtime.controls_enabled = 0;
        runtime.show_loot = 1;
        state.step = kInitialize;
      }
      break;
    }
    case kOrganize:
      ProcessOrganization(state, host);
      break;
    case kInventory:
      if (host.InventoryTransition(std::nullopt) != 0) state.step = kWaitPanels;
      break;
    case kDisassemble:
      if (host.Disassemble(std::nullopt) != 0) state.step = kWaitPanels;
      break;
    case kConsolidate:
      if (host.Consolidate(std::nullopt) != 0) state.step = kWaitPanels;
      break;
    case kWaitInput:
      if (host.Buttons().pressed == 0) break;
      [[fallthrough]];
    case kWaitPanels:
      if (runtime.loot_step == 0) {
        runtime.controls_enabled = 1;
        state.step = kInitialize;
        host.RefreshClaimableRows();
      }
      break;
    case kRemoveClaimed:
      RemoveClaimed(state, host);
      break;
    default:
      host.Refuse("unknown loot selection state");
  }
  runtime.previewed =
      (state.step & kSelectionStateMask) == kInput
          ? static_cast<std::uint8_t>(runtime.cursor.selected + 1U)
          : kPreviewAll;
  return 0;
}

// decomp: vs_menuB_exec (src/MENU/MENUB.PRG/260.c:1954),
// original MENUB.PRG [0x80109E90, 0x8010A24C).
std::int32_t UpdateLootScreen(std::uint8_t& step, LootTerminalHost& host) {
  auto& runtime = host.Runtime();
  runtime.previewed = 0;
  switch (step) {
    case kScreenInitialize:
      InitializeScreen(step, host);
      break;
    case kScreenLoadText:
      LoadScreenText(step, host);
      break;
    case kScreenWaitRows:
      if (!host.RowsReady() || runtime.loot_step != 0) break;
      static_cast<void>(host.Selection(true));
      step = kScreenSelection;
      [[fallthrough]];
    case kScreenSelection: {
      const auto result = host.Selection(false);
      if (result != 0) {
        runtime.result_kind = result == 1 ? 1 : 0;
        step = kScreenTeardown;
      }
      break;
    }
    case kScreenEmpty:
      if (host.InformationDone() &&
          (host.Buttons().pressed & kFaceButtons) != 0) {
        step = kScreenTeardown;
      }
      break;
    case kScreenTeardown:
      runtime.show_inventory = 0;
      runtime.show_loot = 0;
      if (host.DismissScreenText() || !host.UnloadItemText()) break;
      host.ResetPaging();
      host.ReleaseWorkspace();
      if (host.HaveRoomLoot()) host.ClearRoomLoot();
      step = kScreenInitialize;
      return 1;
    default:
      host.Refuse("unknown loot screen state");
  }
  RenderScreen(host);
  return 0;
}
}  // namespace VS::Menu::Loot
