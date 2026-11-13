// Checked Live routing; lower services are bounded callback/refusal controls.
#include <array>
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
    kUpdate = 1,
    kMenuState,
    kFieldResume,
    kRelease,
    kNoticeQuery,
    kNoticeRelease,
    kBeginAbility,
    kGrantAbility,
    kClut,
    kScene,
    kFreeze,
    kRoom,
    kAmbience,
    kIdle,
    kFlag,
    kPending,
    kPlatforms,
    kPose,
    kEffect
};
std::string_view selected;
std::vector<int> events;
Battle_ActionState first { }, replacement { };
Menu_Framework_ActionSelection menu_first { }, menu_replacement { };
Menu_Framework_ActionSelection* menu_owner = &menu_first;
std::int32_t notice = 0;
std::uint32_t pending = 0;
std::uint8_t flag = 1;
std::size_t ends = 0;
bool valid = true;

void Record(int op) { events.push_back(op); }
void Unexpected()
{
    Core_Errorf("battle", "Unexpected bounded service in %.*s",
        static_cast<int>(selected.size()), selected.data());
}
void RequireAbility()
{
    if (Battle_actionState == nullptr)
        Core_Errorf("battle", "Bounded older ability service refused missing owner");
}
} // namespace

extern "C" {
void Effects_SpawnAbilityLearnedEffect() { Record(kEffect); }
void Field_SetMovingPlatformsPausedEntry(int paused)
{
    valid &= paused == 1;
    Record(kPlatforms);
}
void Menu_ShowSceneNotification(int, int, int) { Unexpected(); }
int Menu_IsSceneNotificationPending()
{
    Record(kNoticeQuery);
    const auto result = notice;
    if (selected == "notice-query-result-cache") {
        notice = 1;
        menu_owner = nullptr;
    }
    return result;
}
void Menu_ReleaseSceneNotification()
{
    Record(kNoticeRelease);
    if (selected == "notice-current-clear" || selected == "notice-query-result-cache") {
        first.selection.actionId = 0;
        Battle_actionState = &first;
    }
    if (selected == "notice-service-refused")
        Battle_actionState = nullptr;
}
}

namespace VS::Battle {
int LivePendingBattleAbilityUnlocks()
{
    Record(kPending);
    return static_cast<int>(pending);
}
void LiveBeginPendingAbilityReward()
{
    RequireAbility();
    Record(kBeginAbility);
    if (selected == "loot-negative-replaced")
        valid &=
            Battle_actionState == &replacement && replacement.selection.actionId == 1;
    if (selected == "notice-current-clear" || selected == "notice-query-result-cache")
        valid &= Battle_actionState == &first && first.selection.actionId == 0;
}
void LiveGrantPendingAbilityUnlock()
{
    RequireAbility();
    Record(kGrantAbility);
    Battle_actionFlowMode = 14;
}
}
namespace VS::Menu::Framework {
void UpdateLiveMenuOverlay()
{
    Record(kUpdate);
    menu_owner = selected.ends_with("null-after-update") ? nullptr : &menu_replacement;
    if (selected.ends_with("busy-created"))
        menu_replacement.menuStatus = 1;
    if (selected == "notice-current-clear" || selected == "notice-query-result-cache")
        notice = 0;
    if (selected == "pending-upper-resume")
        Battle_rewardsFollowAction = 0x10000;
}
Menu_Framework_ActionSelection* LiveActionSelectionState()
{
    valid &= !events.empty() && events.front() == kUpdate;
    Record(kMenuState);
    return menu_owner;
}
void ReleaseLiveMenuRows()
{
    Record(kRelease);
    if (selected == "loot-negative-replaced" || selected.starts_with("notice-")) {
        replacement.selection.actionId = 1;
        Battle_actionState = &replacement;
    }
    if (selected == "loot-service-refused")
        Battle_actionState = nullptr;
    if (selected == "pending-upper-resume") {
        Battle_lootMenuOpened = INT32_MIN;
        Battle_rewardsFollowAction = 1;
    }
    if (selected == "pending-row-clears-loot") {
        Battle_lootMenuOpened = 0;
        pending = 1;
        flag = 0;
        Battle_actionState = nullptr;
    }
    if (selected == "pending-row-replaces-action") {
        replacement.selection.actionId = 255;
        Battle_actionState = &replacement;
    }
    if (selected == "pending-missing-after-release")
        Battle_actionState = nullptr;
    if (selected == "pending-invalid-after-release")
        first.selection.actionId = 256;
}
void OpenLiveAbilityUnlockMenu(int, int) { Unexpected(); }
void OpenLiveLootListMenu(Items_LootListNode*) { Unexpected(); }
void OpenLiveChallengeMenu() { Unexpected(); }
}
namespace VS::Field {
void LiveResumeFieldAfterMenu()
{
    Record(kFieldResume);
    if (selected == "loot-negative-replaced") {
        replacement.selection.actionId = 255;
        Battle_actionState = &replacement;
    }
}
void LiveSetRoomAnimationsPaused(int paused)
{
    valid &= paused == 0;
    Record(kRoom);
}
int SetLiveMinimapVisible(int)
{
    Unexpected();
    return 0;
}
}
namespace VS::State {
std::uint8_t LiveScriptFlag(int id)
{
    valid &= id == 0xB6;
    Record(kFlag);
    return flag;
}
}
namespace VS::Actors {
void UnfreezeLiveActorAnimations() { Unexpected(); }
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
}
namespace VS::Render {
void TransformLiveRoomPalettes(int, int, int, std::int16_t, std::int16_t)
{
    Unexpected();
}
void TransformLiveSceneColors(
    int mode, int duration, std::int16_t red, std::int16_t green, std::int16_t blue)
{
    valid &= mode == 8 && duration == 4 && red == 0 && green == 0 && blue == 0;
    Record(kScene);
}
void SetLiveClutAnimationEnabled(int enabled)
{
    valid &= enabled == 1;
    Record(kClut);
}
}
namespace VS::Audio {
void LiveSetRoomAmbienceDucked(int ducked)
{
    valid &= ducked == 0;
    ++ends;
    Record(kAmbience);
    if (selected == "pending-upper-resume" && ends == 2)
        Battle_rewardsFollowAction = 0;
}
}

