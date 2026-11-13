#include <algorithm>
#include <array>
#include <bit>
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "Input/Pad.hpp"
#include "Items/ContainerIndexing.hpp"
#include "Items/ItemRecordsLive.hpp"
#include "Menu/Container/ContainerDetailHelpersLive.hpp"
#include "Menu/Framework/EquipmentDetailsLive.hpp"
#include "Menu/Framework/Framework.hpp"
#include "Menu/Framework/FrameworkLive.hpp"
#include "Menu/Sounds.hpp"

extern "C" {
#include "Input/ControllerLiveState.h"
#include "Items/InventoryLiveState.h"
#include "Menu/Container/ItemView.h"
#include "Menu/Container/Storage.h"
#include "Menu/Framework/FrameworkLiveState.h"
#include "Menu/Framework/MenuItems.h"

Menu_Framework_MenuRow *Menu_Framework_menuItems{};
Menu_Framework_MenuMemory Menu_Framework_menuMemory{};
Items_Container *Menu_Container_containerData{};
Items_Container *Menu_Container_otherStorage{};
Menu_Container_InventoryStorage *Menu_Framework_unusedInventoryStorage{};
u_short Menu_Container_transferredMiscCount{};
Items_Inventory Audio_Seq_unusedInventory{};
Battle_Actor *Battle_actors[42]{};
Battle_Actor *Battle_characterState{};
const unsigned char Items_Inventory_inventoryItemCapacities[8]{8,  16, 16, 8,
                                                               16, 48, 64, 2};
unsigned char *Menu_Inventory_inventoryIndices[7]{};
u_short Menu_Container_containerItemCapacities[8]{32, 64,  64,  32,
                                                  64, 192, 256, 2};
char Menu_Container_categoryTitleVisible{},
    Menu_Container_categoryIconsVisible{}, Menu_Container_controlsPanelMode{},
    Menu_Container_detailsViewPhase{};
int Menu_Container_detailsClosedWithTriangle{};
int Input_buttonsState{}, Input_buttonRepeat{};
union Input_PressedButtonsView Input_buttonsPressed{};
}

