// Checked Live adapters with explicitly bounded callback services, not engine
// proof.
#include <algorithm>
#include <array>
#include <bit>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
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
#include "Effects/Runtime.h"
#include "Field/GameModeLiveState.h"
#include "Field/PlatformPlacementLive.h"
#include "Items/Loot.h"
#include "Items/LootLiveState.h"
#include "Menu/TextBox.h"
Battle_ActionState* Battle_actionState { };
Battle_ActionDefinition Battle_actionDefinitions[256] { };
u_int Battle_actionFlowMode { };
int Battle_rewardsFollowAction { }, Battle_lootMenuOpened { },
    Field_isInteractionPaused { };
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
enum Op {
    kUpdate,
    kMenuState,
    kNoticeQuery,
    kRows,
    kNoticeRelease,
    kClut,
    kScene,
    kFreeze,
    kRoom,
    kAmbience,
    kFlag,
    kPending,
    kIdle,
    kPose,
    kEffect,
    kPlatforms,
    kPalette,
    kUnfreeze,
    kMinimap,
    kNotice,
    kAbilities,
    kLoot,
    kResults,
};
std::string_view selected;
std::vector<Op> events;
Battle_ActionState first { }, replacement { };
Menu_Framework_ActionSelection menu_first { }, menu_replacement { };
Menu_Framework_ActionSelection* menu_owner = &menu_first;
Items_LootListNode loot_first { }, loot_replacement { };
std::int32_t notice = 0;
std::uint32_t pending = 0x1234ABCD;
std::uint8_t b6 = 0, ab = 3;
int shown_action = -1, shown_art = -1, shown_ability = -1;
bool valid = true;

void Record(Op op) { events.push_back(op); }
void Unexpected() { Core_Errorf("battle", "Unexpected bounded service"); }
bool Has(Op op) { return std::find(events.begin(), events.end(), op) != events.end(); }
std::size_t Count(Op op)
{
    return static_cast<std::size_t>(std::count(events.begin(), events.end(), op));
}
} // namespace

extern "C" {
void Effects_SpawnAbilityLearnedEffect() { Record(kEffect); }
void Field_SetMovingPlatformsPausedEntry(int paused)
{
    valid &= paused == 1;
    Record(kPlatforms);
}
void Menu_ShowSceneNotification(int kind, int action, int timed)
{
    valid &= kind == 12 && timed == 1;
    shown_action = action;
    Record(kNotice);
    if (selected == "wait-action-reread") {
        first.selection.actionId = 0;
        Battle_actionState = &first;
    }
    if (selected == "wait-missing-after-notice")
        Battle_actionState = nullptr;
    if (selected == "wait-invalid-after-notice")
        first.selection.actionId = 256;
}
int Menu_IsSceneNotificationPending()
{
    Record(kNoticeQuery);
    const auto result = notice;
    if (selected == "unlock-query-cache") {
        notice = -1;
        menu_owner = nullptr;
    }
    return result;
}
void Menu_ReleaseSceneNotification()
{
    Record(kNoticeRelease);
    if (selected == "unlock-current-pending" || selected == "unlock-query-cache") {
        Battle_actionState = nullptr;
    }
    if (selected == "unlock-missing-after-notice")
        Battle_actionState = nullptr;
    if (selected == "unlock-invalid-after-notice")
        first.selection.actionId = 256;
    if (selected == "unlock-end-invalid-notice-fixed") {
        replacement.selection.actionId = 255;
        Battle_actionState = &replacement;
    }
}
}

