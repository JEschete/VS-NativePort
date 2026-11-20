// Hand-maintained (tools/port/owned.csv).
// Live binding for the item rows (ItemRows.hpp): defines the item texts
// (ItemRowsLiveState.hpp) and implements the hosts over them, the item tables
// the menus show, the battle string context, the menu rows and the disc queue.
#include "Menu/Framework/ItemRowsLive.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

#include "Battle/TargetingState.h"
#include "Content/CdDriveLive.hpp"
#include "Content/DiscLayout.h"
#include "Content/DiscQueue.hpp"
#include "Content/StringTables.hpp"
#include "Core/Log.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Items/EquipmentLive.hpp"
#include "Items/InventoryLive.hpp"
#include "Items/ItemRecords.hpp"
#include "Items/ItemRecordsLive.hpp"
#include "Legacy/Psx.h"
#include "Menu/Container/ContainerDetailHelpersLive.hpp"
#include "Menu/Container/ItemView.hpp"
#include "Menu/Container/Storage.hpp"
#include "Menu/Framework/EquipmentDetailsLiveState.hpp"
#include "Menu/Framework/Framework.hpp"
#include "Menu/Framework/FrameworkLive.hpp"
#include "Menu/Framework/FrameworkLiveState.hpp"
#include "Menu/Framework/ItemRows.hpp"
#include "Menu/Framework/ItemRowsLiveHost.hpp"
#include "Menu/Framework/ItemRowsLiveState.hpp"
#include "Render/EncodedText.hpp"
#include "Render/EncodedTextLive.hpp"

namespace itemhelp_text = VS::Content::text::itemhelp_bin;
namespace itemname_text = VS::Content::text::itemname_bin;

