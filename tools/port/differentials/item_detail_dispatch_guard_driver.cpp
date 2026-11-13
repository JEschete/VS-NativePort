#include <algorithm>
#include <array>
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
#include "Menu/Inventory/ItemDetailDispatchLive.hpp"
#include "Menu/Sounds.hpp"
#include "Render/GameFontLive.hpp"
#include "Render/SpritesLive.hpp"

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
char Menu_Framework_cursorColor{}, Menu_Framework_currentStatusViewItem{};
u_char Menu_Framework_selectedStatusViewElement{};
}

namespace {
namespace inv = VS::Menu::Inventory;
namespace fw = VS::Menu::Framework;
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
  kText,
  kReset,
  kCommand,
  kTabs,
  kDraw,
  kSceneSprite,
  kOverlaySprite,
};
using Call = std::array<int, 4>;
std::string_view name;
int identifier = 0, category = 4, ready = 1;
bool valid = true, preparing = true;
std::vector<Call> calls;
std::vector<int> expected;
std::array<Menu_Framework_MenuRow, fw::kRowCount> rows{}, replacement_rows{};
std::array<u_char, 48> bound{};
std::array<char, fw::kStringBufferBytes> description{},
    replacement_description{};
std::array<u_short, fw::kItemHelpBytes / 2> help{};
std::array<std::array<char, 24>, fw::kItemNamesBytes / 24> names{};
std::array<std::array<u_long, 6>, 16> packets{};
std::size_t packet_count = 0;
Render_FrameScratch frame{};

