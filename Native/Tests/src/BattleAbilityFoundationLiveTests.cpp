#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <span>
#include <stdexcept>
#include <string_view>
#include <type_traits>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Battle/AbilityState.h"
#include "Battle/ActionMenu.h"
#include "Battle/ActionTable.h"
#include "Battle/Actor.h"
#include "Boot/OverlayReset.hpp"
#include "Boot/SessionLive.hpp"
#include "Content/DiscLayout.h"
#include "Content/ModuleData.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "DiscQueue.h"
#include "Menu/BattleAbilities.h"
#include "Menu/BattleAbilities.hpp"
#include "Menu/BattleAbilitiesTerminalLive.hpp"
#include "Menu/BattleAbilitiesUnlockLive.hpp"
#include "Menu/EquipmentDetailControllersLiveFixture.hpp"
#include "Menu/Framework/FrameworkLive.hpp"
#include "Menu/OverlayLoadFixture.hpp"
#include "State/Progress.hpp"

namespace {
namespace abilities = VS::Menu::BattleAbilities;
namespace framework = VS::Menu::Framework;
using equipment_detail_tests::ArrayRestore;
using equipment_detail_tests::ValueRestore;
constexpr std::string_view kModule = "MENU/MENU2.PRG";
constexpr int kAction = 24;
constexpr int kArtAction = 184;
constexpr int kDefenseAction = 47;

template <typename Entry>
bool IsBound(unsigned address, Entry entry) {
  return VS::Content::ResolveModuleAddress(VS::Content::ModuleSymbols(),
                                           kModule, address) ==
         reinterpret_cast<void*>(entry);
}

struct LiveFoundationFixture : equipment_detail_tests::LiveFixture {
  std::vector<std::byte> arena = std::vector<std::byte>(0x20000);
  VS::Core::Memory::ScopedLiveGameHeap heap{arena};
  ValueRestore<decltype(Battle_characterState)> saved_actor{
      Battle_characterState};
  ValueRestore<decltype(VS::State::artsStatus)> saved_progress{
      VS::State::artsStatus};
  ValueRestore<decltype(VS::Input::settings)> saved_settings{
      VS::Input::settings};
  ArrayRestore<Battle_ActionDefinition, std::size(Battle_actionDefinitions)>
      saved_actions{Battle_actionDefinitions};
  ArrayRestore<u_short, std::size(Menu_BattleAbilities_battleAbilityStrings)>
      saved_ability_text{Menu_BattleAbilities_battleAbilityStrings};
  ArrayRestore<u_short,
               std::size(Menu_BattleAbilities_battleAbilityMenuStrings)>
      saved_menu_text{Menu_BattleAbilities_battleAbilityMenuStrings};
  ArrayRestore<char, abilities::kAbilityCapacity> saved_chain{
      Menu_BattleAbilities_unlockedChainAbilities};
  ArrayRestore<char, abilities::kAbilityCapacity> saved_defense{
      Menu_BattleAbilities_unlockedDefenseAbilities};
  ArrayRestore<Menu_Framework_TextHeader, 2> saved_headers{
      VS::Menu::Framework::textHeaders};
  ValueRestore<char> saved_cost_display{VS::Menu::Framework::displayActionCost};
  ValueRestore<u_short*> saved_rows{Menu_BattleAbilities_abilityText};
  ValueRestore<char> saved_count{Menu_BattleAbilities_abilityCount};
  ValueRestore<u_int> saved_state{Menu_BattleAbilities_abilityMenuState};
  ValueRestore<char> saved_type{Menu_BattleAbilities_selectedAbilityType};
  ValueRestore<int> saved_highlight{Menu_BattleAbilities_highlightedAbility};
  ValueRestore<char> saved_first{Menu_BattleAbilities_firstAbility};
  ValueRestore<u_short*> saved_items{VS::Menu::Framework::itemsText};
  ValueRestore<int*> saved_types{VS::Menu::Framework::rowTypeBuf};
  std::array<int, framework::kRowTypeCount> types{};
  Battle_Actor actor{};

