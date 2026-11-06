// Hand-maintained (tools/port/owned.csv).
#include "Menu/Setup/SetupScreen.hpp"  // NOLINT(build/include): typed API

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <format>
#include <optional>
#include <span>
#include <string_view>

#include "Core/Math/ScalarMath.hpp"
#include "Items/Inventory.hpp"
#include "Menu/Setup/Assembly.hpp"
#include "Menu/Setup/Combination.hpp"
#include "Render/EncodedText.hpp"

namespace VS::Menu::Setup {
namespace {
using Framework::MenuRow;
using Framework::PackXy;
using Framework::RowPlacement;
using Framework::RowState;
using Framework::RowText;
using Items::ItemCategory;

constexpr std::uint8_t kTerminator = Render::kTextTerminator;
constexpr std::size_t kMenu12OffsetBytes = 2;
constexpr std::size_t kByteBits = 8;
// An offset no MENU12 block reaches: a refused entry's.
constexpr std::size_t kMissingOffset = static_cast<std::size_t>(-1) / 4;

// The rows slide in from the right edge; the lists step 16 lines a row.
constexpr int kOffscreenX = 320;
constexpr int kRowPitch = 16;

// _drawOk: "OK" beside a row, pulsing with the blink phase.
constexpr int kOkX = 175;
constexpr int kOkTopY = 32;
constexpr int kBlinkAngleScale = 8;
constexpr int kBlinkShift = 5;
constexpr int kBlinkBase = 64;
constexpr std::uint32_t kGreyStep = 0x010101;

// _setMenuTitle.
constexpr int kTitleY = 18;
constexpr int kSubtitleY = 34;
constexpr int kTitleWidth = 140;
constexpr int kTitleBackground = 8;
constexpr int kTitleTargetX = 180;

// _initMenuItem's header, dropping in from the left.
constexpr RowPlacement kEquipmentHeader{
    .row = kHeaderRow,
    .x = -164,
    .y = 18,
    .w = 164,
    .background_width = 8,
};
constexpr int kHeaderTargetX = 16;

// The equipment details' commands.
constexpr int kAllStatPages = 7;
constexpr int kWeaponStatPages = 3;
constexpr int kHideStatPages = -1;
constexpr int kShowDpPp = 3;
constexpr int kShowDpOnly = 1;
constexpr int kHideDpPp = 4;
constexpr int kShowPanel = 1;
constexpr int kHidePanel = 2;

// The prompts: their rows slide to x 194 from y 146 (two choices) or 130
// (three); the cursor's main-menu positions start at 8 and 7.
constexpr int kPromptTargetX = 194;
constexpr int kPromptWidth = kOffscreenX - kPromptTargetX;
constexpr int kConfirmTopY = 146;
constexpr int kThreeChoiceTopY = 130;
constexpr int kConfirmChoices = 2;
constexpr int kThreeChoices = 3;
constexpr int kConfirmCursorBase = 8;
constexpr int kThreeChoiceCursorBase = 7;
constexpr std::uint8_t kPromptRowsAppeared = 2;
constexpr std::uint8_t kThreeChoiceRowsAppeared = 3;
constexpr std::uint8_t kPromptChoosing = 3;
constexpr std::uint8_t kThreeChoiceChoosing = 4;
constexpr int kChoiceUp = 2;
constexpr int kChooseYes = 1;
constexpr int kChooseNo = 2;
constexpr int kChooseCancel = 3;

// The item list.
constexpr RowPlacement kListHeader{
    .row = kItemListRow,
    .x = 155,
    .y = 18,
    .w = 165,
    .background_width = 0,
};
constexpr int kListTopY = 18;
constexpr int kListRowX = 169;
constexpr int kListRowWidth = 151;
constexpr std::uint8_t kListShown = 16;
constexpr std::uint8_t kListChoosing = 17;
constexpr int kListCursorX = 155;
constexpr int kListCursorTop = 10;
constexpr int kFirstRowCursorNudge = 14;
constexpr int kListScrollWindowEnd = kItemListVisibleRows - 1;
constexpr int kListScrollDownAt = 4;
constexpr std::uint8_t kScrollMore = 2;
constexpr std::uint8_t kScrollBack = 1;
constexpr int kListLeftNoSound = 2;
constexpr int kListLeftClosed = -3;
// The entries' flags: unselectable (bit 0), the outset count (bits 9-15,
// shown less 100), the material (16-18), an unused field (19-25) and the
// icon (26-31).
constexpr int kOutsetShift = 9;
constexpr int kOutsetMask = 0x7F;
constexpr int kOutsetBias = 100;
constexpr int kMaterialShift = 16;
constexpr int kMaterialMask = 7;
constexpr int kRowInfoShift = 19;
constexpr int kRowInfoMask = 0x7F;
constexpr int kIconShift = 26;
// The description chunk code that opens a copied description.
constexpr std::uint8_t kChunkCode = 0xF8;

// The selection screens: the title slides to x 16 at width 164, the chosen
// row to (155, 18); five component rows appear one a frame.
constexpr int kSelectedTitleWidth = 164;
constexpr int kSelectedTitleX = 16;
constexpr int kSelectedRowX = 155;
constexpr int kSelectedRowY = 18;
constexpr int kListRowBase = 9;
constexpr std::uint8_t kComponentRows = 5;
constexpr int kDisassemblyRowInfo = 0x19100;
constexpr int kDisassemblyTitle = 58;
constexpr int kRenameRowInfo = 0x1913C;
constexpr int kCategoryRowInfo = 0x239;
constexpr int kCategoryCountShift = 9;
constexpr int kCategoryRowFlags = 0x400000;
constexpr int kDisassemblySubtitleRow = 12;
constexpr int kConfirmIndexStride = 4;

// The workshop's top menu.
constexpr int kTopRowInfo = 0x135;
constexpr int kTopRows = 6;
constexpr int kTopBackgroundOt = 2046;
constexpr int kTopBackgroundBrightness = 128;
constexpr int kTopTitleX = 180;
constexpr int kWeaponsFullCount = 8;
// The top menu's rows, in order.
enum TopRow : std::uint8_t {
  kTopRowAssemble,
  kTopRowAttachGems,
  kTopRowDisassemble,
  kTopRowRename,
  kTopRowRepair,
  kTopRowCombine,
};
constexpr int kEquipMenu = 8;
constexpr int kRepairHoist = 4;
constexpr int kRepairInfoHeader = 3;
constexpr int kRepairPpDivisor = 100;
constexpr std::uint8_t kAccessoryCategory = 7;

// _loadObject.
constexpr int kMenuSequenceModel = 10;
constexpr int kShieldReload = 1;

// _disassembleMenu after the parts: the prompt for unequipped or equipped
// equipment, then Ashley's model reload.
constexpr std::uint8_t kDisassemblyConfirm = kScreenParts + 1;
constexpr std::uint8_t kDisassemblyReload = kDisassemblyConfirm + 2;
// _renameWeaponMenu after the parts: the prompt, then the rename screen.
constexpr std::uint8_t kRenameConfirm = kScreenParts + 1;
constexpr std::uint8_t kRenameStart = kRenameConfirm + 1;
// The top menu's repair row flies out (14).
constexpr int kRepairMenuRow = 14;

enum RepairStep : std::uint8_t {
  kRepairAsk,
  kRepairAnswer,
  kRepairApply,
  kRepairResult,
  kRepairLeave,
};

enum TopStep : std::uint8_t {
  kTopEnter,
  kTopTitle,
  kTopRowsStep,
  kTopChoose,
  kTopAssembly,
  kTopGems,
  kTopDisassembly,
  kTopRename,
  kTopRepair,
  kTopCombination,
  kTopReturn,
  kTopLeave,
};

std::span<const std::uint8_t> TerminatorText() {
  static constexpr std::array<std::uint8_t, 1> kEmpty{kTerminator};
  return kEmpty;
}

// Copies at most `count` bytes of `source` into `target`, padding with the
// terminator.
void CopyPadded(std::span<std::uint8_t> target,
                std::span<const std::uint8_t> source) {
  const std::size_t copied = std::min(source.size(), target.size());
  std::copy_n(source.begin(), copied, target.begin());
  std::fill(target.begin() + static_cast<std::ptrdiff_t>(copied), target.end(),
            kTerminator);
}

void ApplyListFlags(MenuRow& row, std::int32_t flags) {
  row.unselectable = static_cast<std::uint8_t>(flags & 1);
  row.row_icon = static_cast<std::uint8_t>(flags >> kIconShift);
  const int outset = (flags >> kOutsetShift) & kOutsetMask;
  if (outset != 0) {
    row.outset_icon = static_cast<std::uint8_t>(outset - kOutsetBias);
  }
  row.material =
      static_cast<std::uint8_t>((flags >> kMaterialShift) & kMaterialMask);
}

std::uint8_t RowInfo(std::int32_t flags) {
  return static_cast<std::uint8_t>((flags >> kRowInfoShift) & kRowInfoMask);
}

void SlideOutPrompt(Context& context, int action, int rows) {
  if (action == kChooseNo) {
    context.host.PlaySound(MenuSound::kLeave);
  }
  for (int i = 0; i < rows; ++i) {
    context.host.FlyOutRowRight(kPromptRow + i);
  }
}

// The pad's answer to a prompt: Triangle cancels, Cross says no, Circle
// takes the choice; 0 otherwise.
int PromptAnswer(const Framework::Buttons& buttons, int selected) {
  if (Pressed(buttons, pad::kTriangle)) {
    return kChooseCancel;
  }
  if (Pressed(buttons, pad::kCross)) {
    return kChooseNo;
  }
  if (Pressed(buttons, pad::kCircle)) {
    return selected + 1;
  }
  return 0;
}

ItemListState& List(Context& context) { return context.state.item_list; }

// The six visible rows after the window moved, with each entry's gradient.
void RedrawListWindow(Context& context, int previous_window) {
  ItemListState& list = List(context);
  std::array<std::uint8_t, kItemListCapacity> saved{};
  const int visible = std::min<int>(list.count, kItemListVisibleRows);
  for (int i = 1; i < visible; ++i) {
    const int index = i + previous_window;
    const auto entry = static_cast<std::size_t>(index);
    if (entry < saved.size()) {
      saved[entry] = context.host.MenuItem(i + kItemListRow).gradient_state;
    }
  }
  for (int i = 1;;) {
    const int index = i + list.window;
    const auto entry = static_cast<std::size_t>(index);
    if (entry >= list.items.size()) {
      context.host.Refuse(
          std::format("item list entry {} outside the list", entry));
      break;
    }
    MenuRow& row = context.host.SetMenuItem({.row = i + kItemListRow,
                                             .x = kListRowX,
                                             .y = i * kRowPitch + kListTopY,
                                             .w = kListRowWidth,
                                             .background_width = 0},
                                            list.items[entry].name);
    row.gradient_state = saved[entry];
    ApplyListFlags(row, list.items[entry].flags);
    ++i;
    if (i == list.count) {
      break;
    }
    if (i == kItemListVisibleRows) {
      if (list.window + kItemListVisibleRows < list.count) {
        row.is_scrollable = kScrollMore;
      }
      break;
    }
  }
  if (list.window != 0) {
    context.host.MenuItem(kItemListRow + 1).is_scrollable = kScrollBack;
  }
}

void MoveListUp(ItemListState& list) {
  if (list.count < kItemListVisibleRows + 1 || list.window == 0) {
    if (list.selection != 0) {
      --list.selection;
    }
  } else if (list.selection == 2) {
    --list.window;
  } else {
    --list.selection;
  }
}

void MoveListDown(ItemListState& list) {
  if (list.count < kItemListVisibleRows + 1) {
    if (list.selection < list.count - 1) {
      ++list.selection;
    }
  } else if (list.window == list.count - kItemListVisibleRows) {
    if (list.selection < kListScrollWindowEnd) {
      ++list.selection;
    }
  } else if (list.selection == kListScrollDownAt) {
    ++list.window;
  } else {
    ++list.selection;
  }
}

// _navigateItemsList's step 17: the pad over the shown list.
void ChooseListEntry(Context& context, int flags, int previous_selection) {
  ItemListState& list = List(context);
  const Framework::Buttons buttons = context.host.Input();
  MenuRow& current = context.host.MenuItem(list.selection + kItemListRow);
  if (Pressed(buttons, pad::kTriangle)) {
    if ((flags & kListLeftNoSound) == 0) {
      context.host.PlaySound(MenuSound::kLeave);
    }
    list.open = false;
    list.selection = kListLeftClosed;
    return;
  }
  if (Pressed(buttons, pad::kCircle)) {
    if (current.unselectable == 0) {
      list.open = false;
      list.selection = previous_selection;
      if (flags != 0) {
        context.host.PlaySoundEffect(
            previous_selection == 0 ? sfx::kListFirstChoice : sfx::kListChoice);
      }
      return;
    }
    context.host.PlaySound(MenuSound::kInvalid);
  }
  current.selected = 0;
  if (Pressed(buttons, pad::kCross)) {
    context.host.PlaySound(MenuSound::kLeave);
    list.open = false;
    list.selection = -2;
    return;
  }
  const int previous_window = list.window;
  if (Repeated(buttons, pad::kUp)) {
    MoveListUp(list);
  }
  if (Repeated(buttons, pad::kDown)) {
    MoveListDown(list);
  }
  const int entry = list.selection + list.window;
  if (previous_selection != entry) {
    context.host.PlaySound(MenuSound::kChange);
    if (list.window != previous_window) {
      RedrawListWindow(context, previous_window);
    }
    list.row_info_unused =
        RowInfo(list.items[static_cast<std::size_t>(entry)].flags);
  }
  context.host.MenuItem(list.selection + kItemListRow).selected = 1;
  int position = (((list.selection * kRowPitch) + kListCursorTop)
                  << Framework::kHalfwordShift) |
                 kListCursorX;
  if (list.selection == 0) {
    position -= kFirstRowCursorNudge;
  }
  list.cursor = static_cast<std::uint8_t>(context.host.DrawCursor(
      list.cursor, static_cast<std::uint32_t>(position)));
}

void ShowListRow(Context& context) {
  ItemListState& list = List(context);
  const auto& entry = list.items[list.step];
  MenuRow& row =
      context.host.SetMenuItem({.row = list.step + kItemListRow,
                                .x = kOffscreenX,
                                .y = (list.step * kRowPitch) + kListTopY,
                                .w = kListRowWidth,
                                .background_width = 0},
                               entry.name);
  row.state = RowState::kSlideX;
  row.target0 = kListRowX;
  ApplyListFlags(row, entry.flags);
  ++list.step;
  if (list.step == kItemListVisibleRows) {
    if (list.count >= kItemListVisibleRows + 1) {
      row.is_scrollable = kScrollMore;
    }
    list.step = kListShown;
  }
}

void ListEquipment(Context& context, EquipmentRows& rows, bool is_shield,
                   std::size_t description_offset) {
  Items::InventoryIndices& indices = context.host.Indices();
  for (std::size_t i = 0; i < kEquipmentListCapacity; ++i) {
    const int id = is_shield ? indices.shields[i] : indices.weapons[i];
    if (id == 0) {
      continue;
    }
    if (!CheckReference(context, id, kEquipmentListCapacity,
                        is_shield ? "shield" : "weapon")) {
      continue;
    }
    FormatEquipmentRow(context, rows, is_shield,
                       static_cast<std::size_t>(id - 1),
                       context.host.StringBuffer());
    rows.texts[rows.count].description =
        Menu12Text(context, description_offset);
    if (is_shield) {
      const Items::InventoryShield& shield =
          context.host.Inventory().shields[static_cast<std::size_t>(id - 1)];
      std::size_t slots = shield.base.gem_slots;
      if (slots > shield.gems.size()) {
        context.host.Refuse(std::format(
            "shield {} gem slots {} past the gem table", id, slots));
        slots = shield.gems.size();
      }
      const auto gems = std::span(shield.gems).first(slots);
      if (std::ranges::all_of(gems,
                              [](std::uint8_t gem) { return gem == 0; })) {
        rows.texts[rows.count].description =
            Menu12Text(context, menu12::kNoGems);
        rows.types[rows.count] |= 1;
      }
    }
    ++rows.count;
    context.state.available_items[rows.count] = static_cast<std::uint8_t>(id);
  }
}

void CreateEquipmentRows(Context& context, const EquipmentRows& rows,
                         int row_info) {
  context.host.CreateMenuRows(row_info, std::span(rows.texts).first(rows.count),
                              std::span(rows.types).first(rows.count));
}

void RepairDurability(std::uint16_t max_dp, std::uint16_t& current_dp,
                      std::uint16_t& current_pp) {
  const int cost = (max_dp - current_dp) / kRepairPpDivisor;
  const int remaining = current_pp - cost;
  current_dp = max_dp;
  current_pp = static_cast<std::uint16_t>(remaining >= 0 ? remaining : 0);
}

// _repairMenu's step 2: every listed blade, shield and armor piece back to
// full DP for PP, and Ashley's equipped pieces refreshed.
void RepairEverything(Context& context) {
  Items::InventoryContents& inventory = context.host.Inventory();
  Items::InventoryIndices& indices = context.host.Indices();
  for (const std::uint8_t id : indices.blades) {
    if (Items::InventoryBlade* blade =
            id == 0 ? nullptr
                    : InventorySlot(context, inventory.blades, id, "blade")) {
      RepairDurability(blade->max_dp, blade->current_dp, blade->current_pp);
    }
  }
  for (const std::uint8_t id : indices.weapons) {
    const Items::InventoryWeapon* weapon =
        id == 0 ? nullptr
                : InventorySlot(context, inventory.weapons, id, "weapon");
    if (weapon != nullptr && weapon->is_equipped != 0) {
      context.host.EquipWeapon(static_cast<std::size_t>(id - 1));
    }
  }
  for (const std::uint8_t id : indices.shields) {
    Items::InventoryShield* shield =
        id == 0 ? nullptr
                : InventorySlot(context, inventory.shields, id, "shield");
    if (shield == nullptr) {
      continue;
    }
    RepairDurability(shield->base.max_dp, shield->base.current_dp,
                     shield->base.current_pp);
    if (shield->is_equipped != 0) {
      context.host.EquipShield(static_cast<std::size_t>(id - 1));
    }
  }
  for (const std::uint8_t id : indices.armor) {
    Items::InventoryArmor* armor =
        id == 0 ? nullptr
                : InventorySlot(context, inventory.armor, id, "armor");
    if (armor == nullptr || armor->category == kAccessoryCategory) {
      continue;
    }
    RepairDurability(armor->max_dp, armor->current_dp, armor->current_pp);
    if (armor->limb != 0) {
      context.host.EquipArmor(armor->limb - 1,
                              static_cast<std::size_t>(id - 1));
    }
  }
}

void CreateTopRows(Context& context) {
  std::array<RowText, kTopRows> texts{};
  std::span<std::int32_t> types = context.host.RowTypes();
  if (types.size() < texts.size()) {
    context.host.Refuse("row type buffer shorter than the top menu");
    return;
  }
  for (std::size_t i = 0; i < texts.size(); ++i) {
    const int entry = static_cast<int>(i) * 2;
    texts[i] = {
        .name = Menu12Entry(context, entry + menu12::kAssembleWeaponsIndex),
        .description =
            Menu12Entry(context, entry + menu12::kAssembleWeaponsDescIndex)};
    types[i] = 0;
  }
  const int weapons = Items::Inventory::CountItems(ItemCategory::kWeapon,
                                                   context.host.Inventory());
  if (weapons == kWeaponsFullCount) {
    texts[kTopRowAssemble].description =
        Menu12Text(context, menu12::kWeaponsFull);
    types[kTopRowAssemble] = 1;
  }
  if (weapons == 0) {
    texts[kTopRowRename].description = Menu12Text(context, menu12::kNoWeapons);
    types[kTopRowRename] = 1;
  }
  const int workshop = context.host.CurrentWorkshop();
  context.state.combination.workshop = static_cast<std::uint8_t>(workshop);
  if (workshop == 0) {
    // Only disassembly, renaming and gem attachment work outside a workshop.
    for (const TopRow row : {kTopRowRepair, kTopRowCombine, kTopRowAssemble}) {
      texts[row].description = Menu12Text(context, menu12::kNotInWorkshop);
      types[row] = 1;
    }
  }
  const int memory = context.host.CursorMemory();
  if (!context.state.first_entry) {
    context.host.SetCursorMemory(1);
  }
  context.state.first_entry = false;
  context.host.CreateMenuRows(kTopRowInfo, texts, types.first(texts.size()));
  context.host.SetCursorMemory(memory);
}

void OpenTopEntry(Context& context, int selected, std::uint8_t& step) {
  switch (selected) {
    case kTopRowAssemble:
      UpdateAssembly(context, true);
      step = kTopAssembly;
      break;
    case kTopRowAttachGems:
      UpdateGemCategoryMenu(context, true);
      step = kTopGems;
      break;
    case kTopRowDisassemble:
      UpdateDisassemblyCategory(context, true);
      step = kTopDisassembly;
      break;
    case kTopRowRename:
      UpdateWeaponRename(context, true);
      step = kTopRename;
      break;
    case kTopRowRepair:
      UpdateRepair(context, true);
      step = kTopRepair;
      break;
    case kTopRowCombine:
      UpdateCombinationCategory(context, true);
      step = kTopCombination;
      break;
    default:
      break;
  }
}

void ChooseTopEntry(Context& context, std::uint8_t& step) {
  context.host.UpdateInformationVisibility(context.host.HighlightedRow(),
                                           context.host.ItemsListRow());
  const int selected = context.host.SelectedRow();
  const int choice = selected + 1;
  if (choice == 0) {
    return;
  }
  if (choice > 0) {
    OpenTopEntry(context, selected, step);
    return;
  }
  context.host.ClearMenuExcept(kNoRow);
  if (choice == -2) {
    context.host.CloseInformationTextBox();
    step = kTopLeave;
  } else {
    context.host.ReturnFromSetup(0);
    step = kTopEnter;
  }
}

// The submenus' results: -2 leaves the workshop, anything else returns to
// its top menu.
void FinishSubmenu(int result, std::uint8_t& step) {
  if (result != 0) {
    step = result == -2 ? kTopLeave : kTopReturn;
  }
}

void FinishAssembly(Context& context, int result, std::uint8_t& step) {
  if (result == 0) {
    return;
  }
  if (result == -2) {
    step = kTopLeave;
    return;
  }
  step = kTopReturn;
  if (result == 1) {
    context.host.OpenMenu(kEquipMenu);
    context.host.DismissInformationBox();
  } else {
    context.host.InitializeInformationBox();
  }
}

void FinishRename(Context& context, int result, std::uint8_t& step) {
  if (result == 0) {
    return;
  }
  if (result == -2) {
    step = kTopLeave;
    return;
  }
  step = kTopReturn;
  if (result == 1) {
    context.host.OpenMenu(kEquipMenu);
  }
}

void EnterTop(Context& context, std::uint8_t& step) {
  for (std::uint8_t category = 0;
       category <= static_cast<std::uint8_t>(ItemCategory::kMisc); ++category) {
    const Items::ItemsStatus status = Items::Inventory::RebuildDisplayIndices(
        static_cast<ItemCategory>(category), context.host.Indices(),
        context.host.Inventory());
    if (status != Items::ItemsStatus::kOk) {
      context.host.Refuse(Items::Describe(status));
    }
  }
  context.host.SetBackgroundPriority(kTopBackgroundOt,
                                     kTopBackgroundBrightness);
  const MenuRow& title = context.host.MenuItem(kTitleRow);
  if (title.state != RowState::kIdle || title.x != kTopTitleX) {
    SetTitleRow(context, kTitleRow, menu12::kSetup);
  }
  step = kTopRowsStep;
}

int DisassemblyConfirmed(Context& context, int confirmation) {
  EquipmentScreenState& screen = context.state.disassembly;
  switch (confirmation) {
    case kChooseYes:
      context.host.PlaySoundEffect(sfx::kDisassemble);
      if (screen.is_shield) {
        DetachShieldGems(context, context.state.combining_item);
      } else {
        DisassembleWeapon(context, context.state.combining_item);
      }
      if (screen.step == kDisassemblyConfirm) {
        return -1;
      }
      if (screen.is_shield) {
        context.host.UnequipShield();
      } else {
        context.host.UnequipWeapon();
      }
      screen.step = kDisassemblyReload;
      break;
    case kChooseNo:
      screen.step = kScreenTitle;
      break;
    case kChooseCancel:
      return LeaveItemSelection(context, false);
    default:
      break;
  }
  return 0;
}

// _disassembleMenu's step 4: the parts appear, then the prompt (asked twice
// over for equipped equipment).
int AskDisassembly(Context& context) {
  EquipmentScreenState& screen = context.state.disassembly;
  const int slot = context.state.combining_item;
  if (screen.appear_step < kComponentRows) {
    ++screen.appear_step;
    if (screen.is_shield) {
      Framework::CreateShieldGemRow(context.host.ItemRows(), screen.appear_step,
                                    slot + 1, true);
    } else {
      Framework::CreateWeaponComponentRow(context.host.ItemRows(),
                                          screen.appear_step, slot + 1, true);
    }
    return 0;
  }
  if (!CheckReference(context, slot + 1, kEquipmentListCapacity,
                      "disassembled equipment")) {
    return LeaveItemSelection(context, false);
  }
  const Items::InventoryContents& inventory = context.host.Inventory();
  const int equipped =
      screen.is_shield
          ? inventory.shields[static_cast<std::size_t>(slot)].is_equipped
          : inventory.weapons[static_cast<std::size_t>(slot)].is_equipped;
  UpdateConfirmationPrompt(context, true);
  context.host.SetInformationMessage(Menu12Entry(
      context, (screen.is_shield ? kConfirmIndexStride : 0) +
                   menu12::kDisassembleWeaponConfirmIndex + (equipped != 0)));
  screen.step = static_cast<std::uint8_t>(equipped + kDisassemblyConfirm);
  return 0;
}
}  // namespace

std::span<const std::uint8_t> Menu12Text(Context& context, std::size_t offset) {
  const std::span<const std::uint8_t> text = context.host.Menu12();
  if (offset >= text.size() / kMenu12OffsetBytes) {
    context.host.Refuse(std::format("MENU12 offset {} outside the {}-byte text",
                                    offset, text.size()));
    return {};
  }
  return text.subspan(offset * kMenu12OffsetBytes);
}

std::size_t Menu12EntryOffset(Context& context, int index) {
  const std::span<const std::uint8_t> text = context.host.Menu12();
  const auto byte = static_cast<std::size_t>(index) * kMenu12OffsetBytes;
  if (index < 0 || byte + 1 >= text.size()) {
    context.host.Refuse(std::format("MENU12 entry {} outside the {}-byte text",
                                    index, text.size()));
    return kMissingOffset;
  }
  return static_cast<std::size_t>(text[byte] | (text[byte + 1] << kByteBits));
}

std::span<const std::uint8_t> Menu12Entry(Context& context, int index) {
  const std::size_t offset = Menu12EntryOffset(context, index);
  return offset == kMissingOffset ? std::span<const std::uint8_t>{}
                                  : Menu12Text(context, offset);
}

std::span<const std::uint8_t> ItemName(Context& context, int index) {
  const std::span<const std::uint8_t> names =
      context.host.ItemRows().NameText();
  const auto offset = static_cast<std::size_t>(index) * kItemNameRowBytes;
  if (index < 0 || offset >= names.size()) {
    context.host.Refuse(
        std::format("item name {} outside the name table", index));
    return {};
  }
  return names.subspan(offset);
}

bool CheckReference(Context& context, int reference, std::size_t count,
                    std::string_view table) {
  if (reference < 1 || static_cast<std::size_t>(reference) > count) {
    context.host.Refuse(std::format("{} reference {} outside the {}-slot table",
                                    table, reference, count));
    return false;
  }
  return true;
}

std::span<const std::uint8_t> RowTextBytes(
    Context& context, std::optional<Framework::TextRef> text,
    std::span<const std::uint8_t> buffer,
    std::span<const std::uint8_t> weapon_name) {
  if (!text) {
    return {};
  }
  const auto tail = [&context, &text](std::span<const std::uint8_t> table) {
    if (text->offset >= table.size()) {
      context.host.Refuse(
          std::format("row text at {} outside its table", text->offset));
      return std::span<const std::uint8_t>{};
    }
    return table.subspan(text->offset);
  };
  switch (text->source) {
    case Framework::TextSource::kEmpty:
      return TerminatorText();
    case Framework::TextSource::kHelp:
      return tail(context.host.ItemRows().HelpText());
    case Framework::TextSource::kNames:
      return tail(context.host.ItemRows().NameText());
    case Framework::TextSource::kBuffer:
      return tail(buffer);
    case Framework::TextSource::kWeaponName:
      return weapon_name;
    case Framework::TextSource::kNone:
      break;
  }
  return {};
}

void ApplyRowText(Context& context, Framework::RowText& row,
                  const Framework::ItemRowText& text,
                  std::span<const std::uint8_t> buffer) {
  if (text.name) {
    row.name = RowTextBytes(context, text.name, buffer);
  }
  if (text.description) {
    row.description = RowTextBytes(context, text.description, buffer);
  }
}

void FormatEquipmentRow(Context& context, EquipmentRows& rows, bool is_shield,
                        std::size_t slot, std::span<std::uint8_t> buffer) {
  if (rows.count >= kEquipmentListCapacity) {
    context.host.Refuse("equipment list full");
    return;
  }
  const std::size_t row = rows.count;
  Framework::ItemRowsHost& item_rows = context.host.ItemRows();
  const Items::InventoryContents& inventory = context.host.Inventory();
  Framework::ItemRowText text;
  if (is_shield) {
    text = Framework::FormatInventoryShieldRow(
        item_rows, inventory.shields.at(slot), buffer);
  } else {
    const Items::InventoryWeapon& weapon = inventory.weapons.at(slot);
    std::ranges::transform(weapon.name, rows.names[row].begin(),
                           [](char c) { return static_cast<std::uint8_t>(c); });
    text = Framework::FormatInventoryWeaponRow(item_rows, weapon, buffer);
  }
  rows.texts[row] = {
      .name = RowTextBytes(context, text.name, buffer, rows.names[row]),
      .description = RowTextBytes(context, text.description, buffer)};
  rows.types[row] = static_cast<std::int32_t>(text.row_type);
}

int PickEquipmentRow(Context& context, EquipmentScreenState& screen) {
  screen.selected_row = context.host.SelectedRow() + 1;
  if (screen.selected_row == 0) {
    return 0;
  }
  context.host.ClearMenuExcept(kNoRow);
  if (screen.selected_row < 0) {
    return screen.selected_row;
  }
  MenuRow& title = context.host.MenuItem(kTitleRow);
  title.state = RowState::kSlideX;
  title.w = kSelectedTitleWidth;
  title.target0 = kSelectedTitleX;
  MenuRow& chosen = context.host.MenuItem(screen.selected_row + kListRowBase);
  chosen.state = RowState::kSlideXy;
  chosen.target0 = kSelectedRowX;
  chosen.target1 = kSelectedRowY;
  ++screen.step;
  return 0;
}

int SettleEquipmentRow(Context& context, const EquipmentScreenState& screen,
                       int icon) {
  context.host.MenuItem(kTitleRow).row_icon = static_cast<std::uint8_t>(icon);
  MoveMenuRow(context, kTitleRow, kHeaderRow);
  MoveMenuRow(context, screen.selected_row + kListRowBase, kDetailRow);
  const auto row = static_cast<std::size_t>(screen.selected_row);
  if (row >= context.state.available_items.size()) {
    context.host.Refuse(std::format("selected row {} outside the list", row));
    return 0;
  }
  const int id = context.state.available_items[row];
  context.state.combining_item = static_cast<std::uint8_t>(id - 1);
  return id;
}

std::span<std::uint8_t> NextPartBuffer(PartListRows& rows) {
  if (rows.count >= rows.buffers.size()) {
    return {};
  }
  return rows.buffers[rows.count];
}

bool AddPartRow(Context& context, PartListRows& rows,
                const Framework::ItemRowText& text, int id) {
  if (rows.count >= kItemListCapacity) {
    context.host.Refuse("part list full");
    return false;
  }
  const std::size_t row = rows.count;
  rows.texts[row] = {.name = TerminatorText(), .description = TerminatorText()};
  ApplyRowText(context, rows.texts[row], text, rows.buffers[row]);
  rows.types[row] = static_cast<std::int32_t>(text.row_type);
  context.state.available_items[row] = static_cast<std::uint8_t>(id);
  ++rows.count;
  return true;
}

void AddPlaceholderRow(Context& context, PartListRows& rows,
                       std::size_t name_offset,
                       std::size_t description_offset) {
  if (rows.count >= kItemListCapacity) {
    context.host.Refuse("part list full");
    return;
  }
  const std::size_t row = rows.count;
  rows.texts[row] = {.name = Menu12Text(context, name_offset),
                     .description = Menu12Text(context, description_offset)};
  rows.types[row] = 1;
  context.state.available_items[row] = 0;
  ++rows.count;
}

void MarkLastPartInvalid(Context& context, PartListRows& rows,
                         std::size_t description_offset) {
  if (rows.count == 0) {
    return;
  }
  rows.texts[rows.count - 1].description =
      Menu12Text(context, description_offset);
  rows.types[rows.count - 1] |= 1;
}

void ShowPartList(Context& context, const PartListRows& rows) {
  const std::span<std::int32_t> shared = context.host.RowTypes();
  if (shared.size() < rows.count) {
    context.host.Refuse("row type buffer shorter than the part list");
  } else {
    std::ranges::copy(std::span(rows.types).first(rows.count), shared.begin());
  }
  BuildItemList(context, std::span(rows.texts).first(rows.count),
                std::span(rows.types).first(rows.count));
}

int ListedPart(Context& context) {
  const ItemListState& list = List(context);
  const std::size_t row = static_cast<std::size_t>(list.window) +
                          static_cast<std::size_t>(std::max(list.selection, 0));
  if (list.selection < 0 || row >= context.state.available_items.size()) {
    context.host.Refuse(std::format("listed part {} outside the list", row));
    return 0;
  }
  return context.state.available_items[row];
}

int ChoosePart(Context& context, int& selected_row, std::uint8_t& step) {
  const ItemListState& list = List(context);
  context.host.UpdateInformationVisibility(list.selection + list.window,
                                           list.step);
  UpdateItemList(context, kListLeftNoSound);
  selected_row = GetSelectedItemRow(context.state) + 1;
  if (selected_row == 0) {
    return 0;
  }
  context.host.ClearMenuExcept(kNoRow);
  if (selected_row == -2) {
    return -2;
  }
  if (selected_row > 0) {
    const auto row = static_cast<std::size_t>(selected_row - 1);
    if (row >= context.state.available_items.size()) {
      context.host.Refuse(std::format("chosen part {} outside the list", row));
      selected_row = -1;
    } else {
      selected_row = context.state.available_items[row];
    }
  }
  step = 2;
  return 0;
}

void CreateCategoryRows(Context& context, CategoryState& menu, int row_info,
                        std::span<const RowText> texts,
                        std::span<const std::int32_t> row_types) {
  const int memory = context.host.CursorMemory();
  if (!menu.keep_cursor) {
    context.host.SetCursorMemory(1);
  }
  menu.keep_cursor = false;
  context.host.CreateMenuRows(row_info, texts, row_types);
  context.host.SetCursorMemory(memory);
  menu.step = kCategoryChoose;
}

void ReturnToCategory(Context& context, CategoryState& menu, int subtitle_row,
                      std::size_t subtitle_offset) {
  if (context.host.RowsIdle()) {
    context.host.InitializeInformationBox();
    SetTitleRow(context, kTitleRow, menu12::kSetup);
    SetTitleRow(context, subtitle_row, subtitle_offset);
    menu.step = kCategoryRows;
  }
}

// decomp: MENUC.PRG _drawOk (src/MENU/MENUC.PRG/168.c:19)
void DrawConfirmationLabel(Context& context, int row, int brightness) {
  const std::int32_t level =
      (Core::Math::Sine(brightness * kBlinkAngleScale) >> kBlinkShift) +
      kBlinkBase;
  context.host.DrawConfirmationText(
      PackXy(kOkX, (row * kRowPitch) + kOkTopY),
      static_cast<std::uint32_t>(level) * kGreyStep);
}

// decomp: MENUC.PRG _setMenuTitle (src/MENU/MENUC.PRG/168.c:29)
void SetTitleRow(Context& context, int row_id, std::size_t text_offset) {
  MenuRow& row =
      context.host.SetMenuItem({.row = row_id,
                                .x = kOffscreenX,
                                .y = row_id != 0 ? kSubtitleY : kTitleY,
                                .w = kTitleWidth,
                                .background_width = kTitleBackground},
                               Menu12Text(context, text_offset));
  row.state = RowState::kSlideX;
  row.target0 = kTitleTargetX;
  row.selected = 1;
}

// decomp: MENUC.PRG _initMenuItem (src/MENU/MENUC.PRG/168.c:110)
void InitializeEquipmentHeader(Context& context, std::size_t text_offset,
                               int icon) {
  MenuRow& row = context.host.SetMenuItem(kEquipmentHeader,
                                          Menu12Text(context, text_offset));
  row.state = RowState::kDismiss;
  row.target0 = kHeaderTargetX;
  row.selected = 1;
  row.row_icon = static_cast<std::uint8_t>(icon);
  context.host.ResetEquipmentStats();
  int pages = kAllStatPages;
  if (icon == kWeaponIcon) {
    context.host.SetRangeAndRisk(0, 0, 0, 1);
    pages = kWeaponStatPages;
  }
  context.host.UpdateEquipmentStatPages(pages);
  context.host.UpdateDpPpBars(icon == kArmorIcon ? kShowDpOnly : kShowDpPp);
  context.host.UpdatePrimaryStatPanel(kShowPanel);
}

// decomp: MENUC.PRG _copyMenuItem (src/MENU/MENUC.PRG/168.c:141)
void MoveMenuRow(Context& context, int source, int target) {
  if (source != target) {
    context.host.MenuItem(target) = context.host.MenuItem(source);
    context.host.DeactivateRow(source);
  }
}

// decomp: MENUC.PRG _confirmationPrompt (src/MENU/MENUC.PRG/168.c:155)
int UpdateConfirmationPrompt(Context& context, bool initialize) {
  PromptState& prompt = context.state.confirmation;
  if (initialize) {
    context.host.InitInformationTextBox(1);
    prompt.selected = 0;
    prompt.step = 0;
    return 0;
  }
  int action = 0;
  if (prompt.step < kPromptRowsAppeared) {
    MenuRow& row = context.host.SetMenuItem(
        {.row = prompt.step + kPromptRow,
         .x = kOffscreenX,
         .y = (prompt.step * kRowPitch) + kConfirmTopY,
         .w = kPromptWidth,
         .background_width = 0},
        Menu12Entry(context, prompt.step + menu12::kOptionYesIndex));
    row.state = RowState::kSlideX;
    row.target0 = kPromptTargetX;
    ++prompt.step;
  } else if (prompt.step == kPromptRowsAppeared) {
    prompt.step += context.host.RowsIdle() ? 1 : 0;
  } else if (prompt.step == kPromptChoosing) {
    for (int i = 0; i < kConfirmChoices; ++i) {
      context.host.MenuItem(i + kPromptRow).selected =
          static_cast<std::uint8_t>(i == prompt.selected);
    }
    const Framework::Buttons buttons = context.host.Input();
    action = PromptAnswer(buttons, prompt.selected);
    if (action == 0) {
      if (Repeated(buttons, pad::kUp | pad::kDown)) {
        context.host.PlaySound(MenuSound::kChange);
        prompt.selected = static_cast<std::uint8_t>(1 - prompt.selected);
      }
      prompt.cursor = static_cast<std::uint8_t>(context.host.DrawMainMenuCursor(
          prompt.cursor, prompt.selected + kConfirmCursorBase));
    }
  }
  if (action != 0) {
    SlideOutPrompt(context, action, kConfirmChoices);
  }
  return action;
}

// decomp: MENUC.PRG _confirmCombine (src/MENU/MENUC.PRG/168.c:225)
int UpdateThreeChoicePrompt(Context& context, int cancel_row) {
  PromptState& prompt = context.state.three_choice;
  if (cancel_row != 0) {
    context.host.InitInformationTextBox(1);
    prompt.cancel_row = static_cast<std::uint8_t>(cancel_row);
    prompt.selected = 0;
    prompt.step = 0;
    return 0;
  }
  int action = 0;
  if (prompt.step < kThreeChoiceRowsAppeared) {
    const std::span<const std::uint8_t> text =
        prompt.step == prompt.cancel_row
            ? Menu12Text(context, menu12::kCancelCombine)
            : Menu12Entry(context, prompt.step + menu12::kOptionYesIndex);
    MenuRow& row = context.host.SetMenuItem(
        {.row = prompt.step + kPromptRow,
         .x = kOffscreenX,
         .y = (prompt.step * kRowPitch) + kThreeChoiceTopY,
         .w = kPromptWidth,
         .background_width = 0},
        text);
    row.state = RowState::kSlideX;
    row.target0 = kPromptTargetX;
    ++prompt.step;
  } else if (prompt.step == kThreeChoiceRowsAppeared) {
    prompt.step += context.host.RowsIdle() ? 1 : 0;
  } else if (prompt.step == kThreeChoiceChoosing) {
    for (int i = 0; i < kThreeChoices; ++i) {
      context.host.MenuItem(i + kPromptRow).selected =
          static_cast<std::uint8_t>(i == prompt.selected);
    }
    const Framework::Buttons buttons = context.host.Input();
    action = PromptAnswer(buttons, prompt.selected);
    if (action == 0) {
      int choice = prompt.selected;
      if (Repeated(buttons, pad::kUp)) {
        choice += kChoiceUp;
      }
      if (Repeated(buttons, pad::kDown)) {
        ++choice;
      }
      if (choice >= kThreeChoices) {
        choice -= kThreeChoices;
      }
      if (choice != prompt.selected) {
        context.host.PlaySound(MenuSound::kChange);
        prompt.selected = static_cast<std::uint8_t>(choice);
      }
      prompt.cursor = static_cast<std::uint8_t>(context.host.DrawMainMenuCursor(
          prompt.cursor, prompt.selected + kThreeChoiceCursorBase));
    }
  }
  if (action != 0) {
    SlideOutPrompt(context, action, kThreeChoices);
  }
  return action;
}

// decomp: MENUC.PRG _leaveItemSlotSelection (src/MENU/MENUC.PRG/168.c:419)
int LeaveItemSelection(Context& context, bool hide_equipment_panels) {
  context.host.PlaySound(MenuSound::kLeave);
  context.host.ClearMenuExcept(kNoRow);
  context.host.FlyOutRowLeft(kTitleRow);
  if (hide_equipment_panels) {
    context.host.UpdateEquipmentStatPages(kHideStatPages);
    context.host.UpdatePrimaryStatPanel(kHidePanel);
    context.host.UpdateDpPpBars(kHideDpPp);
  }
  context.host.SetMenuCommandLabel();
  return Pressed(context.host.Input(), pad::kTriangle) ? -2 : -1;
}

// decomp: MENUC.PRG _populateItemsList (src/MENU/MENUC.PRG/168.c:450)
void BuildItemList(Context& context, std::span<const RowText> texts,
                   std::span<const std::int32_t> row_types) {
  ItemListState& list = List(context);
  std::size_t count = texts.size();
  if (count > kItemListCapacity) {
    context.host.Refuse(std::format("item list of {} entries past {}", count,
                                    kItemListCapacity));
    count = kItemListCapacity;
  }
  if (row_types.size() < count) {
    context.host.Refuse("item list with fewer row types than texts");
  }
  list.count = static_cast<std::uint8_t>(count);
  list.window = 0;
  list.selection = 0;
  list.step = 1;
  list.open = true;
  list.items.fill({});
  for (std::size_t i = 0; i < count; ++i) {
    Framework::ItemText& entry = list.items[i];
    entry.flags = i < row_types.size() ? row_types[i] : 0;
    if (texts[i].name.empty()) {
      entry.name[0] = kTerminator;
      entry.name[1] = kTerminator;
    } else {
      CopyPadded(std::span(entry.name).first(Framework::kRowNameCopyBytes),
                 texts[i].name.first(std::min(texts[i].name.size(),
                                              Framework::kRowNameCopyBytes)));
      entry.name[Framework::kRowNameCopyBytes] = kTerminator;
      entry.name[Framework::kRowNameCopyBytes + 1] = kTerminator;
    }
    if (texts[i].description.empty()) {
      entry.description[0] = kTerminator;
      entry.description[1] = kTerminator;
    } else {
      // The chunk code and its argument, the text, then two terminators.
      constexpr std::size_t kTextStart = 2;
      constexpr std::size_t kTextEnd =
          kTextStart + Framework::kRowDescriptionCopyBytes;
      entry.description[0] = kChunkCode;
      entry.description[1] = 0;
      const auto body =
          std::span(entry.description)
              .subspan(kTextStart, Framework::kRowDescriptionCopyBytes);
      CopyPadded(body, texts[i].description.first(
                           std::min(texts[i].description.size(),
                                    Framework::kRowDescriptionCopyBytes)));
      entry.description[kTextEnd] = kTerminator;
      entry.description[kTextEnd + 1] = kTerminator;
    }
  }
  const std::int32_t flags = list.items[0].flags;
  MenuRow& row = context.host.SetMenuItem(kListHeader, list.items[0].name);
  row.unselectable = static_cast<std::uint8_t>(flags & 1);
  row.row_icon = static_cast<std::uint8_t>(flags >> kIconShift);
  row.material =
      static_cast<std::uint8_t>((flags >> kMaterialShift) & kMaterialMask);
  row.selected = 1;
  list.row_info_unused = RowInfo(flags);
}

// decomp: MENUC.PRG _getSelectedRow (src/MENU/MENUC.PRG/168.c:511)
int GetSelectedItemRow(const State& state) {
  return state.item_list.open ? -1 : state.item_list.selection;
}

// decomp: MENUC.PRG _navigateItemsList (src/MENU/MENUC.PRG/168.c:518)
void UpdateItemList(Context& context, int flags) {
  ItemListState& list = List(context);
  if (list.step < kItemListVisibleRows) {
    if (list.step == list.count) {
      list.step = kListShown;
    } else {
      ShowListRow(context);
    }
    return;
  }
  const int previous_selection = list.selection + list.window;
  if (previous_selection < 0 ||
      static_cast<std::size_t>(previous_selection) >= list.items.size()) {
    context.host.Refuse(std::format("item list selection {} outside the list",
                                    previous_selection));
    return;
  }
  context.host.SetInformationMessage(
      list.items[static_cast<std::size_t>(previous_selection)].description);
  if (list.step == kListShown) {
    if (context.host.RowsIdle()) {
      list.step = kListChoosing;
    }
  } else if (list.step == kListChoosing) {
    ChooseListEntry(context, flags, previous_selection);
  }
}

// decomp: MENUC.PRG _initMenuHeader (src/MENU/MENUC.PRG/168.c:784)
void AnimateItemHeader(Context& context, int row) {
  MenuRow& header = context.host.MenuItem(row);
  header.state = RowState::kSlideXy;
  header.target0 = kListCursorX;
  header.target1 = kListTopY;
}

// decomp: MENUC.PRG _disassembleMenu (src/MENU/MENUC.PRG/168.c:2100)
int UpdateDisassembly(Context& context, int category) {
  EquipmentScreenState& screen = context.state.disassembly;
  if (category != 0) {
    context.host.InitInformationTextBox(1);
    screen.is_shield = category != 1;
    screen.step = 0;
    return 0;
  }
  switch (screen.step) {
    case kScreenTitle:
      if (context.host.RowsIdle()) {
        SetTitleRow(
            context, kTitleRow,
            Menu12EntryOffset(context, static_cast<int>(screen.is_shield) +
                                           menu12::kDisassembleWeaponIndex));
        screen.step = kScreenList;
      }
      break;
    case kScreenList:
      if (context.host.RowsIdle()) {
        EquipmentRows rows;
        ListEquipment(context, rows, screen.is_shield,
                      screen.is_shield ? menu12::kChooseShieldDisassemble
                                       : menu12::kChooseWeaponDisassemble);
        CreateEquipmentRows(
            context, rows,
            (static_cast<int>(screen.is_shield) + kDisassemblyTitle) |
                kDisassemblyRowInfo);
        screen.step = kScreenPick;
      }
      break;
    case kScreenPick:
      return PickEquipmentRow(context, screen);
    case kScreenSettle:
      if (context.host.RowsIdle()) {
        const int id = SettleEquipmentRow(
            context, screen, screen.is_shield ? kShieldIcon : kWeaponIcon);
        if (screen.is_shield) {
          context.host.SetShieldStats(id);
        } else {
          context.host.SetWeaponStats(id);
        }
        context.host.UpdateEquipmentStatPages(kAllStatPages);
        context.host.UpdateDpPpBars(kShowDpPp);
        screen.appear_step = 0;
        screen.step = kScreenParts;
      }
      break;
    case kScreenParts:
      return AskDisassembly(context);
    case kDisassemblyConfirm:
    case kDisassemblyConfirm + 1: {
      const int confirmation = UpdateConfirmationPrompt(context, false);
      if (confirmation == 0) {
        break;
      }
      context.host.UpdateEquipmentStatPages(kHideStatPages);
      context.host.UpdateDpPpBars(kHideDpPp);
      context.host.ClearMenuExcept(kNoRow);
      context.host.FlyOutRowLeft(kTitleRow);
      return DisassemblyConfirmed(context, confirmation);
    }
    case kDisassemblyReload:
      if (ReloadAshleyEquipment(context,
                                screen.is_shield ? kShieldReload : 0) == 0) {
        return -1;
      }
      break;
    default:
      break;
  }
  return 0;
}

// decomp: MENUC.PRG _disassembleTopMenu (src/MENU/MENUC.PRG/168.c:2285)
int UpdateDisassemblyCategory(Context& context, bool initialize) {
  CategoryState& menu = context.state.disassembly_category;
  if (initialize) {
    menu.keep_cursor = true;
    context.host.FlyOutRowsAndHoistSelection(2, 1);
    menu.step = 0;
    return 0;
  }
  int selection = 0;
  switch (menu.step) {
    case kCategoryRows: {
      if (!context.host.RowsIdle()) {
        break;
      }
      std::array<RowText, 2> texts{};
      std::array<std::int32_t, 2> types{};
      for (int i = 0; i < 2; ++i) {
        auto& text = texts[static_cast<std::size_t>(i)];
        auto& type = types[static_cast<std::size_t>(i)];
        text.name = Menu12Entry(context, (i * 2) + menu12::kWeaponsIndex);
        text.description =
            Menu12Entry(context, i + menu12::kDisassembleWeaponDescIndex);
        const int count = Items::Inventory::CountItems(
            i != 0 ? ItemCategory::kShield : ItemCategory::kWeapon,
            context.host.Inventory());
        type = (count << kCategoryCountShift) + kCategoryRowFlags;
        if (count == 0) {
          text.description = Menu12Text(context, menu12::kNoItems);
          type |= 1;
        }
      }
      CreateCategoryRows(context, menu, kCategoryRowInfo, texts, types);
      break;
    }
    case kCategoryChoose:
      selection = context.host.SelectedRow() + 1;
      if (selection == 0) {
        break;
      }
      if (selection < 0) {
        context.host.ClearMenuExcept(kTitleRow);
        return selection;
      }
      context.host.ClearMenuExcept(kNoRow);
      UpdateDisassembly(context, selection);
      menu.step = kCategoryRun;
      break;
    case kCategoryRun:
      selection = UpdateDisassembly(context, 0);
      if (selection == 0) {
        break;
      }
      if (selection != -1) {
        return selection;
      }
      menu.step = kCategoryReturn;
      break;
    case kCategoryReturn:
      ReturnToCategory(context, menu, kDisassemblySubtitleRow,
                       menu12::kDisassemble);
      break;
    default:
      break;
  }
  return 0;
}

namespace {

// The rename prompt's answer: yes opens the rename screen, no returns to the
// list and cancel leaves.
int AnswerRename(Context& context) {
  EquipmentScreenState& screen = context.state.rename;
  const int answer = UpdateConfirmationPrompt(context, false);
  if (answer == 0) {
    return 0;
  }
  context.host.UpdateEquipmentStatPages(kHideStatPages);
  context.host.UpdateDpPpBars(kHideDpPp);
  if (answer == kChooseYes) {
    context.host.PlaySound(MenuSound::kSelect);
    context.host.ClearMenuExcept(kDetailRow);
    context.host.MenuItem(kDetailRow).background_width = 1;
    screen.step = kRenameStart;
  } else if (answer == kChooseNo) {
    context.host.ClearMenuExcept(kNoRow);
    context.host.FlyOutRowLeft(kTitleRow);
    screen.step = kScreenTitle;
  } else if (answer == kChooseCancel) {
    return LeaveItemSelection(context, false);
  }
  return 0;
}

}  // namespace

// decomp: MENUC.PRG _renameWeaponMenu (src/MENU/MENUC.PRG/168.c:2374)
int UpdateWeaponRename(Context& context, bool initialize) {
  EquipmentScreenState& screen = context.state.rename;
  if (initialize) {
    context.host.InitInformationTextBox(1);
    context.host.SetInformationMessage(
        Menu12Text(context, menu12::kChooseWeaponRename));
    context.host.ClearMenuExcept(kNoRow);
    screen.step = 0;
    return 0;
  }
  switch (screen.step) {
    case kScreenTitle:
      if (context.host.RowsIdle()) {
        SetTitleRow(context, kTitleRow, menu12::kRename);
        screen.step = kScreenList;
      }
      break;
    case kScreenList:
      if (context.host.RowsIdle()) {
        EquipmentRows rows;
        ListEquipment(context, rows, false, menu12::kChooseWeaponRename);
        CreateEquipmentRows(context, rows, kRenameRowInfo);
        screen.step = kScreenPick;
      }
      break;
    case kScreenPick:
      return PickEquipmentRow(context, screen);
    case kScreenSettle:
      if (context.host.RowsIdle()) {
        context.host.SetWeaponStats(
            SettleEquipmentRow(context, screen, kWeaponIcon));
        context.host.UpdateEquipmentStatPages(kAllStatPages);
        context.host.UpdateDpPpBars(kShowDpPp);
        screen.appear_step = 0;
        screen.step = kScreenParts;
      }
      break;
    case kScreenParts:
      if (screen.appear_step < kComponentRows) {
        ++screen.appear_step;
        Framework::CreateWeaponComponentRow(
            context.host.ItemRows(), screen.appear_step,
            context.state.combining_item + 1, true);
        return 0;
      }
      UpdateConfirmationPrompt(context, true);
      context.host.SetInformationMessage(
          Menu12Text(context, menu12::kRenamePrompt));
      screen.step = kRenameConfirm;
      break;
    case kRenameConfirm:
      return AnswerRename(context);
    case kRenameStart:
      if (context.host.RowsIdle()) {
        context.host.SetCursorColor(0);
        const std::span<std::uint8_t> buffer = context.host.StringBuffer();
        if (buffer.size() >= 2) {
          buffer[0] = 1;
          buffer[1] = context.state.combining_item;
        }
        context.host.SetMenuCommandLabel();
        return 1;
      }
      break;
    default:
      break;
  }
  return 0;
}

// decomp: MENUC.PRG _repairMenu (src/MENU/MENUC.PRG/168.c:2509)
int UpdateRepair(Context& context, bool initialize) {
  RepairState& repair = context.state.repair;
  if (initialize) {
    context.host.FlyOutRowsAndHoistSelection(kRepairHoist, 1);
    repair.step = 0;
    return 0;
  }
  switch (repair.step) {
    case kRepairAsk:
      if (context.host.RowsIdle()) {
        UpdateConfirmationPrompt(context, true);
        context.host.SetInformationMessage(
            Menu12Text(context, menu12::kRepairConfirm));
        repair.step = kRepairAnswer;
      }
      break;
    case kRepairAnswer: {
      const int answer = UpdateConfirmationPrompt(context, false);
      if (answer == 0) {
        break;
      }
      repair.returned_from_prompt = -1;
      if (answer == kChooseYes) {
        context.host.PlaySoundEffect(sfx::kRepair);
        repair.step = kRepairApply;
      } else if (answer == kChooseCancel) {
        context.host.PlaySound(MenuSound::kLeave);
        return -2;
      } else {
        context.host.InitializeInformationBox();
        context.host.FlyOutRowRight(kRepairMenuRow);
        repair.step = kRepairLeave;
      }
      break;
    }
    case kRepairApply:
      context.host.InitInformationTextBox(kRepairInfoHeader);
      context.host.SetInformationMessage(
          Menu12Text(context, menu12::kRepairComplete));
      RepairEverything(context);
      repair.step = kRepairResult;
      break;
    case kRepairResult: {
      const Framework::Buttons buttons = context.host.Input();
      if (Pressed(buttons, pad::kLowByte)) {
        context.host.PlaySound(MenuSound::kLeave);
        context.host.FlyOutRowRight(kRepairMenuRow);
        if (Pressed(buttons, pad::kTriangle)) {
          return -2;
        }
        context.host.InitializeInformationBox();
        repair.step = kRepairLeave;
      }
      break;
    }
    case kRepairLeave:
      if (context.host.RowsIdle()) {
        return repair.returned_from_prompt;
      }
      break;
    default:
      break;
  }
  return 0;
}

// decomp: MENUC.PRG vs_menuC_exec (src/MENU/MENUC.PRG/168.c:4550)
int Update(Context& context, std::uint8_t& step) {
  switch (step) {
    case kTopEnter:
      context.state.first_entry = true;
      [[fallthrough]];
    case kTopTitle:
      EnterTop(context, step);
      break;
    case kTopRowsStep:
      if (context.host.RowsIdle()) {
        CreateTopRows(context);
        step = kTopChoose;
      }
      break;
    case kTopChoose:
      ChooseTopEntry(context, step);
      break;
    case kTopAssembly:
      FinishAssembly(context, UpdateAssembly(context, false), step);
      break;
    case kTopGems:
      FinishSubmenu(UpdateGemCategoryMenu(context, false), step);
      break;
    case kTopDisassembly:
      FinishSubmenu(UpdateDisassemblyCategory(context, false), step);
      break;
    case kTopRename:
      FinishRename(context, UpdateWeaponRename(context, false), step);
      break;
    case kTopRepair:
      FinishSubmenu(UpdateRepair(context, false), step);
      break;
    case kTopCombination:
      FinishSubmenu(UpdateCombinationCategory(context, false), step);
      break;
    case kTopReturn:
      context.host.InitializeInformationBox();
      if (context.host.RowsIdle()) {
        step = kTopTitle;
      }
      break;
    case kTopLeave:
      context.host.ReturnFromSetup(1);
      step = kTopEnter;
      break;
    default:
      break;
  }
  return 0;
}

// decomp: MENUC.PRG _loadObject (src/MENU/MENUC.PRG/9258.c:7)
int ReloadAshleyEquipment(Context& context, int mode) {
  if (mode == kShieldReload) {
    context.host.ReleaseWeaponModel(1);
    context.host.ResetEquipmentPose();
    return 0;
  }
  switch (context.host.EquipmentModelLoadStep()) {
    case 0: {
      const std::optional<int> model = context.host.AshleySequenceModel();
      if (!model) {
        context.host.Refuse("no Ashley actor to reload the weapon onto");
        return 0;
      }
      if (*model == kMenuSequenceModel) {
        return 0;
      }
      context.host.ReleaseWeaponModel(0);
      context.host.QueueWeaponAnimations(kMenuSequenceModel);
      context.host.SetEquipmentModelLoadStep(1);
      break;
    }
    case 1:
      if (!context.host.ActorAssetsPending()) {
        context.host.ResetEquipmentPose();
        context.host.SetEquipmentModelLoadStep(0);
        return 0;
      }
      break;
    default:
      break;
  }
  return 1;
}

void ResetModuleStatics(State& state) {
  state.confirmation.cursor = 0;
  state.three_choice.cursor = 0;
  state.item_list.cursor = 0;
  state.assembly.cursor = 0;
  state.assembly.ok_blink = 0;
  state.assembly.gems = {};
  state.gem_attachment.cursor = 0;
  state.gem_attachment.ok_blink = 0;
  state.gem_attachment.staged = {};
  state.gem_attachment.target = {};
  state.combination.blade_menu.cursor = 0;
  state.combination.shield_menu.cursor = 0;
  state.combination.armor_menu.cursor = 0;
}
}  // namespace VS::Menu::Setup