namespace {
namespace detail = VS::Menu::container;
namespace items = VS::Items;
namespace fw = VS::Menu::Framework;
using Call = std::pair<int, int>;
using Memory = std::array<std::uint8_t, sizeof(Menu_Framework_MenuMemory)>;
enum Event {
  kInvalid = 2,
  kClear,
  kRow,
  kContinue,
  kPages,
  kPrimary,
  kLeave,
  kDismiss,
  kBars
};
constexpr int kSelected = 20;
constexpr std::size_t kHeader = 31;
char borrowed_text[] = "borrowed";
int identifier = 0, row_reads = 0;
std::string_view mode;
std::vector<Call> calls;
std::array<Menu_Framework_MenuRow, fw::kRowCount> rows{}, replacement_rows{};
Items_Container owner{}, replacement{};
Items_InventoryWeapon weapon{};
Items_InventoryShield shield{};
Items_WeaponLoadout weapon_target{};
Items_ShieldLoadout shield_target{};
Memory initial_memory{};
std::string expected_reason;
bool passed = true;

void Check(bool value) { passed = value && passed; }

bool CursorPair(int category, int page) {
  auto expected = initial_memory;
  const auto offset = 0xA2 + static_cast<std::size_t>(category) * 2;
  expected.at(offset) = 0;
  expected.at(offset + 1) = static_cast<std::uint8_t>(page);
  return std::bit_cast<Memory>(Menu_Framework_menuMemory) == expected;
}

bool OriginalStatsTarget() {
  return weapon_target.index == 91 && shield_target.index == 87;
}

bool FailureState() {
  if (identifier == 2925 || identifier == 2926)
    return OriginalStatsTarget() && calls.empty();
  if (identifier == 2943)
    return calls.empty() &&
           std::bit_cast<Memory>(Menu_Framework_menuMemory) == initial_memory;
  if (identifier == 2945) {
    if (mode == "missing")
      return calls.empty();
    if (mode == "missing-after-clear")
      return calls == std::vector<Call>{{kClear, 40}};
    return calls == std::vector<Call>{{kClear, 40}, {kRow, 31}} &&
           rows[kHeader].targetPosition0 == 18;
  }
  if (identifier == 2947) {
    const bool fields = Menu_Container_categoryTitleVisible == 7 &&
                        Menu_Container_categoryIconsVisible == 0 &&
                        Menu_Container_controlsPanelMode == 0;
    return fields &&
           (mode == "missing" ? calls.empty()
                              : calls == std::vector<Call>{{kContinue, 40}});
  }
  if (identifier == 2949) {
    if (mode == "bad-category")
      return std::bit_cast<Memory>(Menu_Framework_menuMemory) == initial_memory;
    if (mode == "bad-capacity")
      return CursorPair(2, 31) && calls.empty();
    const int page = mode == "bounds" ? 32 : mode == "low-index" ? -1 : 31;
    return CursorPair(0, page) && calls.empty();
  }
  if (identifier == 2950) {
    const auto expected = mode == "missing-after-clear"
                              ? std::vector<Call>{{kLeave, 0}, {kClear, 40}}
                              : std::vector<Call>{{kLeave, 0}};
    return calls == expected && Menu_Container_detailsViewPhase == 99 &&
           Menu_Container_detailsClosedWithTriangle == 0x12345678;
  }
  return false;
}

void Record(int event, int argument = 0) {
  calls.emplace_back(event, argument);
  if (event == kClear) {
    if (mode == "clear-owner")
      Menu_Framework_menuItems = replacement_rows.data();
    if (mode == "missing-after-clear")
      Menu_Framework_menuItems = nullptr;
  }
  if (event == kRow && ++row_reads == 1) {
    if (mode == "row-owner")
      Menu_Framework_menuItems = replacement_rows.data();
    if (mode == "missing-after-row")
      Menu_Framework_menuItems = nullptr;
  }
  if (event == kPrimary && mode == "callback-writes") {
    Menu_Container_categoryIconsVisible = 9;
    Menu_Container_controlsPanelMode = 10;
  }
  if (event == kPrimary && mode == "current-input")
    Input_buttonsPressed.allButtons = VS::Input::Triangle;
}

void Initialize() {
  initial_memory.fill(0xA5);
  Menu_Framework_menuMemory =
      std::bit_cast<Menu_Framework_MenuMemory>(initial_memory);
  Menu_Framework_menuItems = rows.data();
  Menu_Container_containerData = &owner;
  Menu_Container_categoryTitleVisible = 7;
  Menu_Container_categoryIconsVisible = 8;
  Menu_Container_controlsPanelMode = 9;
  Menu_Container_detailsViewPhase = 99;
  Menu_Container_detailsClosedWithTriangle = 0x12345678;
  weapon_target.index = 91;
  shield_target.index = 87;
  weapon.name[23] = 'z';
  weapon.index = 0xFE;
  shield.index = 0xFF;
  for (auto *table : {&rows, &replacement_rows}) {
    for (auto &row : *table) {
      row.state = Menu_Framework_MenuItemState_Idle;
      row.unselectable = 9;
    }
  }
  rows[kSelected].subText = borrowed_text;
  owner.items.weapons[0].blade = 1;
  owner.items.weapons[31].blade = 64;
  owner.indices.weapons[31] = 65535;
  owner.indices.weapons[1] = 0x8001;
  replacement.items.weapons[0].blade = 1;
  replacement.indices.weapons[31] = 256;
}

void WeaponProbe() {
  items::Live::Commit(
      owner.items.blades[63],
      items::InventoryBlade{.id = 41, .strength = -8, .material = 0x1234});
  items::Live::Commit(owner.items.gems[191],
                      items::InventoryGem{.id = 511, .strength = 11});
  weapon.blade = 64;
  weapon.gems[0] = 192;
  weapon.gems[2] = 192;
  auto *source = &weapon;
  auto *target = &weapon_target;
  auto *records = &owner.items;
  if (mode == "lazy") {
    weapon.blade = 0;
    weapon.gems[0] = weapon.gems[2] = 0;
    records = nullptr;
  }
  if (mode == "current-owner") {
    replacement.items.blades[63].id = 17;
    records = &replacement.items;
  }
  if (mode == "missing") {
    records = nullptr;
    expected_reason = "missing container record owner";
  }
  if (mode == "missing-source") {
    source = nullptr;
    expected_reason = "missing mandatory weapon name source";
  }
  if (mode == "bounds" || mode == "source-before-target") {
    weapon.blade = 65;
    expected_reason = "record index outside live container table";
  }
  if (mode == "missing-target" || mode == "source-before-target") {
    target = nullptr;
    if (mode == "missing-target")
      expected_reason = "missing weapon statistics target";
  }
  detail::BuildLiveWeaponStats(target, source, records);
  if (mode == "lazy")
    Check(weapon_target.blade.id == 0);
  else
    Check(weapon_target.blade.id == (mode == "current-owner" ? 17 : 41));
  Check(weapon_target.index == 0xFE && weapon_target.name[23] == 'z');
  if (mode == "alias")
    Check(weapon_target.gems[0].id == 511 && weapon_target.gems[2].id == 511);
}

void ShieldProbe() {
  items::Live::Commit(owner.items.gems[126],
                      items::InventoryGem{.id = 500, .strength = -9});
  shield.base.id = 31;
  shield.base.strength = 12;
  shield.gems[0] = 255;
  shield.gems[2] = 127;
  auto *source = &shield;
  auto *target = &shield_target;
  auto *records = &owner.items;
  if (mode == "lazy") {
    source = nullptr;
    records = nullptr;
  }
  if (mode == "current-owner") {
    replacement.items.gems[126].id = 17;
    records = &replacement.items;
  }
  if (mode == "missing") {
    records = nullptr;
    expected_reason = "missing container record owner";
  }
  if (mode == "bounds") {
    shield.gems[0] = 128;
    expected_reason = "record index outside live container table";
  }
  if (mode == "missing-target") {
    target = nullptr;
    expected_reason = "missing shield statistics target";
  }
  detail::BuildLiveShieldStats(target, source, records);
  if (mode == "lazy")
    Check(shield_target.base.id == 0);
  else {
    Check(shield_target.index == 0xFF && shield_target.base.id == 31);
    Check(shield_target.gems[0].id == (mode == "current-owner" ? 17 : 500));
    Check(shield_target.gems[2].id == shield_target.gems[0].id);
  }
}

void PanelsProbe() {
  Menu_Framework_menuItems = nullptr;
  Menu_Container_containerData = nullptr;
  const char value = mode == "zero"    ? 0
                     : mode == "alias" ? Menu_Container_controlsPanelMode
                                       : static_cast<char>(255);
  detail::SetLivePanelVisibility(value);
  Check(Menu_Container_categoryTitleVisible == value &&
        Menu_Container_categoryIconsVisible == value &&
        Menu_Container_controlsPanelMode == value &&
        Menu_Container_detailsViewPhase == 99 && calls.empty());
}

void PagingProbe() {
  int category = 0, page = 0, expected = 1;
  Input_buttonRepeat = VS::Input::L1;
  if (mode == "current-owner") {
    Menu_Container_containerData = &replacement;
    expected = 0;
  }
  if (mode == "lazy") {
    Menu_Container_containerData = nullptr;
    Input_buttonsState = VS::Input::L1 | VS::Input::R1;
    category = -1;
    page = expected = 17;
  }
  if (mode == "zero-capacity") {
    Menu_Container_containerData = nullptr;
    Menu_Container_containerItemCapacities[0] = 0;
    Input_buttonRepeat = 0;
    expected = 0;
  }
  if (mode == "missing") {
    Menu_Container_containerData = nullptr;
    expected_reason = "missing container record owner";
  }
  if (mode == "bounds") {
    category = 8;
    expected_reason = "category outside container categories";
  }
  if (mode == "bad-capacity") {
    Menu_Container_containerItemCapacities[0] = 33;
    expected_reason = "capacity exceeds container category storage";
  }
  if (mode == "manage") {
    Menu_Container_containerData = nullptr;
    category = 7;
    Input_buttonRepeat = 0;
    expected = 0;
  }
  if (mode == "alias")
    owner.items.weapons[31].blade = 1;
  Check(detail::PollLiveDetailsPaging(category, page) == expected);
}

void BeginProbe() {
  int selected = mode == "alias" ? 31 : kSelected;
  if (mode == "missing") {
    Menu_Framework_menuItems = nullptr;
    expected_reason = "missing current menu row workspace";
  }
  if (mode == "missing-after-clear" || mode == "missing-after-row")
    expected_reason = "missing current menu row workspace";
  if (mode == "bounds") {
    selected = 40;
    expected_reason = "detail row outside current menu row workspace";
  }
  detail::BeginLiveDetailsTransition(selected);
  const auto &header =
      mode == "clear-owner" ? replacement_rows[kHeader] : rows[kHeader];
  const auto &item = mode == "clear-owner" || mode == "row-owner"
                         ? replacement_rows[static_cast<std::size_t>(selected)]
                         : rows[static_cast<std::size_t>(selected)];
  Check(item.selected == 1 && item.unselectable == 0 &&
        item.subText == nullptr && item.targetPosition0 == 155);
  Check(header.targetPosition0 == (mode == "alias" ? 155 : 18));
  Check(calls.size() == 3);
}

void LayoutProbe() {
  if (mode == "missing") {
    Menu_Framework_menuItems = nullptr;
    expected_reason = "missing current menu row workspace";
  }
  if (mode == "bounds")
    expected_reason = "detail row outside current menu row workspace";
  const int selected = mode == "bounds" ? 40 : mode == "alias" ? 31 : kSelected;
  detail::InitializeLiveDetailsLayout(selected, 3);
  Check(Menu_Container_categoryTitleVisible == 7);
  Check(Menu_Container_categoryIconsVisible ==
        (mode == "callback-writes" ? 9 : 0));
  Check(Menu_Container_controlsPanelMode ==
        (mode == "callback-writes" ? 10 : 0));
  Check(calls ==
        std::vector<Call>{{kContinue, selected}, {kPages, 3}, {kPrimary, 1}});
}

void SelectionProbe() {
  int category = 0, page = 31, expected = 65535;
  if (mode == "current-owner") {
    Menu_Container_containerData = &replacement;
    expected = 256;
  }
  if (mode == "lazy")
    Menu_Framework_menuItems = nullptr;
  if (mode == "alias") {
    owner.indices.weapons[0] = 65535;
    page = 0;
  }
  if (mode == "prefix") {
    category = 2;
    page = 0;
    Menu_Container_containerItemCapacities[0] = 0;
    Menu_Container_containerItemCapacities[1] = 1;
    expected = 0x8001;
  }
  if (mode == "missing") {
    Menu_Container_containerData = nullptr;
    expected_reason = "missing container index owner";
  }
  if (mode == "bounds" || mode == "low-index") {
    page = mode == "bounds" ? 32 : -1;
    expected_reason = "detail page outside current container display storage";
  }
  if (mode == "bad-category") {
    category = 8;
    expected_reason = "detail category outside container categories";
  }
  if (mode == "bad-capacity") {
    category = 2;
    Menu_Container_containerItemCapacities[0] = 33;
    expected_reason = "capacity exceeds container category storage";
  }
  Check(detail::SelectLiveDetailsItem(category, page) == expected);
  Check(CursorPair(category, page) && calls.empty());
}

void ExitProbe() {
  if (mode == "missing") {
    Menu_Framework_menuItems = nullptr;
    expected_reason = "missing current menu row workspace";
  }
  if (mode == "missing-after-clear")
    expected_reason = "missing current menu row workspace";
  if (mode == "current-owner")
    Menu_Framework_menuItems = replacement_rows.data();
  const bool bars = mode != "lazy";
  detail::ExitLiveDetailsView(bars ? -1 : 0);
  Check(Menu_Container_detailsViewPhase == 2);
  Check(Menu_Container_detailsClosedWithTriangle ==
        (mode == "current-input" ? static_cast<int>(VS::Input::Triangle) : 0));
  Check(calls.size() == (bars ? 6U : 5U));
  if (bars)
    Check(calls.back() == Call{kBars, 4});
}
} // namespace