namespace {
void Initialize()
{
    Battle_actionState = &first;
    Battle_actionFlowMode = 99;
    Field_isInteractionPaused = 77;
    menu_first.menuStatus = 1;
    menu_replacement.menuStatus = -1;
    if (selected.ends_with("busy-created"))
        menu_owner = nullptr;
}
bool Loot()
{
    if (selected == "loot-busy-created")
        Battle_actionState = nullptr;
    b::UpdateLiveLootResults();
    if (selected == "loot-busy-created")
        return events == std::vector<int> { kUpdate, kMenuState }
            && Battle_actionFlowMode == 99;
    return events
        == std::vector<int> { kUpdate, kMenuState, kFieldResume, kRelease,
               kBeginAbility };
}
bool Ability()
{
    menu_owner = nullptr;
    if (selected == "ability-busy-one")
        Effects_effectBusyFlags = 1;
    if (selected == "ability-busy-two")
        Effects_effectBusyFlags = 2;
    if (selected == "ability-busy-both")
        Effects_effectBusyFlags = 3;
    if (selected == "ability-upper")
        Effects_effectBusyFlags = INT32_MIN;
    if (selected == "ability-service-refused")
        Effects_effectBusyFlags = 4;
    const bool busy = (static_cast<std::uint32_t>(Effects_effectBusyFlags) & 3U) != 0;
    if (busy || selected == "ability-service-refused")
        Battle_actionState = nullptr;
    b::UpdateLiveAbilityUnlockDelay();
    return busy ? events.empty() && Battle_actionFlowMode == 99
                : events == std::vector<int> { kGrantAbility }
                      && Battle_actionFlowMode == 14;
}
bool Notice()
{
    notice = selected == "notice-pending-negative" ? -1
           : selected == "notice-service-refused"  ? 0
                                                   : 1;
    if (selected == "notice-busy-created")
        Battle_actionState = nullptr;
    b::UpdateLiveAbilityUnlockResults();
    if (selected == "notice-busy-created")
        return events == std::vector<int> { kUpdate, kMenuState } && notice == 1;
    if (selected == "notice-pending-negative")
        return events == std::vector<int> { kUpdate, kMenuState, kNoticeQuery }
            && Field_isInteractionPaused == 77;
    return events
        == std::vector<int> { kUpdate, kMenuState, kNoticeQuery, kFieldResume, kRelease,
               kNoticeRelease, kBeginAbility };
}
bool Pending()
{
    if (selected == "pending-busy-created") {
        Battle_actionState = nullptr;
        Battle_rewardsFollowAction = -1;
        Battle_lootMenuOpened = -1;
    }
    if (selected == "pending-row-clears-loot" || selected == "pending-high-loot-no-state")
        Battle_lootMenuOpened = 0x10000;
    if (selected == "pending-high-loot-no-state")
        Battle_actionState = nullptr;
    b::UpdateLivePendingLootDialog();
    if (selected == "pending-busy-created")
        return events == std::vector<int> { kUpdate, kMenuState }
            && Battle_actionFlowMode == 99;
    if (selected == "pending-upper-resume")
        return events
                == std::vector<int> { kUpdate, kMenuState, kClut, kScene, kFreeze, kRoom,
                       kAmbience, kRelease, kClut, kScene, kFreeze, kRoom, kAmbience }
            && ends == 2 && Battle_actionFlowMode == 2 && Field_isInteractionPaused == 0;
    if (selected == "pending-row-clears-loot")
        return events
                == std::vector<int> { kUpdate, kMenuState, kRelease, kFlag, kPending,
                       kPlatforms }
            && Battle_actionFlowMode == 5 && Field_isInteractionPaused == 77;
    if (selected == "pending-row-replaces-action")
        return events
                == std::vector<int> { kUpdate, kMenuState, kRelease, kFlag, kFlag, kFlag,
                       kPlatforms }
            && Battle_actionFlowMode == 5;
    if (selected == "pending-high-loot-no-state")
        return events
                == std::vector<int> { kUpdate, kMenuState, kRelease, kClut, kScene,
                       kFreeze, kRoom, kAmbience }
            && Battle_actionFlowMode == 2;
    return false;
}
} // namespace

int main(int argc, char** argv)
{
    if (argc != 2)
        return 2;
    selected = argv[1];
    Initialize();
    const bool result = selected.starts_with("loot-")    ? Loot()
                      : selected.starts_with("ability-") ? Ability()
                      : selected.starts_with("notice-")  ? Notice()
                      : selected.starts_with("pending-") ? Pending()
                                                         : false;
    if (!result || !valid) {
        for (int event : events)
            std::fprintf(stderr, "%d ", event);
        std::fprintf(stderr, "\nphase=%u paused=%d follow=%d loot=%d ends=%zu\n",
            Battle_actionFlowMode, Field_isInteractionPaused, Battle_rewardsFollowAction,
            Battle_lootMenuOpened, ends);
        return 1;
    }
    return 0;
}
