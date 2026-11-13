#include "Menu/Container/ContainerMenuDialogs.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <variant>

#include "Core/Math/ScalarMath.hpp"
#include "Input/Pad.hpp"
#include "Menu/Framework/ItemRows.hpp"
#include "Render/EncodedText.hpp"

namespace VS::Menu::container {
namespace {
using Category = Items::ItemCategory;
using Core::Math::WrappingAdd;
constexpr auto kWeaponCriteria = std::to_array<std::uint8_t>({
    0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11,
    12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23,
});
constexpr std::array<std::uint8_t, 7> kGripCriteria{0, 8, 9, 10, 24, 25, 26};
constexpr auto kShieldCriteria = std::to_array<std::uint8_t>({
    1,  4,  5,  6,  7,  8,  9,  10, 40, 41, 42, 27,
    28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39,
});
constexpr auto kArmorCriteria = std::to_array<std::uint8_t>({
    0,  1,  4,  5,  8,  9,  10, 40, 41, 42, 27, 28,
    29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39,
});
constexpr auto kGemCriteria = std::to_array<std::uint8_t>(
    {8, 9, 10, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39});
constexpr std::array<std::uint8_t, 2> kMiscCriteria{0, 56};
constexpr std::int32_t kSortNameEntry = 438;
constexpr std::int32_t kSortDescriptionEntry = 439;
constexpr std::int32_t kSortMenuBase = 96;
constexpr std::int32_t kConfirmYesEntry = 552;
constexpr std::int32_t kDiscardConfirmEntry = 554;
constexpr std::size_t kDiscardConfirmHalfword = 11849;
constexpr std::int32_t kDiscardMenu = 4;
constexpr std::int32_t kTransferMenu = 3;
constexpr std::int32_t kTransferYesEntry = 8;
constexpr std::int32_t kTransferDescriptionEntry = 10;
constexpr std::int32_t kCursorMenuBase = 81;
constexpr std::int32_t kFirstListRow = 20;
constexpr std::uint8_t kListAnimationComplete = 16;
constexpr std::size_t kFirstQuantityDigit = 2;
constexpr std::size_t kFirstMaximumDigit = 6;
constexpr std::size_t kQuantityDigits = 3;
constexpr std::int32_t kQuantitySlideX = -126;
constexpr std::int32_t kQuantityRightX = 66;
constexpr std::int32_t kDigitAdvance = 6;
constexpr std::int32_t kQuantityLeftX = 54;
constexpr std::uint32_t kQuantityCursorY = 0x720000;
constexpr std::uint32_t kDigitMask = 15;
constexpr unsigned kNibbleShift = 4;
constexpr std::array<std::int32_t, 3> kQuantitySteps{1, 10, 100};
constexpr std::uint32_t kQuantityCancelButtons = Input::Triangle | Input::Cross;
constexpr auto kQuantityActionButtons = kQuantityCancelButtons | Input::Circle;

std::span<const std::uint8_t> Criteria(MenuDialogWorld& world,
                                       std::uint8_t category) {
  switch (static_cast<Category>(category)) {
    case Category::kWeapon:
    case Category::kBlade:
      return kWeaponCriteria;
    case Category::kGrip:
      return kGripCriteria;
    case Category::kShield:
      return kShieldCriteria;
    case Category::kArmor:
      return kArmorCriteria;
    case Category::kGem:
      return kGemCriteria;
    case Category::kMisc:
      return kMiscCriteria;
    default:
      world.Refuse("sort category outside the seven original criterion tables");
  }
}

std::span<const std::uint8_t> RequiredEntry(MenuDialogWorld& world,
                                            std::span<const std::uint8_t> table,
                                            std::int32_t index) {
  const auto text = Framework::ItemHelpEntry(table, index);
  if (!text.has_value()) world.Refuse("missing required dialog text entry");
  return table.subspan(text->offset);
}

void CreateDiscardActions(DiscardMenuState& state, MenuDialogWorld& world,
                          bool quantity_confirmation) {
  const auto help = world.HelpText();
  std::array<Framework::RowText, 2> rows;
  for (std::size_t i = 0; i < rows.size(); ++i) {
    const auto name =
        RequiredEntry(world, help, kConfirmYesEntry + static_cast<int>(i));
    std::span<const std::uint8_t> description;
    if (quantity_confirmation) {
      const auto text = Framework::ItemHelpText(help, kDiscardConfirmHalfword);
      if (!text.has_value()) world.Refuse("missing discard confirmation text");
      description = help.subspan(text->offset);
    } else {
      description =
          RequiredEntry(world, help, kDiscardConfirmEntry + state.kind);
    }
    rows[i] = {name, description};
  }
  world.CreateActions(kDiscardMenu, rows);
  state.step = quantity_confirmation ? DiscardStep::kConfirmationInput
                                     : DiscardStep::kSingleInput;
}

void StoreDecimal(std::span<std::uint8_t> digits, std::uint8_t value) {
  auto split = Core::Math::SplitDecimalDigit(value);
  digits[2] = static_cast<std::uint8_t>(split & kDigitMask);
  split = Core::Math::SplitDecimalDigit(split >> kNibbleShift);
  digits[1] = static_cast<std::uint8_t>(split & kDigitMask);
  digits[0] = static_cast<std::uint8_t>(split >> kNibbleShift);
}

Items::InventoryMisc MiscRecord(Items::ContainerWorld& records,
                                std::uint8_t index) {
  const auto item = records.Read(Category::kMisc, index);
  const auto* misc = std::get_if<Items::InventoryMisc>(&item);
  if (misc == nullptr) records.Refuse("discard stack is not a misc record");
  return *misc;
}

void InitializeQuantity(DiscardMenuState& state,
                        DiscardPresentation& presentation,
                        MenuDialogWorld& world) {
  state.maximum = MiscRecord(world.ActiveRecords(), state.record_index).count;
  StoreDecimal(
      std::span(presentation.text).subspan(kFirstMaximumDigit, kQuantityDigits),
      state.maximum);
  std::fill_n(presentation.text.begin() + kFirstQuantityDigit, kQuantityDigits,
              0);
  auto& row = world.QuantityRow(presentation.text, kQuantitySlideX);
  row.state = Framework::RowState::kDismiss;
  row.target0 = 0;
  state.place = 0;
  state.quantity = 0;
  state.step = DiscardStep::kRevealQuantity;
}

std::int32_t PollDiscardActions(DiscardMenuState& state, MenuDialogWorld& world,
                                bool quantity) {
  world.UpdateActions();
  const auto result = WrappingAdd(world.ActionResult(), 1);
  if (result == 0) return 0;
  if (result != 1) {
    world.PlaySound(DialogSound::kLeave);
    return result;
  }
  world.PlaySound(DialogSound::kDiscard);
  auto& records = world.ActiveRecords();
  if (quantity) {
    auto stack = MiscRecord(records, state.record_index);
    if (stack.count != state.quantity) {
      stack.count = static_cast<std::uint8_t>(stack.count - state.quantity);
      records.Write(Category::kMisc, state.record_index, stack);
      return result;
    }
  }
  Items::ClearContainerItem(state.category, state.record_index, records);
  return result;
}

void AdjustQuantity(DiscardMenuState& state, MenuDialogWorld& world) {
  const auto previous_quantity = state.quantity;
  const auto vertical = world.RepeatedButtons();
  const int change = ((vertical & Input::Up) != 0 ? 1 : 0) -
                     ((vertical & Input::Down) != 0 ? 1 : 0);
  auto proposed = static_cast<std::int32_t>(state.quantity);
  if (change != 0) {
    if (state.place >= kQuantitySteps.size()) {
      world.Refuse("discard digit place outside the reached step table");
    }
    proposed += change * kQuantitySteps[state.place];
  }
  if (proposed >= 0 && proposed <= state.maximum) {
    state.quantity = static_cast<std::uint8_t>(proposed);
  }
  const auto horizontal = world.RepeatedButtons();
  const auto previous_place = state.place;
  const int place_change = ((horizontal & Input::Left) != 0 ? 1 : 0) -
                           ((horizontal & Input::Right) != 0 ? 1 : 0);
  if (place_change == 1) {
    if (previous_place == kQuantitySteps.size() - 1) {
      state.quantity = state.maximum;
    } else {
      state.place = static_cast<std::uint8_t>(previous_place + 1U);
    }
  } else if (place_change == -1) {
    if (previous_place == 0) {
      state.quantity = 0;
    } else {
      state.place = static_cast<std::uint8_t>(previous_place - 1U);
    }
  }
  if (previous_place != state.place || previous_quantity != state.quantity) {
    world.PlaySound(DialogSound::kChange);
  }
}

void DrawQuantity(DiscardMenuState& state, DiscardPresentation& presentation,
                  MenuDialogWorld& world) {
  world.SetQuantityFont();
  auto split = Core::Math::SplitDecimalDigit(state.quantity);
  world.DrawQuantityDigit(static_cast<std::uint32_t>(split) & kDigitMask,
                          kQuantityRightX);
  split = Core::Math::SplitDecimalDigit(split >> kNibbleShift);
  world.DrawQuantityDigit(static_cast<std::uint32_t>(split) & kDigitMask,
                          kQuantityRightX - kDigitAdvance);
  world.DrawQuantityDigit(static_cast<std::uint32_t>(split >> kNibbleShift),
                          kQuantityLeftX);
  const auto x = Core::Math::WrappingSubtract(
      kQuantityLeftX, Core::Math::WrappingMultiply(state.place, kDigitAdvance));
  const auto position = static_cast<std::int32_t>(
      static_cast<std::uint32_t>(x) | kQuantityCursorY);
  presentation.cursor_animation = static_cast<std::uint8_t>(
      world.DrawQuantityCursor(presentation.cursor_animation, position));
}

std::int32_t PollQuantity(DiscardMenuState& state,
                          DiscardPresentation& presentation,
                          MenuDialogWorld& world) {
  if ((world.PressedButtons() & kQuantityActionButtons) != 0) {
    StoreDecimal(std::span(presentation.text)
                     .subspan(kFirstQuantityDigit, kQuantityDigits),
                 state.quantity);
    static_cast<void>(world.QuantityRow(presentation.text, 0));
    if ((world.PressedButtons() & kQuantityCancelButtons) != 0) {
      world.PlaySound(DialogSound::kLeave);
      return (world.PressedButtons() & Input::Triangle) != 0 ? -2 : -1;
    }
    if (state.quantity == 0) {
      world.PlaySound(DialogSound::kLeave);
      return -1;
    }
    world.PlaySound(DialogSound::kSelect);
    state.step = DiscardStep::kCreateConfirmation;
    return 0;
  }
  AdjustQuantity(state, world);
  DrawQuantity(state, presentation, world);
  return 0;
}
}  // namespace

// decomp: _displaySortMenu (src/MENU/MENUD.PRG/234.c:1601-1678),
// 80105ACC..80105D23 including the true terminal delay instruction.
std::int32_t UpdateSortMenu(SortMenuState& state, MenuDialogWorld& world,
                            std::optional<Category> entry) {
  if (entry.has_value()) {
    state.category = static_cast<std::uint8_t>(*entry);
    state.step = ActionDialogStep::kCreate;
    return 0;
  }
  if (state.step == ActionDialogStep::kCreate) {
    if (world.RowsReady() == 0) return 0;
    const auto criteria = Criteria(world, state.category);
    const auto help = world.HelpText();
    std::array<Framework::RowText, kWeaponCriteria.size()> rows;
    for (std::size_t i = 0; i < criteria.size(); ++i) {
      const auto stat = criteria[i];
      rows[i] = {RequiredEntry(world, help, stat * 2 + kSortNameEntry),
                 RequiredEntry(world, help, stat * 2 + kSortDescriptionEntry)};
    }
    world.CreateActions(kSortMenuBase + state.category,
                        std::span(rows).first(criteria.size()));
    state.step = ActionDialogStep::kInput;
  } else if (state.step == ActionDialogStep::kInput) {
    world.UpdateActions();
    const auto selected = world.ActionResult();
    const auto result = WrappingAdd(selected, 1);
    if (result == 0) return 0;
    if (selected < 0) {
      world.PlaySound(DialogSound::kLeave);
    } else {
      world.PlaySound(DialogSound::kSelect);
      const auto criteria = Criteria(world, state.category);
      if (static_cast<std::size_t>(selected) >= criteria.size()) {
        world.Refuse("selected sort action outside its actual criterion table");
      }
      world.Sort(static_cast<Category>(state.category), criteria[selected]);
    }
    return result;
  }
  return 0;
}

// decomp: _discardMenu (src/MENU/MENUD.PRG/234.c:1680-1923),
// 80105D24..80106463; caller action rows own equipment/attachment restrictions.
std::int32_t UpdateDiscardMenu(DiscardMenuState& state,
                               DiscardPresentation& presentation,
                               MenuDialogWorld& world,
                               std::optional<DiscardEntry> entry) {
  if (entry.has_value()) {
    state.category = entry->category;
    state.record_index = entry->record_index;
    state.kind = entry->kind;
    world.InitializeInformationBox();
    const auto help = world.HelpText();
    world.SetInformation(
        RequiredEntry(world, help, kDiscardConfirmEntry + state.kind));
    state.step = DiscardStep::kWaitInformation;
    return 0;
  }
  switch (state.step) {
    case DiscardStep::kWaitInformation:
      if (world.InformationDone() && world.RowsReady() != 0) {
        state.step = (state.kind & 1U) != 0 ? DiscardStep::kCreateQuantity
                                            : DiscardStep::kCreateSingle;
      }
      break;
    case DiscardStep::kCreateSingle:
      CreateDiscardActions(state, world, false);
      break;
    case DiscardStep::kCreateQuantity:
      InitializeQuantity(state, presentation, world);
      break;
    case DiscardStep::kSingleInput:
      return PollDiscardActions(state, world, false);
    case DiscardStep::kRevealQuantity:
      if (world.RowsReady() == 0) return 0;
      std::fill_n(presentation.text.begin() + kFirstQuantityDigit,
                  kQuantityDigits, Render::kTextSpaceGlyph);
      static_cast<void>(world.QuantityRow(presentation.text, 0));
      state.step = DiscardStep::kQuantityInput;
      [[fallthrough]];
    case DiscardStep::kQuantityInput:
      return PollQuantity(state, presentation, world);
    case DiscardStep::kCreateConfirmation:
      if (world.RowsReady() != 0) CreateDiscardActions(state, world, true);
      break;
    case DiscardStep::kConfirmationInput:
      return PollDiscardActions(state, world, true);
  }
  return 0;
}

// decomp: func_801071D8 (src/MENU/MENUD.PRG/234.c:2309-2330),
// 801071D8..801072AF; each row borrow precedes its current cursor-memory read.
void RestoreItemRows(ItemRowRestoreWorld& world, std::int32_t category) {
  while (world.ListAnimationRow() < kListAnimationComplete) {
    world.AdvanceListRows();
  }
  const auto memory_index =
      Core::Math::WrappingMultiply(WrappingAdd(category, kCursorMenuBase), 2);
  for (std::int32_t i = kFirstListRow; i < Framework::kNoRow; ++i) {
    auto& row = world.Row(i);
    if (row.state == Framework::RowState::kSlideX) {
      row.state = Framework::RowState::kIdle;
      row.x = row.target0;
    }
    const auto memory = world.CursorMemory();
    if (memory_index < 0 ||
        static_cast<std::size_t>(memory_index) >= memory.size()) {
      world.Refuse("restored list cursor outside actual menu-memory bytes");
    }
    row.selected = i == memory[memory_index] + kFirstListRow ? 1 : 0;
  }
}

// decomp: func_801072B0 (src/MENU/MENUD.PRG/234.c:2332-2391),
// 801072B0..8010748F; full-word results are reloaded after external callbacks.
std::int32_t UpdateTransferConfirmation(TransferConfirmationState& state,
                                        MenuDialogWorld& world,
                                        bool initialize) {
  if (initialize) {
    world.InitializeInformationBox();
    world.PrepareTransferControls();
    state.result = 0;
    state.step = ActionDialogStep::kCreate;
    return 0;
  }
  const auto step = state.step;
  switch (step) {
    case ActionDialogStep::kCreate: {
      const auto table = world.ContainerText();
      const auto direction = world.TransferDirection();
      std::array<Framework::RowText, 2> rows;
      for (std::size_t i = 0; i < rows.size(); ++i) {
        const auto name = RequiredEntry(
            world, table, kTransferYesEntry + static_cast<int>(i));
        rows[i] = {name, RequiredEntry(world, table,
                                       direction + kTransferDescriptionEntry)};
      }
      world.CreateActions(kTransferMenu, rows);
      state.step = ActionDialogStep::kInput;
      break;
    }
    case ActionDialogStep::kInput:
      world.UpdateActions();
      state.result = WrappingAdd(world.ActionResult(), 1);
      if (state.result == 0) break;
      world.FlyOutAction(0);
      world.FlyOutAction(1);
      if (state.result == static_cast<std::int32_t>(step)) {
        world.PlaySound(DialogSound::kSelect);
        world.ClearTransferRows();
        world.HideTransferPanels();
      } else {
        world.PlaySound(DialogSound::kLeave);
        if (state.result == 2) state.result = -1;
      }
      state.step = ActionDialogStep::kWait;
      break;
    case ActionDialogStep::kWait:
      if (world.RowsReady() != 0) return state.result;
      break;
  }
  return 0;
}
}  // namespace VS::Menu::container
