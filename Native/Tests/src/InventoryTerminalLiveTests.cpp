#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <span>
#include <stdexcept>
#include <string_view>
#include <type_traits>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Audio/SfxChannelGroups.hpp"
#include "Audio/SoundPlaybackLive.hpp"
#include "Battle/Actor.h"
#include "Battle/Hud.h"
#include "Boot/OverlayReset.hpp"
#include "Boot/SessionLive.hpp"
#include "Content/CdDriveLive.hpp"
#include "Content/DataStore.hpp"
#include "Content/DiscLayout.h"
#include "Content/DiscReaderLive.hpp"
#include "Content/ModuleData.hpp"
#include "Core/Diagnostics.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "DiscQueue.h"
#include "Field/RoomRenderingLiveState.hpp"
#include "Items/InventoryLive.hpp"
#include "Menu/EquipmentDetailControllersLiveFixture.hpp"
#include "Menu/Framework/FrameworkLive.hpp"
#include "Menu/Framework/ItemRowsLive.hpp"
#include "Menu/Framework/Menus.hpp"
#include "Menu/Inventory/InventoryTerminalLive.hpp"
#include "Menu/InventoryTerminalWorld.hpp"
#include "Menu/Loot/LootScreen.h"
#include "Menu/Loot/LootTerminal.hpp"
#include "Menu/Loot/LootTerminalLive.hpp"
#include "Menu/OverlayLoadFixture.hpp"
#include "State/StateFlags.hpp"

