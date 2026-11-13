#include <array>
#include <bit>
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string_view>
#include <vector>

#include "Actors/ActorRosterLive.hpp"
#include "Actors/AnimationControlLive.hpp"
#include "Audio/AmbienceLive.hpp"
#include "Battle/AbilitiesLive.hpp"
#include "Battle/RewardFlowLive.hpp"
#include "Field/GameModeLive.hpp"
#include "Field/MapLive.hpp"
#include "Field/RoomRenderingLive.hpp"
#include "Menu/Framework/FrameworkLive.hpp"
#include "Render/PaletteAnimationLive.hpp"
#include "State/ScriptFlagsLive.hpp"
extern "C" {
#include "Battle/Action.h"
#include "Battle/ActionFlow.h"
#include "Battle/ActionTable.h"
#include "Battle/Rewards.h"
#include "Field/GameModeLiveState.h"
#include "Field/PlatformPlacementLive.h"
#include "Items/LootLiveState.h"
#include "Menu/TextBox.h"
Battle_ActionState* Battle_actionState { };
Battle_ActionDefinition Battle_actionDefinitions[256] { };
u_int Battle_actionFlowMode { };
int Battle_rewardsFollowAction { }, Battle_lootMenuOpened { }, Field_isInteractionPaused { };
int Effects_effectBusyFlags { };
Items_LootListNode* Items_lootListHead { };
void Core_Errorf(const char*, const char* message, ...)
{
    std::va_list args;
    va_start(args, message);
    std::vfprintf(stderr, message, args);
    va_end(args);
    std::fputc('\n', stderr);
    std::_Exit(85);
}
}

namespace {
namespace b = VS::Battle;
using Word = std::uint32_t;
enum Op { kRoomPalette = 1, kClut, kUnfreeze, kRoom, kMinimap, kAmbience,
    kSceneColors, kFreeze, kIdle, kFlag, kPending, kPose, kEffect, kPlatforms,
    kNotice, kAbilityMenu, kLootMenu, kResults, kUpdate, kMenuState, kRelease };
std::string_view selected;
std::vector<int> events;
Battle_ActionState first { }, replacement { };
Menu_Framework_ActionSelection menu_first { }, menu_replacement { };
Menu_Framework_ActionSelection* menu_owner = &menu_first;
bool valid = true;
Word pending = 0;
std::array<std::uint8_t, 3> b6 { 1, 1, 1 };
std::array<std::uint8_t, 2> ab { 0, 0 };
int b6_reads = 0, ab_reads = 0, pending_reads = 0;
Word seen_art = 0, seen_ability = 0, seen_notice = 0;
Items_LootListNode* seen_loot { };
Items_LootListNode* Head(int index)
{
    return reinterpret_cast<Items_LootListNode*>(static_cast<std::uintptr_t>(0x1000 + index * 64));
}
void Record(int op) { events.push_back(op); }
Word RawWord(int action)
{
    Word value { };
    std::memcpy(&value, reinterpret_cast<const std::byte*>(&Battle_actionDefinitions[action]) + 12, 4);
    return value;
}
void SetWord(int action, Word value)
{
    std::memcpy(reinterpret_cast<std::byte*>(&Battle_actionDefinitions[action]) + 12, &value, 4);
}
void PauseCallback()
{
    if (selected == "begin-unused" || selected == "end-unused") {
        Battle_actionFlowMode = 0xAABBCCDD;
        Battle_actionState = nullptr;
        Battle_rewardsFollowAction = -1;
    }
    if (selected == "grant-replace-pause" || selected == "grant-both-replace") {
        replacement.selection.actionId = 255;
        Battle_actionState = &replacement;
    }
    if (selected == "grant-missing-after-pause")
        Battle_actionState = nullptr;
    if (selected == "grant-invalid-after-pause-fixed")
        first.selection.actionId = 256;
    if (selected == "grant-packed-cache")
        pending = 0x12345678;
}
} // namespace

