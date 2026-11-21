#include "Menu/Container/ContainerDetailHelpersLive.hpp"

#include <cstddef>
#include <cstdlib>
#include <optional>
#include <string_view>

#include "Core/Diagnostics.hpp"
#include "Input/ControllerLiveState.hpp"
#include "Input/Pad.hpp"
#include "Items/ContainerIndexingLive.hpp"
#include "Items/ContainerTransfersLive.hpp"
#include "Items/ItemRecordsLive.hpp"
#include "Menu/Container/ContainerDetailHelpers.hpp"
#include "Menu/Container/ItemView.hpp"
#include "Menu/Container/Storage.hpp"
#include "Menu/Framework/DetailInputLive.hpp"
#include "Menu/Framework/EquipmentDetails.hpp"
#include "Menu/Framework/EquipmentDetailsLive.hpp"
#include "Menu/Framework/FrameworkLive.hpp"
#include "Menu/Framework/FrameworkLiveState.hpp"
#include "Menu/Sounds.hpp"

namespace VS::Menu::container {
namespace {
static_assert(Framework::kCategoryCursorCount == Items::kItemCategoryCount);

[[noreturn]] void Refuse(std::string_view reason) {
  VS::Core::Errorf("menu.container", "Container details refused: %.*s",
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
  if (VS::Menu::Framework::menuItems == nullptr) {
    Refuse("missing current menu row workspace");
  }
}

class LiveWorld final : public Framework::LiveDetailInputWorld<DetailsWorld> {
 public:
  explicit LiveWorld(int category = -1) : category_(category) {}
  std::int32_t ItemCount() override {
    return Items::CountLiveContainerItems(
        category_, containerData == nullptr ? nullptr : &containerData->items);
  }
  void RememberPage(DetailCursor cursor) override {
    const auto category = static_cast<std::size_t>(Category(category_));
    VS::Menu::Framework::menuMemory.container_cursors.positions[category] = {
        .row = cursor.row, .page = cursor.page};
  }
  std::span<const std::uint16_t> DisplayOrder() override {
    return Items::LiveContainerCategoryOrder(Category(category_),
                                             containerData);
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
    categoryIconsVisible = static_cast<std::uint8_t>(visibility);
  }
  void SetControlsVisibility(std::uint8_t visibility) override {
    controlsPanelMode = static_cast<std::uint8_t>(visibility);
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
    detailsViewPhase = static_cast<std::uint8_t>(phase);
  }
  void RememberTriangleClose(bool triangle) override {
    detailsClosedWithTriangle = triangle ? Input::Triangle : 0;
  }
  void HideDpPpBars() override {
    Framework::UpdateLiveDpPpBars(Framework::dp_pp_bars::kSlideOut);
  }

 private:
  int category_;
};
}  // namespace

// decomp: vs_menuD_initUiWeapon, src/MENU/MENUD.PRG/234.c:25; 80102A34.
void BuildLiveWeaponStats(VS::Items::WeaponLoadout* target,
                          const VS::Items::InventoryWeapon* source,
                          VS::Items::ContainerData* container) {
  Items::LiveContainerWorld records(container);
  auto native = target == nullptr ? Items::WeaponLoadout{} : *target;
  const auto status = BuildWeaponStats(native, source, records);
  if (target == nullptr) Refuse("missing weapon statistics target");
  Items::Live::WarnRefusal("BuildContainerWeaponStats", status);
  *target = native;
}

// decomp: vs_menuD_initUiShield, src/MENU/MENUD.PRG/234.c:61; 80102BB0.
void BuildLiveShieldStats(VS::Items::ShieldLoadout* target,
                          const VS::Items::InventoryShield* source,
                          VS::Items::ContainerData* container) {
  Items::LiveContainerWorld records(container);
  auto native = target == nullptr ? Items::ShieldLoadout{} : *target;
  BuildShieldStats(native, source, records);
  if (target == nullptr) Refuse("missing shield statistics target");
  *target = native;
}

// decomp: func_80103D50, src/MENU/MENUD.PRG/234.c:660; 80103D50.
void SetLivePanelVisibility(char visibility) {
  PanelVisibility panels;
  SetPanelVisibility(panels, static_cast<std::uint8_t>(visibility));
  categoryTitleVisible = static_cast<std::uint8_t>(panels.title);
  categoryIconsVisible = static_cast<std::uint8_t>(panels.icons);
  controlsPanelMode = static_cast<std::uint8_t>(panels.controls);
}

// decomp: pollItemNavigation, src/MENU/MENUD.PRG/234.c:667; was
// _pollItemNavigation; 80103D6C.
int PollLiveDetailsPaging(int category, int page) {
  LiveWorld world(category);
  return PollDetailsPaging(world, {page}).index;
}

// decomp: func_80103F64, src/MENU/MENUD.PRG/234.c:736; 80103F64.
void BeginLiveDetailsTransition(int selected_row) {
  LiveWorld world;
  BeginDetailsTransition(world, {selected_row});
}

// decomp: func_80104034, src/MENU/MENUD.PRG/234.c:768; 80104034.
void InitializeLiveDetailsLayout(int selected_row, int stat_pages) {
  LiveWorld world;
  InitializeDetailsLayout(world, {selected_row}, stat_pages);
}

// decomp: func_80104114, src/MENU/MENUD.PRG/234.c:790; 80104114.
int SelectLiveDetailsItem(int category, int page) {
  LiveWorld world(category);
  return SelectDetailsItem(world, {page});
}

// decomp: func_80104170, src/MENU/MENUD.PRG/234.c:797; 80104170.
void ExitLiveDetailsView(int hide_dp_pp) {
  LiveWorld world;
  ExitDetailsView(world, hide_dp_pp != 0);
}
}  // namespace VS::Menu::container
