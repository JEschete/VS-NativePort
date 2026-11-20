#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <iterator>
#include <span>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Actors/ActorRecord.h"
#include "Actors/ModelInstance.h"
#include "Actors/Movement.h"
#include "Battle/AbilityState.h"
#include "Battle/Action.h"
#include "Battle/ActionFlow.hpp"
#include "Battle/Actor.h"
#include "Battle/RewardFlowLive.hpp"
#include "Battle/Rewards.hpp"
#include "Battle/TargetingState.hpp"
#include "Content/ModuleData.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Effects/Runtime.hpp"
#include "Field/EnvironmentLive.hpp"
#include "Field/EnvironmentLiveState.hpp"
#include "Field/GameMode.hpp"
#include "Field/GameModeLiveState.hpp"
#include "Field/Gimmicks.hpp"
#include "Field/Interactions.hpp"
#include "Field/Minimap.hpp"
#include "Field/RoomEffectsLive.hpp"
#include "Field/RoomRenderingLiveState.hpp"
#include "Items/LootLiveState.hpp"
#include "Menu/Framework/FrameworkLiveState.hpp"
#include "Menu/SceneNotifications.hpp"
#include "Menu/SceneNotificationsLive.hpp"
#include "Menu/TextBox.hpp"
#include "Render/PaletteAnimationLive.hpp"
#include "State/Progress.hpp"
#include "State/ScriptFlagsLive.hpp"

namespace {
namespace b = VS::Battle;
namespace render = VS::Render;
struct LiveFixture {
  decltype(std::to_array(Actors_actorRecords)) saved_records =
      std::to_array(Actors_actorRecords);
  render::PaletteAnimation saved_palette = render::LivePaletteAnimation();
  VS::Field::RoomWeatherState saved_weather =
      VS::Field::LoadLiveRoomWeatherState();
  std::int16_t saved_weather_alias = VS::Field::weatherParameters[6];
  std::int16_t saved_effect_sound = VS::Field::LiveRoomEffectState().sound_id;
  decltype(VS::Field::roomTint) saved_tint = VS::Field::roomTint;
  u_int saved_phase = VS::Battle::actionFlowMode;
  int saved_follow = VS::Battle::rewardsFollowAction;
  int saved_pause = VS::Field::isInteractionPaused;
  u_short saved_room_pause = VS::Field::areRoomUpdatesPaused;
  std::uint8_t saved_minimap = VS::Field::isMinimapVisible;
  u_char saved_platform_pause = VS::Field::areMovingPlatformsPaused;
  Battle_ActionState* saved_action = VS::Battle::actionState;
  Battle_Actor* saved_character = Battle_characterState;
  VS::Items::LootListNode* saved_loot = VS::Items::lootListHead;
  decltype(VS::State::artsStatus) saved_progress = VS::State::artsStatus;
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
    VS::Field::weatherAmbientSoundId = -1;
    VS::Field::LiveRoomEffectState().sound_id = -1;
    VS::Battle::actionFlowMode = 99;
    VS::Battle::rewardsFollowAction = 0;
    VS::Field::isInteractionPaused = 77;
    VS::Field::areRoomUpdatesPaused = 0;
    VS::Field::isMinimapVisible = 1;
    VS::Field::areMovingPlatformsPaused = 0;
    VS::Battle::actionState = &action;
    Battle_characterState = &actor;
    actor.stats = &stats;
    actor.model = &model;
    actor.equippedWeaponCategory = 1;
    VS::State::artsStatus = {};
    std::fill_n(VS::State::artsStatus.arts_learned, 12, 4);
    Battle_battleAbilityPointRequirements[0] = 1;
    VS::State::artsStatus.kills.total_kills = 1;
    VS::State::SetLiveScriptFlag(0xAB, 0);
    VS::State::SetLiveScriptFlag(0xB6, 0);
  }
  ~LiveFixture() {
    std::copy(saved_records.begin(), saved_records.end(), Actors_actorRecords);
    const_cast<render::PaletteAnimation&>(render::LivePaletteAnimation()) =
        saved_palette;
    VS::Field::StoreLiveRoomWeatherState(saved_weather);
    VS::Field::weatherParameters[6] = saved_weather_alias;
    VS::Field::LiveRoomEffectState().sound_id = saved_effect_sound;
    VS::Field::roomTint = saved_tint;
    VS::Battle::actionFlowMode = saved_phase;
    VS::Battle::rewardsFollowAction = saved_follow;
    VS::Field::isInteractionPaused = saved_pause;
    VS::Field::areRoomUpdatesPaused = saved_room_pause;
    VS::Field::isMinimapVisible = saved_minimap;
    VS::Field::areMovingPlatformsPaused = saved_platform_pause;
    VS::Battle::actionState = saved_action;
    Battle_characterState = saved_character;
    VS::Items::lootListHead = saved_loot;
    VS::State::artsStatus = saved_progress;
    Battle_battleAbilityPointRequirements[0] = saved_requirement;
    VS::State::SetLiveScriptFlag(0xAB, saved_ab);
    VS::State::SetLiveScriptFlag(0xB6, saved_b6);
  }
};

