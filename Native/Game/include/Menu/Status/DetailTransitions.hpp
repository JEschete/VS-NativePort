#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

#include "Items/ItemRecords.hpp"
#include "Menu/Framework/EquipmentDetails.hpp"
#include "Menu/Framework/Framework.hpp"
#include "Menu/Framework/ItemRows.hpp"

namespace VS::Menu::Status {  // NOLINT(readability-identifier-naming)
struct DetailNavigation {
  std::uint32_t selected = 0;
  std::uint32_t flags = 0;
  std::uint32_t repeat = 0;
  std::uint8_t stat_page = 0;
  std::span<const Framework::StatusViewMoves> links;
};

class DetailTransitionHost {
 public:
  virtual ~DetailTransitionHost() = default;
  virtual void Refuse(std::string_view reason) = 0;
  [[nodiscard]] virtual std::span<Framework::MenuRow> Rows() = 0;
  [[nodiscard]] virtual std::span<const std::uint8_t> StatusText() = 0;
  virtual Framework::MenuRow* CreateTitle(
      const Framework::RowPlacement& placement,
      std::span<const std::uint8_t> text) = 0;
  virtual void FlyOutRight(int row) = 0;
  virtual void SetStatPages(int command) = 0;
  virtual void AnimatePrimaryStats(int command) = 0;
  virtual void PlayLeaveSound() = 0;
  virtual void ClearRowsExcept(int row) = 0;
  virtual void FlyOutLeft(int row) = 0;
  virtual void AnimateDpPpBars(int command) = 0;
};

// Invalid links and cycles without a selectable element are refused.
// decomp: _updateEquipmentDetailSelection (src/MENU/MENU4.PRG/120.c:462)
[[nodiscard]] std::optional<std::uint8_t> NavigateDetailElements(
    const DetailNavigation& input);
// decomp: _setActiveRow (src/MENU/MENU4.PRG/120.c:1299)
[[nodiscard]] bool HoistEquipmentRow(DetailTransitionHost& host, int row);
// decomp: _animateEquipmentDetailTransition (src/MENU/MENU4.PRG/120.c:1318)
[[nodiscard]] bool BeginDetailTransition(DetailTransitionHost& host, int row);
// decomp: _initUi (src/MENU/MENU4.PRG/120.c:1348)
[[nodiscard]] bool InitializeDetailPanel(DetailTransitionHost& host, int row,
                                         int class_affinity);
// These rows borrow their loadout and text; no inventory owner is changed.
// decomp: _setWeaponRow (src/MENU/MENU4.PRG/120.c:1358)
Framework::MenuRow* SetWeaponComponentRow(Framework::ItemRowsHost& host,
                                          int row,
                                          const Items::WeaponLoadout& weapon,
                                          bool slide_in);
// decomp: _setShieldRow (src/MENU/MENU4.PRG/120.c:1418)
Framework::MenuRow* SetShieldGemRow(Framework::ItemRowsHost& host, int row,
                                    const Items::ShieldLoadout& shield,
                                    bool slide_in);
// decomp: _exitEquipmentDetail (src/MENU/MENU4.PRG/120.c:1463)
void ExitEquipmentDetails(DetailTransitionHost& host, bool clear_dp_pp);
}  // namespace VS::Menu::Status
