// Actual checked entrypoints; external services are controlled at their native
// boundaries.
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string_view>
#include <vector>

#include "Actors/ActorStateLive.hpp"
#include "Actors/AnimationControlLive.hpp"
#include "Battle/ActionOutcomesLive.hpp"
#include "Battle/ActionPhasesLive.hpp"
#include "Battle/ActionPresentationLive.hpp"
#include "Battle/ActionSequenceLive.hpp"
#include "Battle/ActorVitalsLive.hpp"
#include "Battle/Ai/PlanningWorkspaceLive.hpp"
#include "Content/ActorAssetsLive.hpp"
#include "Event/CutsceneLifecycleLive.hpp"
#include "Field/CameraLive.hpp"
#include "Field/GameModeLive.hpp"
#include "Items/LootLive.hpp"
#include "Render/CameraViewLive.hpp"
extern "C" {
#include "Battle/Action.h"
#include "Effects/Runtime.h"
#include "Event/RoomTriggerEntryLive.h"
#include "Field/CameraLiveState.h"
#include "Field/Interactions.h"
#include "Render/Camera.h"
Battle_ActionState* Battle_actionState { };
int Effects_effectBusyFlags { }, Render_projectionDistance { };
Field_SphericalCamera Field_sphericalCamera { };
u_int Field_controlMode { };
}
namespace {
using Event = std::array<int, 4>;
std::vector<Event> events;
Battle_ActionState first { }, other { };
std::string_view selected;
int pending { }, asset { }, can_start = -65536, planning { }, busy { }, event_result { };
std::uint32_t flags = 0x08000100;
bool valid = true;
void Record(int code, int a = 0, int b = 0, int c = 0)
{
    events.push_back({ code, a, b, c });
}
void Check(bool condition)
{
    if (!condition)
        std::_Exit(93);
}
void RunSequence(int function)
{
    namespace b = VS::Battle;
    switch (function) {
    case 187:
        b::WaitForLiveActionEffectReady();
        break;
    case 188:
        b::StartLiveReadyActionAnimation();
        break;
    case 192:
        b::WaitForLiveActionResolution();
        break;
    case 193:
        b::WaitForLiveTargetRecovery();
        break;
    case 195:
        b::FinishLiveActionSequence();
        break;
    default:
        std::_Exit(94);
    }
}
}
extern "C" {
void Core_Errorf(const char*, const char* message, ...)
{
    if (selected == "193-empty")
        Check(events.empty()
              && std::string_view(message).find("empty sequence")
                     != std::string_view::npos);
    if (selected == "187-missing-busy")
        Check(events.empty());
    if (selected == "193-count")
        Check(events.empty());
    if (selected == "195-count")
        Check(events == std::vector<Event> { { 14 } });
    std::fprintf(stderr, "%s\n", message);
    std::_Exit(85);
}
int Menu_IsSceneNotificationPending()
{
    Record(1);
    if (selected == "187-owner") {
        first.queue[3].primaryTarget.target.targetActor = 41;
        Battle_actionState = &other;
    }
    return pending;
}
int Effects_CanStartEffectTrack()
{
    Record(4);
    return can_start;
}
int Effects_StartEffectTrack(int track)
{
    Record(7, track);
    if (selected == "188-once")
        valid &= Effects_effectBusyFlags == 2;
    return 0;
}
void Battle_SetHudHidden(int hidden)
{
    Record(16, hidden);
    if (selected == "195-cleanup-current")
        other.selection.primaryTarget.target.targetActor = 41;
}
std::int16_t Event_EvaluateRoomEventTriggersEntry(
    int trigger, int actor, std::uint8_t* context, int kind)
{
    valid &= trigger == 12 && actor == 0 && context == nullptr && kind == 0;
    Record(20, trigger);
    Field_controlMode = 3;
    return static_cast<std::int16_t>(event_result);
}
}
namespace VS::Actors {
std::uint32_t LiveActorStateFlags(int actor)
{
    Record(2, actor);
    return flags;
}
int IsLiveActorActionAnimationBusy(int actor)
{
    Record(11, actor);
    if (selected == "193-shrink" || selected == "195-owner") {
        first.queue[0].targetCount = 3;
        Battle_actionState = &other;
    }
    return busy;
}
void RestoreLiveActorIdlePose(int actor) { Record(17, actor); }
}
namespace VS::Content {
int FindLiveFreeActorAssetRequest()
{
    Record(3);
    return asset;
}
}
namespace VS::Render {
void SetLiveCameraProjectionDistance(int projection)
{
    Render_projectionDistance = projection;
    Record(13, projection);
    if (selected == "195-projection-current") {
        Battle_actionState = &other;
        Field_sphericalCamera.values.distance = 300;
        Field_sphericalCamera.delta.mode = 0;
        Render_projectionDistance = 511;
    }
}
}
namespace VS::Field {
int LiveIsCameraLookAtOnAshley()
{
    Record(14);
    return -65536;
}
void LiveResumeFieldAfterMenu()
{
    Record(15);
    if (selected == "195-cleanup-current")
        Battle_actionState = &other;
}
}
namespace VS::Battle {
void AdvanceLiveActionEffectPhase() { Record(5); }
void StartLiveActionAnimation()
{
    Record(6);
    Effects_effectBusyFlags = 2;
}
void ReleaseLiveQueuedActorAttacks()
{
    Record(9);
    if (selected == "192-owner") {
        valid &= Battle_actionState == &other;
        other.queue[0].primaryTarget.target.targetActor = 41;
    }
}
void BeginLiveActionTrailFade()
{
    Record(10);
    if (selected == "192-owner")
        valid &= Battle_actionState == &other
              && other.queue[0].primaryTarget.target.targetActor == 41;
}
int BeginLiveActorDefeatAnimation(int actor)
{
    Record(12, actor);
    return actor == 0 ? 17 : 0;
}
void BeginLivePostActionRecovery(std::int32_t mode) { Record(18, mode); }
int ResolveLiveActorDefeat(int source, int target, int count)
{
    Record(19, source, target, count);
    if (selected == "195-owner" && events.size() == 7) {
        other.queue[0].primaryTarget.target.targetActor = 1;
        other.queue[0].targetCount = 3;
    }
    return 1;
}
}
namespace VS::Battle::Ai {
int RebuildLiveActorActionPlans(Battle_ActionRequest* action)
{
    Record(8);
    if (selected == "192-owner") {
        valid &= action == &first.queue[2];
        Battle_actionState = &other;
    }
    return planning;
}
}
namespace VS::Event {
bool BeginLiveQueuedCutscene()
{
    Record(21);
    return true;
}
}
namespace VS::Items {
void OpenLivePendingEnemyLoot() { Record(22); }
}