struct LiveMenuFixture : LiveFixture {
  decltype(VS::Menu::Framework::menuState) saved_menu =
      VS::Menu::Framework::menuState;
  decltype(VS::Menu::Framework::actionSelection) saved_selection =
      VS::Menu::Framework::actionSelection;
  VS::Items::LootListNode* saved_lists = VS::Menu::Framework::lootLists;
  VS::Items::BattleLoot* saved_entry = VS::Menu::Framework::lootEntry;
  VS::Menu::Framework::MenuRow* saved_rows = VS::Menu::Framework::menuItems;
  int saved_restrictions = VS::Menu::Framework::actionRestrictions;
  char saved_shortcut = VS::Menu::Framework::shortcutInvoked;
  char saved_opened = VS::Menu::Framework::openedWithoutMainMenu;
  char saved_art = VS::Menu::Framework::unlockedBreakArt;
  char saved_ability = VS::Menu::Framework::unlockedBattleAbility;
  std::array<VS::Menu::Framework::MenuRow, 40> rows{};

  LiveMenuFixture() {
    VS::Menu::Framework::menuState = {};
    VS::Menu::Framework::menuState.control.current_menu = 0x3F;
    VS::Menu::Framework::actionSelection = {};
    VS::Menu::Framework::menuItems = rows.data();
    VS::Menu::Framework::lootLists = nullptr;
    VS::Menu::Framework::lootEntry = nullptr;
  }
  ~LiveMenuFixture() {
    VS::Menu::Framework::menuState = saved_menu;
    VS::Menu::Framework::actionSelection = saved_selection;
    VS::Menu::Framework::lootLists = saved_lists;
    VS::Menu::Framework::lootEntry = saved_entry;
    VS::Menu::Framework::menuItems = saved_rows;
    VS::Menu::Framework::actionRestrictions = saved_restrictions;
    VS::Menu::Framework::shortcutInvoked = saved_shortcut;
    VS::Menu::Framework::openedWithoutMainMenu = saved_opened;
    VS::Menu::Framework::unlockedBreakArt = saved_art;
    VS::Menu::Framework::unlockedBattleAbility = saved_ability;
  }
};

struct LiveDialogFixture : LiveMenuFixture {
  alignas(VS::Core::Memory::kHeapUnitBytes) std::array<std::byte, 4096> arena{};
  VS::Core::Memory::ScopedLiveGameHeap heap{arena};
  u_int saved_control = VS::Field::controlMode;
  int saved_recovery = VS::Battle::recoveryRewardMode;
  int saved_effects = VS::Effects::effectBusyFlags;
  int saved_loot_opened = VS::Battle::lootMenuOpened;
  int* saved_notification = VS::Menu::sceneNotificationBuffer;
  VS::Menu::TextBoxState saved_notice_box = VS::Menu::textBoxes[7];
  std::uint8_t saved_notice_timer = VS::Menu::sceneNotificationTimer;
  std::uint8_t saved_notice_timed = VS::Menu::sceneNotificationTimed;
  std::span<int> notice;

