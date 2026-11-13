#include "Menu/Inventory/ItemDetailDispatch.hpp"

#include "Core/Math/ScalarMath.hpp"
#include "Input/Pad.hpp"

namespace VS::Menu::Inventory {
namespace {
constexpr std::uint8_t kAnimationFrames = 10;
constexpr std::uint32_t kExitButtons = Input::Triangle | Input::Cross;
constexpr std::int32_t kIconCount = 8;
constexpr std::int32_t kIconPitch = 16;
constexpr std::int32_t kIconStartX = 32;
constexpr std::int32_t kIconMergeInset = 12;
constexpr std::int32_t kFirstCategoryRowIcon = 24;
constexpr std::int32_t kLastIconAnimationStep = 4;
constexpr std::int32_t kIconAnimationPitch = 8;
constexpr std::int32_t kDetailHeaderY = 16;
constexpr std::uint8_t kFirstDetailElement = 9;
constexpr std::uint8_t kDetailCategoryCount = 6;
constexpr std::int32_t kReturnToBattle = -2;

void Begin(EquipmentDetailState& state, ItemDetailWorld& world,
           DetailSelection selection) {
  state.selected_row = static_cast<std::uint8_t>(selection.row.index);
  state.selected_page = static_cast<std::uint8_t>(selection.page.index);
  world.BeginTransition(DetailRowId{state.selected_row});
  state.animation_step = kAnimationFrames;
  state.stage = DetailStage::kInitialize;
}

void Initialize(EquipmentDetailState& state, ItemDetailWorld& world,
                bool armor) {
  if (world.RowsReady() == 0) {
    return;
  }
  world.InitializeLayout(
      DetailRowId{state.selected_row},
      armor ? DetailStatPages::kAll : DetailStatPages::kClassAndAffinity);
  world.SetStatistics(world.CurrentItem(DetailPage{state.selected_page}));
  // Armor's original reloads the direct display order after the stats call.
  if (armor &&
      world.ArmorShowsDp(world.CurrentItem(DetailPage{state.selected_page}))) {
    world.OpenDpBar();
  }
  state.stage = DetailStage::kAnimate;
}

void PollInput(EquipmentDetailState& state, ItemDetailWorld& world,
               bool armor) {
  if ((world.PressedButtons() & kExitButtons) != 0) {
    world.Exit(armor);
    state.stage = DetailStage::kReturn;
    if (!armor) {
      world.SetBattleReturnIntent((world.PressedButtons() & Input::Triangle) !=
                                          0
                                      ? BattleReturnIntent::kBattle
                                      : BattleReturnIntent::kMenu);
    }
    return;
  }
  const DetailPage page = world.PollPaging(DetailPage{state.selected_page});
  if (page.index != state.selected_page) {
    state.selected_page = static_cast<std::uint8_t>(page.index);
    const InventorySlot slot = world.SelectItem(page);
    const DetailRowView text = world.FormatItem(slot);
    world.SetStatistics(slot);
    world.ChangePage(DetailRowId{state.selected_row}, text, page);
  }
}

std::int32_t UpdateController(EquipmentDetailState& state,
                              ItemDetailWorld& world,
                              std::optional<DetailSelection> selection,
                              bool armor) {
  if (selection) {
    Begin(state, world, selection.value());
    return 0;
  }
  switch (state.stage) {
    case DetailStage::kInitialize:
      Initialize(state, world, armor);
      break;
    case DetailStage::kAnimate:
      if (state.animation_step != 0) {
        --state.animation_step;
      } else {
        state.stage = DetailStage::kInput;
      }
      break;
    case DetailStage::kInput:
      PollInput(state, world, armor);
      break;
    case DetailStage::kReturn:
      return world.RowsReady();
  }
  return 0;
}

void EnterDispatcher(ItemDetailDispatcherState& state,
                     ItemDetailDispatcherWorld& world,
                     const DetailDispatchEntry& entry) {
  state.category = entry.category;
  world.SetHeaderPlacement(DetailHeaderPlacement::kDetails);
  world.SetBattleReturnIntent(BattleReturnIntent::kMenu);
  world.ResetDetailCursor();
  const auto page = entry.selection
                        ? static_cast<std::uint8_t>(Core::Math::WrappingAdd(
                              entry.selection.value().page.index, 1))
                        : 0;
  world.CategoryHeader().item_page = page;
  world.SetCommand(DetailCommand::kNone);
}
}  // namespace

// decomp: _renderItemCategory (src/MENU/items0.h:46-81).
void DrawItemCategoryIcons(CategoryIconWorld& world,
                           std::int32_t animation_step,
                           std::int32_t highlight_selected) {
  const auto selected = (world.CurrentCategory() - 1) & (kIconCount - 1);
  if (animation_step == kLastIconAnimationStep) {
    world.DrawListTabNavigation();
  }
  const auto y = Core::Math::WrappingSubtract(
      Core::Math::WrappingMultiply(animation_step, kIconAnimationPitch),
      kIconPitch);
  for (std::int32_t category = 0; category < kIconCount; ++category) {
    const auto x =
        (((category + 1) & (kIconCount - 1)) * kIconPitch) + kIconStartX;
    const bool overlay = category == selected && highlight_selected == 1;
    if (overlay) {
      auto& header = world.CategoryHeader();
      if (x >= header.x - kIconMergeInset) {
        header.row_icon =
            static_cast<std::uint8_t>(category + kFirstCategoryRowIcon);
        continue;
      }
    }
    world.DrawIcon({.category = static_cast<std::uint8_t>(category),
                    .x = x,
                    .y = overlay ? kDetailHeaderY : y,
                    .selected_palette = category == selected,
                    .overlay = overlay});
  }
}

// decomp: _armorStatusView (src/MENU/items1.h:464-544).
std::int32_t UpdateArmorDetails(EquipmentDetailState& state,
                                ItemDetailWorld& world,
                                std::optional<DetailSelection> selection) {
  return UpdateController(state, world, selection, true);
}

// decomp: _gemStatusView (src/MENU/items1.h:554-630).
std::int32_t UpdateGemDetails(EquipmentDetailState& state,
                              ItemDetailWorld& world,
                              std::optional<DetailSelection> selection) {
  return UpdateController(state, world, selection, false);
}

// decomp: _itemStatusDispatcher (src/MENU/items1.h:640-688).
std::int32_t UpdateItemDetails(ItemDetailDispatcherState& state,
                               ItemDetailDispatcherWorld& world,
                               std::optional<DetailDispatchEntry> entry) {
  if (entry) {
    EnterDispatcher(state, world, entry.value());
  }
  if (static_cast<std::uint8_t>(state.category) >= kDetailCategoryCount) {
    world.Refuse("detail dispatcher category outside its six controllers");
  }
  const auto result = world.RunController(
      state.category, entry ? entry.value().selection : std::nullopt);
  if (result != 0) {
    if (world.ReturnsToBattle()) {
      return kReturnToBattle;
    }
    world.SetCategoryVisibility(1);
    world.SetHeaderPlacement(DetailHeaderPlacement::kList);
    world.SetCommandVisibility(1);
    world.SetPreviewLayout(PreviewLayout::kItems);
    world.SetCommand(DetailCommand::kMenu);
    return result;
  }
  if (world.RowsReady() != 0) {
    world.SetCursorBright(world.SelectedDetailElement() != kFirstDetailElement);
    world.DrawDetailTabNavigation();
    world.DrawItemDetails();
  }
  return result;
}
}  // namespace VS::Menu::Inventory
