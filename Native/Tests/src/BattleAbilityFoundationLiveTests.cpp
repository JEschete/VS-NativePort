#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Battle/AbilityState.hpp"
#include "Battle/ActionMenu.hpp"
#include "Battle/ActionTable.h"
#include "Battle/Actor.h"
#include "Boot/OverlayReset.hpp"
#include "Boot/SessionLive.hpp"
#include "Content/DiscLayout.h"
#include "Content/DiscQueue.hpp"
#include "Content/ModuleData.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Menu/BattleAbilities.hpp"
#include "Menu/BattleAbilitiesLiveState.hpp"
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

// A text header's bytes up to its NUL, as the cost panel draws them.
std::string HeaderText(const framework::TextHeader& header) {
  return {header.text.begin(), std::ranges::find(header.text, std::uint8_t{0})};
}

template <typename Entry>
bool IsBound(unsigned address, Entry entry) {
  return VS::Content::ResolveModuleAddress(VS::Content::ModuleSymbols(),
                                           kModule, address) ==
         reinterpret_cast<void*>(entry);
}

struct LiveFoundationFixture : equipment_detail_tests::LiveFixture {
  std::vector<std::byte> arena = std::vector<std::byte>(0x20000);
  VS::Core::Memory::ScopedLiveGameHeap heap{arena};
  ValueRestore<decltype(VS::Battle::characterState)> saved_actor{
      VS::Battle::characterState};
  ValueRestore<decltype(VS::State::artsStatus)> saved_progress{
      VS::State::artsStatus};
  ValueRestore<decltype(VS::Input::settings)> saved_settings{
      VS::Input::settings};
  ArrayRestore<Battle_ActionDefinition, std::size(Battle_actionDefinitions)>
      saved_actions{Battle_actionDefinitions};
  ArrayRestore<std::uint16_t, std::size(abilities::battleAbilityStrings)>
      saved_ability_text{abilities::battleAbilityStrings};
  ArrayRestore<std::uint16_t, std::size(abilities::battleAbilityMenuStrings)>
      saved_menu_text{abilities::battleAbilityMenuStrings};
  ArrayRestore<std::uint8_t, abilities::kAbilityCapacity> saved_chain{
      abilities::unlockedChainAbilities};
  ArrayRestore<std::uint8_t, abilities::kAbilityCapacity> saved_defense{
      abilities::unlockedDefenseAbilities};
  ValueRestore<decltype(VS::Menu::Framework::textHeaders)> saved_headers{
      VS::Menu::Framework::textHeaders};
  ValueRestore<char> saved_cost_display{VS::Menu::Framework::displayActionCost};
  ValueRestore<std::uint16_t*> saved_rows{abilities::abilityText};
  ValueRestore<std::uint8_t> saved_count{abilities::menuAbilityCount};
  ValueRestore<std::uint32_t> saved_state{abilities::abilityMenuState};
  ValueRestore<std::uint8_t> saved_type{abilities::selectedAbilityType};
  ValueRestore<std::int32_t> saved_highlight{abilities::highlightedAbility};
  ValueRestore<std::uint8_t> saved_first{abilities::firstAbility};
  ValueRestore<u_short*> saved_items{VS::Menu::Framework::itemsText};
  ValueRestore<int*> saved_types{VS::Menu::Framework::rowTypeBuf};
  std::array<int, framework::kRowTypeCount> types{};
  Battle_Actor actor{};

  LiveFoundationFixture() {
    abilities::ResetLiveAbilityFoundationState();
    VS::Battle::characterState = &actor;
    actor.equippedWeaponCategory = 1;
    VS::State::artsStatus = {};
    VS::State::artsStatus.kills.weapon_category_kills[1] = 65535;
    VS::State::artsStatus.kills.total_kills = 65535;
    VS::Input::settings.cursor_memory = 0;
    VS::Menu::Framework::menuMemory = {};
    VS::Menu::Framework::itemsText = nullptr;
    VS::Menu::Framework::rowTypeBuf = types.data();
    abilities::abilityText = nullptr;
    abilities::unlockedChainAbilities[0] = kAction;
    std::ranges::fill(Battle_actionDefinitions, Battle_ActionDefinition{});
    std::ranges::fill(abilities::battleAbilityStrings, std::uint16_t{0xE7E7});
    std::fill_n(abilities::battleAbilityStrings, 34, std::uint16_t{64});
    abilities::battleAbilityStrings[64] = 0xE702;
    std::ranges::fill(abilities::battleAbilityMenuStrings,
                      std::uint16_t{0xE7E7});
    std::fill_n(abilities::battleAbilityMenuStrings, 16, std::uint16_t{64});
    std::ranges::fill(Battle_actionDefinitions[kAction].name,
                      static_cast<char>(0xE7));
    Battle_actionDefinitions[kAction].name[0] = 1;
    Battle_actionDefinitions[kAction].cost = 255;
  }
  ~LiveFoundationFixture() {
    if (abilities::abilityText != nullptr) {
      VS::Core::Memory::FreeLiveFromHighEnd(abilities::abilityText);
      abilities::abilityText = nullptr;
    }
    if (VS::Menu::Framework::itemsText != nullptr) {
      static_cast<void>(framework::CloseLiveRowsAndRememberCursor());
    }
    framework::ClearLiveMenuExcept(framework::kNoRow);
    abilities::ResetLiveAbilityFoundationState();
  }
  std::span<const std::uint16_t> RowText() const {
    const auto capacity =
        VS::Core::Memory::LivePayloadCapacity(abilities::abilityText);
    if (!capacity ||
        *capacity < abilities::kAbilityRowWords * sizeof(std::uint16_t)) {
      throw std::runtime_error("ability entry did not publish an owned row");
    }
    return {abilities::abilityText, abilities::kAbilityRowWords};
  }
};

