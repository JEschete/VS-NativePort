#include "Menu/Inventory/EquipmentDetailControllersLive.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iterator>
#include <optional>
#include <span>

#include "Battle/Actor.hpp"
#include "Core/Diagnostics.hpp"
#include "Input/ControllerLiveState.hpp"
#include "Items/InventoryLive.hpp"
#include "Items/ItemRecords.hpp"
#include "Items/ItemRecordsLive.hpp"
#include "Menu/Framework/EquipmentDetails.hpp"
#include "Menu/Framework/EquipmentDetailsLive.hpp"
#include "Menu/Framework/EquipmentDetailsLiveState.hpp"
#include "Menu/Framework/FrameworkLive.hpp"
#include "Menu/Framework/FrameworkLiveState.hpp"
#include "Menu/Framework/ItemRows.hpp"
#include "Menu/Framework/ItemRowsLive.hpp"
#include "Menu/Framework/ItemRowsLiveState.hpp"
#include "Menu/Inventory/DetailsPoliciesLive.hpp"
#include "Menu/Inventory/EquipmentDetailControllers.hpp"
#include "Menu/Inventory/ItemDetailLiveSupport.hpp"
#include "Render/Frame.hpp"
#include "Render/GameFontLive.hpp"
#include "Render/Render.hpp"
#include "Render/UiPrimitives.hpp"

namespace VS::Menu::Inventory {
namespace {
using Category = Items::ItemCategory;
constexpr std::size_t kControllerCount = 4;
constexpr std::size_t kShieldControllerIndex = 3;
constexpr std::size_t kGemDigit = 6;
constexpr int kSelectionRowShift = 8;
constexpr int kFirstWeaponGemRow = 3;
constexpr int kLastWeaponGemRow = 5;
constexpr int kLastShieldGemRow = 3;
constexpr int kGemCountIcon = 22 | (1 << 8);
constexpr int kGemIconX = 256;
constexpr int kGemIconY = 32;
constexpr int kGemTextX = 280;
constexpr int kGemTextY = 36;
constexpr std::uint8_t kDigitZero = 0x30;
std::array<EquipmentDetailState, kControllerCount> detailStates{};
std::array gemCountText{'X', ' ', ' ', ' ', ' ', ' ', '0', '\0'};

[[noreturn]] void Refuse(const char* reason) {
  VS::Core::Errorf("menu.inventory", "Equipment details refused: %s", reason);
  std::abort();
}

void RequireRows() {
  if (VS::Menu::Framework::menuItems == nullptr) {
    Refuse("missing menu row workspace");
  }
}

template <typename Record, std::size_t Count>
Record& Referenced(std::array<Record, Count>& table, std::uint32_t reference) {
  if (reference == 0 || reference > Count) {
    Refuse("equipment reference outside its inventory table");
  }
  return table[reference - 1];
}

template <typename Record, std::size_t Count>
void RequireBoundTable(const Record* bound,
                       const std::array<Record, Count>& table) {
  if (bound != table.data()) {
    Refuse("equipment table is not the current inventory");
  }
}

void RequireText(bool names, bool buffer) {
  if (VS::Menu::Framework::itemHelp == nullptr) {
    Refuse("missing item help text");
  }
  if (names && VS::Menu::Framework::itemNames == nullptr) {
    Refuse("missing item name text");
  }
  if (buffer && VS::Menu::Framework::stringBuf == nullptr) {
    Refuse("missing item description workspace");
  }
}

void RequireGem(std::uint8_t reference) {
  if (reference == 0) {
    return;
  }
  RequireBoundTable(VS::Menu::Framework::gems, VS::Items::inventory.gems);
  (void)Referenced(VS::Items::inventory.gems, reference);
}

void RequireWeaponParts(const VS::Items::InventoryWeapon& weapon) {
  RequireBoundTable(VS::Menu::Framework::blades, VS::Items::inventory.blades);
  RequireBoundTable(VS::Menu::Framework::grips, VS::Items::inventory.grips);
  (void)Referenced(VS::Items::inventory.blades, weapon.blade);
  (void)Referenced(VS::Items::inventory.grips, weapon.grip);
  for (std::size_t gem = 0; gem < Items::kWeaponGemCount; ++gem) {
    RequireGem(weapon.gems[gem]);
  }
}

void RequireAshleyStats() {
  if (VS::Battle::characterState == nullptr ||
      VS::Battle::characterState->stats == nullptr) {
    Refuse("missing current equipped actor statistics");
  }
}

std::optional<DetailSelection> Selection(int item_info) {
  if (item_info == 0) {
    return std::nullopt;
  }
  const auto command = static_cast<std::uint32_t>(item_info);
  return DetailSelection{
      DetailRowId{static_cast<std::uint8_t>(command >> kSelectionRowShift)},
      DetailPage{static_cast<std::uint8_t>(command - 1)}};
}

std::span<char> TextView(char* text, std::span<char> weapon_name = {}) {
  if (text == nullptr) {
    Refuse("formatter did not supply a used row text");
  }
  if (auto view = FindLiveItemDetailText(text, weapon_name); !view.empty()) {
    return view;
  }
  Refuse("formatter row text is outside its current owners");
}

class LiveWorld final : public LiveDetailWorld<EquipmentDetailWorld> {
 public:
  explicit LiveWorld(Category category)
      : LiveDetailWorld(RequireRows), category_(category) {}

