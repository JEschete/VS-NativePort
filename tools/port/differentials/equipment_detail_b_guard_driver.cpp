#include <array>
#include <bit>
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <span>
#include <string_view>
#include <vector>

#include "Input/Pad.hpp"
#include "Items/InventoryLive.hpp"
#include "Menu/Framework/EquipmentDetailsLive.hpp"
#include "Menu/Framework/Framework.hpp"
#include "Menu/Framework/FrameworkLive.hpp"
#include "Menu/Framework/ItemRows.hpp"
#include "Menu/Framework/ItemRowsLive.hpp"
#include "Menu/Inventory/EquipmentDetailControllersLive.hpp"
#include "Menu/Sounds.hpp"
#include "Render/GameFontLive.hpp"

extern "C" {
#include "Audio/Seq/Channel.h"
#include "Battle/Actor.h"
#include "Input/ControllerLiveState.h"
#include "Menu/Framework/EquipmentDetailsLiveState.h"
#include "Menu/Framework/FrameworkLiveState.h"
#include "Menu/Framework/ItemRowsLiveState.h"
#include "Menu/Inventory/Inventory.h"
#include "Menu/Inventory/ItemList.h"
#include "Render/Render.h"

Items_Inventory Audio_Seq_unusedInventory{};
Items_InventoryIndices Audio_Seq_unusedInventoryIndices{};
Menu_Framework_MenuRow *Menu_Framework_menuItems{};
Menu_Framework_MenuMemory Menu_Framework_menuMemory{};
u_char *Menu_Inventory_inventoryIndices[7]{};
int Input_buttonsState{}, Input_buttonRepeat{};
union Input_PressedButtonsView Input_buttonsPressed{};
int Menu_Inventory_ashleyRenderState{}, Menu_Inventory_ashleyYOffset{},
    Menu_Inventory_exitToBattle{};
char Menu_Inventory_secondaryTitleEnabled{},
    Menu_Inventory_itemCategoryIconsEnabled{},
    Menu_Inventory_statusCommandState{},
    Menu_Inventory_selectedItemCategoryIconOnTop{};
Items_InventoryWeapon *Menu_Framework_weapons{};
Items_InventoryBlade *Menu_Framework_blades{};
Items_InventoryGrip *Menu_Framework_grips{};
Items_InventoryShield *Menu_Framework_shields{};
Items_InventoryGem *Menu_Framework_gems{};
Items_InventoryArmor *Menu_Framework_armor{};
u_short *Menu_Framework_itemHelp{};
char (*Menu_Framework_itemNames)[24]{};
char *Menu_Framework_stringBuf{};
Battle_Actor *Battle_characterState{};
}