namespace {
namespace inventory = VS::Menu::Inventory;
namespace framework = VS::Menu::Framework;
namespace content = VS::Content;
namespace loot = VS::Menu::Loot;
using equipment_detail_tests::ArrayRestore;
using equipment_detail_tests::ValueRestore;
constexpr std::string_view kItemModule = "MENU/MENU3.PRG";
constexpr std::string_view kLootModule = "MENU/MENUB.PRG";
constexpr unsigned kItemNavigation = 0x801053EC;
constexpr unsigned kLootNavigation = 0x80105454;
constexpr std::size_t kReadBudget = 32;
constexpr std::size_t kAnimationBudget = 32;
constexpr std::uint8_t kLootTeardown = 5;
constexpr std::uint8_t kContainerReturn = 8;
constexpr std::uint8_t kInventoryCountdown = 11;
constexpr unsigned kConfirm = 0x20;
constexpr unsigned kCancel = 0x40;
constexpr std::size_t kSoundSlots = 0x180;
constexpr std::size_t kAshleyActor = 0;
constexpr std::int32_t kLootTitleRow = 3;
constexpr std::uint8_t kBattleMenu = 4;
constexpr char kItemShortcut = 5;
constexpr loot::LootRow kTail{5, 47, 9, 5};

template <typename Entry>
bool IsBound(std::string_view module, unsigned address, Entry entry) {
  return content::ResolveModuleAddress(VS::Content::ModuleSymbols(), module,
                                       address) ==
         reinterpret_cast<void*>(entry);
}

int Navigate(std::string_view module, unsigned address, int initialize) {
  auto* entry = content::ResolveModuleAddress(VS::Content::ModuleSymbols(),
                                              module, address);
  if (entry == nullptr) throw std::runtime_error("missing inventory entry");
  return reinterpret_cast<int (*)(int)>(entry)(initialize);
}

void EnterEmptyNavigation(std::string_view module, unsigned address) {
  REQUIRE(Navigate(module, address, 2) == 0);
  REQUIRE(Navigate(module, address, 0) == 0);
  REQUIRE(VS::Menu::Framework::itemsText == nullptr);
}

struct TerminalSession {
  TerminalSession() { inventory::ResetInventoryTerminalSession(); }
  ~TerminalSession() { inventory::ResetInventoryTerminalSession(); }
};

struct TerminalFixture : equipment_detail_tests::LiveFixture {
  std::vector<std::byte> arena = std::vector<std::byte>(0x20000);
  VS::Core::Memory::ScopedLiveGameHeap heap{arena};
  TerminalSession session;
  ValueRestore<decltype(Battle_characterState)> saved_actor{
      Battle_characterState};
  ValueRestore<Battle_Actor*> saved_ashley{Battle_actors[kAshleyActor]};
  ValueRestore<int> saved_sound_group{VS::Audio::nextSfxChannelGroup7E};
  ValueRestore<decltype(VS::State::stateFlags)> saved_flags{
      VS::State::stateFlags};
  ValueRestore<decltype(VS::Input::settings)> saved_settings{
      VS::Input::settings};
  ValueRestore<decltype(VS::Menu::Framework::menuState)> saved_control{
      VS::Menu::Framework::menuState};
  ValueRestore<decltype(VS::Menu::Framework::actionSelection)> saved_action{
      VS::Menu::Framework::actionSelection};
  ValueRestore<decltype(Battle_hudHidden)> saved_hud{Battle_hudHidden};
  ValueRestore<decltype(Battle_hudSlideOffset)> saved_hud_offset{
      Battle_hudSlideOffset};
  ValueRestore<decltype(VS::Field::isFieldRenderingEnabled)> saved_field{
      VS::Field::isFieldRenderingEnabled};
  ValueRestore<int> saved_background{VS::Menu::Framework::backgroundFadeStep};
  ValueRestore<int> saved_restrictions{VS::Menu::Framework::actionRestrictions};
  ValueRestore<u_short*> saved_items{VS::Menu::Framework::itemsText};
  ValueRestore<int*> saved_types{VS::Menu::Framework::rowTypeBuf};
  ValueRestore<char> saved_freeze{Menu_Inventory_freezeTabArrows};
  ValueRestore<char> saved_title_step{
      Menu_Inventory_secondaryTitleAnimationStep};
  ValueRestore<char> saved_icons_step{
      Menu_Inventory_itemCategoryIconsAnimationStep};
  ValueRestore<char> saved_status_step{
      Menu_Inventory_statusCommandAnimationStep};
  ArrayRestore<u_short, std::size(Menu_Inventory_menuText)> saved_menu_text{
      Menu_Inventory_menuText};
  ArrayRestore<std::uint16_t, std::size(VS::Menu::menuStrings)> saved_titles{
      VS::Menu::menuStrings};
  std::array<int, framework::kRowTypeCount> row_types{};
  std::remove_pointer_t<decltype(Battle_characterState)> actor{};
  std::remove_pointer_t<decltype(actor.stats)> stats{};
  std::array<std::uint32_t, kSoundSlots> sound_offsets{};
  std::array<std::uint16_t, kSoundSlots> sound_banks{};
  VS::Audio::Playback::ScopedLiveEffectTables sounds{
      {.program_offsets = sound_offsets,
       .bank_selectors = sound_banks,
       .program_data = {}}};

  TerminalFixture() {
    framework::ResetLiveMainMenuStatics();
    BindInventory();
    Battle_characterState = &actor;
    Battle_actors[kAshleyActor] = &actor;
    actor.stats = &stats;
    sound_offsets.fill(std::numeric_limits<std::uint32_t>::max());
    VS::State::stateFlags.intro_state = 3;
    VS::Input::settings.cursor_memory = 0;
    VS::Menu::Framework::menuState = {};
    VS::Menu::Framework::actionSelection = {};
    VS::Menu::Framework::backgroundFadeStep = 0;
    VS::Menu::Framework::menuMemory = {};
    VS::Menu::Framework::actionRestrictions = 0;
    VS::Menu::Framework::itemsText = nullptr;
    VS::Menu::Framework::rowTypeBuf = row_types.data();
    Menu_Inventory_secondaryTitleEnabled = 0;
    Menu_Inventory_itemCategoryIconsEnabled = 0;
    Menu_Inventory_statusCommandState = 0;
    Menu_Inventory_secondaryTitleAnimationStep = 0;
    Menu_Inventory_itemCategoryIconsAnimationStep = 0;
    Menu_Inventory_statusCommandAnimationStep = 0;
    for (auto text : {std::span<u_short>(Menu_Inventory_menuText),
                      std::span<u_short>(VS::Menu::menuStrings)}) {
      std::ranges::fill(text, u_short{0xE7E7});
      std::fill_n(text.begin(), 32, u_short{32});
      text[32] = 0xE701;
    }
    inventory::BindLiveLootInventoryText(
        VS::tests::inventory_terminal::LootText(2));
  }
  ~TerminalFixture() {
    CloseList();
    framework::ResetLiveMainMenuStatics();
  }

