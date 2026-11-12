#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string_view>
#include <vector>

#include "Actors/ActorRosterLive.hpp"
#include "Actors/ActorStateLive.hpp"
#include "Actors/AnimationControlLive.hpp"
#include "Actors/SkeletonLive.hpp"
#include "Battle/ActionPhasesLive.hpp"
#include "Battle/BattleModeLive.hpp"
#include "Field/RoomRenderingLive.hpp"
#include "Render/PaletteAnimationLive.hpp"
extern "C" {
#include "Battle/Action.h"
#include "Battle/ActionFlow.h"
#include "Battle/Actor.h"
#include "Battle/Damage.h"
#include "Effects/ActionEffects.h"
#include "Field/Interactions.h"
#include "State/Timers.h"
Battle_Actor* Battle_actors[42] { };
Battle_ActionState* Battle_actionState { };
u_int Battle_actionFlowMode { }, Field_controlMode { };
int Battle_recoveryRewardMode { }, State_tickSpeed { };
void Core_Errorf(const char*, const char* message, ...)
{
    std::fprintf(stderr, "%s\n", message);
    std::_Exit(85);
}
}

namespace {
using Event = std::array<int, 3>;
std::vector<Event> events;
Battle_ActionState first { }, replacement { };
std::string_view selected;
std::array<int, 16> reads { };
bool valid = true;
void Record(int operation, int first_arg = 0, int second = 0)
{
    events.push_back({ operation, first_arg, second });
}
}
extern "C" {
void Battle_SetActorMarkersHidden(int hidden)
{
    Record(5, hidden);
    Battle_recoveryRewardMode = 99;
    State_tickSpeed = 99;
}
void Effects_FadeWeaponTrails(int actor, int bone)
{
    valid &= actor == 7 && bone == -9;
    Record(8, actor, bone);
}
}
namespace VS::Actors {
void RestoreLiveActorIdlePose(int slot)
{
    Record(1, slot);
    if (selected == "movement") {
        valid &= Field_controlMode == 12 && Battle_actionFlowMode == 0;
        Battle_actionFlowMode = 9;
    } else if (selected == "recovery" && slot == 0) {
        Battle_actionState = &replacement;
    }
}
std::uint32_t LiveActorStateFlags(int slot)
{
    Record(2, slot);
    return ++reads.at(static_cast<std::size_t>(slot)) % 2 == 0 ? 1U : 0U;
}
void FreezeLiveActorAnimations() { Record(3); }
int FindLiveActorBoneTag(int actor, int tag, int mode)
{
    valid &= actor == 3 && tag == 0xF0 && mode == 0 && Battle_actionFlowMode == 10;
    first.queue[0].primaryTarget.target.targetActor = 7;
    Battle_actionState = &replacement;
    Record(7, actor, tag);
    return -9;
}
}
namespace VS::Render {
void TransformLiveRoomPalettes(
    int mode, int duration, int red, std::int16_t green, std::int16_t blue)
{
    valid &= mode == 8 && duration == 4 && red == 0 && green == 0 && blue == 0;
    Record(4);
}
void SetLiveClutAnimationEnabled(int enabled)
{
    valid &= enabled == 1;
    Record(6, enabled);
}
}
namespace VS::Field {
void LiveSetRoomAnimationsPaused(int paused)
{
    valid &= paused == 0;
    Record(9, paused);
}
}
namespace VS::Battle {
void LiveSetActorBattleMode(int actor, int mode)
{
    valid &= actor == 0 && mode == 2 && events.size() == 16 && Field_controlMode == 77;
    Record(10, actor, mode);
    Field_controlMode = 88;
}
}

int main(int argc, char** argv)
{
    if (argc != 2)
        return 2;
    selected = argv[1];
    namespace b = VS::Battle;
    if (selected == "missing-recovery") {
        b::BeginLivePostActionRecovery(0);
        return 86;
    }
    if (selected == "missing-trails") {
        b::BeginLiveActionTrailFade();
        return 86;
    }
    if (selected == "unused-state") {
        Battle_actionFlowMode = 77;
        b::ReleaseLiveQueuedActorAttacks();
        b::FinishLiveBattleAbilitySequence();
        valid &= Battle_actionFlowMode == 9;
        b::ResetLiveActionPhase();
        return valid && Battle_actionFlowMode == 0 && events.empty() ? 0 : 1;
    }
    if (selected == "movement") {
        b::WaitForLivePostActionMovement();
        return valid && Battle_actionFlowMode == 9
                    && events == std::vector<Event> { { 1, 0, 0 } }
                 ? 0
                 : 1;
    }
    if (selected == "recovery") {
        first.selection.primaryTarget.target.targetActor = 3;
        replacement.selection.primaryTarget.target.targetActor = 2;
        Battle_actionState = &first;
        b::BeginLivePostActionRecovery(-17);
        valid &= reads[0] == 2 && reads[1] == 2 && reads[2] == 0 && reads[3] == 2;
        valid &= Battle_recoveryRewardMode == -17 && State_tickSpeed == 2
              && Battle_actionFlowMode == 11 && Battle_actionState == &replacement;
        const std::vector<Event> tail(events.end() - 5, events.end());
        valid &= tail
              == std::vector<Event> { { 4, 0, 0 }, { 3, 0, 0 }, { 6, 1, 0 }, { 9, 0, 0 },
                     { 5, 0, 0 } };
        return valid ? 0 : 1;
    }
    if (selected == "trails") {
        first.queueIndex = 3;
        first.queue[0].primaryTarget.target.targetActor = 3;
        replacement.queue[0].primaryTarget.target.targetActor = 99;
        Battle_actionState = &first;
        b::BeginLiveActionTrailFade();
        return valid && events == std::vector<Event> { { 7, 3, 0xF0 }, { 8, 7, -9 } } ? 0
                                                                                      : 1;
    }
    if (selected == "toggle") {
        Field_controlMode = 77;
        b::BeginLiveAshleyWeaponToggle();
        valid &= Field_controlMode == 10 && events.size() == 17;
        for (int slot = 0; slot < 16; ++slot) {
            valid &= events.at(static_cast<std::size_t>(slot)) == Event { 1, slot, 0 };
        }
        return valid ? 0 : 1;
    }
    return 2;
}
