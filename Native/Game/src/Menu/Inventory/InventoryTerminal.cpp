#include "Menu/Inventory/InventoryTerminal.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>

#include "Core/Math/ScalarMath.hpp"
#include "Render/EncodedText.hpp"

namespace VS::Menu::Inventory {
bool BindLootInventoryText(LootInventoryText& asset,
                           std::span<const std::uint8_t> bytes) {
  if (bytes.size() != kLootInventoryTextBytes) return false;
  constexpr std::size_t kOffsetBytes = 2;
  for (std::size_t entry = 0; entry < kLootInventoryTextEntries; ++entry) {
    const auto position = entry * kOffsetBytes;
    const auto offset = static_cast<std::size_t>(
        bytes[position] | (static_cast<unsigned>(bytes[position + 1]) << 8));
    const auto first = offset * kOffsetBytes;
    if (first < kLootInventoryTextEntries * kOffsetBytes ||
        first >= bytes.size() ||
        std::ranges::find(bytes.subspan(first), Render::kTextTerminator) ==
            bytes.subspan(first).end()) {
      return false;
    }
  }
  std::ranges::copy(bytes, asset.bytes.begin());
  asset.loaded = true;
  return true;
}
namespace {
using Category = TerminalCategory;
using Core::Math::WrappingAdd;
constexpr std::size_t kActionRows = 24;
constexpr std::int32_t kTitleRow = 3;
constexpr std::int32_t kTitleX = 180;
constexpr std::int32_t kTitleSlide = 24;
constexpr std::uint16_t kFirstKey = 458;
constexpr std::uint8_t kStackLimit = 100;
constexpr std::int32_t kSortMenuBase = 45;
constexpr std::int32_t kDiscardMenuId = 4;
constexpr std::int32_t kListMenuBase = 30;
constexpr std::int32_t kActionMenuBase = 38;
constexpr std::int32_t kListOptions = 0x19200;
constexpr std::int32_t kManageOptions = 0x200;
constexpr std::int32_t kRestrictionMask = 7;
constexpr std::uint8_t kAnimationSteps = 10;
constexpr std::uint8_t kIconSteps = 4;
constexpr std::int32_t kQuantityRow = 34;
constexpr std::int32_t kQuantityY = 130;
constexpr std::int32_t kQuantityWidth = 126;
constexpr std::int32_t kQuantityLeft = 54;
constexpr std::int32_t kQuantityRight = 66;
constexpr std::int32_t kDigitAdvance = 6;
constexpr std::int32_t kCursorY = 114;
constexpr std::uint32_t kDigitMask = 15;
constexpr unsigned kDigitShift = 4;
constexpr std::uint32_t kCategoryMask = 7;
constexpr unsigned kCursorPageShift = 8;
constexpr std::int32_t kSelectedRowFlag = 8;
constexpr std::int32_t kDiscardKeyRestriction = 3;
constexpr std::int32_t kUseItemResult = 3;
constexpr std::uint8_t kItemUseKind = 8;
constexpr std::uint8_t kItemExecuteType = 13;
constexpr std::uint8_t kBattleMenu = 4;
constexpr std::uint8_t kContainerMenu = 13;
constexpr std::uint8_t kShortcutReturnMenu = 63;
constexpr std::int32_t kContainerAshleyLayout = 3;
constexpr std::int32_t kTitleY = 18;
constexpr std::int32_t kSecondaryTitleY = 34;
constexpr std::int32_t kTitleWidth = 140;
constexpr std::int32_t kTitleBackground = 8;
constexpr std::int32_t kOffscreenX = 320;
constexpr std::int32_t kSecondaryTitleRow = 31;
constexpr std::int32_t kShortcutMiscRow = 10;
constexpr std::int32_t kPixelsPerAnimationStep = 32;
constexpr std::size_t kQuantityDigits = 3;
constexpr std::size_t kMaximumDigitOffset = 6;
constexpr std::int32_t kComparisonActor = 16;
constexpr std::int32_t kHudMenuOffset = 0x200000;
constexpr std::int32_t kClosedBackgroundOrder = -4;
constexpr std::int32_t kNormalMenuBrightness = 128;
enum DiscardStep : std::uint8_t {
  kDiscardWaitPrompt,
  kDiscardSingleConfirm,
  kDiscardCreateQuantity,
  kDiscardSingleInput,
  kDiscardWaitQuantity,
  kDiscardQuantityInput,
  kDiscardQuantityConfirm,
  kDiscardQuantityDecision,
};
enum NavigationStep : std::uint8_t {
  kListRestore,
  kListPopulate,
  kListBrowse,
  kListTransition,
  kListTitle,
  kListWait,
  kListRearrange,
  kListDetails,
  kListBuildActions,
  kListActions,
  kListParent,
  kListDiscard,
  kListSort,
};
enum ShortcutStep : std::uint8_t {
  kShortcutInitialize,
  kShortcutWaitTitle,
  kShortcutPopulate,
  kShortcutInput,
  kShortcutClose,
};
enum ScreenStep : std::uint8_t {
  kScreenIdle = 0,
  kScreenInitialize = 3,
  kScreenWaitText,
  kScreenInventory,
  kScreenEquipment,
  kScreenWaitContainer,
  kScreenContainerReturn,
  kScreenCloseMenu,
  kScreenCloseBattle,
  kScreenCountdown,
  kScreenItemAction,
  kScreenResume,
};
constexpr auto kWeaponStats = std::to_array<std::uint8_t>({
    0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11,
    12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23,
});
constexpr std::array<std::uint8_t, 7> kBladeStats{0, 8, 9, 10, 24, 25, 26};
constexpr auto kGripStats = std::to_array<std::uint8_t>({
    1,  4,  5,  6,  7,  8,  9,  10, 40, 41, 42, 27,
    28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39,
});
constexpr auto kShieldStats = std::to_array<std::uint8_t>({
    0,  1,  4,  5,  8,  9,  10, 40, 41, 42, 27, 28,
    29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39,
});
constexpr auto kEquipmentStats = std::to_array<std::uint8_t>(
    {8, 9, 10, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39});
constexpr std::array<std::uint8_t, 2> kMiscStats{0, 56};

std::span<const std::uint8_t> SortStats(InventoryTerminalHost& host,
                                        Category category) {
  switch (category) {
    case Category::kWeapon:
    case Category::kBlade:
      return kWeaponStats;
    case Category::kGrip:
      return kBladeStats;
    case Category::kShield:
      return kGripStats;
    case Category::kArmor:
      return kShieldStats;
    case Category::kGem:
      return kEquipmentStats;
    case Category::kMisc:
      return kMiscStats;
    default:
      host.Refuse("inventory sort category outside its criteria");
  }
  return {};
}

std::size_t CategoryIndex(Category category) {
  return static_cast<std::size_t>(category);
}
Category CurrentCategory(const TerminalRuntime& runtime) {
  return static_cast<Category>((runtime.cursor.category - 1U) & kCategoryMask);
}
void SelectCategory(TerminalRuntime& runtime, Category category) {
  runtime.cursor.category =
      static_cast<std::uint8_t>((CategoryIndex(category) + 1U) & kCategoryMask);
}
void SetPanels(TerminalRuntime& runtime, std::uint8_t visible) {
  runtime.presentation.secondary_title = visible;
  runtime.presentation.category_icons = visible;
  runtime.presentation.status_command = visible;
}
void ResetAnimations(TerminalRuntime& runtime) {
  runtime.presentation.secondary_step = 0;
  runtime.presentation.icons_step = 0;
  runtime.presentation.status_step = 0;
  runtime.presentation.icon_on_top = 0;
}
void SlideTitle(InventoryTerminalHost& host) {
  auto& title = host.Row(kTitleRow);
  if (title.x <= kTitleX) return;
  if (title.x > kTitleX + kTitleSlide) {
    title.x = static_cast<std::int16_t>(title.x - kTitleSlide);
  }
  title.state = Framework::RowState::kSlideX;
  title.target0 = kTitleX;
}
void RequireSlot(InventoryTerminalHost& host, Category category,
                 std::uint8_t one_based) {
  const auto index = CategoryIndex(category);
  if (index >= CategoryIndex(Category::kManage) || one_based == 0 ||
      one_based > Items::kCategoryCapacities[index]) {
    host.Refuse("inventory selection outside its category table");
  }
}
std::uint8_t ListedSlot(InventoryTerminalHost& host, Category category,
                        std::int32_t row) {
  const auto order = host.Order(category);
  if (row <= 0 || static_cast<std::size_t>(row) > order.size()) {
    host.Refuse("inventory row outside its display order");
  }
  const auto slot = order[static_cast<std::size_t>(row - 1)];
  RequireSlot(host, category, slot);
  return slot;
}
class RowLease {
 public:
  explicit RowLease(InventoryTerminalHost& host) : host_(host) {
    host_.BeginRows();
  }
  ~RowLease() { host_.EndRows(); }
  RowLease(const RowLease&) = delete;
  RowLease& operator=(const RowLease&) = delete;
  RowLease(RowLease&&) = delete;
  RowLease& operator=(RowLease&&) = delete;