  LiveDialogFixture() {
    auto* allocation =
        static_cast<int*>(VS::Core::Memory::AllocateLiveFromHighEnd(
            VS::Menu::kSceneNotificationBytes));
    REQUIRE(allocation != nullptr);
    notice = {allocation, VS::Menu::kSceneNotificationBytes / sizeof(int)};
    VS::Battle::recoveryRewardMode = 0;
    VS::Effects::effectBusyFlags = 0;
    VS::Battle::lootMenuOpened = 0;
    VS::Menu::sceneNotificationBuffer = nullptr;
  }
  ~LiveDialogFixture() {
    if (!VS::Menu::CleanupLiveSceneNotificationOwnedState()) std::terminate();
    if (VS::Core::Memory::LivePayloadCapacity(notice.data())) {
      VS::Core::Memory::FreeLiveFromHighEnd(notice.data());
    }
    VS::Field::controlMode = saved_control;
    VS::Battle::recoveryRewardMode = saved_recovery;
    VS::Effects::effectBusyFlags = saved_effects;
    VS::Battle::lootMenuOpened = saved_loot_opened;
    VS::Menu::sceneNotificationBuffer = saved_notification;
    VS::Menu::textBoxes[7] = saved_notice_box;
    VS::Menu::sceneNotificationTimer = saved_notice_timer;
    VS::Menu::sceneNotificationTimed = saved_notice_timed;
  }
};

TEST_CASE("F04123 live pause drives the production room and palette services",
          "[reward-flow][reward-live][F04123]") {
  LiveFixture fixture;
  VS::Battle::actionState = nullptr;
  Battle_characterState = nullptr;
  b::BeginLiveRewardPause();
  CHECK(VS::Field::isInteractionPaused == 1);
  CHECK(VS::Field::areRoomUpdatesPaused == 1);
  CHECK(VS::Field::isMinimapVisible == 0);
  CHECK_FALSE(render::LivePaletteAnimation().enabled);
  CHECK(render::LivePaletteAnimation().rows.at(0).timer.mode ==
        render::ColorTransform::kThreeQuarterBase);
  CHECK_FALSE(render::LivePaletteAnimation().rows.at(13).timer.active);
}

TEST_CASE("F04124 live restoration includes scene colors but leaves the map",
          "[reward-flow][reward-live][F04124]") {
  LiveFixture fixture;
  VS::Battle::actionState = nullptr;
  b::EndLiveRewardPause();
  CHECK(VS::Field::isInteractionPaused == 0);
  CHECK(VS::Field::areRoomUpdatesPaused == 0);
  CHECK(VS::Field::isMinimapVisible == 1);
  CHECK(render::LivePaletteAnimation().enabled);
  CHECK(render::LivePaletteAnimation().rows.at(13).timer.mode ==
        render::ColorTransform::kRestoreBase);
  CHECK(render::LivePaletteAnimation().tints.at(0).timer.active);
}

TEST_CASE("F04125 live resume requires no unused action or actor owner",
          "[reward-flow][reward-live][F04125]") {
  LiveFixture fixture;
  VS::Battle::actionState = nullptr;
  Battle_characterState = nullptr;
  b::ResumeLiveAfterRewards();
  CHECK(VS::Battle::actionFlowMode == 2);
  CHECK(VS::Field::isInteractionPaused == 0);
  CHECK(VS::Field::isMinimapVisible == 1);
}

TEST_CASE("F04126 live pending acquisition bypasses action and actor stats",
          "[reward-flow][reward-live][F04126]") {
  LiveFixture fixture;
  VS::Battle::actionState = nullptr;
  fixture.actor.stats = nullptr;
  b::CheckLiveRewardUnlocks();
  CHECK(VS::Battle::actionFlowMode == 5);
  CHECK(VS::Field::areMovingPlatformsPaused == 1);
  CHECK(VS::Field::isInteractionPaused == 77);
  CHECK(VS::State::artsStatus.kills.total_kills == 1);
  CHECK(VS::State::artsStatus.kills.battle_abilities_unlocked == 0);
}