namespace {
namespace inventory = VS::Menu::Inventory;
namespace fw = VS::Menu::Framework;
using Call = std::array<int, 4>;
enum Event {
  kClear = 1,
  kRow,
  kReady,
  kContinue,
  kPages,
  kPrimary,
  kBars,
  kStats,
  kComponent,
  kCount,
  kFormat,
  kPage,
  kLeave,
  kDismiss,
  kInvalid,
  kIcon,
  kText
};
constexpr std::array<int, 4> kCapacities{8, 16, 16, 8};
constexpr std::array<int, 4> kPagesForCategory{7, 3, 4, 7};
constexpr std::array<int (*)(int), 4> kEntries{
    inventory::UpdateLiveWeaponDetails, inventory::UpdateLiveBladeDetails,
    inventory::UpdateLiveGripDetails, inventory::UpdateLiveShieldDetails};
std::string_view selected;
int kind = 0, ready = 1, expected_page = 0;
bool valid = true, preparing = true, reset_performed = false;
std::vector<Call> calls;
std::array<Menu_Framework_MenuRow, fw::kRowCount> rows{}, replacement_rows{};
std::array<std::uint8_t, 16> selected_order{};
std::array<char, fw::kStringBufferBytes> description{};
std::array<u_short, fw::kItemHelpBytes / 2> help{};
std::array<std::array<char, 24>, fw::kItemNamesBytes / 24> names{};
Render_FrameScratch frame{};
std::array<std::uint8_t, sizeof(Menu_Framework_MenuMemory)> initial_memory{};
std::uint8_t observed_digit = 0;

void Record(int event, int first = 0, int second = 0, int third = 0) {
  calls.push_back({event, first, second, third});
}
std::span<u_char> DirectOrder() {
  switch (kind) {
  case 0:
    return Audio_Seq_unusedInventoryIndices.weapons;
  case 1:
    return Audio_Seq_unusedInventoryIndices.blades;
  case 2:
    return Audio_Seq_unusedInventoryIndices.grips;
  case 3:
    return Audio_Seq_unusedInventoryIndices.shields;
  default:
    std::exit(2);
  }
}
void UnbindStatistics() {
  switch (kind) {
  case 0:
    Menu_Framework_weapons = nullptr;
    break;
  case 1:
    Menu_Framework_blades = nullptr;
    break;
  case 2:
    Menu_Framework_grips = nullptr;
    break;
  case 3:
    Menu_Framework_shields = nullptr;
    break;
  default:
    std::exit(2);
  }
}
int Step(int command = 0) {
  return kEntries.at(static_cast<std::size_t>(kind))(command);
}
void Initialize() {
  inventory::ResetLiveEquipmentDetailControllers();
  initial_memory.fill(0xA5);
  Menu_Framework_menuMemory =
      std::bit_cast<Menu_Framework_MenuMemory>(initial_memory);
  Menu_Framework_menuItems = rows.data();
  Menu_Framework_weapons = Audio_Seq_unusedInventory.weapons;
  Menu_Framework_blades = Audio_Seq_unusedInventory.blades;
  Menu_Framework_grips = Audio_Seq_unusedInventory.grips;
  Menu_Framework_shields = Audio_Seq_unusedInventory.shields;
  Menu_Framework_gems = Audio_Seq_unusedInventory.gems;
  for (auto &weapon : Audio_Seq_unusedInventory.weapons) {
    weapon.blade = 1;
    weapon.grip = 1;
    weapon.name[0] = 1;
  }
  Audio_Seq_unusedInventory.grips[0].gemSlots = 2;
  Audio_Seq_unusedInventory.grips[15].gemSlots = static_cast<char>(255);
  for (std::size_t i = 0; i < selected_order.size(); ++i) {
    selected_order[i] = static_cast<u_char>(i + 1);
  }
  for (auto &pointer : Menu_Inventory_inventoryIndices)
    pointer = selected_order.data();
  for (std::size_t i = 0; i < DirectOrder().size(); ++i) {
    DirectOrder()[i] = static_cast<u_char>(i + 1);
  }
  Menu_Framework_stringBuf = description.data();
  Menu_Framework_itemHelp = help.data();
  Menu_Framework_itemNames = reinterpret_cast<char (*)[24]>(names.data());
  frame.primitiveCursor = rows.data();
  frame.overlayOrderingTable = replacement_rows.data();
}
void ReachAnimation() {
  valid &= Step((20 << 8) | 1) == 0;
  valid &= Step() == 0;
}
void ReachInput() {
  ReachAnimation();
  for (int tick = 0; tick < 11; ++tick)
    valid &= Step() == 0;
}
bool MemoryPair(int page) {
  auto expected = initial_memory;
  expected[0x3C + static_cast<std::size_t>(kind) * 2] = 0;
  expected[0x3D + static_cast<std::size_t>(kind) * 2] =
      static_cast<std::uint8_t>(page);
  return std::bit_cast<decltype(initial_memory)>(Menu_Framework_menuMemory) ==
         expected;
}
bool Has(int event) {
  for (const auto &call : calls)
    if (call[0] == event)
      return true;
  return false;
}
bool DiagnosticState() {
  if (selected == "missing-entry-rows")
    return calls.empty();
  if (selected == "missing-after-clear") {
    return calls == std::vector<Call>{{kClear, 40, 0, 0}};
  }
  if (selected == "missing-init-rows")
    return calls.empty();
  if (selected == "bounds-page" || selected == "bounds-slot-init" ||
      selected == "missing-stats-after-layout" ||
      selected == "missing-equipped-actor" ||
      selected == "missing-weapon-part") {
    return Has(kContinue) && Has(kPages) && Has(kPrimary) && !Has(kStats);
  }
  if (selected == "missing-order-page")
    return Has(kCount) && !Has(kFormat) && MemoryPair(1);
  if (selected == "zero-slot-page" || selected == "missing-help-page" ||
      selected == "missing-names-page" || selected == "missing-buffer-page") {
    return Has(kCount) && !Has(kFormat) && MemoryPair(1);
  }
  if (selected == "formatter-missing-text" ||
      selected == "formatter-foreign-text") {
    return Has(kFormat) && !Has(kStats) && !Has(kPage);
  }
  if (selected == "missing-row-after-page-stats")
    return Has(kStats) && !Has(kPage);
  if (selected == "missing-frame-grip") {
    return calls == std::vector<Call>{{kCount, kind, 0, 0}};
  }
  if (selected == "missing-gem-component" ||
      selected == "missing-component-text") {
    return calls.empty();
  }
  return false;
}
bool PositiveState() {
  if (selected == "lazy-init-page")
    return calls == std::vector<Call>{{kReady, 0, 0, 0}};
  if (selected == "lazy-wait-owners")
    return calls.empty();
  if (selected == "lazy-input-owners") {
    return kind == 2 ? observed_digit == 50 && calls.size() == 2
                     : calls.empty();
  }
  if (selected == "entry-owner-callback") {
    return replacement_rows[31].targetPosition0 == 18 &&
           replacement_rows[20].targetPosition0 == 155 &&
           rows[20].selected == 0;
  }
  if (selected == "layout-current-map") {
    return Has(kStats) &&
           calls.back() == (kind == 2 ? Call{kStats, kCapacities[kind], 0, 0}
                                      : Call{kBars, 3, 0, 0});
  }
  if (selected == "layout-field-callback") {
    return Menu_Inventory_itemCategoryIconsEnabled == 9 &&
           Menu_Inventory_statusCommandState == 10;
  }
  if (selected == "page-components-reload") {
    const int end = kind == 0 ? 6 : 4;
    int seen = 0;
    for (const auto &call : calls) {
      if (call[0] != kComponent)
        continue;
      const int reference = seen == 0 ? kCapacities[kind] : seen;
      valid &= call == Call{kComponent, seen + 1, reference, 0};
      ++seen;
    }
    return seen == end - 1 && MemoryPair(1);
  }
  if (selected == "grip-exit-count") {
    return observed_digit == 47 && Has(kLeave) && Has(kText) && !Has(kBars) &&
           Menu_Inventory_exitToBattle == static_cast<int>(VS::Input::Triangle);
  }
  if (selected == "grip-mutable-digit") {
    return observed_digit == 51 && Has(kIcon) && Has(kText);
  }
  if (selected == "grip-absent-gem-owner")
    return observed_digit == 50 && Has(kText);
  if (selected == "unused-gem-component")
    return Has(kComponent);
  if (selected == "reset-reentry")
    return Has(kStats) && MemoryPair(1);
  if (selected == "circle-is-not-exit")
    return !Has(kLeave) && Has(kCount);
  return false;
}
void Execute() {
  if (selected == "missing-entry-rows" || selected == "missing-after-clear" ||
      selected == "entry-owner-callback") {
    preparing = false;
    if (selected == "missing-entry-rows")
      Menu_Framework_menuItems = nullptr;
    valid &= Step((20 << 8) | 1) == 0;
    return;
  }
  if (selected == "lazy-init-page" || selected == "bounds-page") {
    valid &= Step((20 << 8) | (kCapacities[kind] + 1)) == 0;
    ready = selected == "lazy-init-page" ? 0 : 1;
    UnbindStatistics();
    calls.clear();
    preparing = false;
    valid &= Step() == 0;
    return;
  }
  if (selected == "missing-init-rows" || selected == "bounds-slot-init" ||
      selected == "layout-current-map" || selected == "layout-field-callback" ||
      selected == "missing-stats-after-layout" ||
      selected == "missing-equipped-actor" ||
      selected == "missing-weapon-part" || selected == "reset-reentry") {
    valid &= Step((20 << 8) | (selected == "reset-reentry" ? 2 : 1)) == 0;
    calls.clear();
    preparing = false;
    if (selected == "missing-init-rows")
      Menu_Framework_menuItems = nullptr;
    if (selected == "bounds-slot-init")
      DirectOrder()[0] = 255;
    if (selected == "missing-equipped-actor")
      Audio_Seq_unusedInventory.weapons[0].isEquipped = 1;
    if (selected == "missing-weapon-part")
      Audio_Seq_unusedInventory.weapons[0].blade = 0;
    valid &= Step() == 0;
    if (selected == "reset-reentry") {
      Input_buttonsState = VS::Input::L1 | VS::Input::R1;
      for (int tick = 0; tick < 11; ++tick)
        valid &= Step() == 0;
      Input_buttonsState = 0;
      Input_buttonRepeat = VS::Input::R1;
      valid &= Step() == 0;
    }
    return;
  }
  if (selected == "lazy-wait-owners") {
    ReachAnimation();
    UnbindStatistics();
    Menu_Framework_menuItems = nullptr;
    DirectOrder()[0] = 255;
    calls.clear();
    preparing = false;
    valid &= Step() == 0;
    return;
  }
  if (selected == "unused-gem-component" ||
      selected == "missing-gem-component" ||
      selected == "missing-component-text") {
    ReachAnimation();
    for (int tick = 0; tick < (kind == 0 ? 2 : 0); ++tick)
      valid &= Step() == 0;
    if (kind == 0)
      Audio_Seq_unusedInventory.grips[0].gemSlots = 0;
    if (kind == 3)
      Audio_Seq_unusedInventory.shields[0].base.gemSlots = 0;
    Menu_Framework_itemHelp = nullptr;
    Menu_Framework_itemNames = nullptr;
    Menu_Framework_gems = nullptr;
    if (selected != "unused-gem-component") {
      if (kind == 0)
        Audio_Seq_unusedInventory.grips[0].gemSlots = 1;
      if (kind == 3)
        Audio_Seq_unusedInventory.shields[0].base.gemSlots = 1;
      if (selected == "missing-gem-component") {
        if (kind == 0)
          Audio_Seq_unusedInventory.weapons[0].gems[0] = 1;
        if (kind == 3)
          Audio_Seq_unusedInventory.shields[0].gems[0] = 1;
      }
    }
    calls.clear();
    preparing = false;
    valid &= Step() == 0;
    return;
  }
  ReachInput();
  calls.clear();
  preparing = false;
  if (selected == "lazy-input-owners") {
    UnbindStatistics();
    Menu_Framework_menuItems = nullptr;
    Menu_Framework_itemHelp = nullptr;
    Menu_Framework_itemNames = nullptr;
    Menu_Framework_stringBuf = nullptr;
    Input_buttonsState = VS::Input::L1 | VS::Input::R1;
  } else if (selected == "missing-frame-grip") {
    frame.primitiveCursor = nullptr;
  } else if (selected == "grip-exit-count") {
    Input_buttonsPressed.allButtons = VS::Input::Triangle;
  } else if (selected == "grip-mutable-digit") {
    Audio_Seq_unusedInventory.grips[0].gemSlots = 3;
  } else if (selected == "grip-absent-gem-owner") {
    Menu_Framework_gems = nullptr;
    Menu_Framework_itemHelp = nullptr;
    Menu_Framework_itemNames = nullptr;
  } else {
    Input_buttonRepeat = VS::Input::R1;
    expected_page = 1;
    if (selected == "missing-order-page")
      Menu_Inventory_inventoryIndices[kind] = nullptr;
    if (selected == "zero-slot-page")
      selected_order[1] = 0;
    if (selected == "missing-help-page")
      Menu_Framework_itemHelp = nullptr;
    if (selected == "missing-names-page")
      Menu_Framework_itemNames = nullptr;
    if (selected == "missing-buffer-page")
      Menu_Framework_stringBuf = nullptr;
    if (selected == "circle-is-not-exit")
      Input_buttonsPressed.allButtons = VS::Input::Circle;
  }
  valid &= Step() == 0;
}
} // namespace