namespace VS::Battle {
int LivePendingBattleAbilityUnlocks()
{
    Record(kPending);
    return std::bit_cast<int>(pending);
}
void LiveBeginPendingAbilityReward() { Unexpected(); }
void LiveGrantPendingAbilityUnlock() { Unexpected(); }
} // namespace VS::Battle
namespace VS::Menu::Framework {
void UpdateLiveMenuOverlay()
{
    Record(kUpdate);
    menu_owner = selected == "unlock-null-menu" || selected == "dispatch-null-menu"
                   ? nullptr
                   : &menu_replacement;
    if (selected == "unlock-busy-created")
        menu_replacement.menuStatus = 1;
    if (selected.starts_with("dispatch-busy-")) {
        menu_replacement.menuStatus = 1;
        Battle_actionFlowMode = 5;
    }
}
Menu_Framework_ActionSelection* LiveActionSelectionState()
{
    valid &= !events.empty() && events.front() == kUpdate;
    Record(kMenuState);
    return menu_owner;
}
void ReleaseLiveMenuRows()
{
    Record(kRows);
    if (selected == "unlock-row-replaced") {
        replacement.selection.actionId = 255;
        Battle_actionState = &replacement;
    }
}
void OpenLiveAbilityUnlockMenu(int art, int ability)
{
    shown_art = art;
    shown_ability = ability;
    Record(kAbilities);
}
void OpenLiveLootListMenu(Items_LootListNode* owner)
{
    valid &= owner == &loot_first;
    valid &= Battle_lootMenuOpened == 0 && Battle_rewardsFollowAction == 1;
    Record(kLoot);
    if (selected == "post-callback-flags") {
        Battle_lootMenuOpened = 0x10000;
        Battle_rewardsFollowAction = INT32_MIN;
        Items_lootListHead = &loot_replacement;
    }
}
void OpenLiveChallengeMenu()
{
    valid &= Battle_lootMenuOpened == 0 && Battle_rewardsFollowAction == 0;
    Record(kResults);
    if (selected == "event-callback-flags") {
        Battle_lootMenuOpened = 0x10000;
        Battle_rewardsFollowAction = INT32_MIN;
    }
}
} // namespace VS::Menu::Framework
namespace VS::Field {
void LiveResumeFieldAfterMenu() { Unexpected(); }
void LiveSetRoomAnimationsPaused(int paused)
{
    valid &= paused == 0 || paused == 1;
    Record(kRoom);
}
int SetLiveMinimapVisible(int visible)
{
    valid &= visible == 0;
    Record(kMinimap);
    return 0;
}
} // namespace VS::Field
namespace VS::State {
std::uint8_t LiveScriptFlag(int id)
{
    valid &= id == 0xB6 || id == 0xAB;
    if (events.empty()
        && (selected.starts_with("event-") || selected.starts_with("post-"))) {
        valid &= Battle_lootMenuOpened == 0
              && Battle_rewardsFollowAction == (selected.starts_with("post-") ? 1 : 0);
    }
    Record(kFlag);
    return id == 0xB6 ? b6 : ab;
}
} // namespace VS::State
namespace VS::Actors {
void UnfreezeLiveActorAnimations() { Record(kUnfreeze); }
void FreezeLiveActorAnimations() { Record(kFreeze); }
void RestoreLiveActorIdlePose(int actor)
{
    valid &= actor == 0;
    Record(kIdle);
}
void PlayLiveActorActionPose(int actor, const std::int16_t* facing, int pose)
{
    valid &= actor == 0 && facing == nullptr && pose == 6;
    Record(kPose);
}
} // namespace VS::Actors
namespace VS::Render {
void TransformLiveRoomPalettes(
    int mode, int duration, int red, std::int16_t green, std::int16_t blue)
{
    valid &= mode == 13 && duration == 4 && red == -4 && green == -4 && blue == 0;
    Record(kPalette);
    if (selected == "wait-action-reread") {
        replacement.selection.actionId = 255;
        Battle_actionState = &replacement;
    }
    if (selected == "wait-missing-after-pause")
        Battle_actionState = nullptr;
    if (selected == "wait-packed-cache")
        pending ^= 0xFFFFFFFF;
}
void TransformLiveSceneColors(
    int mode, int duration, std::int16_t red, std::int16_t green, std::int16_t blue)
{
    valid &= mode == 8 && duration == 4 && red == 0 && green == 0 && blue == 0;
    Record(kScene);
}
void SetLiveClutAnimationEnabled(int enabled)
{
    valid &= enabled == 0 || enabled == 1;
    Record(kClut);
}
} // namespace VS::Render
namespace VS::Audio {
void LiveSetRoomAmbienceDucked(int ducked)
{
    valid &= ducked == 0 || ducked == 1;
    Record(kAmbience);
    if (ducked == 0 && selected == "post-clear-follow")
        Battle_rewardsFollowAction = 0;
    if (ducked == 0 && selected == "unlock-end-invalid-notice-fixed") {
        first.selection.actionId = 256;
    }
}
} // namespace VS::Audio