extern "C" {
void Effects_SpawnAbilityLearnedEffect()
{
    Record(kEffect);
}
void Field_SetMovingPlatformsPausedEntry(int paused)
{
    valid &= paused == 1;
    Record(kPlatforms);
}
void Menu_ShowSceneNotification(int kind, int action, int timed)
{
    valid &= kind == 12 && timed == 1 && Battle_actionFlowMode == 6 && Field_isInteractionPaused == 1;
    seen_notice = static_cast<Word>(action);
    Record(kNotice);
    if (selected == "grant-replace-notice" || selected == "grant-both-replace") {
        replacement.selection.actionId = 255;
        Battle_actionState = &replacement;
    }
    if (selected == "grant-zero-after-notice")
        first.selection.actionId = 0;
    if (selected == "grant-invalid-after-notice")
        first.selection.actionId = 256;
    if (selected == "grant-missing-after-notice")
        Battle_actionState = nullptr;
    if (selected == "grant-invalid-after-pause-fixed") {
        valid &= action == 256;
        first.selection.actionId = 0;
    }
}
}
namespace VS::State {
std::uint8_t LiveScriptFlag(int id)
{
    Record(kFlag);
    if (id == 0xB6)
        return b6.at(static_cast<std::size_t>(b6_reads++));
    valid &= id == 0xAB;
    return ab.at(static_cast<std::size_t>(ab_reads++));
}
}
namespace VS::Actors {
void UnfreezeLiveActorAnimations() { Record(kUnfreeze); }
void FreezeLiveActorAnimations() { Record(kFreeze); }
void RestoreLiveActorIdlePose(int actor)
{
    valid &= actor == 0 && Field_isInteractionPaused == 0;
    Record(kIdle);
}
void PlayLiveActorActionPose(int actor, const std::int16_t* facing, int pose)
{
    valid &= actor == 0 && facing == nullptr && pose == 6 && Battle_actionFlowMode == 5;
    Record(kPose);
    if (selected == "check-pose-clears-follow") {
        Battle_rewardsFollowAction = 0;
        Battle_actionFlowMode = 0xAABBCCDD;
    }
}
}
namespace VS::Render {
void TransformLiveRoomPalettes(int mode, int duration, int red, std::int16_t green, std::int16_t blue)
{
    valid &= mode == 13 && duration == 4 && red == -4 && green == -4 && blue == 0;
    if (selected.starts_with("grant-"))
        valid &= Battle_actionFlowMode == 6;
    Record(kRoomPalette);
    PauseCallback();
}
void TransformLiveSceneColors(int mode, int duration, std::int16_t red, std::int16_t green, std::int16_t blue)
{
    valid &= mode == 8 && duration == 4 && red == 0 && green == 0 && blue == 0;
    Record(kSceneColors);
}
void SetLiveClutAnimationEnabled(int enabled)
{
    valid &= enabled == (!events.empty() && events.back() == kRoomPalette ? 0 : 1);
    if (selected == "end-unused") {
        Battle_actionFlowMode = 0xAABBCCDD;
        Battle_actionState = nullptr;
        Battle_rewardsFollowAction = -1;
    }
    if (selected.starts_with("resume-"))
        valid &= Battle_actionFlowMode == 2;
    Record(kClut);
}
}
namespace VS::Field {
void LiveResumeFieldAfterMenu() { std::abort(); }
void LiveSetRoomAnimationsPaused(int paused)
{
    valid &= paused == (events.back() == kUnfreeze ? 1 : 0);
    Record(kRoom);
}
int SetLiveMinimapVisible(int visible)
{
    valid &= visible == 0;
    Record(kMinimap);
    return visible;
}
}
namespace VS::Audio {
void LiveSetRoomAmbienceDucked(int ducked)
{
    valid &= ducked == (events.back() == kMinimap ? 1 : 0);
    Record(kAmbience);
    if (selected == "resume-clear")
        Battle_rewardsFollowAction = 0;
    if (selected == "resume-set")
        Battle_rewardsFollowAction = INT32_MIN;
    if (selected == "begin-unused" || selected == "end-unused")
        Field_isInteractionPaused = static_cast<int>(0xFFFFABCD);
}
}
namespace VS::Battle {
void LiveBeginPendingAbilityReward() { std::abort(); }
void LiveGrantPendingAbilityUnlock() { std::abort(); }
int LivePendingBattleAbilityUnlocks()
{
    ++pending_reads;
    Record(kPending);
    return std::bit_cast<int>(pending);
}
}
extern "C" int Menu_IsSceneNotificationPending() { std::abort(); }
extern "C" void Menu_ReleaseSceneNotification() { std::abort(); }
namespace VS::Menu::Framework {
void OpenLiveAbilityUnlockMenu(int art, int ability)
{
    valid &= Battle_actionFlowMode == 6 && Field_isInteractionPaused == 1;
    seen_art = static_cast<Word>(art);
    seen_ability = static_cast<Word>(ability);
    Record(kAbilityMenu);
}
void OpenLiveLootListMenu(Items_LootListNode* owner)
{
    valid &= Battle_actionFlowMode == 99 && Field_isInteractionPaused == 77;
    seen_loot = owner;
    Items_lootListHead = Head(2);
    Battle_actionFlowMode = 98;
    Record(kLootMenu);
}
void OpenLiveChallengeMenu()
{
    valid &= Battle_actionFlowMode == 99 && Field_isInteractionPaused == 77;
    Record(kResults);
}
void UpdateLiveMenuOverlay()
{
    Record(kUpdate);
    menu_owner = selected == "menu-null-after-update" ? nullptr : &menu_replacement;
}
Menu_Framework_ActionSelection* LiveActionSelectionState()
{
    valid &= !events.empty() && events.front() == kUpdate;
    Record(kMenuState);
    return menu_owner;
}
void ReleaseLiveMenuRows()
{
    valid &= Field_isInteractionPaused == 0;
    Record(kRelease);
}
}