extern "C" void *Core_ScratchBase() { return &frame; }
extern "C" void Core_Errorf(const char *, const char *format, ...) {
  std::va_list args;
  va_start(args, format);
  std::vfprintf(stderr, format, args);
  va_end(args);
  std::fputc('\n', stderr);
  const bool expected = valid && !preparing && DiagnosticState();
  std::fprintf(stderr, "guard-state-order-lazy-boundaries=%s\n",
               expected ? "PASS" : "FAIL");
  std::_Exit(expected ? 85 : 86);
}
namespace VS::Items::Inventory {
int CountLiveItems(int category, const Items_Inventory *source) {
  valid &= category == kind && source == nullptr;
  Record(kCount, category);
  return 2;
}
} // namespace VS::Items::Inventory
namespace VS::Menu {
void PlayLeaveSound() { Record(kLeave); }
void PlayInvalidSound() { Record(kInvalid); }
} // namespace VS::Menu
namespace VS::Menu::Framework {
void ClearLiveMenuExcept(int row) {
  Record(kClear, row);
  if (!preparing && selected == "missing-after-clear")
    Menu_Framework_menuItems = nullptr;
  if (!preparing && selected == "entry-owner-callback")
    Menu_Framework_menuItems = replacement_rows.data();
}
MenuRow &LiveMenuRow(int row) {
  Record(kRow, row);
  return reinterpret_cast<MenuRow &>(Menu_Framework_menuItems[row]);
}
int AreLiveMenuRowsIdle() {
  Record(kReady);
  return ready;
}
void ContinueLiveItemDetailsTransition(int row) {
  Record(kContinue, row);
  if (!preparing && selected == "missing-stats-after-layout")
    UnbindStatistics();
  if (!preparing && selected == "layout-current-map")
    DirectOrder()[0] = static_cast<u_char>(kCapacities[kind]);
}
void UpdateLiveEquipmentStatPages(int pages) {
  Record(kPages, pages);
  valid &= preparing || pages == kPagesForCategory[kind] || pages == -1;
}
void UpdateLivePrimaryStatPanel(int motion) {
  Record(kPrimary, motion);
  if (!preparing && selected == "layout-field-callback") {
    Menu_Inventory_itemCategoryIconsEnabled = 9;
    Menu_Inventory_statusCommandState = 10;
  }
  if (!preparing && selected == "grip-exit-count")
    DirectOrder()[0] = 16;
}
void UpdateLiveDpPpBars(int flags) { Record(kBars, flags); }
void FlyOutLiveRowLeft(int row) { Record(kDismiss, row); }
void Statistics(int reference) {
  Record(kStats, reference);
  if (!preparing && selected == "missing-row-after-page-stats")
    Menu_Framework_menuItems = nullptr;
  if (!preparing && selected == "reset-reentry" && !reset_performed) {
    reset_performed = true;
    inventory::ResetLiveEquipmentDetailControllers();
  }
}
void SetLiveWeaponStats(int reference) { Statistics(reference); }
void SetLiveBladeStats(int reference) { Statistics(reference); }
void SetLiveGripStats(int reference) { Statistics(reference); }
void SetLiveShieldStats(int reference) { Statistics(reference); }
Menu_Framework_MenuRow *Component(int row, int reference, int slide) {
  Record(kComponent, row, reference, slide);
  if (!preparing && selected == "page-components-reload") {
    DirectOrder()[expected_page] = static_cast<u_char>(
        DirectOrder()[expected_page] % kCapacities[kind] + 1);
  }
  return nullptr;
}
Menu_Framework_MenuRow *CreateLiveWeaponComponentRow(int row, int reference,
                                                     int slide) {
  return Component(row, reference, slide);
}
Menu_Framework_MenuRow *CreateLiveShieldGemRow(int row, int reference,
                                               int slide) {
  return Component(row, reference, slide);
}
void Format(char *text[], int *row_type, void *buffer, char *name = nullptr) {
  valid &= buffer == description.data();
  Record(kFormat);
  if (selected == "formatter-missing-text")
    return;
  static char foreign[] = "foreign";
  text[0] =
      selected == "formatter-foreign-text"
          ? foreign
          : (name == nullptr ? reinterpret_cast<char *>(help.data()) : name);
  text[1] = description.data();
  *row_type = 0x58030000;
}
void FormatLiveInventoryWeaponRow(Items_InventoryWeapon *record, char *text[],
                                  int *row_type, void *buffer) {
  Format(text, row_type, buffer, record->name);
}
void FormatLiveBladeRow(Items_InventoryBlade *, char *text[], int *row_type,
                        void *buffer) {
  Format(text, row_type, buffer);
}
void FormatLiveGripRow(Items_InventoryGrip *, char *text[], int *row_type,
                       void *buffer) {
  Format(text, row_type, buffer);
}
void FormatLiveInventoryShieldRow(Items_InventoryShield *, char *text[],
                                  int *row_type, void *buffer) {
  Format(text, row_type, buffer);
}
void UpdateLiveItemDetailsSelection(int row, char *text[], u_int row_type,
                                    int page) {
  const int expected_row = selected == "reset-reentry" ? 0 : 20;
  valid &= row == expected_row && text[1] == description.data() &&
           row_type == 0x58030000;
  Record(kPage, page);
  if (selected == "page-components-reload")
    DirectOrder()[page] = static_cast<u_char>(kCapacities[kind]);
}
void DrawLiveRowIcon(int icon, int x, int y) {
  valid &= icon == 0x116 && x == 256 && y == 32;
  Record(kIcon);
}
} // namespace VS::Menu::Framework
namespace VS::Render {
void DrawLiveCompactText(const char *text, int xy, void *chain) {
  valid &= xy == static_cast<int>(fw::PackXy(280, 36)) && chain == nullptr;
  observed_digit = static_cast<std::uint8_t>(text[6]);
  Record(kText, observed_digit);
}
} // namespace VS::Render

int main(int argc, char **argv) {
  if (argc != 3)
    return 2;
  kind = std::atoi(argv[1]);
  selected = argv[2];
  Initialize();
  Execute();
  valid &= PositiveState();
  std::printf("positive-fields-order-lazy-boundaries=%s; calls=%zu; digit=%u\n",
              valid ? "PASS" : "FAIL", calls.size(), observed_digit);
  return valid ? 0 : 1;
}
