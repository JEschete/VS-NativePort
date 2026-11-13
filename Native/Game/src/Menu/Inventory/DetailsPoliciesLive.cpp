#include "Menu/Inventory/DetailsPoliciesLive.hpp"

#include <cstddef>
#include <cstdlib>
#include <iterator>

#include "Input/Pad.hpp"
#include "Items/InventoryLive.hpp"
#include "Items/ItemRecords.hpp"
#include "Menu/Framework/DetailInputLive.hpp"
#include "Menu/Framework/EquipmentDetails.hpp"
#include "Menu/Framework/EquipmentDetailsLive.hpp"
#include "Menu/Framework/FrameworkLive.hpp"
#include "Menu/Inventory/DetailsPolicies.hpp"
#include "Menu/Sounds.hpp"

extern "C" {
#include "Core/Diagnostics.h"
#include "Input/ControllerLiveState.h"
#include "Menu/Framework/FrameworkLiveState.h"
#include "Menu/Inventory/Inventory.h"
#include "Menu/Inventory/ItemList.h"
}

namespace VS::Menu::Inventory {
namespace {
constexpr std::size_t kIndexedCategories = Items::kItemCategoryCount - 1;
constexpr std::size_t kCategoryCursorWorkspaceOffset = 0x3A;
static_assert(offsetof(Menu_Framework_MenuMemory, opaque3A) ==
              kCategoryCursorWorkspaceOffset);
static_assert(sizeof(Menu_Framework_menuMemory.opaque3A.categoryCursors[0]) ==
              sizeof(CursorMemory));

[[noreturn]] void Refuse(const char* reason) {
  Core_Errorf("menu.inventory", "Inventory details refused: %s", reason);
  std::abort();
}

void RequireRows() {
  if (Menu_Framework_menuItems == nullptr) {
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
    auto& cursor = Menu_Framework_menuMemory.opaque3A
                       .categoryCursors[static_cast<std::size_t>(category_)]
                       .position;
    cursor.row = memory.row;
    cursor.page = memory.page;
  }
  std::span<const std::uint8_t> DisplayOrder() override {
    RequireCategory(kIndexedCategories);
    const auto index = static_cast<std::size_t>(category_);
    const auto* order = Menu_Inventory_inventoryIndices[index];
    if (order == nullptr) {
      Inventory::Refuse("missing current inventory display order");
    }
    return {order, Items::kCategoryCapacities[index]};
  }
  [[noreturn]] void Refuse(std::string_view reason) override {
    Core_Errorf("menu.inventory", "Inventory details refused: %.*s",
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
    Menu_Inventory_ashleyRenderState = static_cast<int>(layout);
  }
  void SetCategoryVisibility(std::uint8_t visibility) override {
    Menu_Inventory_itemCategoryIconsEnabled = static_cast<char>(visibility);
  }
  void SetCommandVisibility(std::uint8_t visibility) override {
    Menu_Inventory_statusCommandState = static_cast<char>(visibility);
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
    Menu_Inventory_selectedItemCategoryIconOnTop = static_cast<char>(placement);
  }
  void SetBattleReturnIntent(BattleReturnIntent intent) override {
    Menu_Inventory_exitToBattle =
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
  Menu_Inventory_secondaryTitleEnabled =
      static_cast<char>(panels.secondary_title);
  Menu_Inventory_itemCategoryIconsEnabled =
      static_cast<char>(panels.category_icons);
  Menu_Inventory_statusCommandState = static_cast<char>(panels.status_command);
}

// decomp: _itemPaging (src/MENU/items0.h:15-41).
int PollLiveDetailsPaging(int category, int page) {
  LiveWorld world(category);
  return PollDetailsPaging(world, DetailPage{page}).index;
}

// decomp: _startMenuTransition (src/MENU/items0.h:87-102).
void BeginLiveDetailsTransition(int selected_row) {
  LiveWorld world;
  BeginDetailsTransition(world, DetailRowId{selected_row});
}

// decomp: _initMenuLayout (src/MENU/items1.h:21-30).
void InitializeLiveDetailsLayout(int selected_row, int stat_pages) {
  LiveWorld world;
  InitializeDetailsLayout(world, DetailRowId{selected_row}, stat_pages);
}

// decomp: _getItemIndex (src/MENU/items1.h:54-61).
int SelectLiveDetailsItem(int category, int page) {
  LiveWorld world(category);
  return SelectDetailsItem(world, DetailPage{page}).one_based;
}

// decomp: _exitStatusView (src/MENU/items1.h:74-88).
void ExitLiveDetailsView(int clear_dp_pp) {
  LiveWorld world;
  ExitDetailsView(world, clear_dp_pp != 0);
}
}  // namespace VS::Menu::Inventory