TEST_CASE("F00220 live acquisition uses the actual pending and menu policies",
          "[reward-flow][reward-live][F00220]") {
  LiveMenuFixture fixture;
  b::ShowLivePendingRewardUnlock();
  CHECK(VS::Battle::actionFlowMode == 6);
  CHECK(VS::Field::isInteractionPaused == 1);
  CHECK(VS::Menu::Framework::menuState.control.current_menu == 3);
  CHECK(VS::Menu::Framework::actionSelection.menu_status == 1);
  CHECK(VS::Menu::Framework::unlockedBreakArt == 0);
  CHECK(VS::Menu::Framework::unlockedBattleAbility == 1);
  CHECK(VS::State::artsStatus.kills.battle_abilities_unlocked == 0);
}

TEST_CASE("F00221 live loot uses the actual native list owner and menu",
          "[reward-flow][reward-live][F00221]") {
  LiveMenuFixture fixture;
  VS::Items::LootListNode node{};
  VS::Items::lootListHead = &node;
  VS::Battle::actionState = nullptr;
  VS::State::SetLiveScriptFlag(0xAB, 255);
  b::OpenLivePendingLootRewards();
  CHECK(VS::Battle::actionFlowMode == 4);
  CHECK(VS::Field::isInteractionPaused == 1);
  CHECK(VS::Menu::Framework::menuState.control.current_menu == 12);
  CHECK(VS::Menu::Framework::lootLists == &node);
  CHECK(VS::Menu::Framework::lootEntry == nullptr);
  CHECK(VS::Items::lootListHead == &node);
}

TEST_CASE("F00222 live results use the production challenge menu",
          "[reward-flow][reward-live][F00222]") {
  LiveMenuFixture fixture;
  VS::Battle::actionState = nullptr;
  VS::State::SetLiveScriptFlag(0xAB, 3);
  b::OpenLiveRewardResults();
  CHECK(VS::Battle::actionFlowMode == 3);
  CHECK(VS::Field::isInteractionPaused == 1);
  CHECK(VS::Menu::Framework::menuState.control.current_menu == 15);
  CHECK(VS::Menu::Framework::openedWithoutMainMenu == 1);
  CHECK(VS::Menu::Framework::actionSelection.menu_status == 1);
}

TEST_CASE("F00223 live menu updates retain busy state or complete the return",
          "[reward-flow][reward-live][F00223]") {
  LiveMenuFixture fixture;
  const bool returning = GENERATE(false, true);
  VS::Menu::Framework::menuItems = nullptr;
  VS::Menu::Framework::menuState.control.current_menu = returning ? 0x7F : 0x3F;
  VS::Menu::Framework::actionSelection.menu_status = 1;
  VS::Battle::actionState = nullptr;
  b::UpdateLiveRewardResultsDialog();
  CHECK(VS::Battle::actionFlowMode == (returning ? 2 : 99));
  CHECK(VS::Menu::Framework::actionSelection.menu_status ==
        (returning ? 2 : 1));
  CHECK(VS::Field::isInteractionPaused == (returning ? 0 : 77));
}

TEST_CASE("F00196 live continuation resumes field and actual ability recovery",
          "[reward-dialogs][reward-live][F00196]") {
  LiveDialogFixture fixture;
  VS::Menu::Framework::menuItems = nullptr;
  VS::Menu::Framework::menuState.control.current_menu = 0x7F;
  VS::Menu::Framework::actionSelection.menu_status = 1;
  VS::Field::isMinimapVisible = 0;
  VS::Field::areMovingPlatformsPaused = 1;
  b::UpdateLiveLootResults();
  CHECK(VS::Menu::Framework::actionSelection.menu_status == 2);
  CHECK(VS::Field::isInteractionPaused == 0);
  CHECK(VS::Field::areRoomUpdatesPaused == 0);
  CHECK(VS::Field::areMovingPlatformsPaused == 0);
  CHECK(VS::Field::isMinimapVisible == 1);
  CHECK(VS::Battle::actionFlowMode == 0);
  CHECK(VS::Field::controlMode ==
        static_cast<u_int>(VS::Field::FieldControlMode::kGimmickMotion));
}