  void CloseList() {
    if (VS::Menu::Framework::itemsText != nullptr) {
      static_cast<void>(framework::CloseLiveRowsAndRememberCursor());
    }
    framework::ClearLiveMenuExcept(framework::kNoRow);
  }
};

struct TextLoadingFixture : TerminalFixture {
  content::DataStore::ScopedOverride store;
  ArrayRestore<Content_DiscQueueSlot, std::size(Content_discRequestSlots)>
      saved_requests{Content_discRequestSlots};
  ValueRestore<Content_DiscRequestCounts> saved_request_counts{
      Content_discRequestCounts};
  ValueRestore<Content_DiscDrive> saved_drive{Content_discDriveState};
  ValueRestore<u_short*> saved_menu12{VS::Menu::Framework::menu12Text};

  TextLoadingFixture() {
    if (!content::DataStore::Open(R"(Native\data)")) {
      throw std::runtime_error(
          "menu terminal integration needs extracted data");
    }
    content::ResetLiveDiscDrive();
    content::InitializeLiveDiscQueue();
  }
  ~TextLoadingFixture() {
    if (VS::Core::Memory::LivePayloadCapacity(VS::Menu::Framework::itemNames) &&
        !UnloadText()) {
      VS::Core::Errorf("tests",
                       "menu terminal fixture could not drain its item text");
      std::abort();
    }
    content::ResetLiveDiscDrive();
  }
  bool CompleteTextRead() {
    for (std::size_t tick = 0; tick < kReadBudget; ++tick) {
      content::UpdateLiveDiscQueue();
      if (framework::LoadLiveItemText(0) != 0) return true;
    }
    return false;
  }
  bool UnloadText() {
    return CompleteTextRead() && framework::UnloadLiveItemTextWhenReady() != 0;
  }
  bool SettleRows() {
    for (std::size_t tick = 0; tick < kAnimationBudget; ++tick) {
      if (framework::AreLiveMenuRowsIdle() != 0) return true;
      ResetFrame();
      framework::UpdateLiveScreen();
    }
    return false;
  }
};

struct LootFixture : TextLoadingFixture {
  static constexpr std::array kConsolidationRows{loot::LootRow{1, 0, 0, 0},
                                                 loot::LootRow{6, 0, 0, 0},
                                                 loot::LootRow{6, 1, 0, 0}};
  ValueRestore<VS::Items::InventoryContents*> saved_loot_inventory{
      Menu_Loot_inventory};
  ValueRestore<VS::Items::InventoryContents*> saved_backup{
      Menu_Loot_inventoryBackup};
  ValueRestore<Menu_Loot_LootListItem*> saved_loot_rows{Menu_Loot_lootList};
  ValueRestore<Menu_Loot_LootListItem*> saved_working{
      Menu_Loot_lootListWorkingCopy};
  ValueRestore<decltype(VS::Menu::Framework::lootLists)> saved_room_loot{
      VS::Menu::Framework::lootLists};
  ValueRestore<decltype(VS::Menu::Framework::lootEntry)> saved_chest{
      VS::Menu::Framework::lootEntry};
  std::remove_pointer_t<decltype(VS::Menu::Framework::lootEntry)> chest{};
  ValueRestore<u_char> saved_count{Menu_Loot_lootListCount};
  ValueRestore<u_char> saved_offset{Menu_Loot_lootListOffset};
  ValueRestore<char> saved_selected{Menu_Loot_selectedLoot};
  ValueRestore<char> saved_previewed{Menu_Loot_previewedLoot};
  ValueRestore<char> saved_claimed{Menu_Loot_lootClaimed};
  ValueRestore<char> saved_controls{Menu_Loot_controlsEnabled};
  ValueRestore<char> saved_show_inventory{Menu_Loot_showInventory};
  ValueRestore<char> saved_show_loot{Menu_Loot_showLoot};
  ValueRestore<u_char> saved_inventory_step{
      Menu_Loot_inventoryCapacityFlyinTimer};
  ValueRestore<u_char> saved_loot_step{Menu_Loot_lootLeftEdge};