 private:
  InventoryTerminalHost& host_;
};
std::size_t RowCount(InventoryTerminalHost& host, std::int32_t count) {
  if (count < 0 || static_cast<std::size_t>(count) > kTerminalRows) {
    host.Refuse("inventory formatter exceeded its native row workspace");
  }
  return static_cast<std::size_t>(count);
}
void CreateList(InventoryTerminalHost& host, const TerminalRows& rows,
                std::size_t count, Category category) {
  const auto options =
      category == Category::kManage ? kManageOptions : kListOptions;
  host.CreateList(static_cast<std::int32_t>(category) + kListMenuBase | options,
                  std::span(rows.text).first(count),
                  std::span(rows.flags).first(count));
}
void StoreQuantityDigits(InventoryDiscardPresentation& presentation,
                         std::size_t first, std::uint8_t quantity) {
  auto remaining = Core::Math::SplitDecimalDigit(quantity);
  presentation.text[first + 2] =
      static_cast<std::uint8_t>(remaining & kDigitMask);
  remaining = Core::Math::SplitDecimalDigit(remaining >> kDigitShift);
  presentation.text[first + 1] =
      static_cast<std::uint8_t>(remaining & kDigitMask);
  presentation.text[first] =
      static_cast<std::uint8_t>(remaining >> kDigitShift);
}
Framework::MenuRow& QuantityRow(InventoryTerminalHost& host,
                                InventoryDiscardPresentation& presentation,
                                std::int32_t x) {
  return host.SetRow(
      {.row = kQuantityRow, .x = x, .y = kQuantityY, .w = kQuantityWidth},
      presentation.text);
}
void ConfirmDiscard(InventoryDiscardState& state, InventoryTerminalHost& host,
                    bool quantity) {
  std::array<Framework::RowText, 2> rows{};
  const std::array<std::int32_t, 2> flags{};
  for (std::size_t row = 0; row < rows.size(); ++row) {
    rows[row] = {host.Text(TerminalText::kYesNo, static_cast<int>(row)),
                 host.Text(quantity ? TerminalText::kQuantityPrompt
                                    : TerminalText::kDiscardPrompt,
                           state.item.kind)};
  }
  host.CreateActions(kDiscardMenuId, rows, flags);
  state.step = quantity ? kDiscardQuantityDecision : kDiscardSingleInput;
}
void AdjustQuantity(InventoryDiscardState& state, InventoryTerminalHost& host) {
  const auto buttons = host.Buttons().repeat;
  const int vertical = ((buttons & Input::Up) != 0) -
                       static_cast<int>((buttons & Input::Down) != 0);
  constexpr std::array<int, kQuantityDigits> kSteps{1, 10, 100};
  if (state.place >= kSteps.size()) {
    host.Refuse("inventory discard digit outside its step table");
  }
  const auto previous_quantity = state.quantity;
  const auto previous_place = state.place;
  const auto proposed = state.quantity + vertical * kSteps[state.place];
  if (proposed >= 0 && proposed <= state.maximum) {
    state.quantity = static_cast<std::uint8_t>(proposed);
  }
  const int horizontal = ((buttons & Input::Left) != 0) -
                         static_cast<int>((buttons & Input::Right) != 0);
  if (horizontal > 0) {
    if (state.place == 2) {
      state.quantity = state.maximum;
    } else {
      ++state.place;
    }
  } else if (horizontal < 0) {
    if (state.place == 0) {
      state.quantity = 0;
    } else {
      --state.place;
    }
  }
  if (state.quantity != previous_quantity || state.place != previous_place) {
    host.PlaySound(TerminalSound::kChange);
  }
}
std::int32_t QuantityInput(InventoryDiscardState& state,
                           InventoryDiscardPresentation& presentation,
                           InventoryTerminalHost& host) {
  const auto pressed = host.Buttons().pressed;
  if ((pressed & (Input::Triangle | Input::Circle | Input::Cross)) != 0) {
    StoreQuantityDigits(presentation, 2, state.quantity);
    static_cast<void>(QuantityRow(host, presentation, 0));
    if ((pressed & (Input::Triangle | Input::Cross)) != 0 ||
        state.quantity == 0) {
      host.PlaySound(TerminalSound::kLeave);
      return (pressed & Input::Triangle) != 0 ? -2 : -1;
    }
    host.PlaySound(TerminalSound::kSelect);
    state.step = kDiscardQuantityConfirm;
    return 0;
  }
  AdjustQuantity(state, host);
  host.SetQuantityFont();
  auto digits = Core::Math::SplitDecimalDigit(state.quantity);
  host.DrawQuantityDigit(static_cast<std::uint32_t>(digits) & kDigitMask,
                         kQuantityRight);
  digits = Core::Math::SplitDecimalDigit(digits >> kDigitShift);
  host.DrawQuantityDigit(static_cast<std::uint32_t>(digits) & kDigitMask,
                         kQuantityRight - kDigitAdvance);
  host.DrawQuantityDigit(static_cast<std::uint32_t>(digits >> kDigitShift),
                         kQuantityLeft);
  presentation.cursor_animation =
      static_cast<std::uint8_t>(host.DrawQuantityCursor(
          presentation.cursor_animation,
          kQuantityLeft - state.place * kDigitAdvance, kCursorY));
  return 0;
}
std::int32_t DiscardInput(InventoryDiscardState& state,
                          InventoryTerminalHost& host, bool quantity) {
  host.UpdateActions();
  const auto result = WrappingAdd(host.ActionResult(), 1);
  if (result == 0) return 0;
  if (result != 1) {
    host.PlaySound(TerminalSound::kLeave);
    return result;
  }
  RequireSlot(host, state.item.category, state.item.one_based_slot);
  host.PlaySound(TerminalSound::kDiscard);
  if (quantity) {
    const auto slot = state.item.one_based_slot - 1U;
    if (state.item.category != Category::kMisc || slot >= Items::kMiscSlots) {
      host.Refuse("inventory quantity discard is not a miscellaneous stack");
    }
    const auto count = host.Inventory().misc[slot].count;
    if (count != state.quantity) {
      host.SetMiscCount(slot,
                        static_cast<std::uint8_t>(count - state.quantity));
      return result;
    }
  }
  host.ClearItem(state.item.category, state.item.one_based_slot);
  return result;
}
void InitializeList(InventoryNavigationState& state,
                    InventoryTerminalHost& host, ItemRowsContext context,
                    Category category) {
  const RowLease lease(host);
  TerminalRows rows{};
  host.Runtime().freeze_tabs = 1;
  for (std::size_t table = 0; table < CategoryIndex(Category::kManage);
       ++table) {
    host.RebuildInventory(static_cast<Category>(table));
  }
  std::size_t count = 2;
  if (category == Category::kManage) {
    const bool disabled =
        context == ItemRowsContext::kLoot || host.IntroState() < 3;
    for (std::size_t row = 0; row < count; ++row) {
      rows.text[row] = {
          host.Text(TerminalText::kManage, static_cast<int>(row * 2)),
          host.Text(TerminalText::kManage, static_cast<int>(row * 2 + 1))};
      rows.flags[row] = disabled ? 1 : 0;
    }
    if (context == ItemRowsContext::kItemMenu && host.EquipmentTarnished()) {
      for (std::size_t row = 0; row < count; ++row) {
        rows.text[row].description = host.Text(TerminalText::kTarnished);
        rows.flags[row] = 1;
      }
    }
  } else {
    count = RowCount(host, host.PopulateRows(context, category, rows));
  }
  if (context == ItemRowsContext::kItemMenu &&
      (host.Runtime().restrictions & kRestrictionMask) != 0) {
    for (auto& flags : rows.flags) flags |= 1;
  }
  state.no_items = count == 0 ? 1 : 0;
  if (state.no_items != 0) {
    host.SetInformation(host.Text(TerminalText::kNoItems));
    return;
  }
  const auto saved = host.Runtime().cursor_memory_enabled;
  if (state.ignore_cursor_memory != 0) {
    state.ignore_cursor_memory = 0;
  } else {
    host.Runtime().cursor_memory_enabled = 1;
  }
  CreateList(host, rows, count, category);
  if (state.step == 0) host.RestoreRows(category);
  host.Runtime().cursor_memory_enabled = saved;
}
void SelectListRow(InventoryNavigationState& state, InventoryTerminalHost& host,
                   ItemRowsContext context, Category category) {
  const RowLease lease(host);
  TerminalRows rows{};
  const auto count = RowCount(host, host.PopulateRows(context, category, rows));
  if (state.selected_row <= 0 ||
      static_cast<std::size_t>(state.selected_row) > count) {
    host.Refuse("selected inventory row outside its populated list");
  }
  rows.flags[static_cast<std::size_t>(state.selected_row - 1)] |=
      kSelectedRowFlag;
  const auto saved = host.Runtime().cursor_memory_enabled;
  host.Runtime().cursor_memory_enabled = 1;
  CreateList(host, rows, count, category);
  host.Runtime().cursor_memory_enabled = saved;
  host.RestoreRows(category);
  host.Runtime().presentation.status_command = 1;
  host.Runtime().freeze_tabs = 0;
  state.step = kListRearrange;
}
bool ChangeCategory(InventoryNavigationState& state,
                    InventoryTerminalHost& host, Category category) {
  constexpr auto kPrevious = Input::L1 | Input::Left;
  constexpr auto kNext = Input::R1 | Input::Right;
  const auto input = host.Buttons();
  const auto held_previous = input.held & kPrevious;
  const auto held_next = input.held & kNext;
  const bool moving = (held_previous != 0) != (held_next != 0);
  auto changed = static_cast<std::uint32_t>(category);
  if (moving) {
    const auto previous_repeat =
        held_previous == kPrevious ? Input::Left : kPrevious;
    const auto next_repeat = held_next == kNext ? Input::Right : kNext;
    if ((input.repeat & previous_repeat) != 0) {
      changed = (changed - 1U) & kCategoryMask;
    }
    if ((input.repeat & next_repeat) != 0) {
      changed = (changed + 1U) & kCategoryMask;
    }
    if (changed != CategoryIndex(CurrentCategory(host.Runtime()))) {
      host.PlaySound(TerminalSound::kChange);
      SelectCategory(host.Runtime(), static_cast<Category>(changed));
      if (state.step == 2) {
        if (state.no_items == 0) {
          static_cast<void>(host.StoreCursor());
          host.ClearExcept(kTitleRow);
        }
        state.step = kListTransition;
      }
    }
  }
  if (state.step != kListTransition || !host.RowsReady()) return false;
  if ((input.held & (kPrevious | kNext)) != 0 && moving) return false;
  state.step = 1;
  return true;
}
std::optional<std::int32_t> BrowseRows(InventoryNavigationState& state,
                                       InventoryTerminalHost& host,
                                       ItemRowsContext context,
                                       Category category) {
  host.Runtime().presentation.status_command = 1;
  const auto pressed = host.Buttons().pressed;
  if (state.no_items != 0) {
    if ((pressed & (Input::Triangle | Input::Cross)) != 0) {
      host.PlaySound(TerminalSound::kLeave);
      return (pressed & Input::Triangle) != 0 ? -2 : -1;
    }
    if ((pressed & (Input::Circle | Input::Square)) != 0) {
      host.PlaySound(TerminalSound::kInvalid);
    }
    return std::nullopt;
  }
  const bool can_view = category < Category::kMisc;
  if (context == ItemRowsContext::kLoot ||
      (host.Runtime().restrictions & kRestrictionMask) == 0) {
    host.Runtime().presentation.status_command = can_view ? 2 : 1;
  }
  state.selected_row = WrappingAdd(host.SelectedRow(), 1);
  if (state.selected_row != 0) {
    if (state.selected_row < 0 || category == Category::kManage) {
      return state.selected_row;
    }
    SelectListRow(state, host, context, category);
    return 0;
  }
  if ((pressed & Input::Square) != 0) {
    if ((context == ItemRowsContext::kLoot
             ? can_view
             : host.Runtime().presentation.status_command >= 2) &&
        host.RowsReady()) {
      host.PlaySound(TerminalSound::kSelect);
      const auto cursor = host.StoreCursor();
      host.Row(cursor >> kCursorPageShift).outset_icon = 0;
      static_cast<void>(host.Details(ItemDetailRequest{category, cursor}));
      host.Runtime().presentation.secondary_title = 0;
      state.step = kListDetails;
      return 0;
    }
    host.PlaySound(TerminalSound::kInvalid);
  }
  return std::nullopt;
}
std::int32_t Rearrange(InventoryNavigationState& state,
                       InventoryTerminalHost& host, ItemRowsContext context,
                       Category category) {
  if ((host.Buttons().pressed & Input::Square) != 0) {
    host.PlaySound(TerminalSound::kInvalid);
  }
  const auto row = WrappingAdd(host.SelectedRow(), 1);
  if (row == 0) return 0;
  if (row == state.selected_row) {
    state.step = kListBuildActions;
    return 0;
  }
  if (row < 0) {
    if (row != -1) return -2;
    state.step = 0;
    return 0;
  }
  const auto target = ListedSlot(host, category, row);
  const auto selected = ListedSlot(host, category, state.selected_row);
  const auto inventory = host.Inventory();
  if (category == Category::kMisc &&
      inventory.misc[target - 1U].id == inventory.misc[selected - 1U].id) {
    const auto combined =
        static_cast<std::uint8_t>(inventory.misc[selected - 1U].count +
                                  inventory.misc[target - 1U].count);
    host.SetMiscCount(selected - 1U, combined);
    if (combined > kStackLimit) {
      host.SetMiscCount(target - 1U, combined - kStackLimit);
      host.SetMiscCount(selected - 1U, kStackLimit);
    } else {
      // MENUB really clears the selected stack, not the other stack (#405).
      host.ClearItem(Category::kMisc,
                     context == ItemRowsContext::kItemMenu ? target : selected);
    }
  } else {
    auto order = host.Order(category);
    order[static_cast<std::size_t>(row - 1)] = selected;
    order[static_cast<std::size_t>(state.selected_row - 1)] = target;
  }
  host.ClearExcept(kTitleRow);
  state.step = kListTitle;
  return 0;
}
std::int32_t DiscardRestriction(InventoryTerminalHost& host, Category category,
                                std::int32_t slot) {
  const auto inventory = host.Inventory();
  std::int32_t restriction = 0;
  const auto index = static_cast<std::size_t>(slot);
  switch (category) {
    case Category::kWeapon:
      restriction = inventory.weapons[index].is_equipped != 0;
      break;
    case Category::kShield:
      restriction = inventory.shields[index].is_equipped != 0;
      break;
    case Category::kArmor:
      restriction = inventory.armor[index].limb != 0;
      break;
    case Category::kMisc:
      restriction =
          inventory.misc[index].id >= kFirstKey ? kDiscardKeyRestriction : 0;
      break;
    default:
      break;
  }
  const auto weapon = host.ParentWeapon(category, slot);
  if (weapon != 0) {
    if (weapon < 0 || weapon > static_cast<int>(Items::kWeaponSlots)) {
      host.Refuse("inventory discard parent weapon outside its table");
    }
    restriction = host.Inventory()
                              .weapons[static_cast<std::size_t>(weapon - 1)]
                              .is_equipped != 0
                      ? 1
                      : 2;
  }
  const auto shield = host.ParentShield(category, slot);
  if (shield != 0) {
    if (shield < 0 || shield > static_cast<int>(Items::kShieldSlots)) {
      host.Refuse("inventory discard parent shield outside its table");
    }
    restriction = host.Inventory()
                              .shields[static_cast<std::size_t>(shield - 1)]
                              .is_equipped != 0
                      ? 1
                      : 2;
  }
  return restriction;
}
void PopulateActions(InventoryNavigationState& state,
                     InventoryTerminalHost& host, ItemRowsContext context,
                     Category category) {
  const auto slot = ListedSlot(host, category, state.selected_row) - 1;
  std::array<Framework::RowText, 6> rows{};
  std::array<std::int32_t, 6> flags{};
  std::size_t count = 0;
  const auto add = [&](InventoryAction action, TerminalText name,
                       TerminalText description, bool disabled = false,
                       int parameter = 0) {
    rows[count] = {host.Text(name), host.Text(description, parameter)};
    flags[count] = disabled ? 1 : 0;
    state.actions[++count] = action;
  };
  if (category == Category::kMisc) {
    auto description = TerminalText::kUseDescription;
    bool disabled = false;
    if (context == ItemRowsContext::kLoot) {
      description = TerminalText::kUseProhibited;
      disabled = true;
    } else if (host.Inventory().misc[static_cast<std::size_t>(slot)].id >=
               kFirstKey) {
      description = TerminalText::kCannotUse;
      disabled = true;
    } else if ((host.ActionFlags(host.MiscAction(
                    static_cast<std::size_t>(state.selected_row - 1))) &
                2) != 0) {
      description = TerminalText::kInsufficientMp;
      disabled = true;
    }
    add(InventoryAction::kUse, TerminalText::kUse, description, disabled);
  }
  if (category == Category::kBlade || category == Category::kGrip ||
      category == Category::kGem) {
    const bool disabled = host.ParentWeapon(category, slot) == 0 &&
                          host.ParentShield(category, slot) == 0;
    add(InventoryAction::kSearch, TerminalText::kSearch,
        disabled ? TerminalText::kInvalidSearch
                 : TerminalText::kSearchDescription,
        disabled);
  }
  const auto restriction = DiscardRestriction(host, category, slot);
  add(InventoryAction::kDiscard, TerminalText::kDiscard,
      restriction != 0 ? TerminalText::kDiscardRestriction
                       : TerminalText::kDiscardDescription,
      restriction != 0, restriction);
  if (category == Category::kMisc) {
    const bool disabled =
        host.Inventory().misc[static_cast<std::size_t>(slot)].id >= kFirstKey;
    add(InventoryAction::kDiscardAll, TerminalText::kDiscardAll,
        disabled ? TerminalText::kCannotDiscard
                 : TerminalText::kDiscardAllDescription,
        disabled);
  }
  add(InventoryAction::kSort, TerminalText::kSort, TerminalText::kSortHint);
  host.CreateActions(static_cast<int>(category) + kActionMenuBase,
                     std::span(rows).first(count),
                     std::span(flags).first(count));
  state.step = kListActions;
}
void QueueItemUse(InventoryTerminalHost& host, std::uint8_t slot,
                  std::uint8_t action) {
  RequireSlot(host, Category::kMisc, slot);
  host.Runtime().use.slot = static_cast<std::uint8_t>(slot - 1U);
  host.Runtime().use.action = action;
  host.Runtime().use.item = host.Inventory().misc[slot - 1U].id;
}
std::int32_t RunAction(InventoryNavigationState& state,
                       InventoryTerminalHost& host, ItemRowsContext context,
                       Category category, InventoryAction action) {
  switch (action) {
    case InventoryAction::kUse:
      if (context == ItemRowsContext::kItemMenu) {
        host.ClearExcept(Framework::kNoRow);
        QueueItemUse(
            host, ListedSlot(host, category, state.selected_row),
            host.MiscAction(static_cast<std::size_t>(state.selected_row - 1)));
        return kUseItemResult;
      }
      break;
    case InventoryAction::kSearch:
      host.InitializeInformation();
      host.ClearExcept(kTitleRow);
      state.step = kListParent;
      break;
    case InventoryAction::kDiscard:
    case InventoryAction::kDiscardAll: {
      const auto slot = ListedSlot(host, category, state.selected_row);
      std::uint8_t kind = action == InventoryAction::kDiscardAll ? 2 : 0;
      if (kind == 0 && category == Category::kMisc &&
          host.Inventory().misc[slot - 1U].count >= 2) {
        kind = 1;
      }
      static_cast<void>(
          host.Discard(InventoryDiscardEntry{category, slot, kind}));
      state.step = kListDiscard;
      break;
    }
    case InventoryAction::kSort:
      host.InitializeInformation();
      static_cast<void>(host.SortMenu(category));
      state.step = kListSort;
      break;
    case InventoryAction::kNone:
      break;
  }
  return 0;
}
std::int32_t ProcessActions(InventoryNavigationState& state,
                            InventoryTerminalHost& host,
                            ItemRowsContext context, Category category) {
  host.UpdateActionInformation();
  host.UpdateActions();
  const auto selected = WrappingAdd(host.ActionResult(), 1);
  if (selected == 0) return 0;
  if (context == ItemRowsContext::kItemMenu) {
    host.FlyOutExcept(Framework::kNoRow);
  }
  if (selected > 0) {
    if (static_cast<std::size_t>(selected) >= state.actions.size()) {
      host.Refuse("inventory action outside its dispatch table");
    }
    host.PlaySound(TerminalSound::kSelect);
    if (context == ItemRowsContext::kLoot) {
      host.FlyOutExcept(Framework::kNoRow);
    }
    return RunAction(state, host, context, category,
                     state.actions[static_cast<std::size_t>(selected)]);
  }
  host.PlaySound(TerminalSound::kLeave);
  if (context == ItemRowsContext::kLoot) host.FlyOutExcept(Framework::kNoRow);
  if (selected == -2) {
    host.ClearExcept(kTitleRow);
    return -2;
  }
  host.InitializeInformation();
  state.step = 0;
  return 0;
}
void SearchParent(InventoryNavigationState& state, InventoryTerminalHost& host,
                  Category category) {
  if (!host.RowsReady()) return;
  const auto slot = ListedSlot(host, category, state.selected_row) - 1;
  auto parent = host.ParentWeapon(category, slot);
  auto parent_category = Category::kWeapon;
  if (parent == 0) {
    parent = host.ParentShield(category, slot);
    parent_category = Category::kShield;
  }
  SelectCategory(host.Runtime(), parent_category);
  const auto row = WrappingAdd(host.FindItem(parent_category, parent - 1), -1);
  host.Runtime().cursor.positions[CategoryIndex(parent_category)].row =
      static_cast<std::uint8_t>(row);
  host.Runtime().cursor.positions[CategoryIndex(parent_category)].page =
      static_cast<std::uint8_t>(static_cast<std::uint16_t>(row) >>
                                kCursorPageShift);
  state.step = 1;
}
std::uint8_t ShortcutAction(InventoryTerminalHost& host, std::uint16_t item) {
  const auto map = host.MiscActionMap();
  const auto index = static_cast<std::uint32_t>(item - kFirstMiscActionItem);
  if (index >= map.size()) host.Refuse("shortcut item outside its action map");
  std::uint32_t action = map[index];
  if (action - kFirstUpgradableAction < kUpgradableActionCount) {
    for (int step = 0; step < kActionUpgradeSteps; ++step) {
      if (host.ActionUnlocked(action)) ++action;
    }
  }
  return static_cast<std::uint8_t>(action);
}
void PopulateShortcut(MiscShortcutState& state, InventoryTerminalHost& host) {
  const RowLease lease(host);
  TerminalRows rows{};
  host.RebuildInventory(Category::kMisc);
  const auto order = host.Order(Category::kMisc);
  std::size_t count = 0;
  while (count < kTerminalRows) {
    if (count >= order.size()) host.Refuse("shortcut order has no terminator");
    if (order[count] == 0) break;
    RequireSlot(host, Category::kMisc, order[count]);
    const auto slot = order[count] - 1U;
    host.FormatMisc(slot, count, rows);
    const auto item = host.Inventory().misc[slot].id;
    if (item < kFirstKey) state.actions[count] = ShortcutAction(host, item);
    if ((host.Runtime().restrictions & kRestrictionMask) != 0 ||
        item >= kFirstKey ||
        (host.ActionFlags(state.actions[count]) & 2) != 0) {
      rows.flags[count] |= 1;
    }
    ++count;
  }
  state.no_items = count == 0 ? 1 : 0;
  if (count == 0) {
    host.SetInformation(host.Text(TerminalText::kNoItems));
    return;
  }
  const auto saved = host.Runtime().cursor_memory_enabled;
  if (state.no_force_cursor_memory != 0) {
    state.no_force_cursor_memory = 0;
  } else {
    host.Runtime().cursor_memory_enabled = 1;
  }
  CreateList(host, rows, count, Category::kMisc);
  host.Runtime().cursor_memory_enabled = saved;
}
void ShortcutInput(MiscShortcutState& state, InventoryTerminalHost& host) {
  if (state.no_items != 0) {
    const auto pressed = host.Buttons().pressed;
    if ((pressed & (Input::Triangle | Input::Circle | Input::Cross)) == 0) {
      return;
    }
    if ((pressed & Input::Circle) != 0) {
      host.PlaySound(TerminalSound::kInvalid);
      return;
    }
    host.PlaySound(TerminalSound::kLeave);
    host.ClearExcept(Framework::kNoRow);
    host.DismissInformation();
    state.selected_row = -1;
  } else {
    state.selected_row = WrappingAdd(host.SelectedRow(), 1);
    if (state.selected_row == 0) return;
    if (state.selected_row > static_cast<std::int32_t>(state.actions.size())) {
      host.Refuse("shortcut selection outside its action rows");
    }
    host.ClearExcept(Framework::kNoRow);
    host.DismissInformation();
    if (state.selected_row > 0) {
      QueueItemUse(
          host, ListedSlot(host, Category::kMisc, state.selected_row),
          state.actions[static_cast<std::size_t>(state.selected_row - 1)]);
    }
  }
  state.step = kShortcutClose;
}
void LeaveInventory(InventoryTerminalHost& host, bool toggle_first) {
  if (toggle_first) {
    SetPanels(host.Runtime(), 0);
  } else {
    host.SetFieldRenderingEnabled(true);
  }
  if (!toggle_first) SetPanels(host.Runtime(), 0);
  host.ClearExcept(Framework::kNoRow);
  if (toggle_first) host.SetFieldRenderingEnabled(true);
  host.SetHudHidden(false);
  host.Runtime().hud_slide_offset = 0;
  host.RenderAshley(2);
}
void ClosingPresentation(InventoryTerminalHost& host) {
  host.CloseBackground();
  host.DismissInformation();
  host.ClearCommand();
}
bool CloseScreen(InventoryScreenState& state, std::uint8_t& step,
                 InventoryTerminalHost& host) {
  auto& runtime = host.Runtime();
  switch (step) {
    case kScreenCloseMenu:
      ClosingPresentation(host);
      if (host.RowsReady() && host.UnloadItemText()) {
        step = kScreenIdle;
        return true;
      }
      break;
    case kScreenCloseBattle:
      LeaveInventory(host, false);
      state.quit_countdown = kAnimationSteps;
      step = kScreenCountdown;
      break;
    case kScreenCountdown:
      ClosingPresentation(host);
      if (state.quit_countdown != 0) {
        --state.quit_countdown;
        break;
      }
      if (host.RowsReady() && host.UnloadItemText()) {
        runtime.control.current_menu = kBattleMenu;
        step = kScreenIdle;
        return true;
      }
      break;
    case kScreenItemAction:
      ClosingPresentation(host);
      if (host.BackgroundFadeStep() == 0 && host.UnloadItemText()) {
        runtime.use.kind = kItemUseKind;
        runtime.control.return_state = runtime.control.current_menu;
        runtime.control.current_menu = kBattleMenu;
        step = kScreenIdle;
        return true;
      }
      break;
    case kScreenResume:
      state.resume_without_cursor_reset = 1;
      step = kScreenIdle;
      break;
    default:
      break;
  }
  return false;
}
void ScreenSelection(std::uint8_t& step, InventoryTerminalHost& host) {
  const auto result = host.Navigation(0);
  if (result == 0) return;
  if (result < 0) {
    step = result == -2 ? kScreenCloseBattle : kScreenCloseMenu;
    if (step == kScreenCloseMenu) LeaveInventory(host, true);
    return;
  }
  switch (result) {
    case 1:
      static_cast<void>(host.EquipmentMenu(true));
      host.Runtime().freeze_tabs = 0;
      step = kScreenEquipment;
      break;
    case 2:
      host.Runtime().presentation.ashley_layout = kContainerAshleyLayout;
      host.ClearExcept(Framework::kNoRow);
      SetPanels(host.Runtime(), 0);
      step = kScreenWaitContainer;
      break;
    case kUseItemResult:
      SetPanels(host.Runtime(), 0);
      host.ClearExcept(Framework::kNoRow);
      host.SetFieldRenderingEnabled(true);
      host.Runtime().control.execute_ability_type = kItemExecuteType;
      step = kScreenItemAction;
      host.SetHudHidden(false);
      host.Runtime().hud_slide_offset = 0;
      host.RenderAshley(2);
      break;
    default:
      break;
  }
}
std::int32_t AnimatePanel(InventoryTerminalHost& host, std::uint8_t enabled,
                          std::uint8_t& step) {
  if (enabled != 0) {
    if (step < kAnimationSteps) ++step;
    if (step > kAnimationSteps) {
      host.Refuse("inventory panel step outside its curve");
    }
    return host.AnimationOffset(kAnimationSteps - step);
  }
  if (step != 0) --step;
  return (kAnimationSteps - static_cast<std::int32_t>(step)) *
         kPixelsPerAnimationStep;
}
void PresentScreen(InventoryTerminalHost& host) {
  auto& runtime = host.Runtime();
  auto& panels = runtime.presentation;
  const auto offset =
      AnimatePanel(host, panels.secondary_title, panels.secondary_step);
  if (panels.secondary_step != 0 && panels.icon_on_top == 0) {
    const auto category = CurrentCategory(runtime);
    auto& row = host.SetRow(
        {.row = kSecondaryTitleRow,
         .x = offset + kTitleX,
         .y = kSecondaryTitleY,
         .w = kTitleWidth,
         .background_width = kTitleBackground},
        host.Text(TerminalText::kCategoryTitle, static_cast<int>(category)));
    row.selected = 1;
    if (category != Category::kManage) {
      row.max = host.Capacity(category);
      row.count = static_cast<std::uint16_t>(host.CountItems(category));
    }
  }
  host.RenderAshley(0);
  if (panels.category_icons != 0) {
    if (panels.icons_step < kIconSteps) ++panels.icons_step;
  } else if (panels.icons_step != 0) {
    --panels.icons_step;
  }
  host.DrawCategoryIcons(panels.icons_step, panels.icon_on_top);
  const auto status_offset =
      AnimatePanel(host, panels.status_command, panels.status_step);
  if (panels.status_step != 0) {
    host.DrawStatusCommand(status_offset, panels.status_command);
  }
}
}  // namespace

// decomp: _sortItems (src/MENU/items1.h:1077); MENU3 80104530,
// MENUB 80104608. Both originals dispatch shield criteria through
// gripSortStats.
std::int32_t UpdateSortMenu(InventorySortState& state,
                            InventoryTerminalHost& host,
                            std::optional<Category> entry) {
  if (entry) {
    static_cast<void>(SortStats(host, *entry));
    state.category = *entry;
    state.step = 0;
    return 0;
  }
  if (state.step == 0 && host.RowsReady()) {
    const auto stats = SortStats(host, state.category);
    std::array<Framework::RowText, kActionRows> rows{};
    const std::array<std::int32_t, kActionRows> flags{};
    for (std::size_t row = 0; row < stats.size(); ++row) {
      rows[row] = {host.Text(TerminalText::kSortName, stats[row]),
                   host.Text(TerminalText::kSortDescription, stats[row])};
    }
    host.CreateActions(kSortMenuBase + static_cast<int>(state.category),
                       std::span(rows).first(stats.size()),
                       std::span(flags).first(stats.size()));
    state.step = 1;
  } else if (state.step == 1) {
    host.UpdateActions();
    const auto selected = host.ActionResult();
    const auto result = WrappingAdd(selected, 1);
    if (result == 0) return 0;
    if (selected < 0) {
      host.PlaySound(TerminalSound::kLeave);
    } else {
      const auto stats = SortStats(host, state.category);
      if (static_cast<std::size_t>(selected) >= stats.size()) {
        host.Refuse("inventory sort selection outside its criteria");
      }
      host.PlaySound(TerminalSound::kSelect);
      host.Sort(state.category, stats[static_cast<std::size_t>(selected)]);
    }
    return result;
  }
  return 0;
}

// decomp: _discardMenu (src/MENU/items1.h:1185); MENU3 80104788,
// MENUB 80104860. Item clearing remains the inventory callee's responsibility.
std::int32_t UpdateDiscardMenu(InventoryDiscardState& state,
                               InventoryDiscardPresentation& presentation,
                               InventoryTerminalHost& host,
                               std::optional<InventoryDiscardEntry> entry) {
  if (entry) {
    RequireSlot(host, entry->category, entry->one_based_slot);
    if (entry->kind > 2 ||
        (entry->kind == 1 && entry->category != Category::kMisc)) {
      host.Refuse("inventory discard entry has an invalid kind");
    }
    state.item = *entry;
    host.InitializeDiscardInformation();
    host.SetInformation(host.Text(TerminalText::kDiscardPrompt, entry->kind));
    state.step = 0;
    return 0;
  }
  switch (state.step) {
    case kDiscardWaitPrompt:
      if (host.InformationDone() && host.RowsReady()) {
        state.step = (state.item.kind & 1U) != 0 ? kDiscardCreateQuantity
                                                 : kDiscardSingleConfirm;
      }
      break;
    case kDiscardSingleConfirm:
      ConfirmDiscard(state, host, false);
      break;
    case kDiscardCreateQuantity: {
      RequireSlot(host, Category::kMisc, state.item.one_based_slot);
      state.maximum =
          host.Inventory().misc[state.item.one_based_slot - 1U].count;
      StoreQuantityDigits(presentation, kMaximumDigitOffset, state.maximum);
      std::fill_n(presentation.text.begin() + 2, kQuantityDigits,
                  std::uint8_t{0});
      auto& row = QuantityRow(host, presentation, -kQuantityWidth);
      row.state = Framework::RowState::kDismiss;
      row.target0 = 0;
      state.place = 0;
      state.quantity = 0;
      state.step = kDiscardWaitQuantity;
      break;
    }
    case kDiscardSingleInput:
      return DiscardInput(state, host, false);
    case kDiscardWaitQuantity:
      if (!host.RowsReady()) break;
      std::fill_n(presentation.text.begin() + 2, kQuantityDigits,
                  Render::kTextSpaceGlyph);
      static_cast<void>(QuantityRow(host, presentation, 0));
      state.step = kDiscardQuantityInput;
      [[fallthrough]];
    case kDiscardQuantityInput:
      return QuantityInput(state, presentation, host);
    case kDiscardQuantityConfirm:
      if (host.RowsReady()) ConfirmDiscard(state, host, true);
      break;
    case kDiscardQuantityDecision:
      return DiscardInput(state, host, true);
    default:
      break;
  }
  return 0;
}

// decomp: _itemNavigation (src/MENU/items2.h:201); MENU3 801053EC,
// MENUB 80105454. Context differences are retained, including MENUB's merge.
std::int32_t UpdateItemList(InventoryNavigationState& state,
                            InventoryTerminalHost& host,
                            ItemRowsContext context, std::int32_t initialize) {
  if (initialize != 0) {
    SlideTitle(host);
    SetPanels(host.Runtime(), 1);
    host.Runtime().presentation.secondary_step = 0;
    state.ignore_cursor_memory = static_cast<std::uint8_t>(initialize & 1);
    if (state.ignore_cursor_memory != 0 &&
        host.Runtime().cursor_memory_enabled == 0) {
      host.Runtime().cursor = {};
    }
    state.step = 1;
    return 0;
  }
  const auto category = CurrentCategory(host.Runtime());
  switch (state.step) {
    case kListRestore:
    case kListPopulate:
      InitializeList(state, host, context, category);
      state.step = 2;
      break;
    case kListBrowse: {
      const auto result = BrowseRows(state, host, context, category);
      if (result) return *result;
      [[fallthrough]];
    }
    case kListTransition:
      if (ChangeCategory(state, host, category)) {
        InitializeList(state, host, context, CurrentCategory(host.Runtime()));
        state.step = 2;
      }
      break;
    case kListTitle:
      SlideTitle(host);
      state.step = kListWait;
      [[fallthrough]];
    case kListWait:
      if (host.RowsReady()) state.step = 1;
      break;
    case kListRearrange:
      return Rearrange(state, host, context, category);
    case kListDetails: {
      const auto result = host.Details(std::nullopt);
      if (result == 0) break;
      if (result == -2) return -2;
      host.Runtime().presentation.secondary_title = 1;
      SlideTitle(host);
      state.step = 1;
      break;
    }
    case kListBuildActions:
      PopulateActions(state, host, context, category);
      break;
    case kListActions:
      return ProcessActions(state, host, context, category);
    case kListParent:
      SearchParent(state, host, category);
      break;
    case kListDiscard:
    case kListSort: {
      const bool discard = state.step == kListDiscard;
      const auto result =
          discard ? host.Discard(std::nullopt) : host.SortMenu(std::nullopt);
      if (result == 0) break;
      if (discard) host.InitializeInformation();
      host.FlyOutExcept(Framework::kNoRow);
      host.ClearExcept(kTitleRow);
      if (result == -2) return -2;
      state.step = kListWait;
      break;
    }
    default:
      break;
  }
  return 0;
}

// decomp: vs_mainMenu_miscItemsShortcutMenu
// (src/MENU/MAINMENU.PRG/C48.c:480); 800FAEBC..800FB3C8.
void UpdateMiscItemShortcut(MiscShortcutState& state,
                            InventoryTerminalHost& host,
                            std::int32_t initialize) {
  if (initialize != 0) {
    host.InitializeInformation();
    if ((initialize & 1) != 0) state.no_force_cursor_memory = 1;
    state.step = 0;
    return;
  }
  switch (state.step) {
    case kShortcutInitialize: {
      auto& row = host.SetRow({.row = 0,
                               .x = kOffscreenX,
                               .y = kTitleY,
                               .w = kTitleWidth,
                               .background_width = kTitleBackground},
                              host.Text(TerminalText::kItemsTitle));
      row.state = Framework::RowState::kSlideX;
      row.selected = 1;
      row.target0 = kTitleX;
      row.unselectable =
          (host.Runtime().restrictions & kRestrictionMask) != 0 ? 1 : 0;
      state.step = 1;
      static_cast<void>(host.LoadItemText(true));
      break;
    }
    case kShortcutWaitTitle: {
      if (!host.RowsReady() || !host.LoadItemText(false)) break;
      auto& row = host.SetRow({.row = kShortcutMiscRow,
                               .x = kOffscreenX,
                               .y = kSecondaryTitleY,
                               .w = kTitleWidth,
                               .background_width = kTitleBackground},
                              host.Text(TerminalText::kMiscTitle));
      row.state = Framework::RowState::kSlideX;
      row.selected = 1;
      row.target0 = kTitleX;
      state.step = 2;
      break;
    }
    case kShortcutPopulate:
      PopulateShortcut(state, host);
      state.step = kShortcutInput;
      break;
    case kShortcutInput:
      ShortcutInput(state, host);
      break;
    case kShortcutClose:
      if (!host.RowsReady() || !host.UnloadItemText()) break;
      if (state.selected_row > 0) host.Runtime().use.kind = kItemUseKind;
      host.Runtime().control.current_menu = kShortcutReturnMenu;
      host.Runtime().use.command = 2;
      return;
    default:
      break;
  }
  host.RenderScreen();
}

// decomp: vs_menu3_exec (src/MENU/MENU3.PRG/16C.c:1236);
// 80108970..801090C4, including the terminal delay instruction.
std::int32_t UpdateInventoryScreen(InventoryScreenState& state,
                                   std::uint8_t& menu_step,
                                   InventoryTerminalHost& host) {
  auto& runtime = host.Runtime();
  switch (menu_step) {
    case kScreenInitialize:
      SetPanels(runtime, 0);
      host.PopulateBaseStats(kComparisonActor);
      ResetAnimations(runtime);
      if (!host.ItemNamesLoaded()) static_cast<void>(host.LoadItemText(true));
      if (!host.RowsReady()) break;
      host.SetFieldRenderingEnabled(false);
      host.InitializeInformation();
      host.SetHudHidden(true);
      runtime.hud_slide_offset = kHudMenuOffset;
      host.RenderAshley(1);
      static_cast<void>(host.Navigation(state.resume_without_cursor_reset + 1));
      state.resume_without_cursor_reset = 0;
      menu_step = kScreenWaitText;
      [[fallthrough]];
    case kScreenWaitText:
      if (!host.LoadItemText(false)) break;
      menu_step = kScreenInventory;
      [[fallthrough]];
    case kScreenInventory:
      ScreenSelection(menu_step, host);
      break;
    case kScreenEquipment: {
      const auto result = host.EquipmentMenu(false);
      if (result == 0) break;
      if (result == -2) {
        runtime.presentation.secondary_title = 0;
        runtime.presentation.secondary_step = 0;
        menu_step = kScreenCloseBattle;
        break;
      }
      runtime.presentation.category_icons = 1;
      runtime.presentation.icon_on_top = 0;
      runtime.presentation.status_command = 1;
      static_cast<void>(host.Navigation(1));
      menu_step = kScreenInventory;
      break;
    }
    case kScreenWaitContainer:
      if (host.RowsReady()) {
        host.RenderAshley(2);
        menu_step = kScreenContainerReturn;
        runtime.control.return_state = runtime.control.current_menu;
        runtime.control.current_menu = kContainerMenu;
        runtime.restrictions |= kManageOptions;
      }
      break;
    case kScreenContainerReturn:
      if (runtime.control.return_state != 0) {
        host.SetBackgroundPriority(kClosedBackgroundOrder,
                                   kNormalMenuBrightness);
        host.SetFieldRenderingEnabled(true);
        SetPanels(runtime, 0);
        host.ClearExcept(Framework::kNoRow);
        host.SetHudHidden(false);
        runtime.hud_slide_offset = 0;
        state.quit_countdown = kAnimationSteps;
        menu_step = kScreenCountdown;
      } else if (host.RowsReady()) {
        SetPanels(runtime, 0);
        ResetAnimations(runtime);
        host.RenderAshley(1);
        static_cast<void>(host.Navigation(1));
        menu_step = kScreenInventory;
      }
      break;
    case kScreenCloseMenu:
    case kScreenCloseBattle:
    case kScreenCountdown:
    case kScreenItemAction:
    case kScreenResume:
      if (CloseScreen(state, menu_step, host)) return 1;
      break;
    default:
      break;
  }
  PresentScreen(host);
  return 0;
}
}  // namespace VS::Menu::Inventory