TEST_CASE("F00197 live delay uses older ability grant not reward phase six",
          "[reward-dialogs][reward-live][F00197]") {
  LiveDialogFixture fixture;
  VS::Effects::effectBusyFlags = 0x10000;
  b::UpdateLiveAbilityUnlockDelay();
  CHECK(VS::Battle::actionFlowMode == 14);
  CHECK(VS::Field::isInteractionPaused == 1);
  CHECK(VS::Field::areMovingPlatformsPaused == 1);
  CHECK(VS::Menu::Framework::menuState.control.current_menu == 3);
  CHECK(VS::Menu::Framework::actionSelection.menu_status == 1);
  CHECK(VS::Menu::Framework::unlockedBreakArt == 0);
  CHECK(VS::Menu::Framework::unlockedBattleAbility == 1);
}

TEST_CASE("F00197 live busy delay needs no unused action or character owner",
          "[reward-dialogs][reward-live][F00197]") {
  LiveDialogFixture fixture;
  VS::Effects::effectBusyFlags = GENERATE(1, 2, 3, -1);
  VS::Battle::actionState = nullptr;
  Battle_characterState = nullptr;
  b::UpdateLiveAbilityUnlockDelay();
  CHECK(VS::Battle::actionFlowMode == 99);
  CHECK(VS::Field::isInteractionPaused == 77);
  CHECK(VS::Menu::Framework::menuState.control.current_menu == 0x3F);
}

TEST_CASE("F00198 live notice blocks field restoration and ability recovery",
          "[reward-dialogs][reward-live][F00198]") {
  LiveDialogFixture fixture;
  VS::Menu::Framework::menuItems = nullptr;
  VS::Menu::Framework::menuState.control.current_menu = 0x7F;
  VS::Menu::Framework::actionSelection.menu_status = 1;
  VS::Menu::sceneNotificationBuffer = fixture.notice.data();
  VS::Menu::textBoxes[7].flags.done = 0;
  VS::Battle::actionState = nullptr;
  const auto refusals = VS::Menu::RefusedLiveSceneNotificationCount();
  b::UpdateLiveAbilityUnlockResults();
  CHECK(VS::Menu::RefusedLiveSceneNotificationCount() == refusals);
  CHECK(VS::Menu::Framework::actionSelection.menu_status == 2);
  CHECK(VS::Menu::sceneNotificationBuffer == fixture.notice.data());
  CHECK(VS::Field::isInteractionPaused == 77);
  CHECK(VS::Battle::actionFlowMode == 99);
}

TEST_CASE("F00198 live absent notice proceeds to actual older ability recovery",
          "[reward-dialogs][reward-live][F00198]") {
  LiveDialogFixture fixture;
  VS::Menu::Framework::menuItems = nullptr;
  VS::Menu::Framework::menuState.control.current_menu = 0x7F;
  VS::Menu::Framework::actionSelection.menu_status = 1;
  VS::Field::isMinimapVisible = 0;
  b::UpdateLiveAbilityUnlockResults();
  CHECK(VS::Menu::Framework::actionSelection.menu_status == 2);
  CHECK(VS::Menu::sceneNotificationBuffer == nullptr);
  CHECK(VS::Field::isInteractionPaused == 0);
  CHECK(VS::Field::isMinimapVisible == 1);
  CHECK(VS::Battle::actionFlowMode == 0);
  CHECK(VS::Field::controlMode ==
        static_cast<u_int>(VS::Field::FieldControlMode::kGimmickMotion));
}

TEST_CASE("F00198 live busy menu never advances the scene notice timer",
          "[reward-dialogs][reward-live][F00198]") {
  LiveDialogFixture fixture;
  VS::Menu::Framework::actionSelection.menu_status = 1;
  VS::Menu::sceneNotificationBuffer = fixture.notice.data();
  VS::Menu::textBoxes[7].flags.done = 1;
  VS::Menu::sceneNotificationTimed = 1;
  VS::Menu::sceneNotificationTimer = 12;
  VS::Battle::actionState = nullptr;
  b::UpdateLiveAbilityUnlockResults();
  CHECK(VS::Menu::sceneNotificationTimer == 12);
  CHECK(VS::Menu::sceneNotificationBuffer == fixture.notice.data());
  CHECK(VS::Battle::actionFlowMode == 99);
  CHECK(VS::Field::isInteractionPaused == 77);
}