  LootFixture() {
    if (Menu_Loot_inventory != nullptr ||
        Menu_Loot_inventoryBackup != nullptr || Menu_Loot_lootList != nullptr ||
        Menu_Loot_lootListWorkingCopy != nullptr) {
      throw std::runtime_error("loot integration requires an unused workspace");
    }
    loot::ResetLootTerminalSession();
    VS::Menu::Framework::lootLists = nullptr;
    VS::Menu::Framework::lootEntry = &chest;
    std::ranges::fill(Menu_textBoxes, Menu_TextBoxState{});
  }
  ~LootFixture() { loot::ResetLootTerminalSession(); }
  int Open() {
    ResetFrame();
    std::uint8_t step = 0;
    auto* entry = content::ResolveModuleAddress(VS::Content::ModuleSymbols(),
                                                kLootModule, 0x80109E90);
    if (entry == nullptr) throw std::runtime_error("missing loot screen entry");
    return reinterpret_cast<int (*)(unsigned char*)>(entry)(&step);
  }
  void OpenAndLoad() {
    REQUIRE(Open() == 0);
    REQUIRE(CompleteTextRead());
    REQUIRE(Menu_Loot_inventory != nullptr);
    REQUIRE(Menu_Loot_inventoryBackup != nullptr);
    REQUIRE(Menu_Loot_lootList != nullptr);
    REQUIRE(Menu_Loot_lootListWorkingCopy != nullptr);
  }
  void BeginInventoryTransition() {
    REQUIRE(loot::UpdateLiveInventoryTransition(1) == 0);
    REQUIRE(loot::UpdateLiveInventoryTransition(0) == 0);
  }
  static void CheckReleased() {
    CHECK(Menu_Loot_inventory == nullptr);
    CHECK(Menu_Loot_inventoryBackup == nullptr);
    CHECK(Menu_Loot_lootList == nullptr);
    CHECK(Menu_Loot_lootListWorkingCopy == nullptr);
    CHECK(Battle_stringContext.strings[0] == nullptr);
  }
  static void SeedConsolidation() {
    auto& staged = *Menu_Loot_inventory;
    staged.misc[0] = {323, 2, 1};
    staged.misc[1] = {323, 3, 1};
    staged.blades[0].id = 17;
    VS::Items::inventory.misc[0] = {323, 80, 1};
    Menu_Loot_lootListCount = static_cast<u_char>(kConsolidationRows.size());
    for (std::size_t index = 0; index < kConsolidationRows.size(); ++index) {
      Menu_Loot_lootList[index] =
          std::bit_cast<Menu_Loot_LootListItem>(kConsolidationRows[index]);
    }
    Menu_Loot_lootList[loot::kLootRowCapacity - 1] =
        std::bit_cast<Menu_Loot_LootListItem>(kTail);
    REQUIRE(loot::UpdateLiveStackConsolidation(2) == 0);
    Menu_Loot_lootLeftEdge = 6;
  }
  void OpenConsolidationPrompt() {
    REQUIRE(loot::UpdateLiveStackConsolidation(0) == 0);
    REQUIRE(loot::UpdateLiveStackConsolidation(0) == 0);
    REQUIRE(SettleRows());
    REQUIRE(loot::UpdateLiveStackConsolidation(0) == 0);
  }
  static void AnswerConsolidation(bool confirm) {
    VS::Input::buttonsPressed = confirm ? kConfirm : kCancel;
    REQUIRE(loot::UpdateLiveStackConsolidation(0) == 0);
    VS::Input::buttonsPressed = 0;
    Menu_Loot_lootLeftEdge = 6;
    REQUIRE(loot::UpdateLiveStackConsolidation(0) == 0);
  }
  static void CheckRestoredRows() {
    for (std::size_t index = 0; index < kConsolidationRows.size(); ++index) {
      CHECK(std::bit_cast<loot::LootRow>(Menu_Loot_lootList[index]) ==
            kConsolidationRows[index]);
    }
    CHECK(std::bit_cast<loot::LootRow>(
              Menu_Loot_lootList[loot::kLootRowCapacity - 1]) == kTail);
  }
};
}  // namespace

