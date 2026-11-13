#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "Content/ModuleData.h"
#include "Core/Log.hpp"
#include "Input/Pad.hpp"
#include "Items/ItemRecordsLive.hpp"
#include "Menu/Container/ContainerDetailControllersLive.hpp"
#include "Menu/Container/ContainerDetailHelpersLive.hpp"
#include "Menu/Framework/EquipmentDetailsLive.hpp"
#include "Menu/Framework/Framework.hpp"
#include "Menu/Framework/FrameworkLive.hpp"
#include "Menu/Framework/ItemRows.hpp"
#include "Menu/Framework/ItemRowsLive.hpp"
#include "Menu/Sounds.hpp"
#include "Render/EncodedText.hpp"
#include "Render/GameFontLive.hpp"
#include "Render/SpritesLive.hpp"

extern "C" {
#include "Audio/Seq/Channel.h"
#include "Battle/Actor.h"
#include "Input/ControllerLiveState.h"
#include "Items/InventoryLiveState.h"
#include "Menu/Container/ItemView.h"
#include "Menu/Container/Storage.h"
#include "Menu/Framework/EquipmentDetailsLiveState.h"
#include "Menu/Framework/FrameworkLiveState.h"
#include "Menu/Framework/ItemRowsLiveState.h"
#include "Menu/Framework/MenuItems.h"
#include "Menu/Inventory/ItemList.h"
#include "Render/Display.h"
#include "Render/Frame.h"
#include "Render/Render.h"
#include "Render/Text/CompactFont.h"
#include "Render/Text/VariableFont.h"
#include "Render/UiPrimitives.h"

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
Items_InventoryWeapon *Menu_Framework_weapons{};
Items_InventoryBlade *Menu_Framework_blades{};
Items_InventoryGrip *Menu_Framework_grips{};
Items_InventoryShield *Menu_Framework_shields{};
Items_InventoryArmor *Menu_Framework_armor{};
Items_InventoryGem *Menu_Framework_gems{};
u_short *Menu_Framework_itemHelp{};
char (*Menu_Framework_itemNames)[kMenuItemNameRowBytes]{};
char *Menu_Framework_stringBuf{};
char Menu_Framework_cursorColor{};
u_char Menu_Framework_selectedStatusViewElement{};
void *Render_legacyPrimitiveBuffers[2]{};
void *Render_legacyOrderingTables[2]{};
void *Render_legacyCurrentOrderingTable{};
int Render_frameBuf{};
u_char Render_compactGlyphLeftTrim[264]{};
u_char Render_compactGlyphWidths[92]{};
u_char Render_compactGlyphIndices[60]{};
char Render_variableFontGlyphWidths[413]{};
}