namespace {
int CheckPause()
{
    if (selected == "begin-unused") {
        b::BeginLiveRewardPause();
        valid &= events == std::vector<int>{kRoomPalette, kClut, kUnfreeze, kRoom, kMinimap, kAmbience};
        valid &= Field_isInteractionPaused == 1;
    } else {
        b::EndLiveRewardPause();
        valid &= events == std::vector<int>{kClut, kSceneColors, kFreeze, kRoom, kAmbience};
        valid &= Field_isInteractionPaused == 0;
    }
    return valid && Battle_actionFlowMode == 0xAABBCCDD ? 0 : 1;
}
int CheckResume()
{
    Battle_rewardsFollowAction = selected == "resume-clear" ? -1 : 0;
    b::ResumeLiveAfterRewards();
    valid &= Battle_actionFlowMode == 2 && Field_isInteractionPaused == 0;
    valid &= events.back() == (selected == "resume-clear" ? kAmbience : kIdle);
    return valid ? 0 : 1;
}
int CheckUnlocks()
{
    if (selected == "check-pending-no-state" || selected == "check-pose-clears-follow") {
        b6 = {1, 2, 3};
        pending = 0x80000000;
        Battle_actionState = nullptr;
        if (selected == "check-pose-clears-follow")
            Battle_rewardsFollowAction = -1;
    } else {
        Battle_actionState = selected == "check-missing" ? nullptr : &first;
        first.selection.actionId = selected == "check-zero" ? 0
                                : selected == "check-invalid" ? 256 : 255;
        if (selected == "check-position")
            first.selection.primaryTarget.targetsPosition = INT32_MIN;
        if (selected == "check-continued")
            first.chainContinued = static_cast<char>(255);
        if (selected == "check-source")
            first.selection.primaryTarget.target.targetActor = 255;
        if (selected == "check-unlocked")
            Battle_actionDefinitions[255].unlocked = 1;
    }
    b::CheckLiveRewardUnlocks();
    if (selected == "check-missing" || selected == "check-invalid")
        return 86;
    const bool route = selected == "check-pending-no-state" || selected == "check-action";
    const Word expected = selected == "check-pose-clears-follow" ? 0xAABBCCDD : route ? 5 : 2;
    valid &= Battle_actionFlowMode == expected;
    valid &= events.back() == (route || selected == "check-pose-clears-follow" ? kPlatforms : kAmbience);
    if (selected == "check-pose-clears-follow")
        valid &= events[events.size() - 2] == kEffect;
    return valid ? 0 : 1;
}
int CheckGrant()
{
    Battle_actionState = selected == "grant-missing" ? nullptr : &first;
    first.selection.actionId = selected == "grant-invalid" ? 256
                              : selected == "grant-packed-cache" ? 0 : 1;
    SetWord(0, 0xAABB1122);
    SetWord(1, 0xCCDD1234);
    SetWord(255, 0xEEFF0022);
    pending = 0xFEDCBA98;
    b::ShowLivePendingRewardUnlock();
    if (selected == "grant-missing" || selected == "grant-invalid" ||
        selected == "grant-missing-after-pause" || selected == "grant-missing-after-notice" ||
        selected == "grant-invalid-after-notice")
        return 86;
    if (selected == "grant-packed-cache")
        valid &= seen_art == 0xFEDC && seen_ability == 0xBA98 && pending_reads == 1;
    else if (selected == "grant-zero-after-notice" || selected == "grant-invalid-after-pause-fixed")
        valid &= RawWord(0) == 0xAABB9122 && RawWord(1) == 0xCCDD1234;
    else
        valid &= RawWord(255) == 0xEEFF8022 && RawWord(1) == 0xCCDD1234;
    if (selected == "grant-replace-pause" || selected == "grant-both-replace")
        valid &= seen_notice == 255;
    return valid && Battle_actionFlowMode == 6 && Field_isInteractionPaused == 1 ? 0 : 1;
}
int CheckLoot()
{
    ab[0] = selected == "loot-low-unused" ? 2 : 255;
    Items_lootListHead = selected == "loot-owned" ? Head(1) : nullptr;
    b6[0] = 0;
    pending = 1;
    b::OpenLivePendingLootRewards();
    const Word expected = selected == "loot-low-unused" ? 2 : selected == "loot-owned" ? 4 : 5;
    valid &= Battle_actionFlowMode == expected;
    if (selected == "loot-owned")
        valid &= seen_loot == Head(1) && Items_lootListHead == Head(2);
    return valid ? 0 : 1;
}
int CheckResults()
{
    ab = selected == "results-reread" ? std::array<std::uint8_t, 2>{2, 255}
                                     : std::array<std::uint8_t, 2>{255, 0};
    Items_lootListHead = Head(1);
    b::OpenLiveRewardResults();
    valid &= Battle_actionFlowMode == (selected == "results-reread" ? 4U : 3U);
    valid &= ab_reads == (selected == "results-reread" ? 2 : 1);
    return valid && Field_isInteractionPaused == 1 ? 0 : 1;
}
int CheckMenu()
{
    menu_owner = nullptr;
    menu_replacement.menuStatus = selected == "menu-busy-created" ? 1 : -1;
    b::UpdateLiveRewardResultsDialog();
    if (selected == "menu-null-after-update")
        return 86;
    if (selected == "menu-busy-created")
        valid &= events == std::vector<int>{kUpdate, kMenuState} && Battle_actionFlowMode == 99;
    else
        valid &= events[7] == kRelease && Battle_actionFlowMode == 2 && Field_isInteractionPaused == 0;
    return valid ? 0 : 1;
}
} // namespace

int main(int argc, char** argv)
{
    if (argc != 2)
        return 2;
    selected = argv[1];
    Battle_actionFlowMode = 99;
    Field_isInteractionPaused = 77;
    if (selected == "begin-unused" || selected == "end-unused")
        return CheckPause();
    if (selected.starts_with("resume-"))
        return CheckResume();
    if (selected.starts_with("check-"))
        return CheckUnlocks();
    if (selected.starts_with("grant-"))
        return CheckGrant();
    if (selected.starts_with("loot-"))
        return CheckLoot();
    if (selected.starts_with("results-"))
        return CheckResults();
    if (selected.starts_with("menu-"))
        return CheckMenu();
    return 2;
}
