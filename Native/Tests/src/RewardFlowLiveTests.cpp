#include <algorithm>
#include <array>
#include <cstdint>
#include <iterator>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Battle/RewardFlowLive.hpp"
#include "Field/EnvironmentLive.hpp"
#include "Field/RoomEffectsLive.hpp"
#include "Render/PaletteAnimationLive.hpp"
#include "State/ScriptFlagsLive.hpp"
extern "C" {
#include "Actors/ActorRecord.h"
#include "Actors/ModelInstance.h"
#include "Actors/Movement.h"
#include "Audio/Seq/Channel.h"
#include "Battle/AbilityState.h"
#include "Battle/Action.h"
#include "Battle/ActionFlow.h"
#include "Battle/Actor.h"
#include "Battle/Rewards.h"
#include "Battle/TargetingState.h"
#include "Field/EnvironmentLiveState.h"
#include "Field/GameModeLiveState.h"
#include "Field/Gimmicks.h"
#include "Field/Minimap.h"
#include "Field/RoomRenderingLiveState.h"
#include "Items/LootLiveState.h"
#include "Menu/Framework/FrameworkLiveState.h"
#include "Menu/Framework/MenuRows.h"
}

namespace {
namespace b = VS::Battle;
namespace render = VS::Render;
struct LiveFixture {
  decltype(std::to_array(Actors_actorRecords)) saved_records =
      std::to_array(Actors_actorRecords);
  render::PaletteAnimation saved_palette = render::LivePaletteAnimation();
  VS::Field::RoomWeatherState saved_weather =
      VS::Field::LoadLiveRoomWeatherState();
  std::int16_t saved_weather_alias = Field_weatherParameters[6];
  std::int16_t saved_effect_sound = VS::Field::LiveRoomEffectState().sound_id;
  decltype(Field_roomTint) saved_tint = Field_roomTint;
  u_int saved_phase = Battle_actionFlowMode;
  int saved_follow = Battle_rewardsFollowAction;
  int saved_pause = Field_isInteractionPaused;
  u_short saved_room_pause = Field_areRoomUpdatesPaused;
  char saved_minimap = Field_isMinimapVisible;
  u_char saved_platform_pause = Field_areMovingPlatformsPaused;
  Battle_ActionState* saved_action = Battle_actionState;
  Battle_Actor* saved_character = Battle_characterState;
  Items_LootListNode* saved_loot = Items_lootListHead;
  decltype(Audio_Seq_unusedArtsStatus) saved_progress =
      Audio_Seq_unusedArtsStatus;
  u_short saved_requirement = Battle_battleAbilityPointRequirements[0];
  std::uint8_t saved_ab = VS::State::LiveScriptFlag(0xAB);
  std::uint8_t saved_b6 = VS::State::LiveScriptFlag(0xB6);
  Battle_ActionState action{};
  Battle_Actor actor{};
  Battle_ActorStats stats{};
  Actors_ModelInstance model{};

