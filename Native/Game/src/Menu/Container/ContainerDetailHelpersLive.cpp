#include "Menu/Container/ContainerDetailHelpersLive.hpp"

#include <cstddef>
#include <cstdlib>
#include <iterator>
#include <optional>
#include <string_view>

#include "Input/Pad.hpp"
#include "Items/ContainerIndexingLive.hpp"
#include "Items/ContainerTransfersLive.hpp"
#include "Items/ItemRecordsLive.hpp"
#include "Menu/Container/ContainerDetailHelpers.hpp"
#include "Menu/Framework/DetailInputLive.hpp"
#include "Menu/Framework/EquipmentDetails.hpp"
#include "Menu/Framework/EquipmentDetailsLive.hpp"
#include "Menu/Framework/FrameworkLive.hpp"
#include "Menu/Sounds.hpp"

extern "C" {
#include "Core/Diagnostics.h"
#include "Input/ControllerLiveState.h"
#include "Menu/Container/ItemView.h"
#include "Menu/Container/Storage.h"
#include "Menu/Framework/FrameworkLiveState.h"
}

namespace VS::Menu::container {
namespace {
constexpr std::size_t kFirstContainerCursor = 2;
constexpr std::size_t kContainerCursorFieldOffset = 0xA0;
static_assert(offsetof(Menu_Framework_MenuMemory, opaqueA0) ==
              kContainerCursorFieldOffset);
static_assert(std::size(Menu_Framework_menuMemory.opaqueA0) ==
              kFirstContainerCursor + Items::kItemCategoryCount * 2);

[[noreturn]] void Refuse(std::string_view reason) {
  Core_Errorf("menu.container", "Container details refused: %.*s",
              static_cast<int>(reason.size()), reason.data());
  std::abort();
}

Items::ItemCategory Category(int category) {
  if (category < 0 ||
      static_cast<std::size_t>(category) >= Items::kItemCategoryCount) {
    Refuse("detail category outside container categories");
  }
  return static_cast<Items::ItemCategory>(category);
}

void RequireRows() {
  if (Menu_Framework_menuItems == nullptr) {
    Refuse("missing current menu row workspace");
  }
}

class LiveWorld final : public Framework::LiveDetailInputWorld<DetailsWorld> {
 public:
  explicit LiveWorld(int category = -1) : category_(category) {}
  std::int32_t ItemCount() override {
    return Items::CountLiveContainerItems(
        category_, Menu_Container_containerData == nullptr
                       ? nullptr
                       : &Menu_Container_containerData->items);
  }
  void RememberPage(DetailCursor cursor) override {
    const auto category = static_cast<std::size_t>(Category(category_));
    const auto offset = kFirstContainerCursor + category * sizeof(DetailCursor);
    Menu_Framework_menuMemory.opaqueA0[offset] = static_cast<char>(cursor.row);
    Menu_Framework_menuMemory.opaqueA0[offset + 1] =
        static_cast<char>(cursor.page);
  }
  std::span<const std::uint16_t> DisplayOrder() override {
    return Items::LiveContainerCategoryOrder(Category(category_),
                                             Menu_Container_containerData);
  }
  [[noreturn]] void Refuse(std::string_view reason) override {
    container::Refuse(reason);
  }
  void ClearRows() override {
    RequireRows();
    Framework::ClearLiveMenuExcept(Framework::kNoRow);
  }
  Framework::MenuRow& Row(DetailRow row) override {
    RequireRows();
    if (row.index < 0 ||
        static_cast<std::size_t>(row.index) >= Framework::kRowCount) {
      Refuse("detail row outside current menu row workspace");
    }
    return Framework::LiveMenuRow(row.index);
  }
  void SetIconsVisibility(std::uint8_t visibility) override {
    Menu_Container_categoryIconsVisible = static_cast<char>(visibility);
  }
  void SetControlsVisibility(std::uint8_t visibility) override {
    Menu_Container_controlsPanelMode = static_cast<char>(visibility);
  }
  void ContinueTransition(DetailRow selected) override {
    RequireRows();
    Framework::ContinueLiveItemDetailsTransition(selected.index);
    if (selected.index < 0 ||
        static_cast<std::size_t>(selected.index) >= Framework::kRowCount) {
      Refuse("detail row outside current menu row workspace");
    }
  }
  void ShowStatPages(std::int32_t pages) override {
    Framework::UpdateLiveEquipmentStatPages(pages);
  }
  void MovePrimaryStats(PrimaryStatsMotion motion) override {
    Framework::UpdateLivePrimaryStatPanel(static_cast<int>(motion));
  }
  void PlayLeaveSound() override { Menu::PlayLeaveSound(); }
  void PlayInvalidSound() override { Menu::PlayInvalidSound(); }
  void DismissCommandRow() override {
    RequireRows();
    Framework::FlyOutLiveRowLeft(-1);
  }
  void SetPhase(DetailPhase phase) override {
    Menu_Container_detailsViewPhase = static_cast<char>(phase);
  }
  void RememberTriangleClose(bool triangle) override {
    Menu_Container_detailsClosedWithTriangle = triangle ? Input::Triangle : 0;
  }
  void HideDpPpBars() override {
    Framework::UpdateLiveDpPpBars(Framework::dp_pp_bars::kSlideOut);
  }