void Record(int event, int a = 0, int b = 0, int c = 0) {
  calls.push_back({event, a, b, c});
}
bool EventsMatch() {
  std::vector<int> actual;
  for (const auto &call : calls) {
    actual.push_back(call[0]);
  }
  return actual == expected;
}
std::span<u_char> Direct() {
  switch (category) {
  case 0:
    return Audio_Seq_unusedInventoryIndices.weapons;
  case 1:
    return Audio_Seq_unusedInventoryIndices.blades;
  case 2:
    return Audio_Seq_unusedInventoryIndices.grips;
  case 3:
    return Audio_Seq_unusedInventoryIndices.shields;
  case 4:
    return Audio_Seq_unusedInventoryIndices.armor;
  case 5:
    return Audio_Seq_unusedInventoryIndices.gems;
  default:
    std::exit(2);
  }
}
void UnbindStats() {
  if (category == 4) {
    Menu_Framework_armor = nullptr;
  } else {
    Menu_Framework_gems = nullptr;
  }
}
int Step(int command = 0) {
  return category == 4 ? inv::UpdateLiveArmorDetails(command)
                       : inv::UpdateLiveGemDetails(command);
}
void Initialize() {
  inv::ResetLiveEquipmentDetailControllers();
  inv::ResetLiveItemDetailDispatch();
  Menu_Framework_menuItems = rows.data();
  Menu_Framework_weapons = Audio_Seq_unusedInventory.weapons;
  Menu_Framework_blades = Audio_Seq_unusedInventory.blades;
  Menu_Framework_grips = Audio_Seq_unusedInventory.grips;
  Menu_Framework_shields = Audio_Seq_unusedInventory.shields;
  Menu_Framework_gems = Audio_Seq_unusedInventory.gems;
  Menu_Framework_armor = Audio_Seq_unusedInventory.armor;
  for (auto &record : Audio_Seq_unusedInventory.armor) {
    record.id = 143;
    record.category = 1;
    record.material = 1;
  }
  for (auto &record : Audio_Seq_unusedInventory.gems) {
    record.id = 261;
  }
  for (auto &weapon : Audio_Seq_unusedInventory.weapons) {
    weapon.blade = 1;
    weapon.grip = 1;
    weapon.name[0] = 1;
  }
  for (auto &grip : Audio_Seq_unusedInventory.grips) {
    grip.id = 1;
    grip.gemSlots = 2;
  }
  for (std::size_t index = 0; index < bound.size(); ++index) {
    bound[index] = static_cast<u_char>(index + 1);
  }
  for (auto &pointer : Menu_Inventory_inventoryIndices) {
    pointer = bound.data();
  }
  for (std::size_t index = 0; index < Direct().size(); ++index) {
    Direct()[index] = static_cast<u_char>(index + 1);
  }
  Menu_Framework_itemHelp = help.data();
  Menu_Framework_itemNames = reinterpret_cast<char (*)[24]>(names.data());
  Menu_Framework_stringBuf = description.data();
  Menu_Framework_menuMemory.opaque3A.currentCategory = 1;
  frame.primitiveCursor = packets.data();
  frame.orderingTable = bound.data();
  frame.overlayOrderingTable = replacement_rows.data();
  rows[31].x = 61;
  replacement_rows[31].x = 60;
}
void ReachAnimation(int page = 1) {
  valid &= Step((20 << 8) | page) == 0;
  valid &= Step() == 0;
}
void ReachInput() {
  ReachAnimation();
  for (int tick = 0; tick < 11; ++tick) {
    valid &= Step() == 0;
  }
}
void Target(std::initializer_list<int> events) {
  calls.clear();
  preparing = false;
  expected = events;
}
void InitializeTarget() {
  valid &= Step((20 << 8) | 1) == 0;
  Target({kReady, kContinue, kPages, kPrimary});
}
bool Positive() {
  if (!EventsMatch() || !valid) {
    return false;
  }
  if (name == "entry-owner-swap") {
    return replacement_rows[20].selected == 1 && rows[20].selected == 0;
  }
  if (name == "layout-order-reload") {
    return Menu_Framework_currentStatusViewItem == Direct().size();
  }
  if (name == "page-text-owner-alias" ||
      name == "formatter-buffer-owner-alias") {
    return Menu_Framework_currentStatusViewItem == 2 &&
           (name == "formatter-buffer-owner-alias" || description[0] == 'b') &&
           Menu_Framework_stringBuf == replacement_description.data();
  }
  if (name == "accessory-unused-buffer") {
    return Menu_Framework_currentStatusViewItem == 2 &&
           Menu_Framework_stringBuf == nullptr;
  }
  if (name == "exit-priority") {
    return Menu_Inventory_selectedItemCategoryIconOnTop == 2 &&
           Menu_Inventory_exitToBattle == static_cast<int>(VS::Input::Triangle);
  }
  if (name == "icons-merged-unused-overlay") {
    return packet_count == 7 && rows[31].rowIcon == 24;
  }
  if (name == "icons-after-tabs-owner") {
    return packet_count == 7 && replacement_rows[31].rowIcon == 24 &&
           rows[31].rowIcon == 0;
  }
  if (name == "icons-cached-category") {
    return packet_count == 8 && packets[0][4] == 0x37FD8078 &&
           Menu_Framework_menuMemory.opaque3A.currentCategory == 5;
  }
  if (name == "dispatcher-return-menu") {
    return Menu_Inventory_itemCategoryIconsEnabled == 1 &&
           Menu_Inventory_selectedItemCategoryIconOnTop == 0 &&
           Menu_Inventory_statusCommandState == 1 &&
           Menu_Inventory_ashleyRenderState == 0;
  }
  if (name == "dispatcher-nonzero-empty-entry-bytes") {
    return rows[0].selected == 1 && rows[31].itemPage == 0;
  }
  return true;
}
void Controllers() {
  if (name == "entry-no-rows" || name == "entry-after-clear-no-rows" ||
      name == "entry-invalid-row" || name == "entry-owner-swap") {
    Target(name == "entry-no-rows" ? std::initializer_list<int>{}
           : name == "entry-after-clear-no-rows"
               ? std::initializer_list<int>{kClear}
           : name == "entry-invalid-row"
               ? std::initializer_list<int>{kClear, kRow}
               : std::initializer_list<int>{kClear, kRow, kRow});
    if (name == "entry-no-rows") {
      Menu_Framework_menuItems = nullptr;
    }
    valid &= Step(((name == "entry-invalid-row" ? 40 : 20) << 8) | 1) == 0;
    return;
  }
  if (name == "lazy-init" || name == "init-invalid-page") {
    valid &= Step((20 << 8) | (static_cast<int>(Direct().size()) + 1)) == 0;
    ready = name == "lazy-init" ? 0 : 1;
    UnbindStats();
    Target(name == "lazy-init" ? std::initializer_list<int>{kReady}
                               : std::initializer_list<int>{kReady, kContinue,
                                                            kPages, kPrimary});
    valid &= Step() == 0;
    return;
  }
  if (name.starts_with("init-") || name == "layout-order-reload" ||
      name == "armor-dp-reloads" || name == "non-limb-no-actor") {
    InitializeTarget();
    if (name == "init-no-rows") {
      Menu_Framework_menuItems = nullptr;
      expected.clear();
    } else if (name == "init-null-table") {
      UnbindStats();
    } else if (name == "init-high-slot") {
      Direct()[0] = 255;
    } else if (name == "init-zero-slot") {
      Direct()[0] = 0;
      if (category == 4) {
        expected.push_back(kStats);
      }
    } else if (name == "init-equipped-no-actor") {
      Audio_Seq_unusedInventory.armor[0].limb = 6;
    } else if (name == "layout-order-reload" || name == "non-limb-no-actor") {
      expected.push_back(kStats);
      if (category == 4) {
        expected.push_back(kBars);
      }
    } else if (name == "armor-dp-reloads" ||
               name == "init-after-stats-bad-order") {
      expected.push_back(kStats);
    }
    valid &= Step() == 0;
    return;
  }
  if (name == "lazy-wait") {
    ReachAnimation();
    UnbindStats();
    Menu_Framework_menuItems = nullptr;
    Direct()[0] = 255;
    Target({});
    valid &= Step() == 0;
    return;
  }
  if (name == "return-only-ready" || name == "reentry-after-return") {
    ReachInput();
    Input_buttonsPressed.allButtons = VS::Input::Cross;
    valid &= Step() == 0;
    Input_buttonsPressed.allButtons = 0;
    ready = 7;
    if (name == "return-only-ready") {
      Target({kReady});
      valid &= Step() == 7;
    } else {
      Target({kClear, kRow, kRow});
      valid &= Step((31 << 8) | 2) == 0;
      valid &= rows[31].selected == 1;
    }
    return;
  }
  ReachInput();
  if (name == "lazy-input") {
    UnbindStats();
    Menu_Framework_menuItems = nullptr;
    Menu_Inventory_inventoryIndices[category] = nullptr;
    Menu_Framework_itemHelp = nullptr;
    Menu_Framework_itemNames = nullptr;
    Menu_Framework_stringBuf = nullptr;
    Input_buttonsState = VS::Input::L1 | VS::Input::R1;
    Target({});
  } else if (name == "exit-priority") {
    Input_buttonsPressed.allButtons = VS::Input::Triangle | VS::Input::R1;
    Target({kLeave, kClear, kDismiss, kPages, kPrimary});
    if (category == 4) {
      expected.push_back(kBars);
    }
  } else if (name == "circle-stays") {
    Input_buttonsPressed.allButtons = VS::Input::Circle;
    Target({kCount});
  } else {
    Input_buttonRepeat = VS::Input::R1;
    Target({kCount});
    if (name == "input-null-order")
      Menu_Inventory_inventoryIndices[category] = nullptr;
    if (name == "input-zero-slot")
      bound[1] = 0;
    if (name == "input-high-slot")
      bound[1] = 255;
    if (name == "input-no-help")
      Menu_Framework_itemHelp = nullptr;
    if (name == "input-no-names")
      Menu_Framework_itemNames = nullptr;
    if (name == "input-no-buffer")
      Menu_Framework_stringBuf = nullptr;
    if (name == "input-bad-armor-category")
      Audio_Seq_unusedInventory.armor[1].category = 0;
    if (name == "formatter-null-text" || name == "formatter-foreign-text" ||
        name == "formatter-unbind-table") {
      expected.push_back(kFormat);
    }
    if (name == "stats-unbind-rows") {
      expected.insert(expected.end(), {kFormat, kStats});
    }
    if (name == "page-text-owner-alias" || name == "accessory-unused-buffer" ||
        name == "formatter-buffer-owner-alias") {
      expected.insert(expected.end(), {kFormat, kStats, kPage});
      if (name == "accessory-unused-buffer") {
        Audio_Seq_unusedInventory.armor[1].category = 7;
        Menu_Framework_stringBuf = nullptr;
      }
    }
  }
  valid &= Step() == 0;
}
void Icons() {
  Target({});
  if (name == "icons-no-scene-frame")
    frame.orderingTable = nullptr;
  if (name == "icons-no-overlay-frame")
    frame.overlayOrderingTable = nullptr;
  if (name == "icons-no-overlay-frame")
    expected = {kRow};
  if (name == "icons-after-tabs-no-rows")
    expected = {kTabs};
  if (name == "icons-lazy-rows")
    Menu_Framework_menuItems = nullptr;
  if (name == "icons-merged-unused-overlay") {
    frame.overlayOrderingTable = nullptr;
    rows[31].x = 60;
    expected = {kRow};
  }
  if (name == "icons-after-tabs-owner")
    expected = {kTabs, kRow};
  if (name == "icons-cached-category")
    expected = {kTabs, kRow, kOverlaySprite};
  const bool tabs =
      name.starts_with("icons-after-tabs") || name == "icons-cached-category";
  const bool highlight =
      name != "icons-lazy-rows" && name != "icons-no-scene-frame";
  if (name == "icons-lazy-rows")
    expected.assign(8, kSceneSprite);
  if (name == "icons-merged-unused-overlay" ||
      name == "icons-after-tabs-owner" || name == "icons-cached-category") {
    expected.insert(expected.end(), 7, kSceneSprite);
  }
  inv::DrawLiveItemCategoryIcons(tabs ? 4 : 2, highlight ? 1 : 0);
}
void Dispatcher() {
  if (name == "dispatcher-nonzero-empty-entry-bytes") {
    ready = 0;
    Target({kReset, kRow, kCommand, kClear, kRow, kRow, kReady});
    valid &= inv::UpdateLiveItemDetails((0x10000 << 4) | category) == 0;
    return;
  }
  if (name == "dispatcher-invalid-category" ||
      name == "dispatcher-entry-no-rows" ||
      name == "dispatcher-after-reset-no-rows") {
    Target(name == "dispatcher-invalid-category"
               ? std::initializer_list<int>{kReset, kRow, kCommand}
               : std::initializer_list<int>{kReset});
    if (name == "dispatcher-entry-no-rows")
      Menu_Framework_menuItems = nullptr;
    valid &= inv::UpdateLiveItemDetails(name == "dispatcher-invalid-category"
                                            ? 6
                                            : (((20 << 8) | 1) << 4) |
                                                  category) == 0;
    return;
  }
  ReachInput();
  Input_buttonsState = VS::Input::L1 | VS::Input::R1;
  ready = 0;
  valid &= inv::UpdateLiveItemDetails(category) == 0;
  if (name == "dispatcher-lazy-frame") {
    frame.primitiveCursor = nullptr;
    Target({kReady});
    valid &= inv::UpdateLiveItemDetails(0) == 0;
    return;
  }
  if (name == "dispatcher-return-menu" || name == "dispatcher-return-battle") {
    Input_buttonsPressed.allButtons = VS::Input::Cross;
    valid &= Step() == 0;
    Input_buttonsPressed.allButtons = 0;
    ready = 7;
    Menu_Inventory_exitToBattle = name == "dispatcher-return-battle" ? -7 : 0;
    Target(name == "dispatcher-return-menu"
               ? std::initializer_list<int>{kReady, kCommand}
               : std::initializer_list<int>{kReady});
    valid &= inv::UpdateLiveItemDetails(0) ==
             (name == "dispatcher-return-battle" ? -2 : 7);
    return;
  }
  ready = 1;
  Target({kReady});
  if (name == "dispatcher-no-frame") {
    frame.primitiveCursor = nullptr;
  } else {
    expected.push_back(kTabs);
  }
  if (name == "dispatcher-invalid-cursor")
    Menu_Framework_selectedStatusViewElement = 255;
  if (name == "dispatcher-draws")
    expected.push_back(kDraw);
  valid &= inv::UpdateLiveItemDetails(0) == 0;
}
} // namespace