  LiveFixture() {
    std::fill(std::begin(Actors_actorRecords), std::end(Actors_actorRecords),
              nullptr);
    render::InitializeLiveClutAnimation();
    Field_weatherAmbientSoundId = -1;
    VS::Field::LiveRoomEffectState().sound_id = -1;
    Battle_actionFlowMode = 99;
    Battle_rewardsFollowAction = 0;
    Field_isInteractionPaused = 77;
    Field_areRoomUpdatesPaused = 0;
    Field_isMinimapVisible = 1;
    Field_areMovingPlatformsPaused = 0;
    Battle_actionState = &action;
    Battle_characterState = &actor;
    actor.stats = &stats;
    actor.model = &model;
    actor.equippedWeaponCategory = 1;
    Audio_Seq_unusedArtsStatus = {};
    std::fill_n(Audio_Seq_unusedArtsStatus.artsLearned, 12, 4);
    Battle_battleAbilityPointRequirements[0] = 1;
    Audio_Seq_unusedArtsStatus.kills.totalKills = 1;
    VS::State::SetLiveScriptFlag(0xAB, 0);
    VS::State::SetLiveScriptFlag(0xB6, 0);
  }
  ~LiveFixture() {
    std::copy(saved_records.begin(), saved_records.end(), Actors_actorRecords);
    const_cast<render::PaletteAnimation&>(render::LivePaletteAnimation()) =
        saved_palette;
    VS::Field::StoreLiveRoomWeatherState(saved_weather);
    Field_weatherParameters[6] = saved_weather_alias;
    VS::Field::LiveRoomEffectState().sound_id = saved_effect_sound;
    Field_roomTint = saved_tint;
    Battle_actionFlowMode = saved_phase;
    Battle_rewardsFollowAction = saved_follow;
    Field_isInteractionPaused = saved_pause;
    Field_areRoomUpdatesPaused = saved_room_pause;
    Field_isMinimapVisible = saved_minimap;
    Field_areMovingPlatformsPaused = saved_platform_pause;
    Battle_actionState = saved_action;
    Battle_characterState = saved_character;
    Items_lootListHead = saved_loot;
    Audio_Seq_unusedArtsStatus = saved_progress;
    Battle_battleAbilityPointRequirements[0] = saved_requirement;
    VS::State::SetLiveScriptFlag(0xAB, saved_ab);
    VS::State::SetLiveScriptFlag(0xB6, saved_b6);
  }
};

struct LiveMenuFixture : LiveFixture {
  decltype(Menu_Framework_menuState) saved_menu = Menu_Framework_menuState;
  decltype(Menu_Framework_actionSelection) saved_selection =
      Menu_Framework_actionSelection;
  Items_LootListNode* saved_lists = Menu_Framework_lootLists;
  Items_BattleLoot* saved_entry = Menu_Framework_lootEntry;
  Menu_Framework_MenuRow* saved_rows = Menu_Framework_menuItems;
  int saved_restrictions = Menu_Framework_actionRestrictions;
  char saved_shortcut = Menu_Framework_shortcutInvoked;
  char saved_opened = Menu_Framework_openedWithoutMainMenu;
  char saved_art = Menu_Framework_unlockedBreakArt;
  char saved_ability = Menu_Framework_unlockedBattleAbility;
  std::array<Menu_Framework_MenuRow, 40> rows{};

