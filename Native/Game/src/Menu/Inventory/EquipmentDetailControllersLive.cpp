#include "Menu/Inventory/EquipmentDetailControllersLive.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iterator>
#include <optional>
#include <span>

#include "Items/ItemRecords.hpp"
#include "Items/ItemRecordsLive.hpp"
#include "Menu/Framework/EquipmentDetails.hpp"
#include "Menu/Framework/EquipmentDetailsLive.hpp"
#include "Menu/Framework/FrameworkLive.hpp"
#include "Menu/Framework/ItemRows.hpp"
#include "Menu/Framework/ItemRowsLive.hpp"
#include "Menu/Inventory/DetailsPoliciesLive.hpp"
#include "Menu/Inventory/EquipmentDetailControllers.hpp"
#include "Menu/Inventory/ItemDetailLiveSupport.hpp"
#include "Render/GameFontLive.hpp"

extern "C" {
#include "Audio/Seq/Channel.h"
#include "Battle/Actor.h"
#include "Core/Diagnostics.h"
#include "Input/ControllerLiveState.h"
#include "Menu/Framework/EquipmentDetailsLiveState.h"
#include "Menu/Framework/FrameworkLiveState.h"
#include "Menu/Framework/ItemRowsLiveState.h"
#include "Render/Frame.h"
#include "Render/Render.h"
#include "Render/UiPrimitives.h"
}

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
std::array<EquipmentDetailState, kControllerCount> g_states{};
std::array g_gem_count_text{'X', ' ', ' ', ' ', ' ', ' ', '0', '\0'};

[[noreturn]] void Refuse(const char* reason) {
  Core_Errorf("menu.inventory", "Equipment details refused: %s", reason);
  std::abort();
}

void RequireRows() {
  if (Menu_Framework_menuItems == nullptr) {
    Refuse("missing menu row workspace");
  }
}

template <typename Record, std::size_t Count>
Record& Referenced(Record (&table)[Count], std::uint32_t reference) {
  if (reference == 0 || reference > Count) {
    Refuse("equipment reference outside its inventory table");
  }
  return table[reference - 1];
}

template <typename Record, std::size_t Count>
void RequireBoundTable(const Record* bound, Record (&table)[Count]) {
  if (bound != table) {
    Refuse("equipment table is not the current inventory");
  }
}

void RequireText(bool names, bool buffer) {
  if (Menu_Framework_itemHelp == nullptr) {
    Refuse("missing item help text");
  }
  if (names && Menu_Framework_itemNames == nullptr) {
    Refuse("missing item name text");
  }
  if (buffer && Menu_Framework_stringBuf == nullptr) {
    Refuse("missing item description workspace");
  }
}

void RequireGem(std::uint8_t reference) {
  if (reference == 0) {
    return;
  }
  RequireBoundTable(Menu_Framework_gems, Audio_Seq_unusedInventory.gems);
  (void)Referenced(Audio_Seq_unusedInventory.gems, reference);
}

void RequireWeaponParts(const Items_InventoryWeapon& weapon) {
  RequireBoundTable(Menu_Framework_blades, Audio_Seq_unusedInventory.blades);
  RequireBoundTable(Menu_Framework_grips, Audio_Seq_unusedInventory.grips);
  (void)Referenced(Audio_Seq_unusedInventory.blades, weapon.blade);
  (void)Referenced(Audio_Seq_unusedInventory.grips, weapon.grip);
  for (std::size_t gem = 0; gem < Items::kWeaponGemCount; ++gem) {
    RequireGem(weapon.gems[gem]);
  }
}