namespace {
namespace detail = VS::Menu::container;
namespace fw = VS::Menu::Framework;
namespace items = VS::Items;
namespace render = VS::Render;
constexpr std::array<int, 6> kCapacities{32, 64, 64, 32, 64, 192};
constexpr std::array<int (*)(int), 6> kControllers{
    detail::UpdateLiveWeaponDetails, detail::UpdateLiveBladeDetails,
    detail::UpdateLiveGripDetails,   detail::UpdateLiveShieldDetails,
    detail::UpdateLiveArmorDetails,  detail::UpdateLiveGemDetails};
constexpr int kRow = 20;
enum Event {
  kClear = 1,
  kRowRead,
  kReady,
  kLayout,
  kStats,
  kComponent,
  kFormat,
  kChange,
  kBars,
  kTabs,
  kDetails,
  kCommand,
  kReset,
  kIcon,
  kLeave
};
using Call = std::array<int, 3>;
std::vector<Call> calls;
std::string mode, expected_reason;
int identifier = 0, category = 0, ready = 1;
bool passed = true;
Items_Container owner{}, replacement{};
std::array<Menu_Framework_MenuRow, fw::kRowCount> rows{}, replacement_rows{};
std::array<std::array<char, fw::kStringBufferBytes>, 2> buffers{};
std::array<std::array<char, fw::kItemNamesBytes>, 2> names{};
std::array<u_short, fw::kItemHelpBytes / 2> help{};
std::array<std::vector<u_long>, 2> primitives{
    std::vector<u_long>(0x20000 / sizeof(u_long)),
    std::vector<u_long>(0x20000 / sizeof(u_long))};
std::array<std::array<u_long, 0x2088 / sizeof(u_long)>, 2> ordering{};
char *formatted_name{};
char *formatted_description{};
char foreign_text[] = "foreign";
std::size_t active_frame = 0;

void Check(bool condition) { passed = condition && passed; }

void Bind(Items_Container &container, std::size_t text_owner = 0) {
  Menu_Container_containerData = &container;
  Menu_Framework_weapons = container.items.weapons;
  Menu_Framework_blades = container.items.blades;
  Menu_Framework_grips = container.items.grips;
  Menu_Framework_shields = container.items.shields;
  Menu_Framework_armor = container.items.armor;
  Menu_Framework_gems = container.items.gems;
  Menu_Framework_stringBuf = buffers.at(text_owner).data();
  Menu_Framework_itemNames = reinterpret_cast<char (*)[fw::kItemNameRowBytes]>(
      names.at(text_owner).data());
}

void Frame(std::size_t index) {
  active_frame = index;
  Render_frameBuf = static_cast<int>(index);
  Render_legacyPrimitiveBuffers[index] = primitives[index].data();
  Render_legacyOrderingTables[index] = ordering[index].data();
  Render_frameScratch.primitiveCursor = primitives[index].data();
  Render_frameScratch.overlayOrderingTable = ordering[index].data() + 2;
  Render_frameScratch.orderingTable = ordering[index].data() + 18;
  ClearOTagR(ordering[index].data(), static_cast<int>(ordering[index].size()));
}

std::vector<std::array<u_long, 4>> Sprites() {
  std::vector<std::array<u_long, 4>> result;
  const void *packet = &ordering[active_frame].back();
  for (std::size_t count = 0; packet != nullptr && count < 4096; ++count) {
    const auto *words = static_cast<const u_long *>(packet);
    const auto length = static_cast<const P_TAG *>(packet)->len;
    if (length == render::kSpritePacketWords - 1 &&
        (words[render::kSpriteColorWord] >> 24) == render::kSpriteCommand)
      result.push_back(
          {words[render::kSpriteColorWord], words[render::kSpritePositionWord],
           words[render::kSpriteTextureWord], words[render::kSpriteSizeWord]});
    else if (length == 4 && (words[1] >> 24) == render::kSpriteCommand)
      result.push_back({words[1], words[2], words[3], words[4]});
    packet = isendprim(packet) ? nullptr : nextPrim(packet);
  }
  return result;
}

void LoadFonts(const char *path) {
  std::ifstream stream(path, std::ios::binary);
  const std::vector<unsigned char> image{std::istreambuf_iterator<char>(stream),
                                         {}};
  for (const auto &entry :
       std::span(Content_moduleData, Content_moduleDataCount))
    Check(VS::Content::CopyModuleDataEntry(entry, image));
  render::SetLiveMenuFontStyle(0);
}

std::span<std::uint16_t> Order(Items_Container &container) {
  switch (category) {
  case 0:
    return container.indices.weapons;
  case 1:
    return container.indices.blades;
  case 2:
    return container.indices.grips;
  case 3:
    return container.indices.shields;
  case 4:
    return container.indices.armor;
  default:
    return container.indices.gems;
  }
}

void Seed(Items_Container &container) {
  items::InventoryWeapon weapon{.blade = 64, .grip = 64};
  weapon.name.fill(static_cast<char>(VS::Render::kTextTerminator));
  weapon.name[0] = 'w';
  items::Live::Commit(container.items.weapons[0], weapon);
  items::Live::Commit(std::span(container.items.weapons).back(), weapon);
  items::InventoryBlade blade{
      .id = 1, .category = 1, .type_bits = 1, .material = 1};
  items::Live::Commit(container.items.blades[0], blade);
  items::Live::Commit(std::span(container.items.blades).back(), blade);
  items::InventoryGrip grip{.id = 1, .category = 1, .gem_slots = 3};
  items::Live::Commit(container.items.grips[0], grip);
  items::Live::Commit(std::span(container.items.grips).back(), grip);
  items::InventoryShield shield{};
  shield.base.id = 1;
  shield.base.category = 6;
  shield.base.material = 1;
  shield.base.gem_slots = 3;
  items::Live::Commit(container.items.shields[0], shield);
  items::Live::Commit(std::span(container.items.shields).back(), shield);
  items::InventoryArmor armor{.id = 143, .category = 1, .material = 1};
  items::Live::Commit(container.items.armor[0], armor);
  items::Live::Commit(std::span(container.items.armor).back(), armor);
  items::InventoryGem gem{.id = 261};
  items::Live::Commit(container.items.gems[0], gem);
  items::Live::Commit(std::span(container.items.gems).back(), gem);
  const auto order = Order(container);
  order[0] =
      static_cast<std::uint16_t>(category == 4 ? 1 : kCapacities.at(category));
  order[1] = 1;
}

void Record(int event, int first = 0, int second = 0) {
  calls.push_back({event, first, second});
  if (event == kReset && mode == "current-owner")
    Menu_Framework_menuItems = replacement_rows.data();
  if (event == kReady && mode == "missing-after-ready")
    Menu_Framework_menuItems = nullptr;
  if (event == kStats && mode == "current-text" && formatted_name != nullptr) {
    if (formatted_name != nullptr)
      formatted_name[0] = 's';
    if (formatted_description != nullptr)
      formatted_description[0] = 't';
    Bind(replacement, 1);
    Menu_Framework_menuItems = replacement_rows.data();
  }
  if (event == kStats && category == 4) {
    if (mode == "armor-callback-token-to-64")
      Order(owner)[0] = 64;
    if (mode == "armor-callback-token-to-17-accessory")
      Order(owner)[0] = 17;
    if (mode == "armor-callback-owner-to-64" ||
        mode == "armor-callback-owner-accessory" ||
        mode == "armor-callback-owner-same-token")
      Bind(replacement, 1);
    if (mode == "armor-callback-owner-only")
      Menu_Container_containerData = &replacement;
    if (mode == "armor-after-stats-zero")
      Order(owner)[0] = 0;
    if (mode == "armor-after-stats-high")
      Order(owner)[0] = 65;
    if (mode == "armor-after-stats-missing-owner")
      Menu_Container_containerData = nullptr;
  }
}

void Statistics(int slot, int selected_category) {
  Record(kStats, slot, selected_category);
  if (slot != 0 && selected_category == 0) {
    Items_WeaponLoadout target{};
    detail::BuildLiveWeaponStats(
        &target, &Menu_Container_containerData->items.weapons[slot - 1],
        &Menu_Container_containerData->items);
    if (mode == "zero-optional-parts") {
      Check(target.blade.id == 0 && target.grip.id == 0);
      Check(target.gems[0].id == 0 && target.name[0] == 'w');
    }
  }
  if (slot != 0 && selected_category == 3) {
    Items_ShieldLoadout target{};
    detail::BuildLiveShieldStats(
        &target, &Menu_Container_containerData->items.shields[slot - 1],
        &Menu_Container_containerData->items);
  }
}

void Format(char *name, char *text[], int *row_type, void *buffer,
            bool accessory = false) {
  text[0] = name;
  if (mode == "foreign-text")
    text[0] = foreign_text;
  text[1] = mode == "alias" ? name
            : accessory     ? reinterpret_cast<char *>(help.data())
                            : static_cast<char *>(buffer);
  *row_type = 0x7C050000;
  formatted_name = text[0];
  formatted_description = text[1];
  Record(kFormat, category);
}

void Initialize() {
  detail::ResetLiveContainerDetailControllers();
  for (auto &table : {std::span(rows), std::span(replacement_rows)}) {
    for (auto &row : table)
      row.state = Menu_Framework_MenuItemState_Idle;
  }
  Menu_Framework_menuItems = rows.data();
  Menu_Framework_itemHelp = help.data();
  buffers[0][0] = 'd';
  buffers[1][0] = 'e';
  names[0][0] = 'n';
  names[1][0] = 'm';
  Menu_Container_categoryTitleVisible = 7;
  Menu_Container_categoryIconsVisible = 8;
  Menu_Container_controlsPanelMode = 9;
  Menu_Container_detailsViewPhase = 99;
  Menu_Container_detailsClosedWithTriangle = 0x12345678;
  Menu_Framework_selectedStatusViewElement = 9;
  Seed(owner);
  Seed(replacement);
  Audio_Seq_unusedInventory.armor[0].category = 1;
  Bind(owner);
  Frame(0);
}

void Open(int (*update)(int), int page = 0, int row = kRow) {
  Check(update((row << 8) | (page + 1)) == 0);
  Check(update(0) == 0);
}

void Wait(int (*update)(int)) {
  for (int tick = 0; tick < 11; ++tick)
    Check(update(0) == 0);
}

bool FailureState() {
  if (mode.starts_with("armor-after-stats-"))
    return calls.back() == Call{kStats, 16, 4};
  if (mode == "row-bounds")
    return rows[31].state == Menu_Framework_MenuItemState_SlideY &&
           rows[31].targetPosition0 == 18 &&
           calls.back() == Call{kRowRead, 31, 0};
  if (mode == "foreign-text")
    return calls.back()[0] == kFormat;
  if (mode == "missing-equipped-stats" || mode == "wide-reference")
    return std::ranges::count(calls, kStats,
                              [](Call call) { return call[0]; }) == 0;
  if (mode == "flag-only-gem")
    return calls.back() == Call{kStats, 32, 3};
  if (mode == "missing-blade-at-component")
    return std::ranges::count(calls, kComponent,
                              [](Call call) { return call[0]; }) == 0;
  if (mode == "missing-grip-at-component")
    return std::ranges::count(calls, kComponent,
                              [](Call call) { return call[0]; }) == 1;
  if (mode == "bounds" && identifier == 2944)
    return calls.empty();
  if (mode == "bounds" && identifier == 2959)
    return Menu_Container_detailsViewPhase == 1 &&
           Menu_Container_detailsClosedWithTriangle == 0 &&
           calls.back()[0] == kCommand;
  if (mode == "missing" && identifier == 2959)
    return Menu_Container_detailsViewPhase == 1 &&
           Menu_Container_detailsClosedWithTriangle == 0 &&
           calls.back()[0] == kReset;
  if (mode == "missing-after-ready")
    return calls.back() == Call{kTabs, 1, 0};
  if (mode == "missing" || mode == "bounds" || mode == "record-bounds" ||
      mode == "bound-alias")
    return std::ranges::count(calls, kStats,
                              [](Call call) { return call[0]; }) == 0;
  return passed;
}

void ControllerProbe() {
  auto *update = kControllers.at(category);
  if (mode.starts_with("armor-owner-")) {
    const int reference = mode.find("-16-") != std::string::npos   ? 16
                          : mode.find("-17-") != std::string::npos ? 17
                                                                   : 64;
    const bool accessory = mode.ends_with("-accessory");
    const int selected_category = accessory ? 7 : 1;
    const int inventory_category = mode.find("-same-") != std::string::npos
                                       ? selected_category
                                   : accessory ? 1
                                               : 7;
    owner.items.armor[reference - 1] = owner.items.armor[0];
    owner.items.armor[reference - 1].category =
        static_cast<char>(selected_category);
    owner.items.armor[reference - 1].id = accessory ? 223 : 143;
    for (auto &armor : Audio_Seq_unusedInventory.armor)
      armor.category = static_cast<char>(inventory_category);
    Order(owner)[0] = static_cast<std::uint16_t>(reference);
    Open(update);
    const auto dp = std::ranges::count(calls, Call{kBars, 1, 0});
    Check(dp == (accessory ? 0 : 1));
    Check(std::ranges::find(calls, Call{kStats, reference, 4}) != calls.end());
    std::cout << "ARMOR_POLICY {\"initial_token\":" << reference
              << ",\"gate_token\":" << reference
              << ",\"gate_owner\":0,\"container_category\":"
              << selected_category
              << ",\"inventory_category\":" << inventory_category
              << ",\"dp_calls\":" << dp << "}\n";
    return;
  }
  if (mode.starts_with("armor-callback-") ||
      mode.starts_with("armor-after-stats-")) {
    const bool accessory = mode.ends_with("-accessory");
    const bool rebound = mode.starts_with("armor-callback-owner-");
    const int selected = mode == "armor-callback-owner-same-token" ? 16
                         : accessory                               ? 17
                                                                   : 64;
    const int initial_category = accessory ? 1 : 7;
    const int selected_category = accessory ? 7 : 1;
    for (auto *container : {&owner, &replacement}) {
      for (auto &armor : container->items.armor) {
        armor = owner.items.armor[0];
        armor.category = static_cast<char>(initial_category);
      }
    }
    Order(owner)[0] = 16;
    Order(replacement)[0] = static_cast<std::uint16_t>(selected);
    auto &selected_owner = rebound ? replacement : owner;
    selected_owner.items.armor[selected - 1].category =
        static_cast<char>(selected_category);
    for (auto &armor : Audio_Seq_unusedInventory.armor)
      armor.category = static_cast<char>(initial_category);
    if (mode.starts_with("armor-after-stats-"))
      expected_reason = mode == "armor-after-stats-missing-owner"
                            ? "missing current container display-order owner"
                            : "reference outside its used record table";
    Open(update);
    if (!expected_reason.empty()) {
      Check(false);
      return;
    }
    const auto dp = std::ranges::count(calls, Call{kBars, 1, 0});
    Check(dp == (accessory ? 0 : 1));
    Check(std::ranges::find(calls, Call{kStats, 16, 4}) != calls.end());
    Check(Menu_Container_containerData == &selected_owner);
    Check(Order(selected_owner)[0] == selected);
    if (mode == "armor-callback-owner-only")
      Check(Menu_Framework_armor == owner.items.armor);
    std::cout << "ARMOR_POLICY {\"initial_token\":16,\"gate_token\":"
              << selected << ",\"gate_owner\":" << (rebound ? 1 : 0)
              << ",\"bound_table_owner\":"
              << (Menu_Framework_armor == replacement.items.armor ? 1 : 0)
              << ",\"container_category\":" << selected_category
              << ",\"inventory_category\":" << initial_category
              << ",\"dp_calls\":" << dp << "}\n";
    return;
  }
  if (mode == "armor-gate-reentry") {
    owner.items.armor[15] = owner.items.armor[0];
    owner.items.armor[15].category = 1;
    Order(owner)[0] = 16;
    Audio_Seq_unusedInventory.armor[15].category = 7;
    Open(update);
    Check(std::ranges::count(calls, Call{kBars, 1, 0}) == 1);
    calls.clear();
    replacement.items.armor[63].category = 7;
    Order(replacement)[0] = 64;
    Bind(replacement, 1);
    Open(update);
    Check(std::ranges::find(calls, Call{kStats, 64, 4}) != calls.end());
    Check(std::ranges::count(calls, Call{kBars, 1, 0}) == 0);
    return;
  }
  if (mode == "armor-page-sixty-four") {
    Order(owner)[63] = 64;
    Open(update, 63);
    Check(std::ranges::find(calls, Call{kStats, 64, 4}) != calls.end());
    Check(std::ranges::count(calls, Call{kBars, 1, 0}) == 1);
    return;
  }
  if (mode == "row-bounds") {
    expected_reason = "detail row outside current menu row workspace";
    (void)update((40 << 8) | 1);
    Check(false);
    return;
  }
  if (mode == "lazy") {
    Menu_Container_containerData = nullptr;
    Menu_Framework_stringBuf = nullptr;
    Menu_Framework_itemNames = nullptr;
    Menu_Framework_itemHelp = nullptr;
    Render_frameScratch = {};
    Check(update((kRow << 8) | 1) == 0);
    Check(calls.front()[0] == kClear);
    return;
  }
  if (mode == "missing" || mode == "bounds" || mode == "record-bounds" ||
      mode == "bound-alias") {
    Check(update((kRow << 8) |
                 (mode == "bounds" ? kCapacities.at(category) + 1 : 1)) == 0);
    if (mode == "missing") {
      expected_reason = "missing current container display-order owner";
      Menu_Container_containerData = nullptr;
    } else if (mode == "bounds") {
      expected_reason = "current page outside direct container display order";
    } else if (mode == "record-bounds") {
      expected_reason = "reference outside its used record table";
      Order(owner)[0] =
          static_cast<std::uint16_t>(kCapacities.at(category) + 1);
    } else {
      expected_reason = "bound table is not the current container";
      Bind(replacement);
      Menu_Container_containerData = &owner;
    }
    calls.clear();
    (void)update(0);
    Check(false);
    return;
  }
  if (mode == "current-owner") {
    Check(update((kRow << 8) | 1) == 0);
    Bind(replacement, 1);
    Order(replacement)[0] = 1;
    Check(update(0) == 0);
    Check(calls.back()[0] != kFormat);
    Check(std::ranges::find(calls, Call{kStats, 1, category}) != calls.end());
    return;
  }
  if (mode == "inventory-gate-bounds") {
    // Retain the original token17 input. #366 intentionally changes its
    // historical refusal to successful current-container category evaluation.
    Order(owner)[0] = 17;
    Open(update);
    Check(owner.items.armor[16].category == 0);
    Check(std::ranges::find(calls, Call{kStats, 17, 4}) != calls.end());
    Check(std::ranges::count(calls, Call{kBars, 1, 0}) == 1);
    return;
  }
  if (mode == "supported-empty") {
    Order(owner)[0] = 0;
    Open(update);
    Check(std::ranges::find(calls, Call{kStats, 0, category}) != calls.end());
    return;
  }
  if (mode == "zero-optional-parts") {
    auto &weapon = std::span(owner.items.weapons).back();
    weapon.blade = 0;
    weapon.grip = 0;
    Menu_Framework_blades = nullptr;
    Menu_Framework_grips = nullptr;
    Menu_Framework_gems = nullptr;
    Open(update);
    Check(std::ranges::find(calls, Call{kStats, 32, 0}) != calls.end());
    return;
  }
  if (mode == "wide-reference") {
    expected_reason = "reference outside its used record table";
    Order(owner)[0] = 256;
    Open(update);
    Check(false);
    return;
  }
  if (mode == "missing-equipped-stats") {
    expected_reason = "missing current equipped actor statistics";
    if (category == 0)
      std::span(owner.items.weapons).back().isEquipped = 1;
    else if (category == 3)
      std::span(owner.items.shields).back().isEquipped = 1;
    else
      owner.items.armor[0].limb = 1;
    Open(update);
    Check(false);
    return;
  }
  if (mode == "flag-only-gem") {
    expected_reason = "record index outside live container table";
    auto &shield = std::span(owner.items.shields).back();
    shield.base.gemSlots = 0;
    shield.gems[0] = 0x80;
    Menu_Framework_gems = nullptr;
    Open(update);
    Check(false);
    return;
  }
  if (mode == "missing-blade-at-component" ||
      mode == "missing-grip-at-component") {
    expected_reason = "reference outside its used record table";
    auto &weapon = std::span(owner.items.weapons).back();
    const bool blade = mode == "missing-blade-at-component";
    if (blade)
      weapon.blade = 0;
    else
      weapon.grip = 0;
    Open(update);
    (void)update(0);
    if (!blade)
      (void)update(0);
    Check(false);
    return;
  }
  if (mode == "unoccupied-gems-lazy") {
    if (category == 0)
      std::span(owner.items.grips).back().gemSlots = 0;
    else
      std::span(owner.items.shields).back().base.gemSlots = 0;
    Open(update);
    if (category == 0) {
      Check(update(0) == 0);
      Check(update(0) == 0);
    }
    Menu_Framework_gems = nullptr;
    Menu_Framework_itemHelp = nullptr;
    Menu_Framework_itemNames = nullptr;
    Menu_Framework_stringBuf = nullptr;
    Wait(update);
    return;
  }
  if (mode == "accessory") {
    owner.items.armor[0].category = 7;
    Open(update);
    Wait(update);
    Menu_Framework_stringBuf = nullptr;
    Input_buttonRepeat = VS::Input::R1;
    Check(update(0) == 0);
    Check(formatted_description == reinterpret_cast<char *>(help.data()));
    return;
  }
  if (mode == "alias") {
    Open(update, 0, 31);
  } else {
    Open(update);
  }
  Wait(update);
  if (mode == "missing-text" || mode == "foreign-text") {
    expected_reason =
        mode == "foreign-text"
            ? "formatter text outside its borrowed container owners"
            : "missing used container item name or help text";
    if (mode == "missing-text")
      Menu_Framework_itemHelp = nullptr;
    Input_buttonRepeat = VS::Input::R1;
    (void)update(0);
    Check(false);
    return;
  }
  if (mode == "missing-frame") {
    expected_reason = "missing used container presentation frame";
    Render_frameScratch = {};
    Input_buttonsPressed.allButtons = VS::Input::Cross;
    (void)update(0);
    Check(false);
    return;
  }
  if (mode == "current-text" || mode == "alias") {
    Input_buttonRepeat = VS::Input::R1;
    Check(update(0) == 0);
    Check(formatted_name != nullptr);
    Check(std::ranges::count(calls, kChange,
                             [](Call call) { return call[0]; }) == 1);
    return;
  }
  Input_buttonsPressed.allButtons = VS::Input::Cross | VS::Input::R1;
  Check(update(0) == 0);
  Check(Menu_Container_detailsViewPhase == 2);
  ready = -7;
  Check(update(0) == -7);
  Check(update((kRow << 8) | 1) == 0);
  if (category == 2) {
    Check(!Sprites().empty());
    Check(std::ranges::count(calls, kIcon, [](Call call) { return call[0]; }) ==
          1);
  }
}

void IconProbe() {
  Menu_Framework_menuMemory.opaqueA0[0] = 1;
  if (mode == "missing") {
    expected_reason = "missing used container presentation frame";
    Render_frameScratch = {};
  }
  if (mode == "bounds") {
    expected_reason = "outside current scene ordering table";
    Render_frameScratch.orderingTable = ordering[0].data();
  }
  if (mode == "lazy") {
    Menu_Framework_menuItems = nullptr;
    Menu_Container_containerData = nullptr;
    Menu_Framework_itemNames = nullptr;
    Menu_Framework_itemHelp = nullptr;
  }
  if (mode == "alias") {
    rows[31].x = 60;
    detail::DrawLiveCategoryIcons(0, 1);
    Check(rows[31].rowIcon == 24);
    Check(Sprites().size() == 7);
    return;
  }
  if (mode == "header-missing") {
    expected_reason = "missing current container menu row workspace";
    Menu_Framework_menuItems = nullptr;
    detail::DrawLiveCategoryIcons(0, 1);
    Check(false);
    return;
  }
  detail::DrawLiveCategoryIcons(0, 0);
  const auto actual = Sprites();
  if (!expected_reason.empty()) {
    Check(false);
    return;
  }
  if (mode == "current-owner") {
    const auto previous = primitives[0];
    Frame(1);
    detail::DrawLiveCategoryIcons(0, 0);
    Check(Sprites() == actual);
    Check(previous == primitives[0]);
    return;
  }
  Frame(0);
  for (int icon = 0; icon < 8; ++icon) {
    const int x = (((icon + 1) & 7) * 16) + 32;
    auto *packet = render::QueueLiveSpriteWithTexturePage(
        0x80, static_cast<int>(fw::PackXy(x, -16)), 0x100010,
        ordering[0].data() + 15);
    packet[4] =
        (120 + icon * 16) | 0x8000 | (icon == 0 ? 0x37FD0000U : 0x37FE0000U);
  }
  Check(actual.size() == 8);
  Check(Sprites() == actual);
}

void DispatcherProbe() {
  if (mode == "row-bounds") {
    expected_reason = "detail row outside current menu row workspace";
    (void)detail::UpdateLiveItemDetails((((40 << 8) | 1) << 4) | 1);
    Check(false);
    return;
  }
  if (mode == "missing") {
    expected_reason = "missing current container menu row workspace";
    Menu_Framework_menuItems = nullptr;
  }
  if (mode == "bounds")
    expected_reason = "category outside its six controllers";
  if (mode == "lazy") {
    ready = 0;
    Menu_Container_containerData = nullptr;
    Menu_Framework_itemNames = nullptr;
    Menu_Framework_itemHelp = nullptr;
    Render_frameScratch = {};
  }
  if (mode == "missing-after-ready")
    expected_reason = "missing current container menu row workspace";
  const int selected_category = mode == "bounds" ? 6 : 1;
  Check(detail::UpdateLiveItemDetails((((kRow << 8) | 1) << 4) |
                                      selected_category) == 0);
  if (!expected_reason.empty()) {
    Check(false);
    return;
  }
  if (mode == "current-owner")
    Check(replacement_rows[31].itemPage == 1);
  if (mode == "lazy")
    return;
  Check(Menu_Container_detailsViewPhase == 1);
  Check(detail::UpdateLiveItemDetails(0) == 0);
  for (int tick = 0; tick < 11; ++tick)
    Check(detail::UpdateLiveItemDetails(0) == 0);
  Input_buttonsPressed.allButtons = VS::Input::Cross;
  Check(detail::UpdateLiveItemDetails(0) == 0);
  if (mode == "close-fullword") {
    Menu_Container_detailsClosedWithTriangle = 0x10000;
    Check(detail::UpdateLiveItemDetails(0) == -2);
  } else {
    ready = -7;
    Check(detail::UpdateLiveItemDetails(0) == -7);
    Check(Menu_Container_detailsViewPhase == 0);
    Check(Menu_Container_categoryIconsVisible == 1);
    Check(Menu_Container_controlsPanelMode == 1);
  }
}
} // namespace