 private:
  int category_;
};
}  // namespace

// decomp: vs_menuD_initUiWeapon, MENU/MENUD.PRG/234.c:21; 80102A34.
void BuildLiveWeaponStats(Items_WeaponLoadout* target,
                          Items_InventoryWeapon* source,
                          Items_ContainerData* container) {
  Items::LiveContainerWorld records(container);
  auto native =
      target == nullptr ? Items::WeaponLoadout{} : Items::Live::Typed(*target);
  const auto weapon = Items::Live::TypedOrNone(source);
  const auto status =
      BuildWeaponStats(native, Items::Live::OrNull(weapon), records);
  if (target == nullptr) Refuse("missing weapon statistics target");
  Items::Live::WarnRefusal("BuildContainerWeaponStats", status);
  Items::Live::Commit(*target, native);
}

// decomp: vs_menuD_initUiShield, MENU/MENUD.PRG/234.c:57; 80102BB0.
void BuildLiveShieldStats(Items_ShieldLoadout* target,
                          Items_InventoryShield* source,
                          Items_ContainerData* container) {
  Items::LiveContainerWorld records(container);
  auto native =
      target == nullptr ? Items::ShieldLoadout{} : Items::Live::Typed(*target);
  const auto shield = Items::Live::TypedOrNone(source);
  BuildShieldStats(native, Items::Live::OrNull(shield), records);
  if (target == nullptr) Refuse("missing shield statistics target");
  Items::Live::Commit(*target, native);
}

// decomp: func_80103D50, MENU/MENUD.PRG/234.c:659; 80103D50.
void SetLivePanelVisibility(char visibility) {
  PanelVisibility panels;
  SetPanelVisibility(panels, static_cast<std::uint8_t>(visibility));
  Menu_Container_categoryTitleVisible = static_cast<char>(panels.title);
  Menu_Container_categoryIconsVisible = static_cast<char>(panels.icons);
  Menu_Container_controlsPanelMode = static_cast<char>(panels.controls);
}

// decomp: _pollItemNavigation, MENU/MENUD.PRG/234.c:666; 80103D6C.
int PollLiveDetailsPaging(int category, int page) {
  LiveWorld world(category);
  return PollDetailsPaging(world, {page}).index;
}

// decomp: func_80103F64, MENU/MENUD.PRG/234.c:735; 80103F64.
void BeginLiveDetailsTransition(int selected_row) {
  LiveWorld world;
  BeginDetailsTransition(world, {selected_row});
}

// decomp: func_80104034, MENU/MENUD.PRG/234.c:767; 80104034.
void InitializeLiveDetailsLayout(int selected_row, int stat_pages) {
  LiveWorld world;
  InitializeDetailsLayout(world, {selected_row}, stat_pages);
}

// decomp: func_80104114, MENU/MENUD.PRG/234.c:789; 80104114.
int SelectLiveDetailsItem(int category, int page) {
  LiveWorld world(category);
  return SelectDetailsItem(world, {page});
}

// decomp: func_80104170, MENU/MENUD.PRG/234.c:796; 80104170.
void ExitLiveDetailsView(int hide_dp_pp) {
  LiveWorld world;
  ExitDetailsView(world, hide_dp_pp != 0);
}
}  // namespace VS::Menu::container