  LiveFoundationFixture() {
    abilities::ResetLiveAbilityFoundationState();
    Battle_characterState = &actor;
    actor.equippedWeaponCategory = 1;
    VS::State::artsStatus = {};
    VS::State::artsStatus.kills.weapon_category_kills[1] = 65535;
    VS::State::artsStatus.kills.total_kills = 65535;
    VS::Input::settings.cursor_memory = 0;
    VS::Menu::Framework::menuMemory = {};
    VS::Menu::Framework::itemsText = nullptr;
    VS::Menu::Framework::rowTypeBuf = types.data();
    Menu_BattleAbilities_abilityText = nullptr;
    Menu_BattleAbilities_unlockedChainAbilities[0] = kAction;
    std::ranges::fill(Battle_actionDefinitions, Battle_ActionDefinition{});
    std::ranges::fill(Menu_BattleAbilities_battleAbilityStrings,
                      u_short{0xE7E7});
    std::fill_n(Menu_BattleAbilities_battleAbilityStrings, 34, u_short{64});
    Menu_BattleAbilities_battleAbilityStrings[64] = 0xE702;
    std::ranges::fill(Menu_BattleAbilities_battleAbilityMenuStrings,
                      u_short{0xE7E7});
    std::fill_n(Menu_BattleAbilities_battleAbilityMenuStrings, 16, u_short{64});
    std::ranges::fill(Battle_actionDefinitions[kAction].name,
                      static_cast<char>(0xE7));
    Battle_actionDefinitions[kAction].name[0] = 1;
    Battle_actionDefinitions[kAction].cost = 255;
  }
  ~LiveFoundationFixture() {
    if (Menu_BattleAbilities_abilityText != nullptr) {
      VS::Core::Memory::FreeLiveFromHighEnd(Menu_BattleAbilities_abilityText);
      Menu_BattleAbilities_abilityText = nullptr;
    }
    if (VS::Menu::Framework::itemsText != nullptr) {
      static_cast<void>(framework::CloseLiveRowsAndRememberCursor());
    }
    framework::ClearLiveMenuExcept(framework::kNoRow);
    abilities::ResetLiveAbilityFoundationState();
  }
  std::span<const u_short> RowText() const {
    const auto capacity =
        VS::Core::Memory::LivePayloadCapacity(Menu_BattleAbilities_abilityText);
    if (!capacity ||
        *capacity < abilities::kAbilityRowWords * sizeof(u_short)) {
      throw std::runtime_error("ability entry did not publish an owned row");
    }
    return {Menu_BattleAbilities_abilityText, abilities::kAbilityRowWords};
  }
};

struct LiveTerminalFixture : LiveFoundationFixture {
  ArrayRestore<char, std::size(Battle_chainAbilityActionIds)> saved_chain_order{
      Battle_chainAbilityActionIds};
  ArrayRestore<char, std::size(Battle_defenseAbilityActionIds)>
      saved_defense_order{Battle_defenseAbilityActionIds};
  ValueRestore<decltype(VS::Menu::Framework::shortcutInvoked)> saved_shortcut{
      VS::Menu::Framework::shortcutInvoked};
  ValueRestore<decltype(VS::Menu::Framework::menuState)> saved_control{
      VS::Menu::Framework::menuState};

  LiveTerminalFixture() {
    framework::ResetLiveMainMenuStatics();
    abilities::ResetLiveAbilityTerminalColdState();
    VS::Menu::Framework::shortcutInvoked = 1;
    std::ranges::fill(Battle_chainAbilityActionIds, char{0});
    std::ranges::fill(Battle_defenseAbilityActionIds, char{0});
    Battle_chainAbilityActionIds[0] = kAction;
    Battle_defenseAbilityActionIds[0] = kDefenseAction;
    Battle_actionDefinitions[kAction].unlocked = 1;
    Battle_actionDefinitions[kDefenseAction].unlocked = 1;
    std::ranges::fill(Battle_actionDefinitions[kDefenseAction].name,
                      static_cast<char>(0xE7));
    Battle_actionDefinitions[kDefenseAction].name[0] = 2;
  }
  ~LiveTerminalFixture() {
    abilities::ResetLiveAbilityTerminalColdState();
    framework::ResetLiveMainMenuStatics();
  }
  static int OpenList(bool defense) {
    return defense ? abilities::LiveUpdateDefenseAbilityMenu(0)
                   : abilities::LiveUpdateChainAbilityMenu(0);
  }
  static void CompleteAndCheckReset(menu_overlay_tests::Load& load,
                                    u_short* owner, char& learned) {
    CHECK(load.Complete() == 1);
    CHECK(load.request->state == Content_DiscRequestState_Free);
    CHECK(learned == 0);
    CHECK(Menu_BattleAbilities_abilityText == nullptr);
    CHECK(VS::Core::Memory::LivePayloadCapacity(owner).has_value());
    if (Menu_BattleAbilities_abilityText == nullptr) {
      VS::Core::Memory::FreeLiveFromHighEnd(owner);
    }
  }
};
}  // namespace