TEST_CASE("Inventory terminals retain all six distinct shared-header entries",
          "[inventory-terminal-live][F02591][F02592][F02597]") {
  CHECK(IsBound(kItemModule, 0x80104530, &inventory::UpdateLiveSortMenu));
  CHECK(IsBound(kLootModule, 0x80104608, &inventory::UpdateLiveLootSortMenu));
  CHECK(IsBound(kItemModule, 0x80104788, &inventory::UpdateLiveDiscardMenu));
  CHECK(
      IsBound(kLootModule, 0x80104860, &inventory::UpdateLiveLootDiscardMenu));
  CHECK(IsBound(kItemModule, kItemNavigation, &inventory::UpdateLiveItemList));
  CHECK(IsBound(kLootModule, kLootNavigation,
                &inventory::UpdateLiveLootItemList));
}

TEST_CASE("Inventory shortcut and screen keep their original module entries",
          "[inventory-terminal-live][F02553][F02636]") {
  CHECK(IsBound("MENU/MAINMENU.PRG", 0x800FAEBC,
                &inventory::UpdateLiveMiscItemShortcut));
  CHECK(
      IsBound(kItemModule, 0x80108970, &inventory::UpdateLiveInventoryScreen));
}

TEST_CASE("F02597 Live Manage uses independent MENU3 and MENUB text and flags",
          "[inventory-terminal-live][F02597]") {
  TerminalFixture fixture;
  REQUIRE(Navigate(kItemModule, kItemNavigation, 1) == 0);
  REQUIRE(Navigate(kItemModule, kItemNavigation, 0) == 0);
  CHECK(fixture.FirstItem().name[0] == 1);
  CHECK(fixture.FirstItem().flags == 0);
  fixture.CloseList();
  VS::Boot::ResetMenubStatics();
  REQUIRE(Navigate(kLootModule, kLootNavigation, 1) == 0);
  REQUIRE(Navigate(kLootModule, kLootNavigation, 0) == 0);
  CHECK(fixture.FirstItem().name[0] == 2);
  CHECK(fixture.FirstItem().flags == 1);
}

TEST_CASE("F02597 Live loot does not inherit item-menu global restrictions",
          "[inventory-terminal-live][F02597]") {
  TerminalFixture fixture;
  fixture.AddWeapon();
  VS::Menu::Framework::actionRestrictions = 7;
  VS::Menu::Framework::menuMemory.opaque3A.currentCategory = 1;
  REQUIRE(Navigate(kItemModule, kItemNavigation, 2) == 0);
  REQUIRE(Navigate(kItemModule, kItemNavigation, 0) == 0);
  CHECK((fixture.FirstItem().flags & 1) != 0);
  fixture.CloseList();
  REQUIRE(Navigate(kLootModule, kLootNavigation, 2) == 0);
  REQUIRE(Navigate(kLootModule, kLootNavigation, 0) == 0);
  CHECK((fixture.FirstItem().flags & 1) == 0);
}

TEST_CASE(
    "F02597 overlay reload resets only its file-backed navigation controller",
    "[inventory-terminal-live][inventory-reload][F02597][menu-overlay-"
    "reload]") {
  const bool loot = GENERATE(false, true);
  TerminalFixture fixture;
  const auto module = loot ? kLootModule : kItemModule;
  const auto address = loot ? kLootNavigation : kItemNavigation;
  const auto other_module = loot ? kItemModule : kLootModule;
  const auto other_address = loot ? kItemNavigation : kLootNavigation;
  menu_overlay_tests::Load load(
      loot ? 12 : 4,
      loot ? Content_DiscFile{VS_MENUB_PRG_LBA, VS_MENUB_PRG_SIZE}
           : Content_DiscFile{VS_MENU3_PRG_LBA, VS_MENU3_PRG_SIZE});
  VS::Menu::Framework::menuMemory.opaque3A.currentCategory = 1;
  EnterEmptyNavigation(other_module, other_address);
  EnterEmptyNavigation(module, address);
  fixture.AddWeapon();
  CHECK(load.Poll() == 0);
  CHECK(Navigate(module, address, 0) == 0);
  CHECK(VS::Menu::Framework::itemsText == nullptr);
  CHECK(load.Complete() == 1);
  CHECK(Navigate(other_module, other_address, 0) == 0);
  CHECK(VS::Menu::Framework::itemsText == nullptr);
  REQUIRE(Navigate(module, address, 0) == 0);
  CHECK(VS::Menu::Framework::itemsText != nullptr);
}