TEST_CASE("F00224 live loot branch uses actual reward resume not field resume",
          "[reward-dialogs][reward-live][F00224]") {
  LiveDialogFixture fixture;
  VS::Menu::Framework::menuItems = nullptr;
  VS::Menu::Framework::menuState.control.current_menu = 0x7F;
  VS::Menu::Framework::actionSelection.menu_status = 1;
  VS::Battle::lootMenuOpened = INT32_MIN;
  VS::Battle::rewardsFollowAction = 0x10000;
  VS::Battle::actionState = nullptr;
  Battle_characterState = nullptr;
  VS::Field::isMinimapVisible = 0;
  VS::Field::areMovingPlatformsPaused = 1;
  b::UpdateLivePendingLootDialog();
  CHECK(VS::Menu::Framework::actionSelection.menu_status == 2);
  CHECK(VS::Battle::actionFlowMode == 2);
  CHECK(VS::Field::isInteractionPaused == 0);
  CHECK(VS::Field::isMinimapVisible == 0);
  CHECK(VS::Field::areMovingPlatformsPaused == 1);
  CHECK(VS::Battle::lootMenuOpened == INT32_MIN);
}

TEST_CASE("F00224 live empty loot selects actual pending unlock policy lazily",
          "[reward-dialogs][reward-live][F00224]") {
  LiveDialogFixture fixture;
  VS::Menu::Framework::menuItems = nullptr;
  VS::Menu::Framework::menuState.control.current_menu = 0x7F;
  VS::Menu::Framework::actionSelection.menu_status = 1;
  VS::Battle::actionState = nullptr;
  fixture.actor.stats = nullptr;
  b::UpdateLivePendingLootDialog();
  CHECK(VS::Menu::Framework::actionSelection.menu_status == 2);
  CHECK(VS::Battle::actionFlowMode == 5);
  CHECK(VS::Field::areMovingPlatformsPaused == 1);
  CHECK(VS::Field::isInteractionPaused == 77);
  CHECK(VS::Battle::lootMenuOpened == 0);
}

TEST_CASE("Reward dialogs retain four original void module entrypoints",
          "[reward-dialogs][reward-abi]") {
  using Entry = void (*)();
  constexpr std::array<Entry, 4> kEntries{
      b::UpdateLiveLootResults, b::UpdateLiveAbilityUnlockDelay,
      b::UpdateLiveAbilityUnlockResults, b::UpdateLivePendingLootDialog};
  constexpr std::array<std::uint32_t, 4> kAddresses{0x800797BC, 0x8007980C,
                                                    0x8007983C, 0x8007B884};
  const auto symbols = VS::Content::ModuleSymbols();
  for (std::size_t index = 0; index < kEntries.size(); ++index) {
    CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                            kAddresses.at(index)) ==
          reinterpret_cast<void*>(kEntries.at(index)));
  }
}

TEST_CASE("F00225 live wait opens new reward phase six through actual services",
          "[reward-dispatch][reward-live][F00225]") {
  LiveDialogFixture fixture;
  VS::Effects::effectBusyFlags = 0x10000;
  b::WaitForLiveRewardUnlockEffects();
  CHECK(VS::Battle::actionFlowMode == 6);
  CHECK(VS::Field::isInteractionPaused == 1);
  CHECK(VS::Menu::Framework::menuState.control.current_menu == 3);
  CHECK(VS::Menu::Framework::actionSelection.menu_status == 1);
  CHECK(VS::Menu::Framework::unlockedBattleAbility == 1);
  CHECK(VS::Battle::recoveryRewardMode == 0);
}