namespace VS::Menu {
void PlayLeaveSound() { Record(kLeave); }
void PlayInvalidSound() {}
} // namespace VS::Menu

namespace VS::Menu::Framework {
MenuRow &LiveMenuRow(int row) {
  Record(kRowRead, row);
  return reinterpret_cast<MenuRow &>(Menu_Framework_menuItems[row]);
}
void ClearLiveMenuExcept(int row) { Record(kClear, row); }
int AreLiveMenuRowsIdle() {
  Record(kReady);
  return ready;
}
void ContinueLiveItemDetailsTransition(int row) { Record(kLayout, row); }
void UpdateLiveEquipmentStatPages(int command) { Record(kLayout, command); }
void UpdateLivePrimaryStatPanel(int command) { Record(kLayout, command); }
void UpdateLiveDpPpBars(int command) { Record(kBars, command); }
void FlyOutLiveRowLeft(int row) { Record(kLayout, row); }
void SetLiveWeaponStats(int slot) { Statistics(slot, 0); }
void SetLiveBladeStats(int slot) { Statistics(slot, 1); }
void SetLiveGripStats(int slot) { Statistics(slot, 2); }
void SetLiveShieldStats(int slot) { Statistics(slot, 3); }
void SetLiveArmorStats(int slot) { Statistics(slot, 4); }
void SetLiveGemStats(int slot) { Statistics(slot, 5); }
Menu_Framework_MenuRow *CreateLiveWeaponComponentRow(int row, int slot,
                                                     int slide) {
  Record(kComponent, row, slot);
  (void)slide;
  return &Menu_Framework_menuItems[row];
}
Menu_Framework_MenuRow *CreateLiveShieldGemRow(int row, int slot, int slide) {
  return CreateLiveWeaponComponentRow(row, slot, slide);
}
void FormatLiveInventoryWeaponRow(Items_InventoryWeapon *record, char *text[],
                                  int *row_type, void *buffer) {
  Format(record->name, text, row_type, buffer);
}
void FormatLiveInventoryShieldRow(Items_InventoryShield *, char *text[],
                                  int *row_type, void *buffer) {
  Format(reinterpret_cast<char *>(Menu_Framework_itemNames), text, row_type,
         buffer);
}
void FormatLiveBladeRow(Items_InventoryBlade *, char *text[], int *row_type,
                        void *buffer) {
  Format(reinterpret_cast<char *>(Menu_Framework_itemNames), text, row_type,
         buffer);
}
void FormatLiveGripRow(Items_InventoryGrip *, char *text[], int *row_type,
                       void *buffer) {
  Format(reinterpret_cast<char *>(Menu_Framework_itemHelp), text, row_type,
         buffer);
}
void FormatLiveInventoryArmorRow(Items_InventoryArmor *armor, char *text[],
                                 int *row_type, void *buffer) {
  Format(reinterpret_cast<char *>(Menu_Framework_itemNames), text, row_type,
         buffer, armor->category == 7);
}
void FormatLiveGemRow(Items_InventoryGem *, char *text[], int *row_type,
                      void *buffer) {
  Format(reinterpret_cast<char *>(Menu_Framework_itemNames), text, row_type,
         buffer);
}
void UpdateLiveItemDetailsSelection(int row, char *text[], u_int type,
                                    int page) {
  Check(text[0] == formatted_name);
  Check(text[1] == formatted_description);
  Check(type == 0x7C050000);
  Check(page == 1);
  if (mode == "current-text") {
    Check(text[0][0] == 's');
    Check(text[1][0] == 't');
    Check(Menu_Framework_menuItems == replacement_rows.data());
  }
  Record(kChange, row, page);
}
void DrawLiveRowIcon(int type, int x, int y) {
  Check(type == 0x116 && x == 256 && y == 32);
  Record(kIcon, type);
}
void ResetLiveItemDetailCursor() { Record(kReset); }
void SetLiveCommandLabel(int command) { Record(kCommand, command); }
void DrawLiveTabNavigation(int command) { Record(kTabs, command); }
void UpdateLiveItemDetails() { Record(kDetails); }
} // namespace VS::Menu::Framework

