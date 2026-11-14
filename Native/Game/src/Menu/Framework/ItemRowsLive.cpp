// Hand-maintained (tools/port/owned.csv).
// Live binding for the item rows (ItemRows.hpp): defines the item texts
// (ItemRowsLiveState.h) and implements the hosts over them, the item tables
// the menus show, the battle string context, the menu rows and the disc queue.
#include "Menu/Framework/ItemRowsLive.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

#include "Content/CdDriveLive.hpp"
#include "Core/Log.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Items/EquipmentLive.hpp"
#include "Items/ItemRecordsLive.hpp"
#include "Menu/Container/ContainerDetailHelpersLive.hpp"
#include "Menu/Framework/Framework.hpp"
#include "Menu/Framework/FrameworkLive.hpp"
#include "Menu/Framework/ItemRows.hpp"
#include "Menu/Framework/ItemRowsLiveHost.hpp"
#include "Render/EncodedText.hpp"
#include "Render/EncodedTextLive.hpp"

extern "C" {
#include "Audio/Seq/Channel.h"
#include "Battle/TargetingState.h"
#include "Content/DiscLayout.h"
#include "Content/StringTables.h"
#include "DiscQueue.h"
#include "Items/Inventory.h"
#include "Items/Loadout.h"
#include "Legacy/Psx.h"
#include "Menu/Container/ItemView.h"
#include "Menu/Container/Storage.h"
#include "Menu/Framework/EquipmentDetailsLiveState.h"
#include "Menu/Framework/FrameworkLiveState.h"
#include "Menu/Framework/ItemRowsLiveState.h"
}