TEST_CASE("F00225 live busy mask does not require action or ability owners",
          "[reward-dispatch][reward-live][F00225]") {
  LiveDialogFixture fixture;
  VS::Effects::effectBusyFlags = GENERATE(1, 2, 3, INT32_MIN + 2);
  VS::Battle::actionState = nullptr;
  Battle_characterState = nullptr;
  b::WaitForLiveRewardUnlockEffects();
  CHECK(VS::Battle::actionFlowMode == 99);
  CHECK(VS::Field::isInteractionPaused == 77);
  CHECK(VS::Menu::Framework::menuState.control.current_menu == 0x3F);
}

TEST_CASE("F00226 live busy menu preserves production notice timing",
          "[reward-dispatch][reward-live][F00226]") {
  LiveDialogFixture fixture;
  VS::Menu::Framework::actionSelection.menu_status = 1;
  VS::Menu::sceneNotificationBuffer = fixture.notice.data();
  VS::Menu::textBoxes[7].flags.done = 1;
  VS::Menu::sceneNotificationTimed = 1;
  VS::Menu::sceneNotificationTimer = 12;
  VS::Battle::actionState = nullptr;
  b::UpdateLiveRewardUnlockDialog();
  CHECK(VS::Menu::Framework::actionSelection.menu_status == 1);
  CHECK(VS::Menu::sceneNotificationTimer == 12);
  CHECK(VS::Menu::sceneNotificationBuffer == fixture.notice.data());
  CHECK(VS::Battle::actionFlowMode == 99);
  CHECK(VS::Field::isInteractionPaused == 77);
}

void PrepareClosedUnlock() {
  VS::Menu::Framework::menuItems = nullptr;
  VS::Menu::Framework::menuState.control.current_menu = 0x7F;
  VS::Menu::Framework::actionSelection.menu_status = 1;
  VS::Battle::actionState = nullptr;
  VS::Field::controlMode = 0x89ABCDEF;
  VS::Field::isMinimapVisible = 0;
}

TEST_CASE("F00226 live close ends reward pause then checks actual unlocks",
          "[reward-dispatch][reward-live][F00226]") {
  LiveDialogFixture fixture;
  PrepareClosedUnlock();
  b::UpdateLiveRewardUnlockDialog();
  CHECK(VS::Menu::Framework::actionSelection.menu_status == 2);
  CHECK(VS::Menu::sceneNotificationBuffer == nullptr);
  CHECK(VS::Battle::actionFlowMode == 5);
  CHECK(VS::Field::isInteractionPaused == 0);
}

TEST_CASE(
    "F00226 live close preserves field control while starting unlock effects",
    "[reward-dispatch][reward-live][F00226]") {
  LiveDialogFixture fixture;
  PrepareClosedUnlock();
  b::UpdateLiveRewardUnlockDialog();
  CHECK(VS::Field::areMovingPlatformsPaused == 1);
  CHECK(VS::Field::controlMode == 0x89ABCDEF);
  CHECK(VS::Field::isMinimapVisible == 0);
  CHECK(render::LivePaletteAnimation().enabled);
}

TEST_CASE("F00227 live event start resets full flags before challenge results",
          "[reward-dispatch][reward-live][F00227]") {
  LiveDialogFixture fixture;
  VS::Battle::lootMenuOpened = INT32_MIN;
  VS::Battle::rewardsFollowAction = 0x10000;
  VS::Battle::actionState = nullptr;
  Battle_characterState = nullptr;
  VS::State::SetLiveScriptFlag(0xAB, 3);
  b::BeginLiveEventRewardResults();
  CHECK(VS::Battle::lootMenuOpened == 0);
  CHECK(VS::Battle::rewardsFollowAction == 0);
  CHECK(VS::Battle::actionFlowMode == 3);
  CHECK(VS::Menu::Framework::menuState.control.current_menu == 15);
  CHECK(VS::Menu::Framework::actionSelection.menu_status == 1);
  CHECK(VS::Field::isInteractionPaused == 1);
}