  InventorySlot CurrentItem(DetailPage page) override {
    const auto order = CurrentOrder();
    if (page.index < 0 ||
        static_cast<std::size_t>(page.index) >= order.size()) {
      Refuse("current equipment page outside its inventory display order");
    }
    return InventorySlot{order[static_cast<std::size_t>(page.index)]};
  }
  void SetStatistics(InventorySlot slot) override {
    const int reference = slot.one_based;
    switch (category_) {
      case Category::kWeapon:
        if (reference != 0) {
          RequireBoundTable(VS::Menu::Framework::weapons,
                            VS::Items::inventory.weapons);
          const auto& weapon =
              Referenced(VS::Items::inventory.weapons, reference);
          if (weapon.is_equipped != 0) {
            RequireAshleyStats();
          } else {
            RequireWeaponParts(weapon);
          }
        }
        Framework::SetLiveWeaponStats(reference);
        break;
      case Category::kBlade:
        RequireBoundTable(VS::Menu::Framework::blades,
                          VS::Items::inventory.blades);
        (void)Referenced(VS::Items::inventory.blades, reference);
        Framework::SetLiveBladeStats(reference);
        break;
      case Category::kGrip:
        RequireBoundTable(VS::Menu::Framework::grips,
                          VS::Items::inventory.grips);
        (void)Referenced(VS::Items::inventory.grips, reference);
        Framework::SetLiveGripStats(reference);
        break;
      case Category::kShield:
        if (reference != 0) {
          RequireBoundTable(VS::Menu::Framework::shields,
                            VS::Items::inventory.shields);
          const auto& shield =
              Referenced(VS::Items::inventory.shields, reference);
          if (shield.is_equipped != 0) {
            RequireAshleyStats();
          } else {
            for (std::size_t gem = 0; gem < Items::kWeaponGemCount; ++gem) {
              RequireGem(shield.gems[gem]);
            }
          }
        }
        Framework::SetLiveShieldStats(reference);
        break;
      default:
        Refuse("unsupported equipment detail category");
    }
  }
  void OpenDpPpBars() override {
    Framework::UpdateLiveDpPpBars(Framework::dp_pp_bars::kBoth);
  }
  void CreateComponentRow(std::int32_t row, InventorySlot slot,
                          bool slide_in) override {
    RequireRows();
    if (category_ == Category::kWeapon) {
      RequireWeaponRow(row, slot);
      (void)Framework::CreateLiveWeaponComponentRow(row, slot.one_based,
                                                    slide_in ? 1 : 0);
    } else if (category_ == Category::kShield) {
      RequireShieldRow(row, slot);
      (void)Framework::CreateLiveShieldGemRow(row, slot.one_based,
                                              slide_in ? 1 : 0);
    } else {
      Refuse("component rows requested for a component-only item");
    }
  }
  void Exit(bool clear_dp_pp) override {
    ExitLiveDetailsView(clear_dp_pp ? 1 : 0);
  }
  DetailPage PollPaging(DetailPage page) override {
    return DetailPage{
        PollLiveDetailsPaging(static_cast<int>(category_), page.index)};
  }
  InventorySlot SelectItem(DetailPage page) override {
    return InventorySlot{static_cast<std::uint8_t>(
        SelectLiveDetailsItem(static_cast<int>(category_), page.index))};
  }
  DetailRowView FormatItem(InventorySlot slot) override {
    std::array<char*, 2> text{};
    int row_type = 0;
    std::span<char> weapon_name;
    RequireText(category_ != Category::kGrip, true);
    switch (category_) {
      case Category::kWeapon: {
        auto& weapon = Referenced(VS::Items::inventory.weapons, slot.one_based);
        RequireBoundTable(VS::Menu::Framework::weapons,
                          VS::Items::inventory.weapons);
        RequireWeaponParts(weapon);
        weapon_name = weapon.name;
        Framework::FormatLiveInventoryWeaponRow(&weapon, text.data(), &row_type,
                                                VS::Menu::Framework::stringBuf);
        break;
      }
      case Category::kBlade:
        Framework::FormatLiveBladeRow(
            &Referenced(VS::Items::inventory.blades, slot.one_based),
            text.data(), &row_type, VS::Menu::Framework::stringBuf);
        break;
      case Category::kGrip:
        Framework::FormatLiveGripRow(
            &Referenced(VS::Items::inventory.grips, slot.one_based),
            text.data(), &row_type, VS::Menu::Framework::stringBuf);
        break;
      case Category::kShield: {
        auto& shield = Referenced(VS::Items::inventory.shields, slot.one_based);
        RequireBoundTable(VS::Menu::Framework::shields,
                          VS::Items::inventory.shields);
        for (std::size_t gem = 0; gem < Items::kWeaponGemCount; ++gem) {
          RequireGem(shield.gems[gem]);
        }
        Framework::FormatLiveInventoryShieldRow(&shield, text.data(), &row_type,
                                                VS::Menu::Framework::stringBuf);
        break;
      }
      default:
        Refuse("unsupported equipment row formatter");
    }
    return {.name = TextView(text[0], weapon_name),
            .description = TextView(text[1]),
            .row_type = static_cast<std::uint32_t>(row_type)};
  }
  void ChangePage(DetailRowId row, const DetailRowView& text,
                  DetailPage page) override {
    RequireRows();
    if (row.index < 0 ||
        static_cast<std::size_t>(row.index) >= Framework::kRowCount) {
      Refuse("equipment selection row outside its menu workspace");
    }
    std::array<char*, 2> legacy_text{text.name.data(), text.description.data()};
    Framework::UpdateLiveItemDetailsSelection(row.index, legacy_text.data(),
                                              text.row_type, page.index);
  }
  std::uint8_t GripGemSlots(InventorySlot slot) override {
    return Referenced(VS::Items::inventory.grips, slot.one_based).gem_slots;
  }
  void DrawGripGemSlotCount(std::uint8_t slots) override {
    gemCountText[kGemDigit] =
        static_cast<char>(static_cast<std::uint8_t>(slots + kDigitZero));
    if (VS::Render::FrameScratchpad().primitive_cursor == nullptr ||
        VS::Render::FrameScratchpad().overlay_ordering_table == nullptr) {
      Refuse("missing current equipment presentation frame");
    }
    Framework::DrawLiveRowIcon(kGemCountIcon, kGemIconX, kGemIconY);
    Render::DrawLiveCompactText(
        gemCountText.data(),
        static_cast<int>(Framework::PackXy(kGemTextX, kGemTextY)), nullptr);
  }

