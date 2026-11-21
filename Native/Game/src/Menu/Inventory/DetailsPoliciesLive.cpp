#include "Menu/Inventory/DetailsPoliciesLive.hpp"

#include <cstddef>
#include <cstdlib>

#include "Core/Diagnostics.hpp"
#include "Input/ControllerLiveState.hpp"
#include "Input/Pad.hpp"
#include "Items/InventoryLive.hpp"
#include "Items/ItemRecords.hpp"
#include "Menu/Framework/DetailInputLive.hpp"
#include "Menu/Framework/EquipmentDetails.hpp"
#include "Menu/Framework/EquipmentDetailsLive.hpp"
#include "Menu/Framework/FrameworkLive.hpp"
#include "Menu/Framework/FrameworkLiveState.hpp"
#include "Menu/Inventory/DetailsPolicies.hpp"
#include "Menu/Inventory/Inventory.hpp"
#include "Menu/Inventory/ItemList.hpp"
#include "Menu/Sounds.hpp"

namespace VS::Menu::Inventory {
namespace {
constexpr std::size_t kIndexedCategories = Items::kItemCategoryCount - 1;

[[noreturn]] void Refuse(const char* reason) {
  VS::Core::Errorf("menu.inventory", "Inventory details refused: %s", reason);
  std::abort();
}

void RequireRows() {
  if (VS::Menu::Framework::menuItems == nullptr) {
    Refuse("missing menu row workspace");
  }
}

void RequireRow(DetailRowId row) {
  RequireRows();
  if (row.index < 0 ||
      static_cast<std::size_t>(row.index) >= Framework::kRowCount) {
    Refuse("detail row outside the menu row workspace");
  }
}

class LiveWorld final : public Framework::LiveDetailInputWorld<DetailsWorld> {
 public:
  explicit LiveWorld(int category = -1) : category_(category) {}

  std::int32_t ItemCount() override {
    RequireCategory(Items::kItemCategoryCount);
    return Items::Inventory::CountLiveItems(category_, nullptr);
  }
  void RememberPage(CursorMemory memory) override {
    RequireCategory(kIndexedCategories);
    auto& cursor = VS::Menu::Framework::menuMemory.inventory_cursors
                       .positions[static_cast<std::size_t>(category_)];
    cursor.row = memory.row;
    cursor.page = memory.page;
  }
  std::span<const std::uint8_t> DisplayOrder() override {
    RequireCategory(kIndexedCategories);
    const auto index = static_cast<std::size_t>(category_);
    const auto* order = VS::Menu::Inventory::categoryOrders[index];
    if (order == nullptr) {
      Inventory::Refuse("missing current inventory display order");
    }
    return {order, Items::kCategoryCapacities[index]};
  }
  [[noreturn]] void Refuse(std::string_view reason) override {
    VS::Core::Errorf("menu.inventory", "Inventory details refused: %.*s",
                     static_cast<int>(reason.size()), reason.data());
    std::abort();
  }
  void ClearRows() override {
    RequireRows();
    Framework::ClearLiveMenuExcept(Framework::kNoRow);
  }
  Framework::MenuRow& Row(DetailRowId row) override {
    RequireRow(row);
    return Framework::LiveMenuRow(row.index);
  }
  void SetPreviewLayout(PreviewLayout layout) override {
    VS::Menu::Inventory::ashleyRenderState = static_cast<int>(layout);
  }
  void SetCategoryVisibility(std::uint8_t visibility) override {
    VS::Menu::Inventory::itemCategoryIconsEnabled =
        static_cast<std::uint8_t>(visibility);
  }
  void SetCommandVisibility(std::uint8_t visibility) override {
    VS::Menu::Inventory::statusCommandState =
        static_cast<std::uint8_t>(visibility);
  }
  void ContinueTransition(DetailRowId row) override {
    RequireRow(row);
    Framework::ContinueLiveItemDetailsTransition(row.index);
  }
  void ShowStatPages(std::int32_t pages) override {
    Framework::UpdateLiveEquipmentStatPages(pages);
  }
  void MovePrimaryStats(StatPanelMotion motion) override {
    Framework::UpdateLivePrimaryStatPanel(static_cast<int>(motion));
  }
  void PlayLeaveSound() override { Menu::PlayLeaveSound(); }
  void PlayInvalidSound() override { Menu::PlayInvalidSound(); }
  void DismissCommandRow() override {
    RequireRows();
    Framework::FlyOutLiveRowLeft(-1);
  }
  void SetHeaderPlacement(CategoryHeaderPlacement placement) override {
    VS::Menu::Inventory::selectedItemCategoryIconOnTop =
        static_cast<std::uint8_t>(placement);
  }
  void SetBattleReturnIntent(BattleReturnIntent intent) override {
    VS::Menu::Inventory::exitToBattle =
        intent == BattleReturnIntent::kBattle ? Input::Triangle : 0;
  }
  void HideDpPpBars() override {
    Framework::UpdateLiveDpPpBars(Framework::dp_pp_bars::kSlideOut);
  }

 private:
  void RequireCategory(std::size_t count) const {
    if (category_ < 0 || static_cast<std::size_t>(category_) >= count) {
      Inventory::Refuse("detail category outside the inventory tables");
    }
  }
  int category_;
};
}  // namespace

// decomp: _toggleMenuUi (src/MENU/_toggleUi.h:9-14).
void SetLivePanelVisibility(char visibility) {
  PanelVisibility panels;
  SetPanelVisibility(panels, static_cast<std::uint8_t>(visibility));
  VS::Menu::Inventory::secondaryTitleEnabled =
      static_cast<std::uint8_t>(panels.secondary_title);
  VS::Menu::Inventory::itemCategoryIconsEnabled =
      static_cast<std::uint8_t>(panels.category_icons);
  VS::Menu::Inventory::statusCommandState =
      static_cast<std::uint8_t>(panels.status_command);
}

// decomp: itemPaging (src/MENU/items0.h:15-41; was _itemPaging).
int PollLiveDetailsPaging(int category, int page) {
  LiveWorld world(category);
  return PollDetailsPaging(world, DetailPage{page}).index;
}

// decomp: startMenuTransition (src/MENU/items0.h:87-102; was
// _startMenuTransition).
void BeginLiveDetailsTransition(int selected_row) {
  LiveWorld world;
  BeginDetailsTransition(world, DetailRowId{selected_row});
}

// decomp: initMenuLayout (src/MENU/items1.h:26-35; was _initMenuLayout).
void InitializeLiveDetailsLayout(int selected_row, int stat_pages) {
  LiveWorld world;
  InitializeDetailsLayout(world, DetailRowId{selected_row}, stat_pages);
}

// decomp: getItemIndex (src/MENU/items1.h:59-66; was _getItemIndex).
int SelectLiveDetailsItem(int category, int page) {
  LiveWorld world(category);
  return SelectDetailsItem(world, DetailPage{page}).one_based;
}

// decomp: exitStatusView (src/MENU/items1.h:79-93; was _exitStatusView).
void ExitLiveDetailsView(int clear_dp_pp) {
  LiveWorld world;
  ExitDetailsView(world, clear_dp_pp != 0);
}
}  // namespace VS::Menu::Inventory