namespace VS::Menu::Framework {
namespace {
// The typed constants name the disc's tables.
static_assert(kItemNamesBytes == VS_ITEMNAME_BIN_SIZE);
static_assert(kItemHelpBytes == VS_ITEMHELP_BIN_SIZE);
static_assert(kMenu12Bytes == VS_MENU12_BIN_SIZE);
static_assert(kRowDescriptionBufferBytes == kStringBufferBytes);
static_assert(item_text::kBattleKnifeDesc ==
              Text_Menu_ITEMHELP_BIN_Index_battleKnifeDesc);
static_assert(item_text::kGemFeldspar ==
              Text_Menu_ITEMHELP_BIN_Index_gemFeldspar);
static_assert(item_text::kCureRootDesc ==
              Text_Menu_ITEMHELP_BIN_Index_cureRootDesc);
static_assert(item_text::kWarriorEquipment ==
              Text_Menu_ITEMHELP_BIN_Index_warriorEquipment);
static_assert(item_text::kWeaponBasePrestige ==
              Text_Menu_ITEMHELP_BIN_Index_weaponBasePrestige);
static_assert(item_text::kArmorBasePrestige ==
              Text_Menu_ITEMHELP_BIN_Index_armorBasePrestige);
static_assert(item_text::kDagger == Text_Menu_ITEMHELP_BIN_Index_dagger);
static_assert(item_text::kBlunt == Text_Menu_ITEMHELP_BIN_Index_blunt);
static_assert(item_text::kOneHanded == Text_Menu_ITEMHELP_BIN_Index_oneHanded);
static_assert(item_text::kPhantomPointsDesc ==
              Text_Menu_ITEMHELP_BIN_Index_phantomPointsDesc);
static_assert(item_text::kHumanClassDesc ==
              Text_Menu_ITEMHELP_BIN_Index_humanClassDesc);
static_assert(item_text::kPhysicalAffinityDesc ==
              Text_Menu_ITEMHELP_BIN_Index_physicalAffinityDesc);
static_assert(item_text::kBluntTypeDesc ==
              Text_Menu_ITEMHELP_BIN_Index_bluntTypeDesc);
static_assert(item_text::kRangeDesc == Text_Menu_ITEMHELP_BIN_Index_rangeDesc);
static_assert(item_text::kWeaponPrestige ==
              Text_Menu_ITEMHELP_BIN_Index_weaponPrestige);
static_assert(item_text::kArmorPrestige ==
              Text_Menu_ITEMHELP_BIN_Index_armorPrestige);
static_assert(item_text::kRoodNecklace ==
              Text_Menu_ITEMHELP_BIN_Index_roodNecklace);
static_assert(item_text::kShortGrip == Text_Menu_ITEMHELP_BIN_Index_shortGrip);
static_assert(item_text::kArmorCategory ==
              Text_Menu_ITEMHELP_BIN_Index_armorCategory);
static_assert(item_text::kAttackStr == Text_Menu_ITEMHELP_BIN_Index_attackStr);
static_assert(item_text::kDefStr == Text_Menu_ITEMHELP_BIN_Index_defStr);
static_assert(item_text::kArmorBasePrestigeOffset ==
              Text_Menu_ITEMHELP_BIN_Offset_armorBasePrestige);
static_assert(item_text::kArmorCategoryOffset ==
              Text_Menu_ITEMHELP_BIN_Offset_armorCategory);
static_assert(item_text::kClassTemplateOffset ==
              Text_Menu_ITEMHELP_BIN_Offset_classTemplate);
static_assert(item_text::kWeaponDescTemplateOffset ==
              Text_Menu_ITEMHELP_BIN_Offset_weaponDescTemplate);
static_assert(item_text::kGripTemplateOffset ==
              Text_Menu_ITEMHELP_BIN_Offset_gripTemplate);
static_assert(item_text::kArmorTemplateOffset ==
              Text_Menu_ITEMHELP_BIN_Offset_armorTemplate);
static_assert(item_text::kNoneOffset == Text_Menu_ITEMHELP_BIN_Offset_none);
static_assert(item_text::kNoGemsOffset == Text_Menu_ITEMHELP_BIN_Offset_noGems);
static_assert(item_text::kEndsSetupOffset ==
              Text_Menu_ITEMHELP_BIN_Offset_endsSetup);
static_assert(item_text::kItem0Name == Text_Menu_ITEMNAME_BIN_Index_item0);
static_assert(item_text::kWoodName == Text_Menu_ITEMNAME_BIN_Index_wood);
static_assert(sizeof(*Menu_Framework_itemNames) == kItemNameRowBytes);

// decomp: vs_mainMenu_weaponHands' module image values (2D10.c:157).
constexpr std::array<u_char, kMenuWeaponHandCategories> kInitialWeaponHands{
    0, 0, 1, 0, 1, 0, 1, 1, 1, 0,
};

// The read the last load queued (_itemNamesCdQueueSlot, 2D10.c:1138) and the
// load state (_itemNamesLoading, 2D10.c:1142).
Content_DiscQueueSlot* g_item_text_request = nullptr;
ItemTextLoadState g_item_text_load;

void Warn(std::string_view what) { VS_LOG_WARN("menu.framework", "{}", what); }

[[nodiscard]] std::uint8_t* Bytes(void* block) {
  return static_cast<std::uint8_t*>(block);
}

// The text tables in the loaded block; empty before the first load.
[[nodiscard]] std::span<const std::uint8_t> HelpBytes() {
  if (Menu_Framework_itemHelp == nullptr) {
    return {};
  }
  return {Bytes(Menu_Framework_itemHelp), kItemHelpBytes};
}

[[nodiscard]] std::span<const std::uint8_t> NameBytes() {
  if (Menu_Framework_itemNames == nullptr) {
    return {};
  }
  return {Bytes(Menu_Framework_itemNames), kItemNamesBytes};
}

// The empty text a refused row shows.
[[nodiscard]] char* EmptyText() {
  static std::array<char, 1> empty{};
  empty.front() = static_cast<char>(Render::kTextTerminator);
  return empty.data();
}

// The record a 1-based reference names in the bound table: Ashley's
// inventory's or the open container's, told apart by address (the only
// tables the menus bind).
template <typename Legacy>
[[nodiscard]] std::optional<Items::Live::TypedRecordOf<Legacy>> Referenced(
    const Legacy* table, std::span<const Legacy> inventory,
    std::span<const Legacy> container, int reference) {
  std::span<const Legacy> bound;
  if (table != nullptr && table == inventory.data()) {
    bound = inventory;
  } else if (table != nullptr && table == container.data()) {
    bound = container;
  } else {
    Warn(
        "item rows: the item table is neither the inventory's nor the "
        "container's");
    return std::nullopt;
  }
  const Legacy* record =
      Items::ReferencedSlot(bound, static_cast<std::uint32_t>(reference));
  if (record == nullptr) {
    return std::nullopt;
  }
  return Items::Live::Typed(*record);
}

// The menu rows are vs_battle_menuItems' records viewed in place
// (FrameworkLive.cpp pins the layout).
[[nodiscard]] Menu_Framework_MenuRow* LegacyRow(MenuRow* row) {
  return reinterpret_cast<Menu_Framework_MenuRow*>(row);
}

[[nodiscard]] Items_ContainerData* OpenContainer() {
  return Menu_Container_containerData == nullptr
             ? nullptr
             : &Menu_Container_containerData->items;
}

template <typename Legacy, std::size_t Count>
[[nodiscard]] std::span<const Legacy> ContainerTable(
    Legacy (Items_ContainerData::*table)[Count]) {
  Items_ContainerData* container = OpenContainer();
  if (container == nullptr) {
    return {};
  }
  return container->*table;
}

// The legacy pointer a row text names, for the copied menus.
[[nodiscard]] char* LegacyText(TextRef text, void* buffer, char* weapon_name) {
  switch (text.source) {
    case TextSource::kEmpty:
      return EmptyText();
    case TextSource::kHelp:
      return reinterpret_cast<char*>(Menu_Framework_itemHelp) + text.offset;
    case TextSource::kNames:
      return reinterpret_cast<char*>(Menu_Framework_itemNames) + text.offset;
    case TextSource::kBuffer:
      return static_cast<char*>(buffer);
    case TextSource::kWeaponName:
      return weapon_name;
    case TextSource::kNone:
      break;
  }
  return nullptr;
}

template <typename Legacy>
using RowFormat = ItemRowText (*)(ItemTextHost&,
                                  const Items::Live::TypedRecordOf<Legacy>&,
                                  std::span<std::uint8_t>);

// Runs the typed formatter over the record and writes the texts it sets and
// the row type back in the original's shape.
template <typename Legacy>
void FormatLegacyRow(RowFormat<Legacy> format, Legacy* record,
                     char* menu_text[], int* row_type, void* buffer,
                     char* weapon_name) {
  LiveItemRowsHost host;
  if (record == nullptr || menu_text == nullptr || row_type == nullptr) {
    host.Refuse("item rows: a null record, text array or row type");
    return;
  }
  const std::span<std::uint8_t> bytes =
      buffer == nullptr
          ? std::span<std::uint8_t>{}
          : std::span<std::uint8_t>{Bytes(buffer), kRowDescriptionBufferBytes};
  const ItemRowText text = format(host, Items::Live::Typed(*record), bytes);
  if (text.name) {
    menu_text[0] = LegacyText(*text.name, buffer, weapon_name);
  }
  if (text.description) {
    menu_text[1] = LegacyText(*text.description, buffer, weapon_name);
  }
  *row_type = static_cast<int>(text.row_type);
}

// The ITEMNAME/ITEMHELP/MENU12 block and its read.
class LiveItemTextLoadHost final : public ItemTextLoadHost {
 public:
  void Refuse(std::string_view what) override { Warn(what); }