int main(int argc, char **argv) {
  if (argc != 5)
    return 2;
  identifier = std::atoi(argv[1]);
  mode = argv[2];
  category = identifier >= 2953 && identifier <= 2958 ? identifier - 2953 : 1;
  Initialize();
  LoadFonts(argv[3]);
  Check(VS::Core::SetLogFile(argv[4]));
  VS::Core::SetLogObserver([](const VS::Core::LogRecord &record) {
    if (record.level != VS::Core::LogLevel::Error)
      return;
    const bool expected =
        !expected_reason.empty() &&
        record.message.find(expected_reason) != std::string_view::npos &&
        FailureState() && passed;
    std::fprintf(stderr, "diagnostic=%.*s; boundaries=%s\n",
                 static_cast<int>(record.message.size()), record.message.data(),
                 expected ? "PASS" : "FAIL");
    VS::Core::FlushLogFile();
    std::_Exit(expected ? 85 : 86);
  });
  if (identifier == 2944)
    IconProbe();
  else if (identifier == 2959)
    DispatcherProbe();
  else
    ControllerProbe();
  Check(expected_reason.empty());
  std::cout << "boundaries=" << (passed ? "PASS" : "FAIL") << '\n';
  VS::Core::FlushLogFile();
  return passed ? 0 : 79;
}
