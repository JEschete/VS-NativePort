#include "Menu/Inventory/DetailsPolicies.hpp"

#include <cstddef>

#include "Core/Math/ScalarMath.hpp"
#include "Input/Pad.hpp"

namespace VS::Menu::Inventory {
namespace {
constexpr std::uint32_t kPagingButtons = Input::L1 | Input::R1;
constexpr std::int16_t kHeaderY = 18;
constexpr std::int16_t kSelectedX = 155;
}  // namespace

// decomp: _toggleMenuUi (src/MENU/_toggleUi.h:9-14).
void SetPanelVisibility(PanelVisibility& panels, std::uint8_t visibility) {
  panels = {visibility, visibility, visibility};
}

// decomp: _itemPaging (src/MENU/items0.h:15-41).
DetailPage PollDetailsPaging(DetailsWorld& world, DetailPage page) {
  if ((world.HeldButtons() & kPagingButtons) == kPagingButtons) {
    return page;
  }
  const DetailPage previous = page;
  const std::int32_t items = world.ItemCount();
  const std::uint32_t repeat = world.RepeatButtons();
  if ((repeat & Input::L1) != 0) {
    page.index = Core::Math::WrappingAdd(
        page.index, Core::Math::WrappingSubtract(items, 1));
  }
  if ((repeat & Input::R1) != 0) {
    page.index = Core::Math::WrappingAdd(page.index, 1);
  }
  if (page.index >= items) {
    page.index = Core::Math::WrappingSubtract(page.index, items);
  }
  if ((world.PressedButtons() & kPagingButtons) != 0 && page == previous) {
    world.PlayInvalidSound();
  }
  return page;
}

// decomp: _startMenuTransition (src/MENU/items0.h:87-102).
void BeginDetailsTransition(DetailsWorld& world, DetailRowId selected) {
  world.ClearRows();
  auto& category = world.Row(kCategoryHeaderRow);
  category.state = Framework::RowState::kSlideY;
  category.target0 = kHeaderY;
  auto& item = world.Row(selected);
  item.state = Framework::RowState::kSlideX;
  item.target0 = kSelectedX;
  item.selected = 1;
  item.sub_text = nullptr;
}

// decomp: _initMenuLayout (src/MENU/items1.h:21-30).
void InitializeDetailsLayout(DetailsWorld& world, DetailRowId selected,
                             std::int32_t stat_pages) {
  world.SetPreviewLayout(PreviewLayout::kDetails);
  world.SetCategoryVisibility(0);
  world.SetCommandVisibility(0);
  world.ContinueTransition(selected);
  world.ShowStatPages(stat_pages);
  world.MovePrimaryStats(StatPanelMotion::kOpen);
}

// decomp: _getItemIndex (src/MENU/items1.h:54-61).
InventorySlot SelectDetailsItem(DetailsWorld& world, DetailPage page) {
  world.RememberPage({0, static_cast<std::uint8_t>(page.index)});
  const auto order = world.DisplayOrder();
  if (page.index < 0 || static_cast<std::size_t>(page.index) >= order.size()) {
    world.Refuse("detail page outside the current inventory display order");
  }
  return InventorySlot{order[static_cast<std::size_t>(page.index)]};
}

// decomp: _exitStatusView (src/MENU/items1.h:74-88).
void ExitDetailsView(DetailsWorld& world, bool clear_dp_pp) {
  world.PlayLeaveSound();
  world.ClearRows();
  world.DismissCommandRow();
  world.ShowStatPages(-1);
  world.MovePrimaryStats(StatPanelMotion::kClose);
  world.SetHeaderPlacement(CategoryHeaderPlacement::kReturning);
  world.SetBattleReturnIntent((world.PressedButtons() & Input::Triangle) != 0
                                  ? BattleReturnIntent::kBattle
                                  : BattleReturnIntent::kMenu);
  if (clear_dp_pp) {
    world.HideDpPpBars();
  }
}
}  // namespace VS::Menu::Inventory
