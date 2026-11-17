#include "Menu/Status/DetailTransitionsLive.hpp"

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

#include "Core/Log.hpp"
#include "Items/ItemRecordsLive.hpp"
#include "Menu/Framework/EquipmentDetailsLive.hpp"
#include "Menu/Framework/FrameworkLive.hpp"
#include "Menu/Framework/ItemRowsLiveHost.hpp"
#include "Menu/Sounds.hpp"
#include "Menu/Status/DetailTransitions.hpp"

extern "C" {
#include "Input/ControllerLiveState.h"
#include "Menu/Framework/EquipmentDetailsLiveState.h"
#include "Menu/Framework/FrameworkLiveState.h"
#include "Menu/Status/ActorStatus.h"
}

namespace VS::Menu::Status {  // NOLINT(readability-identifier-naming)
namespace {
void Refuse(std::string_view reason) {
  VS_LOG_WARN("menu.status", "{}", reason);
}

bool RowsAvailable() {
  if (Menu_Framework_menuItems == nullptr) {
    Refuse("status transition: missing menu workspace");
    return false;
  }
  return true;
}

class LiveTransitionHost final : public DetailTransitionHost {
 public:
  void Refuse(std::string_view reason) override { Status::Refuse(reason); }
  std::span<Framework::MenuRow> Rows() override {
    return Menu_Framework_menuItems == nullptr
               ? std::span<Framework::MenuRow>{}
               : std::span{&Framework::LiveMenuRow(0), Framework::kRowCount};
  }
  std::span<const std::uint8_t> StatusText() override {
    const auto bytes = std::as_bytes(std::span(Menu_Status_statusStrings));
    return {reinterpret_cast<const std::uint8_t*>(bytes.data()), bytes.size()};
  }
  Framework::MenuRow* CreateTitle(const Framework::RowPlacement& placement,
                                  std::span<const std::uint8_t> text) override {
    if (!RowsAvailable()) {
      return nullptr;
    }
    return &Framework::SetLiveMenuItemText(placement, text);
  }
  void FlyOutRight(int row) override { Framework::FlyOutLiveRowRight(row); }
  void SetStatPages(int command) override {
    Framework::UpdateLiveEquipmentStatPages(command);
  }
  void AnimatePrimaryStats(int command) override {
    Framework::UpdateLivePrimaryStatPanel(command);
  }
  void PlayLeaveSound() override { Menu::PlayLeaveSound(); }
  void ClearRowsExcept(int row) override {
    Framework::ClearLiveMenuExcept(row);
  }
  void FlyOutLeft(int row) override { Framework::FlyOutLiveRowLeft(row); }
  void AnimateDpPpBars(int command) override {
    Framework::UpdateLiveDpPpBars(command);
  }
};
}  // namespace

// decomp: _updateEquipmentDetailSelection (src/MENU/MENU4.PRG/120.c:462)
int NavigateLiveDetailElements(unsigned int selected, int flags) {
  Framework::StatusViewNavigation links{};
  for (std::size_t row = 0; row < links.size(); ++row) {
    std::ranges::copy(Menu_Framework_statusViewNavigationMap[row],
                      links[row].begin());
  }
  const auto result = NavigateDetailElements(
      {.selected = selected,
       .flags = static_cast<std::uint32_t>(flags),
       .repeat = static_cast<std::uint32_t>(Input_buttonRepeat),
       .stat_page = static_cast<std::uint8_t>(Menu_Framework_itemStatPage),
       .links = links});
  if (!result) {
    Refuse("status transition: invalid or cyclic detail navigation");
    return std::bit_cast<int>(selected);
  }
  return *result;
}

// decomp: _setActiveRow (src/MENU/MENU4.PRG/120.c:1299)
void HoistLiveEquipmentRow(int row) {
  LiveTransitionHost host;
  static_cast<void>(HoistEquipmentRow(host, row));
}

// decomp: _animateEquipmentDetailTransition (src/MENU/MENU4.PRG/120.c:1318)
void BeginLiveDetailTransition(int row) {
  LiveTransitionHost host;
  static_cast<void>(BeginDetailTransition(host, row));
}

// decomp: _initUi (src/MENU/MENU4.PRG/120.c:1348)
void InitializeLiveDetailPanel(int row, int class_affinity) {
  LiveTransitionHost host;
  static_cast<void>(InitializeDetailPanel(host, row, class_affinity));
}

// decomp: _setWeaponRow (src/MENU/MENU4.PRG/120.c:1358)
void SetLiveWeaponComponentRow(int row, Items_WeaponLoadout* weapon, int init) {
  if (weapon == nullptr) {
    Refuse("status transition: missing weapon");
    return;
  }
  if (!RowsAvailable()) {
    return;
  }
  Framework::LiveItemRowsHost host;
  static_cast<void>(SetWeaponComponentRow(
      host, row, Items::Live::Typed(*weapon), (init & 1) != 0));
}

// decomp: _setShieldRow (src/MENU/MENU4.PRG/120.c:1418)
void SetLiveShieldGemRow(int row, Items_ShieldLoadout* shield, int init) {
  if (shield == nullptr) {
    Refuse("status transition: missing shield");
    return;
  }
  if (!RowsAvailable()) {
    return;
  }
  Framework::LiveItemRowsHost host;
  static_cast<void>(
      SetShieldGemRow(host, row, Items::Live::Typed(*shield), (init & 1) != 0));
}

// decomp: _exitEquipmentDetail (src/MENU/MENU4.PRG/120.c:1463)
void ExitLiveEquipmentDetails(int clear_dp_pp) {
  if (!RowsAvailable()) {
    return;
  }
  LiveTransitionHost host;
  ExitEquipmentDetails(host, clear_dp_pp != 0);
}
}  // namespace VS::Menu::Status