  LiveMenuFixture() {
    Menu_Framework_menuState = {};
    Menu_Framework_menuState.currentMenu = 0x3F;
    Menu_Framework_actionSelection = {};
    Menu_Framework_menuItems = rows.data();
    Menu_Framework_lootLists = nullptr;
    Menu_Framework_lootEntry = nullptr;
  }
  ~LiveMenuFixture() {
    Menu_Framework_menuState = saved_menu;
    Menu_Framework_actionSelection = saved_selection;
    Menu_Framework_lootLists = saved_lists;
    Menu_Framework_lootEntry = saved_entry;
    Menu_Framework_menuItems = saved_rows;
    Menu_Framework_actionRestrictions = saved_restrictions;
    Menu_Framework_shortcutInvoked = saved_shortcut;
    Menu_Framework_openedWithoutMainMenu = saved_opened;
    Menu_Framework_unlockedBreakArt = saved_art;
    Menu_Framework_unlockedBattleAbility = saved_ability;
  }
};

TEST_CASE("F04123 live pause drives the production room and palette services",
          "[reward-flow][reward-live][F04123]") {
  LiveFixture fixture;
  Battle_actionState = nullptr;
  Battle_characterState = nullptr;
  b::BeginLiveRewardPause();
  CHECK(Field_isInteractionPaused == 1);
  CHECK(Field_areRoomUpdatesPaused == 1);
  CHECK(Field_isMinimapVisible == 0);
  CHECK_FALSE(render::LivePaletteAnimation().enabled);
  CHECK(render::LivePaletteAnimation().rows.at(0).timer.mode ==
        render::ColorTransform::kThreeQuarterBase);
  CHECK_FALSE(render::LivePaletteAnimation().rows.at(13).timer.active);
}

TEST_CASE("F04124 live restoration includes scene colors but leaves the map",
          "[reward-flow][reward-live][F04124]") {
  LiveFixture fixture;
  Battle_actionState = nullptr;
  b::EndLiveRewardPause();
  CHECK(Field_isInteractionPaused == 0);
  CHECK(Field_areRoomUpdatesPaused == 0);
  CHECK(Field_isMinimapVisible == 1);
  CHECK(render::LivePaletteAnimation().enabled);
  CHECK(render::LivePaletteAnimation().rows.at(13).timer.mode ==
        render::ColorTransform::kRestoreBase);
  CHECK(render::LivePaletteAnimation().tints.at(0).timer.active);
}

TEST_CASE("F04125 live resume requires no unused action or actor owner",
          "[reward-flow][reward-live][F04125]") {
  LiveFixture fixture;
  Battle_actionState = nullptr;
  Battle_characterState = nullptr;
  b::ResumeLiveAfterRewards();
  CHECK(Battle_actionFlowMode == 2);
  CHECK(Field_isInteractionPaused == 0);
  CHECK(Field_isMinimapVisible == 1);
}

TEST_CASE("F04126 live pending acquisition bypasses action and actor stats",
          "[reward-flow][reward-live][F04126]") {
  LiveFixture fixture;
  Battle_actionState = nullptr;
  fixture.actor.stats = nullptr;
  b::CheckLiveRewardUnlocks();
  CHECK(Battle_actionFlowMode == 5);
  CHECK(Field_areMovingPlatformsPaused == 1);
  CHECK(Field_isInteractionPaused == 77);
  CHECK(Audio_Seq_unusedArtsStatus.kills.totalKills == 1);
  CHECK(Audio_Seq_unusedArtsStatus.kills.battleAbilitiesUnlocked == 0);
}

TEST_CASE("F00220 live acquisition uses the actual pending and menu policies",
          "[reward-flow][reward-live][F00220]") {
  LiveMenuFixture fixture;
  b::ShowLivePendingRewardUnlock();
  CHECK(Battle_actionFlowMode == 6);
  CHECK(Field_isInteractionPaused == 1);
  CHECK(Menu_Framework_menuState.currentMenu == 3);
  CHECK(Menu_Framework_actionSelection.menuStatus == 1);
  CHECK(Menu_Framework_unlockedBreakArt == 0);
  CHECK(Menu_Framework_unlockedBattleAbility == 1);
  CHECK(Audio_Seq_unusedArtsStatus.kills.battleAbilitiesUnlocked == 0);
}

TEST_CASE("F00221 live loot uses the actual native list owner and menu",
          "[reward-flow][reward-live][F00221]") {
  LiveMenuFixture fixture;
  Items_LootListNode node{};
  Items_lootListHead = &node;
  Battle_actionState = nullptr;
  VS::State::SetLiveScriptFlag(0xAB, 255);
  b::OpenLivePendingLootRewards();
  CHECK(Battle_actionFlowMode == 4);
  CHECK(Field_isInteractionPaused == 1);
  CHECK(Menu_Framework_menuState.currentMenu == 12);
  CHECK(Menu_Framework_lootLists == &node);
  CHECK(Menu_Framework_lootEntry == nullptr);
  CHECK(Items_lootListHead == &node);
}

TEST_CASE("F00222 live results use the production challenge menu",
          "[reward-flow][reward-live][F00222]") {
  LiveMenuFixture fixture;
  Battle_actionState = nullptr;
  VS::State::SetLiveScriptFlag(0xAB, 3);
  b::OpenLiveRewardResults();
  CHECK(Battle_actionFlowMode == 3);
  CHECK(Field_isInteractionPaused == 1);
  CHECK(Menu_Framework_menuState.currentMenu == 15);
  CHECK(Menu_Framework_openedWithoutMainMenu == 1);
  CHECK(Menu_Framework_actionSelection.menuStatus == 1);
}

TEST_CASE("F00223 live menu updates retain busy state or complete the return",
          "[reward-flow][reward-live][F00223]") {
  LiveMenuFixture fixture;
  const bool returning = GENERATE(false, true);
  Menu_Framework_menuItems = nullptr;
  Menu_Framework_menuState.currentMenu = returning ? 0x7F : 0x3F;
  Menu_Framework_actionSelection.menuStatus = 1;
  Battle_actionState = nullptr;
  b::UpdateLiveRewardResultsDialog();
  CHECK(Battle_actionFlowMode == (returning ? 2 : 99));
  CHECK(Menu_Framework_actionSelection.menuStatus == (returning ? 2 : 1));
  CHECK(Field_isInteractionPaused == (returning ? 0 : 77));
}
}  // namespace
