#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <stdexcept>
#include <string_view>
#include <type_traits>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Content/DataStore.hpp"
#include "Content/ModuleData.h"
#include "Core/Memory/HeapLive.hpp"
#include "Menu/EquipmentDetailControllersLiveFixture.hpp"
#include "Menu/Framework/FrameworkLive.hpp"
#include "Menu/Inventory/InventoryTerminalLive.hpp"
#include "Menu/InventoryTerminalWorld.hpp"

extern "C" {
#include "Battle/Actor.h"
#include "Boot/OverlayReset.h"
}

namespace {
namespace inventory = VS::Menu::Inventory;
namespace framework = VS::Menu::Framework;
namespace content = VS::Content;
using equipment_detail_tests::ArrayRestore;
using equipment_detail_tests::ValueRestore;
constexpr std::string_view kItemModule = "MENU/MENU3.PRG";
constexpr std::string_view kLootModule = "MENU/MENUB.PRG";
constexpr unsigned kItemNavigation = 0x801053EC;
constexpr unsigned kLootNavigation = 0x80105454;

template <typename Entry>
bool IsBound(std::string_view module, unsigned address, Entry entry) {
  return content::ResolveModuleAddress(
             {Content_moduleSymbols, Content_moduleSymbolCount}, module,
             address) == reinterpret_cast<void*>(entry);
}

int Navigate(std::string_view module, unsigned address, int initialize) {
  auto* entry = content::ResolveModuleAddress(
      {Content_moduleSymbols, Content_moduleSymbolCount}, module, address);
  if (entry == nullptr) throw std::runtime_error("missing inventory entry");
  return reinterpret_cast<int (*)(int)>(entry)(initialize);
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
  ValueRestore<decltype(Audio_Seq_unusedStateFlags)> saved_flags{
      Audio_Seq_unusedStateFlags};
  ValueRestore<decltype(Input_settings)> saved_settings{Input_settings};
  ValueRestore<int> saved_restrictions{Menu_Framework_actionRestrictions};
  ValueRestore<u_short*> saved_items{Menu_Framework_itemsText};
  ValueRestore<int*> saved_types{Menu_Framework_rowTypeBuf};
  ValueRestore<char> saved_freeze{Menu_Inventory_freezeTabArrows};
  ValueRestore<char> saved_title_step{
      Menu_Inventory_secondaryTitleAnimationStep};
  ValueRestore<char> saved_icons_step{
      Menu_Inventory_itemCategoryIconsAnimationStep};
  ValueRestore<char> saved_status_step{
      Menu_Inventory_statusCommandAnimationStep};
  ArrayRestore<u_short, std::size(Menu_Inventory_menuText)> saved_menu_text{
      Menu_Inventory_menuText};
  std::array<int, framework::kRowTypeCount> row_types{};
  std::remove_pointer_t<decltype(Battle_characterState)> actor{};
  std::remove_pointer_t<decltype(actor.stats)> stats{};

  TerminalFixture() {
    Battle_characterState = &actor;
    actor.stats = &stats;
    Audio_Seq_unusedStateFlags.introState = 3;
    Input_settings.cursorMemory = 0;
    Menu_Framework_menuMemory = {};
    Menu_Framework_actionRestrictions = 0;
    Menu_Framework_itemsText = nullptr;
    Menu_Framework_rowTypeBuf = row_types.data();
    std::ranges::fill(Menu_Inventory_menuText, u_short{0xE7E7});
    std::fill_n(Menu_Inventory_menuText, 32, u_short{32});
    Menu_Inventory_menuText[32] = 0xE701;
    inventory::BindLiveLootInventoryText(
        VS::tests::inventory_terminal::LootText(2));
  }
  ~TerminalFixture() { CloseList(); }

  void CloseList() {
    if (Menu_Framework_itemsText != nullptr) {
      static_cast<void>(framework::CloseLiveRowsAndRememberCursor());
    }
    framework::ClearLiveMenuExcept(framework::kNoRow);
  }
  framework::ItemText FirstItem() const {
    const auto capacity =
        VS::Core::Memory::LivePayloadCapacity(Menu_Framework_itemsText);
    if (!capacity || *capacity < sizeof(framework::ItemText)) {
      throw std::runtime_error("inventory did not produce an owned row");
    }
    framework::ItemText result;
    std::memcpy(&result, Menu_Framework_itemsText, sizeof result);
    return result;
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
  Boot_ResetMenubStatics();
  REQUIRE(Navigate(kLootModule, kLootNavigation, 1) == 0);
  REQUIRE(Navigate(kLootModule, kLootNavigation, 0) == 0);
  CHECK(fixture.FirstItem().name[0] == 2);
  CHECK(fixture.FirstItem().flags == 1);
}

TEST_CASE("F02597 Live loot does not inherit item-menu global restrictions",
          "[inventory-terminal-live][F02597]") {
  TerminalFixture fixture;
  fixture.AddWeapon();
  Menu_Framework_actionRestrictions = 7;
  Menu_Framework_menuMemory.opaque3A.currentCategory = 1;
  REQUIRE(Navigate(kItemModule, kItemNavigation, 2) == 0);
  REQUIRE(Navigate(kItemModule, kItemNavigation, 0) == 0);
  CHECK((fixture.FirstItem().flags & 1) != 0);
  fixture.CloseList();
  REQUIRE(Navigate(kLootModule, kLootNavigation, 2) == 0);
  REQUIRE(Navigate(kLootModule, kLootNavigation, 0) == 0);
  CHECK((fixture.FirstItem().flags & 1) == 0);
}

TEST_CASE(
    "F02597 overlay reload preserves navigation BSS but cold reset clears it",
    "[inventory-terminal-live][F02597]") {
  TerminalFixture fixture;
  Menu_Framework_menuMemory.opaque3A.currentCategory = 1;
  REQUIRE(Navigate(kItemModule, kItemNavigation, 2) == 0);
  REQUIRE(Navigate(kItemModule, kItemNavigation, 0) == 0);
  REQUIRE(Menu_Framework_itemsText == nullptr);
  fixture.AddWeapon();
  Boot_ResetMenu3Statics();
  REQUIRE(Navigate(kItemModule, kItemNavigation, 0) == 0);
  CHECK(Menu_Framework_itemsText == nullptr);
  inventory::ResetInventoryTerminalSession();
  REQUIRE(Navigate(kItemModule, kItemNavigation, 0) == 0);
  CHECK(Menu_Framework_itemsText != nullptr);
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
  inventory::ResetInventoryTerminalSession();
  CHECK(std::ranges::equal(inventory::LiveLootInventoryText(), expected));
}