TEST_CASE("MENUB text loads its original module bytes again after a cold reset",
          "[inventory-terminal-live][F02597]") {
  content::DataStore::ScopedOverride restore;
  TerminalSession session;
  REQUIRE(content::DataStore::Open(R"(Native\data)").has_value());
  const auto* store = content::DataStore::Get();
  if (store == nullptr) {
    FAIL("data store was not opened");
    return;
  }
  const auto* entry = store->Find(kLootModule);
  if (entry == nullptr) {
    FAIL("MENUB module was not extracted");
    return;
  }
  std::array<std::uint8_t, inventory::kLootInventoryTextBytes> expected{};
  REQUIRE(store->Read(*entry, 0x7A80, expected));
  CHECK(std::ranges::equal(inventory::LiveLootInventoryText(), expected));
  inventory::BindLiveLootInventoryText(
      VS::tests::inventory_terminal::LootText());
  VS::Boot::ResetMenubStatics();
  CHECK(std::ranges::equal(inventory::LiveLootInventoryText(),
                           VS::tests::inventory_terminal::LootText()));
  inventory::ResetInventoryTerminalSession();
  CHECK(std::ranges::equal(inventory::LiveLootInventoryText(), expected));
}

TEST_CASE("F02553 MAINMENU reload restarts the live shortcut title phase",
          "[inventory-terminal-live][inventory-reload][F02553][menu-overlay-"
          "reload]") {
  TextLoadingFixture fixture;
  const int logical_submenu = GENERATE(0, 4);
  inventory::UpdateLiveMiscItemShortcut(0);
  REQUIRE(fixture.UnloadText());
  framework::ClearLiveMenuExcept(framework::kNoRow);
  REQUIRE(fixture.SettleRows());
  {
    menu_overlay_tests::Load load(4, {VS_MENU3_PRG_LBA, VS_MENU3_PRG_SIZE});
    CHECK(load.Complete() == 1);
  }
  inventory::UpdateLiveMiscItemShortcut(0);
  CHECK(VS::Menu::Framework::itemNames == nullptr);
  menu_overlay_tests::Load load(logical_submenu,
                                {VS_MAINMENU_PRG_LBA, VS_MAINMENU_PRG_SIZE});
  VS::Menu::Framework::shortcutInvoked = kItemShortcut;
  CHECK(load.Complete() == 0);
  inventory::UpdateLiveMiscItemShortcut(0);
  CHECK(VS::Core::Memory::LivePayloadCapacity(VS::Menu::Framework::itemNames)
            .has_value());
  REQUIRE(fixture.UnloadText());
}

TEST_CASE("F02636 only MENU3 reload clears the live exit countdown",
          "[inventory-terminal-live][inventory-reload][F02636]") {
  TerminalFixture fixture;
  VS::Menu::Framework::itemNames = nullptr;
  VS::Menu::Framework::menuState.returnState = 1;
  std::uint8_t step = kContainerReturn;
  REQUIRE(inventory::UpdateLiveInventoryScreen(&step) == 0);
  REQUIRE(step == kInventoryCountdown);
  VS::Boot::ResetMenubStatics();
  CHECK(inventory::UpdateLiveInventoryScreen(&step) == 0);
  VS::Boot::ResetMenu3Statics();
  CHECK(inventory::UpdateLiveInventoryScreen(&step) == 1);
  CHECK(VS::Menu::Framework::menuState.currentMenu == kBattleMenu);
}

