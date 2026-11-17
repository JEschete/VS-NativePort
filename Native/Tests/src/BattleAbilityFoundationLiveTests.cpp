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

#include "Content/ModuleData.h"
#include "Core/Memory/HeapLive.hpp"
#include "Menu/BattleAbilities.hpp"
#include "Menu/BattleAbilitiesUnlockLive.hpp"
#include "Menu/EquipmentDetailControllersLiveFixture.hpp"
#include "Menu/Framework/FrameworkLive.hpp"

extern "C" {
#include "Battle/AbilityState.h"
#include "Battle/ActionTable.h"
#include "Battle/Actor.h"
#include "Boot/OverlayReset.h"
#include "Menu/BattleAbilities.h"
}

namespace {
namespace abilities = VS::Menu::BattleAbilities;
namespace framework = VS::Menu::Framework;
using equipment_detail_tests::ArrayRestore;
using equipment_detail_tests::ValueRestore;
constexpr std::string_view kModule = "MENU/MENU2.PRG";
constexpr int kAction = 24;
constexpr int kArtAction = 184;

template <typename Entry>
bool IsBound(unsigned address, Entry entry) {
  return VS::Content::ResolveModuleAddress(
             {Content_moduleSymbols, Content_moduleSymbolCount}, kModule,
             address) == reinterpret_cast<void*>(entry);
}

struct LiveFoundationFixture : equipment_detail_tests::LiveFixture {
  std::vector<std::byte> arena = std::vector<std::byte>(0x20000);
  VS::Core::Memory::ScopedLiveGameHeap heap{arena};
  ValueRestore<decltype(Battle_characterState)> saved_actor{
      Battle_characterState};
  ValueRestore<decltype(Audio_Seq_unusedArtsStatus)> saved_progress{
      Audio_Seq_unusedArtsStatus};
  ValueRestore<decltype(Input_settings)> saved_settings{Input_settings};
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
      Menu_Framework_textHeaders};
  ValueRestore<char> saved_cost_display{Menu_Framework_displayActionCost};
  ValueRestore<u_short*> saved_rows{Menu_BattleAbilities_abilityText};
  ValueRestore<char> saved_count{Menu_BattleAbilities_abilityCount};
  ValueRestore<u_int> saved_state{Menu_BattleAbilities_abilityMenuState};
  ValueRestore<char> saved_type{Menu_BattleAbilities_selectedAbilityType};
  ValueRestore<int> saved_highlight{Menu_BattleAbilities_highlightedAbility};
  ValueRestore<char> saved_first{Menu_BattleAbilities_firstAbility};
  ValueRestore<u_short*> saved_items{Menu_Framework_itemsText};
  ValueRestore<int*> saved_types{Menu_Framework_rowTypeBuf};
  std::array<int, framework::kRowTypeCount> types{};
  Battle_Actor actor{};

  LiveFoundationFixture() {
    abilities::ResetLiveAbilityFoundationState();
    Battle_characterState = &actor;
    actor.equippedWeaponCategory = 1;
    Audio_Seq_unusedArtsStatus = {};
    Audio_Seq_unusedArtsStatus.kills.weaponCategoryKills[1] = 65535;
    Audio_Seq_unusedArtsStatus.kills.totalKills = 65535;
    Input_settings.cursorMemory = 0;
    Menu_Framework_menuMemory = {};
    Menu_Framework_itemsText = nullptr;
    Menu_Framework_rowTypeBuf = types.data();
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
    if (Menu_Framework_itemsText != nullptr) {
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
          "[battle-abilities-live][F02614][reload]") {
  LiveFoundationFixture fixture;
  REQUIRE(abilities::LiveUpdateBreakArtUnlock(1) == 0);
  CHECK(Battle_actionDefinitions[kArtAction].unlocked == 1);
  Menu_textBoxes[7].flags.done = 1;
  REQUIRE(abilities::LiveUpdateBreakArtUnlock(0) == 0);
  Boot_ResetMenu2Statics();
  CHECK(abilities::LiveUpdateBreakArtUnlock(0) == 1);
}

TEST_CASE("MENU2 reload restores unlock counts and phase without progression",
          "[battle-abilities-live][F02616][reload]") {
  LiveFoundationFixture fixture;
  REQUIRE(abilities::LiveUpdateAbilityUnlock(1) == 0);
  CHECK(Audio_Seq_unusedArtsStatus.kills.battleAbilitiesUnlocked == 1);
  Boot_ResetMenu2Statics();
  REQUIRE(abilities::LiveUpdateAbilityUnlock(0) == 0);
  Menu_textBoxes[7].flags.done = 1;
  CHECK(abilities::LiveUpdateAbilityUnlock(0) == 1);
  CHECK(Audio_Seq_unusedArtsStatus.kills.battleAbilitiesUnlocked == 1);
}

TEST_CASE(
    "F02619 module ABI publishes owned rows and unsigned Risk through Live",
    "[battle-abilities-live][F02615][F02618][F02619]") {
  LiveFoundationFixture fixture;
  std::array text{
      reinterpret_cast<u_short*>(Battle_actionDefinitions[kAction].name),
      reinterpret_cast<u_short*>(abilities::LiveUnlockDescription(kAction))};
  auto* address = VS::Content::ResolveModuleAddress(
      {Content_moduleSymbols, Content_moduleSymbolCount}, kModule, 0x80102928);
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
  CHECK(std::string_view(Menu_Framework_textHeaders[0].text) == "#255");
  CHECK(std::string_view(Menu_Framework_textHeaders[1].text) == "RISK");
}