  void StartItemTextLoad() override {
    g_item_text_request = nullptr;
    void* block = Core::Memory::AllocateLiveFromHighEnd(kItemTextBytes);
    if (block == nullptr) {
      Warn("item text: no memory for the text block");
      return;
    }
    Menu_Framework_itemNames = static_cast<char (*)[kItemNameRowBytes]>(block);
    Menu_Framework_itemHelp =
        reinterpret_cast<u_short*>(Bytes(block) + kItemNamesBytes);
    Menu_Framework_menu12Text = reinterpret_cast<u_short*>(
        Bytes(block) + kItemNamesBytes + kItemHelpBytes);
    // The three files are contiguous on the disc.
    const Content_DiscFile file{.lba = VS_ITEMNAME_BIN_LBA,
                                .size = static_cast<int>(kItemTextBytes)};
    g_item_text_request = Content::AllocateLiveDiscRequest(&file);
    if (g_item_text_request != nullptr) {
      Content::EnqueueLiveDiscRequest(g_item_text_request, block);
    } else {
      g_item_text_request =
          Content::CompleteLiveRefusedDiscRequest(&file, block);
    }
  }

  [[nodiscard]] std::optional<bool> ItemTextLoaded() override {
    if (g_item_text_request == nullptr) {
      return std::nullopt;
    }
    return g_item_text_request->state == Content_DiscRequestState_Loaded;
  }