extern "C" void Core_Errorf(const char *category, const char *format, ...) {
  std::array<char, 1024> text{};
  va_list arguments;
  va_start(arguments, format);
  std::vsnprintf(text.data(), text.size(), format, arguments);
  va_end(arguments);
  const bool checked = !expected_reason.empty() &&
                       std::string_view(text.data()).find(expected_reason) !=
                           std::string_view::npos &&
                       FailureState();
  std::fprintf(stderr, "%s: %s; boundaries=%s\n", category, text.data(),
               checked ? "PASS" : "FAIL");
  std::exit(checked ? 85 : 91);
}

extern "C" void Core_Warnf(const char *, const char *, ...) {
  std::fputs("unexpected warning; boundaries=FAIL\n", stderr);
  std::exit(92);
}

namespace VS::Menu {
void PlayInvalidSound() { Record(kInvalid); }
void PlayLeaveSound() { Record(kLeave); }
} // namespace VS::Menu

namespace VS::Core {
int NextLiveRandomBounded(int) { std::exit(93); }
int NextLiveRandomAveraged(int) { std::exit(93); }
} // namespace VS::Core

namespace VS::Menu::Framework {
void ClearLiveMenuExcept(int except) { Record(kClear, except); }
MenuRow &LiveMenuRow(int row) {
  auto *borrowed = Menu_Framework_menuItems;
  Record(kRow, row);
  return reinterpret_cast<MenuRow *>(borrowed)[row];
}
void ContinueLiveItemDetailsTransition(int row) { Record(kContinue, row); }
void UpdateLiveEquipmentStatPages(int command) { Record(kPages, command); }
void UpdateLivePrimaryStatPanel(int command) { Record(kPrimary, command); }
void FlyOutLiveRowLeft(int row) { Record(kDismiss, row); }
void UpdateLiveDpPpBars(int command) {
  Check(Menu_Container_detailsViewPhase == 2);
  Record(kBars, command);
}
} // namespace VS::Menu::Framework

int main(int argc, char **argv) {
  if (argc != 3)
    return 2;
  identifier = std::atoi(argv[1]);
  mode = argv[2];
  Initialize();
  switch (identifier) {
  case 2925:
    WeaponProbe();
    break;
  case 2926:
    ShieldProbe();
    break;
  case 2942:
    PanelsProbe();
    break;
  case 2943:
    PagingProbe();
    break;
  case 2945:
    BeginProbe();
    break;
  case 2947:
    LayoutProbe();
    break;
  case 2949:
    SelectionProbe();
    break;
  case 2950:
    ExitProbe();
    break;
  default:
    return 3;
  }
  Check(expected_reason.empty());
  std::cout << identifier << ' ' << mode
            << " boundaries=" << (passed ? "PASS" : "FAIL") << '\n';
  return passed ? 0 : 1;
}