 private:
  std::span<const std::uint8_t> CurrentOrder() const {
    switch (category_) {
      case Category::kWeapon:
        return VS::Items::inventoryIndices.weapons;
      case Category::kBlade:
        return VS::Items::inventoryIndices.blades;
      case Category::kGrip:
        return VS::Items::inventoryIndices.grips;
      case Category::kShield:
        return VS::Items::inventoryIndices.shields;
      default:
        Refuse("unsupported equipment display order");
    }
  }
  void RequireWeaponRow(int row, InventorySlot slot) const {
    RequireBoundTable(VS::Menu::Framework::weapons,
                      VS::Items::inventory.weapons);
    const auto& weapon =
        Referenced(VS::Items::inventory.weapons, slot.one_based);
    if (row == 1) {
      RequireBoundTable(VS::Menu::Framework::blades,
                        VS::Items::inventory.blades);
      (void)Referenced(VS::Items::inventory.blades, weapon.blade);
      RequireText(true, true);
      return;
    }
    RequireBoundTable(VS::Menu::Framework::grips, VS::Items::inventory.grips);
    const auto& grip = Referenced(VS::Items::inventory.grips, weapon.grip);
    if (row == 2) {
      RequireText(false, true);
    } else if (row >= kFirstWeaponGemRow && row <= kLastWeaponGemRow &&
               row - kFirstWeaponGemRow < grip.gem_slots) {
      RequireGem(
          weapon.gems[static_cast<std::size_t>(row - kFirstWeaponGemRow)]);
      RequireText(false, false);
    }
  }
  void RequireShieldRow(int row, InventorySlot slot) const {
    RequireBoundTable(VS::Menu::Framework::shields,
                      VS::Items::inventory.shields);
    const auto& shield =
        Referenced(VS::Items::inventory.shields, slot.one_based);
    if (row >= 1 && row <= kLastShieldGemRow &&
        row - 1 < shield.base.gem_slots) {
      RequireGem(shield.gems[static_cast<std::size_t>(row - 1)]);
      RequireText(false, false);
    }
  }
  Category category_;
};
}  // namespace

// decomp: _weaponStatusView (src/MENU/items1.h:98-185).
int UpdateLiveWeaponDetails(int item_info) {
  LiveWorld world(Category::kWeapon);
  return UpdateWeaponDetails(detailStates[0], world, Selection(item_info));
}

// decomp: _bladeStatusView (src/MENU/items1.h:195-265).
int UpdateLiveBladeDetails(int item_info) {
  LiveWorld world(Category::kBlade);
  return UpdateBladeDetails(detailStates[1], world, Selection(item_info));
}

// decomp: _gripStatusView (src/MENU/items1.h:275-355).
int UpdateLiveGripDetails(int item_info) {
  LiveWorld world(Category::kGrip);
  return UpdateGripDetails(detailStates[2], world, Selection(item_info));
}

// decomp: _shieldStatusView (src/MENU/items1.h:365-454).
int UpdateLiveShieldDetails(int item_info) {
  LiveWorld world(Category::kShield);
  return UpdateShieldDetails(detailStates[kShieldControllerIndex], world,
                             Selection(item_info));
}

void ResetLiveEquipmentDetailControllers() {
  detailStates = {};
  gemCountText = {'X', ' ', ' ', ' ', ' ', ' ', '0', '\0'};
}
}  // namespace VS::Menu::Inventory