namespace VS::Menu::Framework {
namespace {
// The typed constants name the disc's tables.
static_assert(kItemNamesBytes == VS_ITEMNAME_BIN_SIZE);
static_assert(kItemHelpBytes == VS_ITEMHELP_BIN_SIZE);
static_assert(kMenu12Bytes == VS_MENU12_BIN_SIZE);
static_assert(kRowDescriptionBufferBytes == kStringBufferBytes);
static_assert(item_text::kBattleKnifeDesc ==
              itemhelp_text::index::kBattleKnifeDesc);
static_assert(item_text::kGemFeldspar == itemhelp_text::index::kGemFeldspar);
static_assert(item_text::kCureRootDesc == itemhelp_text::index::kCureRootDesc);
static_assert(item_text::kWarriorEquipment ==
              itemhelp_text::index::kWarriorEquipment);
static_assert(item_text::kWeaponBasePrestige ==
              itemhelp_text::index::kWeaponBasePrestige);
static_assert(item_text::kArmorBasePrestige ==
              itemhelp_text::index::kArmorBasePrestige);
static_assert(item_text::kDagger == itemhelp_text::index::kDagger);
static_assert(item_text::kBlunt == itemhelp_text::index::kBlunt);
static_assert(item_text::kOneHanded == itemhelp_text::index::kOneHanded);
static_assert(item_text::kPhantomPointsDesc ==
              itemhelp_text::index::kPhantomPointsDesc);
static_assert(item_text::kHumanClassDesc ==
              itemhelp_text::index::kHumanClassDesc);
static_assert(item_text::kPhysicalAffinityDesc ==
              itemhelp_text::index::kPhysicalAffinityDesc);
static_assert(item_text::kBluntTypeDesc ==
              itemhelp_text::index::kBluntTypeDesc);
static_assert(item_text::kRangeDesc == itemhelp_text::index::kRangeDesc);
static_assert(item_text::kWeaponPrestige ==
              itemhelp_text::index::kWeaponPrestige);
static_assert(item_text::kArmorPrestige ==
              itemhelp_text::index::kArmorPrestige);
static_assert(item_text::kRoodNecklace == itemhelp_text::index::kRoodNecklace);
static_assert(item_text::kShortGrip == itemhelp_text::index::kShortGrip);
static_assert(item_text::kArmorCategory ==
              itemhelp_text::index::kArmorCategory);
static_assert(item_text::kAttackStr == itemhelp_text::index::kAttackStr);
static_assert(item_text::kDefStr == itemhelp_text::index::kDefStr);
static_assert(item_text::kArmorBasePrestigeOffset ==
              itemhelp_text::offset::kArmorBasePrestige);
static_assert(item_text::kArmorCategoryOffset ==
              itemhelp_text::offset::kArmorCategory);
static_assert(item_text::kClassTemplateOffset ==
              itemhelp_text::offset::kClassTemplate);
static_assert(item_text::kWeaponDescTemplateOffset ==
              itemhelp_text::offset::kWeaponDescTemplate);
static_assert(item_text::kGripTemplateOffset ==
              itemhelp_text::offset::kGripTemplate);
static_assert(item_text::kArmorTemplateOffset ==
              itemhelp_text::offset::kArmorTemplate);
static_assert(item_text::kNoneOffset == itemhelp_text::offset::kNone);
static_assert(item_text::kNoGemsOffset == itemhelp_text::offset::kNoGems);
static_assert(item_text::kEndsSetupOffset == itemhelp_text::offset::kEndsSetup);
static_assert(item_text::kItem0Name == itemname_text::index::kItem0);
static_assert(item_text::kWoodName == itemname_text::index::kWood);
static_assert(sizeof(*itemNames) == kItemNameRowBytes);

// decomp: vs_mainMenu_weaponHands' module image values (2D10.c:157).
constexpr std::array<u_char, kWeaponHandCategories> kInitialWeaponHands{
    0, 0, 1, 0, 1, 0, 1, 1, 1, 0,
};

// The read the last load queued (_itemNamesCdQueueSlot, 2D10.c:1138) and the
// load state (_itemNamesLoading, 2D10.c:1142).
VS::Content::DiscQueueSlot* g_item_text_request = nullptr;
ItemTextLoadState g_item_text_load;

void Warn(std::string_view what) {
  VS::Core::LogWarn("menu.framework", "{}", what);
}

[[nodiscard]] std::uint8_t* Bytes(void* block) {
  return static_cast<std::uint8_t*>(block);
}

// The text tables in the loaded block; empty before the first load.
[[nodiscard]] std::span<const std::uint8_t> HelpBytes() {
  if (itemHelp == nullptr) {
    return {};
  }
  return {Bytes(itemHelp), kItemHelpBytes};
}

[[nodiscard]] std::span<const std::uint8_t> NameBytes() {
  if (itemNames == nullptr) {
    return {};
  }
  return {Bytes(itemNames), kItemNamesBytes};
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
template <typename Record>
[[nodiscard]] std::optional<Record> Referenced(
    const Record* table, std::span<const Record> inventory,
    std::span<const Record> container, int reference) {
  std::span<const Record> bound;
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
  const Record* record =
      Items::ReferencedSlot(bound, static_cast<std::uint32_t>(reference));
  if (record == nullptr) {
    return std::nullopt;
  }
  return *record;
}

[[nodiscard]] VS::Items::ContainerData* OpenContainer() {
  return VS::Menu::container::containerData == nullptr
             ? nullptr
             : &VS::Menu::container::containerData->items;
}

template <typename Record, std::size_t Count>
[[nodiscard]] std::span<const Record> ContainerTable(
    std::array<Record, Count> VS::Items::ContainerData::* table) {
  VS::Items::ContainerData* container = OpenContainer();
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
      return reinterpret_cast<char*>(itemHelp) + text.offset;
    case TextSource::kNames:
      return reinterpret_cast<char*>(itemNames) + text.offset;
    case TextSource::kBuffer:
      return static_cast<char*>(buffer);
    case TextSource::kWeaponName:
      return weapon_name;
    case TextSource::kNone:
      break;
  }
  return nullptr;
}

template <typename Record>
using RowFormat = ItemRowText (*)(ItemTextHost&, const Record&,
                                  std::span<std::uint8_t>);

// Runs the typed formatter over the record and writes the texts it sets and
// the row type back in the original's shape.
template <typename Record>
void FormatRecordRow(RowFormat<Record> format, const Record* record,
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
  const ItemRowText text = format(host, *record, bytes);
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
    itemNames = static_cast<char (*)[kItemNameRowBytes]>(block);
    itemHelp = reinterpret_cast<u_short*>(Bytes(block) + kItemNamesBytes);
    menu12Text = reinterpret_cast<u_short*>(Bytes(block) + kItemNamesBytes +
                                            kItemHelpBytes);
    // The three files are contiguous on the disc.
    const VS::Content::DiscFile file{.lba = VS_ITEMNAME_BIN_LBA,
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
    return g_item_text_request->state == VS::Content::DiscRequestState::kLoaded;
  }

  void ReleaseItemTextRequest() override {
    Content::ReleaseLiveDiscRequest(g_item_text_request);
    g_item_text_request = nullptr;
  }

  [[nodiscard]] bool ItemTextAllocated() override {
    return itemNames != nullptr;
  }

  void FreeItemText() override {
    Core::Memory::FreeLiveFromHighEnd(itemNames);
    itemNames = nullptr;
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
  return weaponHands;
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
  VS::Items::WeaponLoadout loadout{};
  if (VS::Menu::Framework::weapons == VS::Items::inventory.weapons.data()) {
    Items::LoadLiveInventoryWeaponStats(&loadout, &weapon);
  } else if (VS::Items::ContainerData* container = OpenContainer()) {
    VS::Menu::container::BuildLiveWeaponStats(&loadout, &weapon, container);
  } else {
    return std::nullopt;
  }
  return loadout;
}

std::optional<Items::ShieldLoadout> LiveItemRowsHost::InventoryShieldLoadout(
    const Items::InventoryShield& shield) {
  VS::Items::ShieldLoadout loadout{};
  if (VS::Menu::Framework::shields == VS::Items::inventory.shields.data()) {
    Items::LoadLiveInventoryShieldStats(&loadout, &shield);
  } else if (VS::Items::ContainerData* container = OpenContainer()) {
    VS::Menu::container::BuildLiveShieldStats(&loadout, &shield, container);
  } else {
    return std::nullopt;
  }
  return loadout;
}

std::optional<Items::InventoryWeapon> LiveItemRowsHost::Weapon(int reference) {
  return Referenced<VS::Items::InventoryWeapon>(
      VS::Menu::Framework::weapons, VS::Items::inventory.weapons,
      ContainerTable(&VS::Items::ContainerData::weapons), reference);
}

std::optional<Items::InventoryBlade> LiveItemRowsHost::Blade(int reference) {
  return Referenced<VS::Items::InventoryBlade>(
      VS::Menu::Framework::blades, VS::Items::inventory.blades,
      ContainerTable(&VS::Items::ContainerData::blades), reference);
}

std::optional<Items::InventoryGrip> LiveItemRowsHost::Grip(int reference) {
  return Referenced<VS::Items::InventoryGrip>(
      VS::Menu::Framework::grips, VS::Items::inventory.grips,
      ContainerTable(&VS::Items::ContainerData::grips), reference);
}

std::optional<Items::InventoryShield> LiveItemRowsHost::Shield(int reference) {
  return Referenced<VS::Items::InventoryShield>(
      VS::Menu::Framework::shields, VS::Items::inventory.shields,
      ContainerTable(&VS::Items::ContainerData::shields), reference);
}

std::optional<Items::InventoryArmor> LiveItemRowsHost::Armor(int reference) {
  return Referenced<VS::Items::InventoryArmor>(
      VS::Menu::Framework::armor, VS::Items::inventory.armor,
      ContainerTable(&VS::Items::ContainerData::armor), reference);
}

std::optional<Items::InventoryGem> LiveItemRowsHost::Gem(int reference) {
  return Referenced<VS::Items::InventoryGem>(
      VS::Menu::Framework::gems, VS::Items::inventory.gems,
      ContainerTable(&VS::Items::ContainerData::gems), reference);
}

std::span<std::uint8_t> LiveItemRowsHost::StringBuffer() {
  if (stringBuf == nullptr) {
    return {};
  }
  return {Bytes(stringBuf), kStringBufferBytes};
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

MenuRow* CreateLiveWeaponComponentRow(int row, int weapon_index, int slide) {
  LiveItemRowsHost host;
  return CreateWeaponComponentRow(host, row, weapon_index, (slide & 1) != 0);
}

MenuRow* CreateLiveShieldGemRow(int row, int shield_index, int slide) {
  LiveItemRowsHost host;
  return CreateShieldGemRow(host, row, shield_index, (slide & 1) != 0);
}

void FormatLiveWeaponRow(VS::Items::WeaponLoadout* weapon, char* menu_text[],
                         int* row_type, void* buffer) {
  FormatRecordRow<VS::Items::WeaponLoadout>(FormatWeaponRow, weapon, menu_text,
                                            row_type, buffer, nullptr);
}

void FormatLiveInventoryWeaponRow(VS::Items::InventoryWeapon* weapon,
                                  char* menu_text[], int* row_type,
                                  void* buffer) {
  FormatRecordRow<VS::Items::InventoryWeapon>(
      FormatInventoryWeaponRow, weapon, menu_text, row_type, buffer,
      weapon == nullptr ? nullptr : weapon->name.data());
}

void FormatLiveBladeRow(VS::Items::InventoryBlade* blade, char* menu_text[],
                        int* row_type, void* buffer) {
  FormatRecordRow<VS::Items::InventoryBlade>(FormatBladeRow, blade, menu_text,
                                             row_type, buffer, nullptr);
}

void FormatLiveGripRow(VS::Items::InventoryGrip* grip, char* menu_text[],
                       int* row_type, void* buffer) {
  FormatRecordRow<VS::Items::InventoryGrip>(FormatGripRow, grip, menu_text,
                                            row_type, buffer, nullptr);
}

void FormatLiveShieldRow(VS::Items::ShieldLoadout* shield, char* menu_text[],
                         int* row_type, void* buffer) {
  FormatRecordRow<VS::Items::ShieldLoadout>(FormatShieldRow, shield, menu_text,
                                            row_type, buffer, nullptr);
}

void FormatLiveInventoryShieldRow(VS::Items::InventoryShield* shield,
                                  char* menu_text[], int* row_type,
                                  void* buffer) {
  FormatRecordRow<VS::Items::InventoryShield>(
      FormatInventoryShieldRow, shield, menu_text, row_type, buffer, nullptr);
}

void FormatLiveArmorRow(VS::Items::ArmorLoadout* armor, char* menu_text[],
                        int* row_type, void* buffer) {
  FormatRecordRow<VS::Items::ArmorLoadout>(FormatArmorRow, armor, menu_text,
                                           row_type, buffer, nullptr);
}

void FormatLiveInventoryArmorRow(VS::Items::InventoryArmor* armor,
                                 char* menu_text[], int* row_type,
                                 void* buffer) {
  FormatRecordRow<VS::Items::InventoryArmor>(
      FormatInventoryArmorRow, armor, menu_text, row_type, buffer, nullptr);
}

void FormatLiveGemRow(VS::Items::InventoryGem* gem, char* menu_text[],
                      int* row_type, void* buffer) {
  FormatRecordRow<VS::Items::InventoryGem>(FormatGemRow, gem, menu_text,
                                           row_type, buffer, nullptr);
}

void FormatLiveMiscItemRow(VS::Items::InventoryMisc* misc, char* menu_text[],
                           int* row_type, void* buffer) {
  FormatRecordRow<VS::Items::InventoryMisc>(FormatMiscItemRow, misc, menu_text,
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
  itemNames = nullptr;
  std::ranges::copy(kInitialWeaponHands, weaponHands);
}
}  // namespace VS::Menu::Framework

// decomp: vs_mainMenu_itemHelp (MENU/MAINMENU.PRG 2D10.c:42)
u_short* VS::Menu::Framework::itemHelp = nullptr;
// decomp: vs_mainMenu_itemNames (MENU/MAINMENU.PRG 2D10.c:150): the module
// image value is null.
char (*VS::Menu::Framework::itemNames)[VS::Menu::Framework::kItemNameRowBytes] =
    nullptr;
// decomp: vs_mainMenu_menu12Text (MENU/MAINMENU.PRG 2D10.c:1132)
u_short* VS::Menu::Framework::menu12Text = nullptr;
// decomp: vs_mainMenu_weaponHands (MENU/MAINMENU.PRG 2D10.c:157)
u_char VS::Menu::Framework::weaponHands
    [VS::Menu::Framework::kWeaponHandCategories] = {0, 0, 1, 0, 1,
                                                    0, 1, 1, 1, 0};