TEST_CASE(
    "F00228 live post action resets full flags before the actual loot menu",
    "[reward-dispatch][reward-live][F00228]") {
  LiveDialogFixture fixture;
  VS::Items::LootListNode node{};
  VS::Items::lootListHead = &node;
  VS::Battle::lootMenuOpened = INT32_MIN;
  VS::Battle::rewardsFollowAction = INT32_MIN;
  VS::Battle::actionState = nullptr;
  Battle_characterState = nullptr;
  VS::State::SetLiveScriptFlag(0xAB, 3);
  b::BeginLivePostActionRewards();
  CHECK(VS::Battle::lootMenuOpened == 0);
  CHECK(VS::Battle::rewardsFollowAction == 1);
  CHECK(VS::Battle::actionFlowMode == 4);
  CHECK(VS::Menu::Framework::menuState.control.current_menu == 12);
  CHECK(VS::Menu::Framework::lootLists == &node);
  CHECK(VS::Field::isInteractionPaused == 1);
}

TEST_CASE("F00229 live unknown full phases are not narrowed or booleanized",
          "[reward-dispatch][reward-live][F00229]") {
  LiveDialogFixture fixture;
  const u_int phase = GENERATE(0U, 2U, 7U, 0x10003U, 0x10004U, 0x10005U,
                               0x10006U, 0x80000003U, 0xFFFFFFFFU);
  VS::Battle::actionFlowMode = phase;
  VS::Battle::actionState = nullptr;
  Battle_characterState = nullptr;
  CHECK(b::UpdateLiveRewardSequence() == 0);
  CHECK(VS::Battle::actionFlowMode == phase);
  CHECK(VS::Menu::Framework::actionSelection.menu_status == 0);
  CHECK(VS::Field::isInteractionPaused == 77);
}

TEST_CASE("F00229 live handled waiting phases preserve the original int result",
          "[reward-dispatch][reward-live][F00229]") {
  LiveDialogFixture fixture;
  const u_int phase = GENERATE(3U, 4U, 5U, 6U);
  VS::Battle::actionFlowMode = phase;
  VS::Menu::Framework::actionSelection.menu_status = 1;
  VS::Effects::effectBusyFlags = 2;
  VS::Battle::actionState = nullptr;
  Battle_characterState = nullptr;
  CHECK(b::UpdateLiveRewardSequence() == 1);
  CHECK(VS::Battle::actionFlowMode == phase);
  CHECK(VS::Field::isInteractionPaused == 77);
}

TEST_CASE("F00229 live completed handler returns one before next idle dispatch",
          "[reward-dispatch][reward-live][F00229]") {
  LiveDialogFixture fixture;
  VS::Battle::actionFlowMode = 3;
  VS::Menu::Framework::menuItems = nullptr;
  VS::Menu::Framework::menuState.control.current_menu = 0x7F;
  VS::Menu::Framework::actionSelection.menu_status = 1;
  VS::Battle::actionState = nullptr;
  Battle_characterState = nullptr;
  CHECK(b::UpdateLiveRewardSequence() == 1);
  CHECK(VS::Battle::actionFlowMode == 2);
  CHECK(b::UpdateLiveRewardSequence() == 0);
  CHECK(VS::Field::isInteractionPaused == 0);
}

TEST_CASE("Reward dispatch keeps four VOID0 and the distinct INT0 module entry",
          "[reward-dispatch][reward-abi]") {
  using VoidEntry = void (*)();
  constexpr std::array<VoidEntry, 4> kEntries{
      b::WaitForLiveRewardUnlockEffects, b::UpdateLiveRewardUnlockDialog,
      b::BeginLiveEventRewardResults, b::BeginLivePostActionRewards};
  constexpr std::array<std::uint32_t, 4> kAddresses{0x8007B908, 0x8007B938,
                                                    0x8007B9A0, 0x8007B9CC};
  const auto symbols = VS::Content::ModuleSymbols();
  for (std::size_t index = 0; index < kEntries.size(); ++index) {
    CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                            kAddresses.at(index)) ==
          reinterpret_cast<void*>(kEntries.at(index)));
  }
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x8007B9FC) ==
        reinterpret_cast<void*>(&b::UpdateLiveRewardSequence));
}
}  // namespace