void RequireAshleyStats() {
  if (Battle_characterState == nullptr ||
      Battle_characterState->stats == nullptr) {
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
          RequireBoundTable(Menu_Framework_weapons,
                            Audio_Seq_unusedInventory.weapons);
          const auto& weapon =
              Referenced(Audio_Seq_unusedInventory.weapons, reference);
          if (weapon.isEquipped != 0) {
            RequireAshleyStats();
          } else {
            RequireWeaponParts(weapon);
          }
        }
        Framework::SetLiveWeaponStats(reference);
        break;
      case Category::kBlade:
        RequireBoundTable(Menu_Framework_blades,
                          Audio_Seq_unusedInventory.blades);
        (void)Referenced(Audio_Seq_unusedInventory.blades, reference);
        Framework::SetLiveBladeStats(reference);
        break;
      case Category::kGrip:
        RequireBoundTable(Menu_Framework_grips,
                          Audio_Seq_unusedInventory.grips);
        (void)Referenced(Audio_Seq_unusedInventory.grips, reference);
        Framework::SetLiveGripStats(reference);
        break;
      case Category::kShield:
        if (reference != 0) {
          RequireBoundTable(Menu_Framework_shields,
                            Audio_Seq_unusedInventory.shields);
          const auto& shield =
              Referenced(Audio_Seq_unusedInventory.shields, reference);
          if (shield.isEquipped != 0) {
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
        auto& weapon =
            Referenced(Audio_Seq_unusedInventory.weapons, slot.one_based);
        RequireBoundTable(Menu_Framework_weapons,
                          Audio_Seq_unusedInventory.weapons);
        RequireWeaponParts(weapon);
        weapon_name = weapon.name;
        Framework::FormatLiveInventoryWeaponRow(&weapon, text.data(), &row_type,
                                                Menu_Framework_stringBuf);
        break;
      }
      case Category::kBlade:
        Framework::FormatLiveBladeRow(
            &Referenced(Audio_Seq_unusedInventory.blades, slot.one_based),
            text.data(), &row_type, Menu_Framework_stringBuf);
        break;
      case Category::kGrip:
        Framework::FormatLiveGripRow(
            &Referenced(Audio_Seq_unusedInventory.grips, slot.one_based),
            text.data(), &row_type, Menu_Framework_stringBuf);
        break;
      case Category::kShield: {
        auto& shield =
            Referenced(Audio_Seq_unusedInventory.shields, slot.one_based);
        RequireBoundTable(Menu_Framework_shields,
                          Audio_Seq_unusedInventory.shields);
        for (std::size_t gem = 0; gem < Items::kWeaponGemCount; ++gem) {
          RequireGem(shield.gems[gem]);
        }
        Framework::FormatLiveInventoryShieldRow(&shield, text.data(), &row_type,
                                                Menu_Framework_stringBuf);
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
    return Items::Live::Typed(
               Referenced(Audio_Seq_unusedInventory.grips, slot.one_based))
        .gem_slots;
  }
  void DrawGripGemSlotCount(std::uint8_t slots) override {
    g_gem_count_text[kGemDigit] =
        static_cast<char>(static_cast<std::uint8_t>(slots + kDigitZero));
    if (Render_frameScratch.primitiveCursor == nullptr ||
        Render_frameScratch.overlayOrderingTable == nullptr) {
      Refuse("missing current equipment presentation frame");
    }
    Framework::DrawLiveRowIcon(kGemCountIcon, kGemIconX, kGemIconY);
    Render::DrawLiveCompactText(
        g_gem_count_text.data(),
        static_cast<int>(Framework::PackXy(kGemTextX, kGemTextY)), nullptr);
  }

 private:
  std::span<const std::uint8_t> CurrentOrder() const {
    switch (category_) {
      case Category::kWeapon:
        return Audio_Seq_unusedInventoryIndices.weapons;
      case Category::kBlade:
        return Audio_Seq_unusedInventoryIndices.blades;
      case Category::kGrip:
        return Audio_Seq_unusedInventoryIndices.grips;
      case Category::kShield:
        return Audio_Seq_unusedInventoryIndices.shields;
      default:
        Refuse("unsupported equipment display order");
    }
  }
  void RequireWeaponRow(int row, InventorySlot slot) const {
    RequireBoundTable(Menu_Framework_weapons,
                      Audio_Seq_unusedInventory.weapons);
    const auto& weapon =
        Referenced(Audio_Seq_unusedInventory.weapons, slot.one_based);
    if (row == 1) {
      RequireBoundTable(Menu_Framework_blades,
                        Audio_Seq_unusedInventory.blades);
      (void)Referenced(Audio_Seq_unusedInventory.blades, weapon.blade);
      RequireText(true, true);
      return;
    }
    RequireBoundTable(Menu_Framework_grips, Audio_Seq_unusedInventory.grips);
    const auto& grip = Referenced(Audio_Seq_unusedInventory.grips, weapon.grip);
    if (row == 2) {
      RequireText(false, true);
    } else if (row >= kFirstWeaponGemRow && row <= kLastWeaponGemRow &&
               row - kFirstWeaponGemRow < grip.gemSlots) {
      RequireGem(
          weapon.gems[static_cast<std::size_t>(row - kFirstWeaponGemRow)]);
      RequireText(false, false);
    }
  }
  void RequireShieldRow(int row, InventorySlot slot) const {
    RequireBoundTable(Menu_Framework_shields,
                      Audio_Seq_unusedInventory.shields);
    const auto& shield =
        Referenced(Audio_Seq_unusedInventory.shields, slot.one_based);
    if (row >= 1 && row <= kLastShieldGemRow &&
        row - 1 < shield.base.gemSlots) {
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
  return UpdateWeaponDetails(g_states[0], world, Selection(item_info));
}

// decomp: _bladeStatusView (src/MENU/items1.h:195-265).
int UpdateLiveBladeDetails(int item_info) {
  LiveWorld world(Category::kBlade);
  return UpdateBladeDetails(g_states[1], world, Selection(item_info));
}

// decomp: _gripStatusView (src/MENU/items1.h:275-355).
int UpdateLiveGripDetails(int item_info) {
  LiveWorld world(Category::kGrip);
  return UpdateGripDetails(g_states[2], world, Selection(item_info));
}

// decomp: _shieldStatusView (src/MENU/items1.h:365-454).
int UpdateLiveShieldDetails(int item_info) {
  LiveWorld world(Category::kShield);
  return UpdateShieldDetails(g_states[kShieldControllerIndex], world,
                             Selection(item_info));
}

void ResetLiveEquipmentDetailControllers() {
  g_states = {};
  g_gem_count_text = {'X', ' ', ' ', ' ', ' ', ' ', '0', '\0'};
}
}  // namespace VS::Menu::Inventory