  void ReleaseItemTextRequest() override {
    Content::ReleaseLiveDiscRequest(g_item_text_request);
    g_item_text_request = nullptr;
  }

  [[nodiscard]] bool ItemTextAllocated() override {
    return Menu_Framework_itemNames != nullptr;
  }

  void FreeItemText() override {
    Core::Memory::FreeLiveFromHighEnd(Menu_Framework_itemNames);
    Menu_Framework_itemNames = nullptr;
  }
};
}  // namespace

char* ResolveLiveItemRowText(TextRef text, void* buffer, char* weapon_name) {
  return LegacyText(text, buffer, weapon_name);
}

void LiveItemRowsHost::Refuse(std::string_view what) { Warn(what); }

std::span<const std::uint8_t> LiveItemRowsHost::HelpText() {
  return HelpBytes();
}

std::span<const std::uint8_t> LiveItemRowsHost::NameText() {
  return NameBytes();
}

std::span<const std::uint8_t> LiveItemRowsHost::WeaponHands() {
  return Menu_Framework_weaponHands;
}

Render::EncodedTextArguments LiveItemRowsHost::TextArguments() {
  return Render::LiveEncodedTextArguments();
}

void LiveItemRowsHost::SetStringArgument(std::size_t slot,
                                         std::span<const std::uint8_t> text) {
  if (slot >= std::size(Battle_stringContext.strings)) {
    Warn("item rows: a string argument past the string context");
    return;
  }
  Battle_stringContext.strings[slot] =
      reinterpret_cast<char*>(const_cast<std::uint8_t*>(text.data()));
}

std::optional<Items::WeaponLoadout> LiveItemRowsHost::InventoryWeaponLoadout(
    const Items::InventoryWeapon& weapon) {
  Items_InventoryWeapon legacy{};
  Items::Live::Commit(legacy, weapon);
  Items_WeaponLoadout loadout{};
  if (Menu_Framework_weapons == Audio_Seq_unusedInventory.weapons) {
    Items::LoadLiveInventoryWeaponStats(&loadout, &legacy);
  } else if (Items_ContainerData* container = OpenContainer()) {
    VS::Menu::container::BuildLiveWeaponStats(&loadout, &legacy, container);
  } else {
    return std::nullopt;
  }
  return Items::Live::Typed(loadout);
}

std::optional<Items::ShieldLoadout> LiveItemRowsHost::InventoryShieldLoadout(
    const Items::InventoryShield& shield) {
  Items_InventoryShield legacy{};
  Items::Live::Commit(legacy, shield);
  Items_ShieldLoadout loadout{};
  if (Menu_Framework_shields == Audio_Seq_unusedInventory.shields) {
    Items::LoadLiveInventoryShieldStats(&loadout, &legacy);
  } else if (Items_ContainerData* container = OpenContainer()) {
    VS::Menu::container::BuildLiveShieldStats(&loadout, &legacy, container);
  } else {
    return std::nullopt;
  }
  return Items::Live::Typed(loadout);
}

std::optional<Items::InventoryWeapon> LiveItemRowsHost::Weapon(int reference) {
  return Referenced<Items_InventoryWeapon>(
      Menu_Framework_weapons, Audio_Seq_unusedInventory.weapons,
      ContainerTable(&Items_ContainerData::weapons), reference);
}

std::optional<Items::InventoryBlade> LiveItemRowsHost::Blade(int reference) {
  return Referenced<Items_InventoryBlade>(
      Menu_Framework_blades, Audio_Seq_unusedInventory.blades,
      ContainerTable(&Items_ContainerData::blades), reference);
}

std::optional<Items::InventoryGrip> LiveItemRowsHost::Grip(int reference) {
  return Referenced<Items_InventoryGrip>(
      Menu_Framework_grips, Audio_Seq_unusedInventory.grips,
      ContainerTable(&Items_ContainerData::grips), reference);
}

std::optional<Items::InventoryShield> LiveItemRowsHost::Shield(int reference) {
  return Referenced<Items_InventoryShield>(
      Menu_Framework_shields, Audio_Seq_unusedInventory.shields,
      ContainerTable(&Items_ContainerData::shields), reference);
}

std::optional<Items::InventoryArmor> LiveItemRowsHost::Armor(int reference) {
  return Referenced<Items_InventoryArmor>(
      Menu_Framework_armor, Audio_Seq_unusedInventory.armor,
      ContainerTable(&Items_ContainerData::armor), reference);
}

std::optional<Items::InventoryGem> LiveItemRowsHost::Gem(int reference) {
  return Referenced<Items_InventoryGem>(
      Menu_Framework_gems, Audio_Seq_unusedInventory.gems,
      ContainerTable(&Items_ContainerData::gems), reference);
}

std::span<std::uint8_t> LiveItemRowsHost::StringBuffer() {
  if (Menu_Framework_stringBuf == nullptr) {
    return {};
  }
  return {Bytes(Menu_Framework_stringBuf), kStringBufferBytes};
}

void LiveItemRowsHost::DeactivateRow(int row) { DeactivateLiveRow(row); }

MenuRow* LiveItemRowsHost::SetMenuItem(const RowPlacement& placement,
                                       TextRef text) {
  return &SetLiveMenuItemText(placement, TextBytes(text));
}

std::span<const std::uint8_t> LiveItemRowsHost::TextBytes(TextRef text) {
  static constexpr std::array<std::uint8_t, 1> kEmpty{Render::kTextTerminator};
  const auto tail = [&text](std::span<const std::uint8_t> table) {
    return text.offset < table.size() ? table.subspan(text.offset)
                                      : std::span<const std::uint8_t>{};
  };
  switch (text.source) {
    case TextSource::kEmpty:
      return kEmpty;
    case TextSource::kHelp:
      return tail(HelpText());
    case TextSource::kNames:
      return tail(NameText());
    case TextSource::kBuffer:
      return StringBuffer();
    case TextSource::kNone:
    case TextSource::kWeaponName:
      break;
  }
  return {};
}

Menu_Framework_MenuRow* CreateLiveWeaponComponentRow(int row, int weapon_index,
                                                     int slide) {
  LiveItemRowsHost host;
  return LegacyRow(
      CreateWeaponComponentRow(host, row, weapon_index, (slide & 1) != 0));
}

Menu_Framework_MenuRow* CreateLiveShieldGemRow(int row, int shield_index,
                                               int slide) {
  LiveItemRowsHost host;
  return LegacyRow(
      CreateShieldGemRow(host, row, shield_index, (slide & 1) != 0));
}

void FormatLiveWeaponRow(Items_WeaponLoadout* weapon, char* menu_text[],
                         int* row_type, void* buffer) {
  FormatLegacyRow<Items_WeaponLoadout>(FormatWeaponRow, weapon, menu_text,
                                       row_type, buffer, nullptr);
}

void FormatLiveInventoryWeaponRow(Items_InventoryWeapon* weapon,
                                  char* menu_text[], int* row_type,
                                  void* buffer) {
  FormatLegacyRow<Items_InventoryWeapon>(
      FormatInventoryWeaponRow, weapon, menu_text, row_type, buffer,
      weapon == nullptr ? nullptr : weapon->name);
}

void FormatLiveBladeRow(Items_InventoryBlade* blade, char* menu_text[],
                        int* row_type, void* buffer) {
  FormatLegacyRow<Items_InventoryBlade>(FormatBladeRow, blade, menu_text,
                                        row_type, buffer, nullptr);
}

void FormatLiveGripRow(Items_InventoryGrip* grip, char* menu_text[],
                       int* row_type, void* buffer) {
  FormatLegacyRow<Items_InventoryGrip>(FormatGripRow, grip, menu_text, row_type,
                                       buffer, nullptr);
}

void FormatLiveShieldRow(Items_ShieldLoadout* shield, char* menu_text[],
                         int* row_type, void* buffer) {
  FormatLegacyRow<Items_ShieldLoadout>(FormatShieldRow, shield, menu_text,
                                       row_type, buffer, nullptr);
}

void FormatLiveInventoryShieldRow(Items_InventoryShield* shield,
                                  char* menu_text[], int* row_type,
                                  void* buffer) {
  FormatLegacyRow<Items_InventoryShield>(FormatInventoryShieldRow, shield,
                                         menu_text, row_type, buffer, nullptr);
}

void FormatLiveArmorRow(Items_ArmorLoadout* armor, char* menu_text[],
                        int* row_type, void* buffer) {
  FormatLegacyRow<Items_ArmorLoadout>(FormatArmorRow, armor, menu_text,
                                      row_type, buffer, nullptr);
}

void FormatLiveInventoryArmorRow(Items_InventoryArmor* armor, char* menu_text[],
                                 int* row_type, void* buffer) {
  FormatLegacyRow<Items_InventoryArmor>(FormatInventoryArmorRow, armor,
                                        menu_text, row_type, buffer, nullptr);
}

void FormatLiveGemRow(Items_InventoryGem* gem, char* menu_text[], int* row_type,
                      void* buffer) {
  FormatLegacyRow<Items_InventoryGem>(FormatGemRow, gem, menu_text, row_type,
                                      buffer, nullptr);
}

void FormatLiveMiscItemRow(Items_InventoryMisc* misc, char* menu_text[],
                           int* row_type, void* buffer) {
  FormatLegacyRow<Items_InventoryMisc>(FormatMiscItemRow, misc, menu_text,
                                       row_type, buffer, nullptr);
}

int LoadLiveItemText(int load) {
  LiveItemTextLoadHost host;
  return LoadItemText(g_item_text_load, host, load != 0);
}

int UnloadLiveItemTextWhenReady() {
  LiveItemTextLoadHost host;
  return UnloadItemTextWhenReady(g_item_text_load, host);
}

void ResetLiveItemRowsStatics() {
  Menu_Framework_itemNames = nullptr;
  std::ranges::copy(kInitialWeaponHands, Menu_Framework_weaponHands);
}
}  // namespace VS::Menu::Framework

extern "C" {
// decomp: vs_mainMenu_itemHelp (MENU/MAINMENU.PRG 2D10.c:42)
u_short* Menu_Framework_itemHelp = nullptr;
// decomp: vs_mainMenu_itemNames (MENU/MAINMENU.PRG 2D10.c:150): the module
// image value is null.
char (*Menu_Framework_itemNames)[VS::Menu::Framework::kItemNameRowBytes] =
    nullptr;
// decomp: vs_mainMenu_menu12Text (MENU/MAINMENU.PRG 2D10.c:1132)
u_short* Menu_Framework_menu12Text = nullptr;
// decomp: vs_mainMenu_weaponHands (MENU/MAINMENU.PRG 2D10.c:157)
u_char Menu_Framework_weaponHands[kMenuWeaponHandCategories] = {0, 0, 1, 0, 1,
                                                                0, 1, 1, 1, 0};
}