extern "C" void *Core_ScratchBase() { return &frame; }
extern "C" void Core_Errorf(const char *, const char *format, ...) {
  std::va_list arguments;
  va_start(arguments, format);
  std::vfprintf(stderr, format, arguments);
  va_end(arguments);
  const bool passed = valid && !preparing && EventsMatch();
  std::fprintf(stderr, "\nused-owner-order-lazy-boundaries=%s\n",
               passed ? "PASS" : "FAIL");
  std::_Exit(passed ? 85 : 86);
}
namespace VS::Items::Inventory {
int CountLiveItems(int current_category, const Items_Inventory *source) {
  valid &= current_category == category && source == nullptr;
  Record(kCount, current_category);
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
  valid &= row == 40;
  if (!preparing && name == "entry-after-clear-no-rows")
    Menu_Framework_menuItems = nullptr;
  if (!preparing && name == "entry-owner-swap")
    Menu_Framework_menuItems = replacement_rows.data();
}
MenuRow &LiveMenuRow(int row) {
  Record(kRow, row);
  valid &= row >= 0 && row < static_cast<int>(kRowCount);
  return reinterpret_cast<MenuRow &>(Menu_Framework_menuItems[row]);
}
int AreLiveMenuRowsIdle() {
  Record(kReady);
  return ready;
}
void ContinueLiveItemDetailsTransition(int row) {
  Record(kContinue, row);
  if (!preparing && name == "init-after-layout-null-table")
    UnbindStats();
  if (!preparing && name == "layout-order-reload")
    Direct()[0] = static_cast<u_char>(Direct().size());
}
void UpdateLiveEquipmentStatPages(int pages) { Record(kPages, pages); }
void UpdateLivePrimaryStatPanel(int motion) { Record(kPrimary, motion); }
void UpdateLiveDpPpBars(int flags) { Record(kBars, flags); }
void FlyOutLiveRowLeft(int row) { Record(kDismiss, row); }
void Statistics(int reference) {
  Record(kStats, reference);
  Menu_Framework_currentStatusViewItem = static_cast<char>(reference);
  if (!preparing && name == "stats-unbind-rows")
    Menu_Framework_menuItems = nullptr;
  if (!preparing && name == "init-after-stats-bad-order")
    Direct()[0] = 0;
  if (!preparing && name == "armor-dp-reloads") {
    Direct()[0] = 2;
    Audio_Seq_unusedInventory.armor[1].category = 7;
  }
  if (!preparing && name == "page-text-owner-alias") {
    names[0][0] = 'a';
    description[0] = 'b';
    Menu_Framework_stringBuf = replacement_description.data();
    Menu_Framework_itemNames = nullptr;
  }
}
void SetLiveWeaponStats(int reference) { Statistics(reference); }
void SetLiveBladeStats(int reference) { Statistics(reference); }
void SetLiveGripStats(int reference) { Statistics(reference); }
void SetLiveShieldStats(int reference) { Statistics(reference); }
void SetLiveArmorStats(int reference) { Statistics(reference); }
void SetLiveGemStats(int reference) { Statistics(reference); }
Menu_Framework_MenuRow *CreateLiveWeaponComponentRow(int row, int reference,
                                                     int slide) {
  Record(kComponent, row, reference, slide);
  return nullptr;
}
Menu_Framework_MenuRow *CreateLiveShieldGemRow(int row, int reference,
                                               int slide) {
  Record(kComponent, row, reference, slide);
  return nullptr;
}
void Format(char *text[], int *row_type, void *buffer, bool accessory = false) {
  Record(kFormat);
  valid &= buffer == Menu_Framework_stringBuf;
  if (!preparing && name == "formatter-null-text")
    return;
  static char foreign[] = "foreign";
  text[0] = !preparing && name == "formatter-foreign-text" ? foreign
                                                           : names[0].data();
  text[1] = accessory ? reinterpret_cast<char *>(help.data())
                      : static_cast<char *>(buffer);
  *row_type = 0x58030000;
  if (!preparing && name == "formatter-buffer-owner-alias") {
    Menu_Framework_stringBuf = replacement_description.data();
  }
  if (!preparing && name == "formatter-unbind-table")
    UnbindStats();
}
void FormatLiveInventoryWeaponRow(Items_InventoryWeapon *, char *text[],
                                  int *type, void *buffer) {
  Format(text, type, buffer);
}
void FormatLiveBladeRow(Items_InventoryBlade *, char *text[], int *type,
                        void *buffer) {
  Format(text, type, buffer);
}
void FormatLiveGripRow(Items_InventoryGrip *, char *text[], int *type,
                       void *buffer) {
  Format(text, type, buffer);
}
void FormatLiveInventoryShieldRow(Items_InventoryShield *, char *text[],
                                  int *type, void *buffer) {
  Format(text, type, buffer);
}
void FormatLiveInventoryArmorRow(Items_InventoryArmor *record, char *text[],
                                 int *type, void *buffer) {
  Format(text, type, buffer, record->category == 7);
}
void FormatLiveGemRow(Items_InventoryGem *, char *text[], int *type,
                      void *buffer) {
  Format(text, type, buffer);
}
void UpdateLiveItemDetailsSelection(int row, char *text[], u_int type,
                                    int page) {
  valid &= row == 20 && text[0] == names[0].data() && type == 0x58030000 &&
           page == 1;
  valid &= text[1] == (name == "accessory-unused-buffer"
                           ? reinterpret_cast<char *>(help.data())
                           : description.data());
  Record(kPage, page);
}
void DrawLiveRowIcon(int, int, int) { Record(kIcon); }
void ResetLiveItemDetailCursor() {
  Record(kReset);
  Menu_Framework_selectedStatusViewElement = 9;
  if (!preparing && name == "dispatcher-after-reset-no-rows")
    Menu_Framework_menuItems = nullptr;
}
void SetLiveCommandLabel(int command) { Record(kCommand, command); }
void DrawLiveTabNavigation(int mode) {
  Record(kTabs, mode);
  if (!preparing && (name == "icons-after-tabs-no-rows" ||
                     name == "dispatcher-after-tabs-no-rows"))
    Menu_Framework_menuItems = nullptr;
  if (!preparing && name == "icons-after-tabs-owner")
    Menu_Framework_menuItems = replacement_rows.data();
  if (!preparing && name == "icons-cached-category")
    Menu_Framework_menuMemory.opaque3A.currentCategory = 5;
}
void UpdateLiveItemDetails() { Record(kDraw); }
} // namespace VS::Menu::Framework
namespace VS::Render {
u_long *Sprite(int event, int wh, int xy) {
  Record(event, wh, xy);
  if (packet_count >= packets.size()) {
    std::exit(86);
  }
  return packets[packet_count++].data();
}
u_long *QueueLiveDefaultSprite(int wh, int xy) {
  return Sprite(kOverlaySprite, wh, xy);
}
u_long *QueueLiveSpriteWithTexturePage(int brightness, int xy, int wh,
                                       u_long *chain) {
  valid &=
      brightness == kDefaultSpriteBrightness && chain == frame.orderingTable;
  return Sprite(kSceneSprite, wh, xy);
}
void DrawLiveCompactText(const char *, int, void *) { Record(kText); }
} // namespace VS::Render

int main(int argc, char **argv) {
  if (argc != 3)
    return 2;
  identifier = std::atoi(argv[1]);
  category = identifier == 2580 ? 5 : 4;
  name = argv[2];
  Initialize();
  if (identifier == 2568)
    Icons();
  else if (identifier == 2581)
    Dispatcher();
  else
    Controllers();
  const bool passed = Positive();
  std::printf("used-owner-order-lazy-boundaries=%s; calls=%zu\n",
              passed ? "PASS" : "FAIL", calls.size());
  return passed ? 0 : 1;
}