struct LiveTerminalFixture : LiveFoundationFixture {
  ArrayRestore<std::int8_t, std::size(VS::Battle::chainAbilityActionIds)>
      saved_chain_order{VS::Battle::chainAbilityActionIds.data()};
  ArrayRestore<std::int8_t, std::size(VS::Battle::defenseAbilityActionIds)>
      saved_defense_order{VS::Battle::defenseAbilityActionIds.data()};
  ValueRestore<decltype(VS::Menu::Framework::shortcutInvoked)> saved_shortcut{
      VS::Menu::Framework::shortcutInvoked};
  ValueRestore<decltype(VS::Menu::Framework::menuState)> saved_control{
      VS::Menu::Framework::menuState};

  LiveTerminalFixture() {
    framework::ResetLiveMainMenuStatics();
    abilities::ResetLiveAbilityTerminalColdState();
    VS::Menu::Framework::shortcutInvoked = 1;
    std::ranges::fill(VS::Battle::chainAbilityActionIds, char{0});
    std::ranges::fill(VS::Battle::defenseAbilityActionIds, char{0});
    VS::Battle::chainAbilityActionIds[0] = kAction;
    VS::Battle::defenseAbilityActionIds[0] = kDefenseAction;
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
                                    std::uint16_t* owner,
                                    std::uint8_t& learned) {
    CHECK(load.Complete() == 1);
    CHECK(load.request->state == VS::Content::DiscRequestState::kFree);
    CHECK(learned == 0);
    CHECK(abilities::abilityText == nullptr);
    CHECK(VS::Core::Memory::LivePayloadCapacity(owner).has_value());
    if (abilities::abilityText == nullptr) {
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
  VS::Menu::textBoxes[7].flags.done = 1;
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
  VS::Menu::textBoxes[7].flags.done = 1;
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
  CHECK(abilities::menuAbilityCount == 1);
  CHECK(abilities::selectedAbilityType == 24);
  CHECK(fixture.RowText()[0] == 0x10FA);
  CHECK(fixture.RowText()[1] == 0xE701);
  CHECK(fixture.RowText()[17] == 0xE702);
  CHECK(HeaderText(VS::Menu::Framework::textHeaders[0]) == "#255");
  CHECK(HeaderText(VS::Menu::Framework::textHeaders[1]) == "RISK");
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
  REQUIRE(abilities::menuAbilityCount == 1);
  auto* owner = abilities::abilityText;
  REQUIRE(VS::Core::Memory::LivePayloadCapacity(owner).has_value());
  VS::Input::settings.mapped_chain_abilities[0] = kAction;
  VS::Boot::ResetMenu2Statics();
  CHECK(abilities::unlockedChainAbilities[0] == 0);
  CHECK(abilities::unlockedDefenseAbilities[0] == 0);
  CHECK(VS::Input::settings.mapped_chain_abilities[0] == kAction);
  REQUIRE(VS::Core::Memory::LivePayloadCapacity(owner).has_value());
  VS::Core::Memory::FreeLiveFromHighEnd(owner);
  REQUIRE(fixture.OpenList(defense) == 0);
  CHECK(abilities::menuAbilityCount == 1);
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
  auto* owner = abilities::abilityText;
  REQUIRE(VS::Core::Memory::LivePayloadCapacity(owner).has_value());
  if (actual_session) {
    const auto entry = +[] { return 0; };
    REQUIRE(VS::Boot::RunLiveSessionLoop(entry, entry) == 0);
  } else {
    abilities::ResetLiveAbilityTerminalColdState();
  }
  CHECK(abilities::abilityText == owner);
  CHECK(abilities::menuAbilityCount == 1);
  CHECK(abilities::unlockedChainAbilities[0] == 0);
  CHECK(VS::Core::Memory::LivePayloadCapacity(owner).has_value());
}

TEST_CASE("Loaded MENU2 requests dispatch the image reset before menu reentry",
          "[battle-ability-terminal-live][menu-overlay-reload]") {
  LiveTerminalFixture fixture;
  menu_overlay_tests::Load load(3, {VS_MENU2_PRG_LBA, VS_MENU2_PRG_SIZE});
  const bool defense = GENERATE(false, true);
  auto& learned = defense ? abilities::unlockedDefenseAbilities
                          : abilities::unlockedChainAbilities;
  const int action = defense ? kDefenseAction : kAction;
  REQUIRE(fixture.OpenList(defense) == 0);
  auto* owner = abilities::abilityText;
  CHECK(load.Poll() == 0);
  CHECK(learned[0] == action);
  CHECK(abilities::abilityText == owner);
  fixture.CompleteAndCheckReset(load, owner, learned[0]);
  learned[0] = static_cast<std::uint8_t>(action);
  CHECK(load.Poll() == 1);
  CHECK(learned[0] == action);
}