namespace {
void Initialize()
{
    Battle_actionState = &first;
    Battle_actionFlowMode = 99;
    Field_isInteractionPaused = 77;
    menu_first.menuStatus = 1;
    menu_replacement.menuStatus = -1;
    Battle_lootMenuOpened = INT32_MIN;
    Battle_rewardsFollowAction = 0;
}
bool Wait()
{
    menu_owner = nullptr;
    if (selected == "wait-busy-one")
        Effects_effectBusyFlags = 1;
    if (selected == "wait-busy-two")
        Effects_effectBusyFlags = 2;
    if (selected == "wait-busy-both")
        Effects_effectBusyFlags = 3;
    if (selected == "wait-busy-upper-two")
        Effects_effectBusyFlags = INT32_MIN + 2;
    if (selected.starts_with("wait-busy-")) {
        Battle_actionState = nullptr;
        b::WaitForLiveRewardUnlockEffects();
        return events.empty() && Battle_actionFlowMode == 99
            && Field_isInteractionPaused == 77;
    }
    Effects_effectBusyFlags = selected == "wait-upper-clear" ? INT32_MIN : 4;
    if (selected == "wait-missing")
        Battle_actionState = nullptr;
    if (selected == "wait-invalid")
        first.selection.actionId = 256;
    if (selected == "wait-action-reread" || selected == "wait-missing-after-pause"
        || selected == "wait-missing-after-notice"
        || selected == "wait-invalid-after-notice") {
        first.selection.actionId = 1;
    }
    b::WaitForLiveRewardUnlockEffects();
    if (selected == "wait-action-reread") {
        return shown_action == 255 && Battle_actionDefinitions[0].unlocked == 1
            && Battle_actionDefinitions[1].unlocked == 0 && Battle_actionFlowMode == 6
            && !Has(kPending) && !Has(kAbilities);
    }
    return events
            == std::vector<Op> { kPending, kPalette, kClut, kUnfreeze, kRoom, kMinimap,
                   kAmbience, kAbilities }
        && shown_art == 0x1234 && shown_ability == 0xABCD && Battle_actionFlowMode == 6
        && Field_isInteractionPaused == 1;
}
bool Unlock()
{
    pending = 1;
    if (selected == "unlock-notice-negative")
        notice = -1;
    if (selected == "unlock-busy-created")
        Battle_actionState = nullptr;
    if (selected == "unlock-row-replaced" || selected == "unlock-missing-after-notice"
        || selected == "unlock-invalid-after-notice"
        || selected == "unlock-end-invalid-notice-fixed"
        || selected == "unlock-empty-resume") {
        pending = 0;
        b6 = 1;
    }
    b::UpdateLiveRewardUnlockDialog();
    if (selected == "unlock-busy-created") {
        return events == std::vector<Op> { kUpdate, kMenuState }
            && Battle_actionFlowMode == 99;
    }
    if (selected == "unlock-notice-negative") {
        return events == std::vector<Op> { kUpdate, kMenuState, kNoticeQuery }
            && Field_isInteractionPaused == 77;
    }
    if (selected == "unlock-empty-resume") {
        return Battle_actionFlowMode == 2 && Field_isInteractionPaused == 0
            && Count(kAmbience) == 2 && !Has(kPending);
    }
    const std::vector<Op> prefix { kUpdate, kMenuState, kNoticeQuery, kClut, kScene,
        kFreeze, kRoom, kAmbience, kRows, kNoticeRelease };
    std::vector<Op> expected = prefix;
    if (b6 == 0) {
        expected.insert(expected.end(), { kFlag, kPending, kPlatforms });
    } else {
        expected.insert(expected.end(), { kFlag, kFlag, kFlag, kPlatforms });
    }
    return events == expected && Battle_actionFlowMode == 5
        && Field_isInteractionPaused == 0;
}
bool EventStart()
{
    Battle_actionState = nullptr;
    menu_owner = nullptr;
    Battle_rewardsFollowAction = 0x10000;
    if (selected == "event-low-stage")
        ab = 0;
    b::BeginLiveEventRewardResults();
    if (selected == "event-low-stage") {
        return Battle_actionFlowMode == 2 && Battle_lootMenuOpened == 0
            && Battle_rewardsFollowAction == 0 && Count(kFlag) == 2 && !Has(kResults)
            && !Has(kIdle);
    }
    const bool changed = selected == "event-callback-flags";
    return Battle_actionFlowMode == 3 && Field_isInteractionPaused == 1
        && Battle_lootMenuOpened == (changed ? 0x10000 : 0)
        && Battle_rewardsFollowAction == (changed ? INT32_MIN : 0)
        && Count(kResults) == 1;
}
bool PostStart()
{
    Battle_actionState = nullptr;
    menu_owner = nullptr;
    Battle_rewardsFollowAction = INT32_MIN;
    Items_lootListHead = &loot_first;
    if (selected == "post-low-stage" || selected == "post-clear-follow")
        ab = 0;
    b::BeginLivePostActionRewards();
    if (ab == 0) {
        const bool cleared = selected == "post-clear-follow";
        return Battle_actionFlowMode == 2 && Battle_lootMenuOpened == 0
            && Battle_rewardsFollowAction == (cleared ? 0 : 1)
            && Count(kIdle) == (cleared ? 0U : 1U) && !Has(kLoot);
    }
    const bool changed = selected == "post-callback-flags";
    return Battle_actionFlowMode == 4 && Field_isInteractionPaused == 1
        && Battle_lootMenuOpened == (changed ? 0x10000 : 0)
        && Battle_rewardsFollowAction == (changed ? INT32_MIN : 1)
        && Items_lootListHead == (changed ? &loot_replacement : &loot_first);
}
bool Dispatch()
{
    Battle_actionState = nullptr;
    menu_owner = nullptr;
    if (selected.starts_with("dispatch-unknown-")) {
        constexpr std::array<std::string_view, 7> kNames { "dispatch-unknown-zero",
            "dispatch-unknown-resume", "dispatch-unknown-high3", "dispatch-unknown-high4",
            "dispatch-unknown-high5", "dispatch-unknown-high6",
            "dispatch-unknown-negative" };
        constexpr std::array<u_int, 7> kPhases { 0, 2, 0x10003, 0x10004, 0x10005, 0x10006,
            0xFFFFFFFF };
        const auto found = std::find(kNames.begin(), kNames.end(), selected);
        if (found == kNames.end())
            return false;
        const auto phase = kPhases.at(static_cast<std::size_t>(found - kNames.begin()));
        Battle_actionFlowMode = phase;
        return b::UpdateLiveRewardSequence() == 0 && Battle_actionFlowMode == phase
            && events.empty();
    }
    Battle_actionFlowMode =
        selected == "dispatch-busy-results" || selected == "dispatch-complete-results" ? 3
        : selected == "dispatch-busy-loot" || selected == "dispatch-null-menu"         ? 4
        : selected == "dispatch-effects-busy" || selected == "dispatch-effects-missing"
            ? 5
            : 6;
    if (selected == "dispatch-effects-busy")
        Effects_effectBusyFlags = 2;
    if (selected == "dispatch-effects-missing")
        Effects_effectBusyFlags = 4;
    if (selected == "dispatch-complete-results")
        ab = 0;
    pending = 1;
    const int result = b::UpdateLiveRewardSequence();
    if (selected.starts_with("dispatch-busy-")) {
        return result == 1 && Battle_actionFlowMode == 5
            && events == std::vector<Op> { kUpdate, kMenuState };
    }
    if (selected == "dispatch-effects-busy") {
        return result == 1 && Battle_actionFlowMode == 5 && events.empty();
    }
    if (selected == "dispatch-complete-results") {
        return result == 1 && Battle_actionFlowMode == 2
            && b::UpdateLiveRewardSequence() == 0;
    }
    return result == 1 && Battle_actionFlowMode == 5 && Has(kNoticeRelease)
        && Has(kPending);
}
} // namespace

int main(int argc, char** argv)
{
    if (argc != 2)
        return 2;
    selected = argv[1];
    Initialize();
    const bool result = selected.starts_with("wait-")     ? Wait()
                      : selected.starts_with("unlock-")   ? Unlock()
                      : selected.starts_with("event-")    ? EventStart()
                      : selected.starts_with("post-")     ? PostStart()
                      : selected.starts_with("dispatch-") ? Dispatch()
                                                          : false;
    if (!result || !valid) {
        for (Op event : events)
            std::fprintf(stderr, "%d ", event);
        std::fprintf(stderr, "\nphase=%u pause=%d follow=%d loot=%d\n",
            Battle_actionFlowMode, Field_isInteractionPaused, Battle_rewardsFollowAction,
            Battle_lootMenuOpened);
        return 1;
    }
    return 0;
}