TEST_CASE(
    "Ability foundation retains all six original MENU2 entries",
    "[battle-abilities-live][F02614][F02615][F02616][F02617][F02618][F02619]") {
  CHECK(IsBound(0x80103C3C, &abilities::LiveUpdateBreakArtUnlock));
  CHECK(IsBound(0x80103DD8, &abilities::LiveUnlockDescription));
  CHECK(IsBound(0x80103E20, &abilities::LiveUpdateAbilityUnlock));
  CHECK(IsBound(0x80104578, &abilities::LiveUpdateUnlockNotifications));
  CHECK(IsBound(0x80102864, &abilities::LiveSetRiskCost));
  CHECK(IsBound(0x80102928, &abilities::LiveCreateAbilityRowsEntry));
}

TEST_CASE("MENU2 reload restores the file-backed Break Art timeout",
          "[battle-abilities-live][F02614][reload][menu-overlay-reload]") {
  LiveFoundationFixture fixture;
  menu_overlay_tests::Load load(3, {VS_MENU2_PRG_LBA, VS_MENU2_PRG_SIZE});
  REQUIRE(abilities::LiveUpdateBreakArtUnlock(1) == 0);
  CHECK(Battle_actionDefinitions[kArtAction].unlocked == 1);
  Menu_textBoxes[7].flags.done = 1;
  REQUIRE(abilities::LiveUpdateBreakArtUnlock(0) == 0);
  CHECK(load.Complete() == 1);
  CHECK(abilities::LiveUpdateBreakArtUnlock(0) == 1);
}

TEST_CASE("MENU2 reload restores unlock counts and phase without progression",
          "[battle-abilities-live][F02616][reload][menu-overlay-reload]") {
  LiveFoundationFixture fixture;
  menu_overlay_tests::Load load(3, {VS_MENU2_PRG_LBA, VS_MENU2_PRG_SIZE});
  REQUIRE(abilities::LiveUpdateAbilityUnlock(1) == 0);
  CHECK(VS::State::artsStatus.kills.battle_abilities_unlocked == 1);
  CHECK(load.Complete() == 1);
  REQUIRE(abilities::LiveUpdateAbilityUnlock(0) == 0);
  Menu_textBoxes[7].flags.done = 1;
  CHECK(abilities::LiveUpdateAbilityUnlock(0) == 1);
  CHECK(VS::State::artsStatus.kills.battle_abilities_unlocked == 1);
}

TEST_CASE(
    "F02619 module ABI publishes owned rows and unsigned Risk through Live",
    "[battle-abilities-live][F02615][F02618][F02619]") {
  LiveFoundationFixture fixture;
  std::array text{
      reinterpret_cast<u_short*>(Battle_actionDefinitions[kAction].name),
      reinterpret_cast<u_short*>(abilities::LiveUnlockDescription(kAction))};
  auto* address = VS::Content::ResolveModuleAddress(
      VS::Content::ModuleSymbols(), kModule, 0x80102928);
  if (address == nullptr) {
    FAIL("missing ability row entry");
    return;
  }
  const auto entry = reinterpret_cast<int (*)(int, int, u_short**)>(address);
  REQUIRE(entry(1, 24, text.data()) == 1);
  CHECK(Menu_BattleAbilities_abilityCount == 1);
  CHECK(Menu_BattleAbilities_selectedAbilityType == 24);
  CHECK(fixture.RowText()[0] == 0x10FA);
  CHECK(fixture.RowText()[1] == 0xE701);
  CHECK(fixture.RowText()[17] == 0xE702);
  CHECK(std::string_view(VS::Menu::Framework::textHeaders[0].text) == "#255");
  CHECK(std::string_view(VS::Menu::Framework::textHeaders[1].text) == "RISK");
}