int main(int argc, char** argv)
{
    if (argc != 2)
        return 2;
    selected = argv[1];
    Battle_actionState = &first;
    Render_projectionDistance = 512;
    Field_sphericalCamera.values.distance = -9;
    first.preferredCameraDistance = -9;
    other.preferredCameraDistance = 100;
    Field_controlMode = 3;
    if (selected == "187-missing-busy" || selected == "187-missing") {
        Battle_actionState = nullptr;
        pending = selected == "187-missing-busy" ? 1 : 0;
        RunSequence(187);
    } else if (selected == "187-owner") {
        first.queueIndex = 0xFFFFFFFF;
        RunSequence(187);
        Check(events == std::vector<Event> { { 1 }, { 2, 41 }, { 3 }, { 4 }, { 5 } });
    } else if (selected == "187-busy-unused") {
        pending = -1;
        first.queue[0].targetCount = 65535;
        first.queue[0].primaryTarget.target.targetActor = 255;
        RunSequence(187);
        Check(events == std::vector<Event> { { 1 } });
    } else if (selected == "188-unused") {
        Battle_actionState = nullptr;
        Effects_effectBusyFlags = -1;
        RunSequence(188);
        Check(events.empty());
    } else if (selected == "188-once") {
        Effects_effectBusyFlags = 1;
        RunSequence(188);
        Check(events == std::vector<Event> { { 6 }, { 7, 0 } });
    } else if (selected == "192-missing") {
        Battle_actionState = nullptr;
        RunSequence(192);
    } else if (selected == "192-owner" || selected == "192-full-result") {
        first.queueIndex = 0xFFFFFFFE;
        planning = selected == "192-full-result" ? 65536 : 0;
        RunSequence(192);
        Check(events
              == (planning == 0 ? std::vector<Event> { { 8 }, { 9 }, { 10 } }
                                : std::vector<Event> { { 8 } }));
    } else if (selected.starts_with("193-")) {
        if (selected == "193-missing")
            Battle_actionState = nullptr;
        if (selected != "193-empty")
            first.chainStep = 1;
        auto& action = first.queue[0];
        action.targetCount = selected == "193-native255" ? 255 : 1;
        action.primaryTarget.targetsPosition = 1;
        action.primaryTarget.target.targetActor = 7;
        for (auto& target : action.targets)
            target.target.targetActor = 41;
        if (selected == "193-count")
            action.targetCount = 256;
        if (selected == "193-shrink") {
            action.targets[1].target.targetActor = 1;
            action.targets[2].target.targetActor = 0;
        }
        if (selected == "193-native255")
            busy = -2;
        if (selected == "193-world-unused") {
            action.targets[0].targetsPosition = -1;
            action.targets[0].target.targetActor = 255;
        }
        RunSequence(193);
        if (selected == "193-shrink")
            Check(events
                  == std::vector<Event> { { 11, 41 }, { 11, 1 }, { 11, 0 }, { 18, 0 } });
        if (selected == "193-native255") {
            Check(events.size() == 255);
            for (const auto& event : events)
                Check(event == Event { 11, 41 });
        }
        if (selected == "193-world-unused")
            Check(events == std::vector<Event> { { 18, 0 } });
    } else if (selected.starts_with("195-")) {
        if (selected == "195-unused" || selected == "195-used-missing") {
            Battle_actionState = nullptr;
            Render_projectionDistance = 511;
            Field_sphericalCamera.delta.mode = selected == "195-unused" ? 0 : 1;
        }
        if (selected == "195-projection-current") {
            Field_sphericalCamera.delta.mode = 1;
            Render_projectionDistance = 513;
        }
        if (selected == "195-count" || selected == "195-native255"
            || selected == "195-owner") {
            first.chainStep = 1;
            first.queue[0].targetCount = selected == "195-count"     ? 256
                                       : selected == "195-native255" ? 255
                                                                     : 1;
            for (auto& target : first.queue[0].targets)
                target.target.targetActor = 41;
            if (selected == "195-native255")
                busy = -2;
        }
        if (selected == "195-event1" || selected == "195-event-other") {
            Field_controlMode = 0;
            event_result = selected == "195-event1" ? 0x10001 : 2;
        }
        if (selected == "195-owner") {
            first.queue[0].targets[1].target.targetActor = 1;
            first.queue[0].targets[2].target.targetActor = 0;
        }
        RunSequence(195);
        if (selected == "195-unused")
            Check(events.empty());
        if (selected == "195-projection-current")
            Check(events == std::vector<Event> { { 13, 512 } }
                  && Field_sphericalCamera.values.distance == 108);
        if (selected == "195-cleanup-current")
            Check(events == std::vector<Event> { { 14 }, { 15 }, { 16, 0 }, { 17, 41 } });
        if (selected == "195-event1" || selected == "195-event-other")
            Check(events
                  == std::vector<Event> { { 14 }, { 15 }, { 16, 0 }, { 17, 0 },
                      { 20, 12 }, { selected == "195-event1" ? 21 : 22 } });
        if (selected == "195-native255")
            Check(events.size() == 256 && events[0] == Event { 14 });
        if (selected == "195-owner")
            Check(events
                  == std::vector<Event> { { 14 }, { 11, 41 }, { 11, 1 }, { 11, 0 },
                      { 15 }, { 16, 0 }, { 17, 0 } });
    } else {
        return 2;
    }
    return valid ? 0 : 95;
}