TEST_CASE("Loot terminal module entries retain their distinct call shapes",
          "[loot-terminal-live][F02843][F02844][F02845][F02864][F02865]") {
  CHECK(IsBound(kLootModule, 0x801066CC, &loot::UpdateLiveInventoryTransition));
  CHECK(IsBound(kLootModule, 0x80106B80, &loot::UpdateLiveDisassembly));
  CHECK(IsBound(kLootModule, 0x801073E0, &loot::UpdateLiveStackConsolidation));
  CHECK(IsBound(kLootModule, 0x80109750, &loot::UpdateLiveLootSelection));
  CHECK(IsBound(kLootModule, 0x80109E90, &loot::UpdateLiveLootScreen));
}

TEST_CASE(
    "F02843 MENUB reload restarts its controller without releasing storage",
    "[loot-terminal-live][inventory-reload][F02843][menu-overlay-reload]") {
  LootFixture fixture;
  fixture.OpenAndLoad();
  auto* const owned = Menu_Loot_inventory;
  fixture.BeginInventoryTransition();
  {
    menu_overlay_tests::Load load(4, {VS_MENU3_PRG_LBA, VS_MENU3_PRG_SIZE});
    CHECK(load.Complete() == 1);
  }
  Menu_Inventory_selectedItemCategoryIconOnTop = 1;
  REQUIRE(loot::UpdateLiveInventoryTransition(0) == 0);
  CHECK(Menu_Inventory_selectedItemCategoryIconOnTop == 0);
  framework::ClearLiveMenuExcept(framework::kNoRow);
  REQUIRE(fixture.SettleRows());
  menu_overlay_tests::Load load(12, {VS_MENUB_PRG_LBA, VS_MENUB_PRG_SIZE});
  CHECK(load.Complete() == 1);
  CHECK(Menu_Loot_inventory == owned);
  REQUIRE(loot::UpdateLiveInventoryTransition(0) == 0);
  CHECK(framework::LiveMenuRow(kLootTitleRow).state ==
        framework::RowState::kSlideX);
  CHECK(std::ranges::equal(inventory::LiveLootInventoryText(),
                           VS::tests::inventory_terminal::LootText(2)));
}

TEST_CASE(
    "F02865 live teardown and cold session permit another owned workspace",
    "[loot-terminal-live][F02865]") {
  const bool cold = GENERATE(false, true);
  LootFixture fixture;
  const auto refusals = loot::LootTerminalLiveRefusalCount();
  for (int visit = 0; visit < 2; ++visit) {
    fixture.OpenAndLoad();
    Battle_stringContext.strings[0] =
        Menu_Loot_inventory->weapons[0].name.data();
    if (cold) {
      const auto entry = +[] { return 0; };
      CHECK(VS::Boot::RunLiveSessionLoop(entry, entry) == 0);
      REQUIRE(fixture.UnloadText());
    } else {
      std::uint8_t step = kLootTeardown;
      REQUIRE(loot::UpdateLiveLootScreen(&step) == 1);
      CHECK(step == 0);
    }
    fixture.CheckReleased();
  }
  CHECK(loot::LootTerminalLiveRefusalCount() == refusals);
}

TEST_CASE(
    "F02845 live consolidation conserves small loot on confirm and cancel",
    "[loot-terminal-live][F02845][issue-412]") {
  const bool confirm = GENERATE(false, true);
  LootFixture fixture;
  fixture.OpenAndLoad();
  fixture.SeedConsolidation();
  auto& staged = *Menu_Loot_inventory;
  const auto before = staged;
  REQUIRE(loot::UpdateLiveStackConsolidation(0) == 0);
  CHECK(Menu_Loot_lootListCount == 1);
  CHECK(staged.misc[0].count == 5);
  fixture.OpenConsolidationPrompt();
  fixture.AnswerConsolidation(confirm);
  const auto after = staged;
  std::uint32_t total = 0;
  for (const auto& item : after.misc) {
    if (item.id == 323) total += item.count;
  }
  CHECK(total == 5);
  CHECK(after.blades == before.blades);
  if (!confirm) {
    CHECK(after == before);
    fixture.CheckRestoredRows();
  }
}