TEST_CASE("Ability terminal retains all six original MENU2 entries",
          "[battle-ability-terminal-live]") {
  CHECK(IsBound(0x80102B5C, &abilities::LiveToggleButtonAssignment));
  CHECK(IsBound(0x80102CAC, &abilities::LiveUpdateAssignments));
  CHECK(IsBound(0x801034FC, &abilities::LiveUpdateChainAbilityMenu));
  CHECK(IsBound(0x80103670, &abilities::LiveUpdateDefenseAbilityMenu));
  CHECK(IsBound(0x801037E4, &abilities::LiveDrawNextUnlockPoints));
  CHECK(IsBound(0x801038D4, &abilities::LiveUpdateAbilityScreen));
}

TEST_CASE("MENU2 reload restarts learned lists without releasing heap owners",
          "[battle-ability-terminal-live][F02622][F02623][reload]") {
  LiveTerminalFixture fixture;
  const bool defense = GENERATE(false, true);
  REQUIRE(fixture.OpenList(defense) == 0);
  REQUIRE(Menu_BattleAbilities_abilityCount == 1);
  auto* owner = Menu_BattleAbilities_abilityText;
  REQUIRE(VS::Core::Memory::LivePayloadCapacity(owner).has_value());
  VS::Input::settings.mapped_chain_abilities[0] = kAction;
  VS::Boot::ResetMenu2Statics();
  CHECK(Menu_BattleAbilities_unlockedChainAbilities[0] == 0);
  CHECK(Menu_BattleAbilities_unlockedDefenseAbilities[0] == 0);
  CHECK(VS::Input::settings.mapped_chain_abilities[0] == kAction);
  REQUIRE(VS::Core::Memory::LivePayloadCapacity(owner).has_value());
  VS::Core::Memory::FreeLiveFromHighEnd(owner);
  REQUIRE(fixture.OpenList(defense) == 0);
  CHECK(Menu_BattleAbilities_abilityCount == 1);
}

TEST_CASE("F02625 actual screen entry creates categories from its caller byte",
          "[battle-ability-terminal-live][F02625]") {
  LiveTerminalFixture fixture;
  VS::Menu::Framework::shortcutInvoked = 0;
  std::uint8_t phase = 3;
  auto* address = VS::Content::ResolveModuleAddress(
      VS::Content::ModuleSymbols(), kModule, 0x801038D4);
  if (address == nullptr) {
    FAIL("missing ability screen entry");
    return;
  }
  REQUIRE(reinterpret_cast<int (*)(std::uint8_t*)>(address)(&phase) == 0);
  CHECK(phase == 5);
  CHECK(VS::Menu::Framework::itemsText != nullptr);
}

TEST_CASE("Ability terminal cold reset preserves foundation row ownership",
          "[battle-ability-terminal-live][F02621][reload]") {
  LiveTerminalFixture fixture;
  const bool actual_session = GENERATE(false, true);
  REQUIRE(fixture.OpenList(false) == 0);
  auto* owner = Menu_BattleAbilities_abilityText;
  REQUIRE(VS::Core::Memory::LivePayloadCapacity(owner).has_value());
  if (actual_session) {
    const auto entry = +[] { return 0; };
    REQUIRE(VS::Boot::RunLiveSessionLoop(entry, entry) == 0);
  } else {
    abilities::ResetLiveAbilityTerminalColdState();
  }
  CHECK(Menu_BattleAbilities_abilityText == owner);
  CHECK(Menu_BattleAbilities_abilityCount == 1);
  CHECK(Menu_BattleAbilities_unlockedChainAbilities[0] == 0);
  CHECK(VS::Core::Memory::LivePayloadCapacity(owner).has_value());
}

TEST_CASE("Loaded MENU2 requests dispatch the image reset before menu reentry",
          "[battle-ability-terminal-live][menu-overlay-reload]") {
  LiveTerminalFixture fixture;
  menu_overlay_tests::Load load(3, {VS_MENU2_PRG_LBA, VS_MENU2_PRG_SIZE});
  const bool defense = GENERATE(false, true);
  auto& learned = defense ? Menu_BattleAbilities_unlockedDefenseAbilities
                          : Menu_BattleAbilities_unlockedChainAbilities;
  const int action = defense ? kDefenseAction : kAction;
  REQUIRE(fixture.OpenList(defense) == 0);
  auto* owner = Menu_BattleAbilities_abilityText;
  CHECK(load.Poll() == 0);
  CHECK(learned[0] == action);
  CHECK(Menu_BattleAbilities_abilityText == owner);
  fixture.CompleteAndCheckReset(load, owner, learned[0]);
  learned[0] = static_cast<char>(action);
  CHECK(load.Poll() == 1);
  CHECK(learned[0] == action);
}
