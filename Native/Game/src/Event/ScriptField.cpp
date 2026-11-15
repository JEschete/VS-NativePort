// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Actors/RoomEnemyMemoryLive.hpp"
#include "Audio/MusicLive.hpp"
#include "Battle/HudStateLive.hpp"
#include "Battle/RewardFlowLive.hpp"
#include "Field/TransitionsLive.hpp"
#include "Menu/Framework/FrameworkLive.hpp"
#include "State/ScriptFlagsLive.hpp"
extern "C" {
#include "Legacy/Psx.h"
#include "Event/ScriptAudio.h"
#include "Event/ScriptField.h"
#include "Event/ScriptVm.h"
#include "Event/Weather.h"
#include "Field/Doors.h"
#include "Field/Mpd.h"

// decomp: D_800E9BF0 (D_800E9BF0@BATTLE/BATTLE.PRG)
u_char Event_kMenuTransitionModes[320]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800F4B88 (D_800F4B88@BATTLE/BATTLE.PRG)
Event_WeatherTween Event_weatherTween;

// decomp: BATTLE.PRG func_800B91C4 (src/BATTLE/BATTLE.PRG/4A0A8.c:1780)
intptr_t Event_ScriptClearRoomActorCache(u_char* instruction, short thread_index)
{
    VS::Actors::ForgetLiveRoomEnemies(instruction[2], instruction[1]);
    return 0;
}

// decomp: BATTLE.PRG func_800B92D0 (src/BATTLE/BATTLE.PRG/4A0A8.c:1813)
intptr_t Event_ScriptSetHudVisible(u_char* instruction, short thread_index)
{
    VS::Battle::SetLiveGameplayHudHidden(instruction[1] ^ 1);
    return 0;
}

// decomp: BATTLE.PRG func_800B92F4 (src/BATTLE/BATTLE.PRG/4A0A8.c:1819)
intptr_t Event_ScriptRunMenuTransition(u_char* script, short thread_index)
{
    switch (script[1]) {
    case 10:
    case 11:
    case 12:
        if (Event_isMenuTransitionActive == 0) {
            VS::State::SetLiveScriptFlag(0xB6, Event_kMenuTransitionModes[script[1] - 10]);
            VS::Battle::BeginLiveEventRewardResults();
            Event_isMenuTransitionActive = 1;
            return 1;
        }
        goto finishState;
    case 14:
        if (Event_isMenuTransitionActive == 0) {
            if (VS::State::LiveScriptFlag(0xB6) == 1) {
                VS::State::SetLiveScriptFlag(0xB6, 2);
            }
            VS::Battle::BeginLiveEventRewardResults();
            Event_musicSlot = 2;
            Event_isMenuTransitionActive = 1;
            return 1;
        }
    finishState:
        if (VS::Battle::UpdateLiveRewardSequence() != 0) {
            return 1;
        }
        VS::State::SetLiveScriptFlag(0xB6, 0);
        Event_isMenuTransitionActive = 0;
        return 4;
    case 13:
        if (Event_isMenuTransitionActive == 0) {
            VS::State::SetLiveScriptFlag(0xB6, 1);
            VS::Battle::BeginLiveEventRewardResults();
            Event_isMenuTransitionActive = 1;
            return 1;
        }
        goto finishTransition;
    case 15:
        if (Event_isMenuTransitionActive == 0) {
            VS::Battle::BeginLivePostActionRewards();
            Event_isMenuTransitionActive = 1;
            return 1;
        }
    finishTransition:
        if (VS::Battle::UpdateLiveRewardSequence() == 0) {
            Event_isMenuTransitionActive = 0;
            return 4;
        }
        return 1;
    default:
        if (Event_isMenuTransitionActive == 0) {
            VS::Menu::Framework::OpenLiveGameMenu(script[1], 2);
            Event_isMenuTransitionActive = 1;
            return 1;
        }
        VS::Menu::Framework::UpdateLiveMenuOverlay();
        if (VS::Menu::Framework::LiveActionSelectionState()->menuStatus == 2) {
            VS::Menu::Framework::ReleaseLiveMenuRows();
            Event_isMenuTransitionActive = 0;
            break;
        }
        return 1;
    }
    return 4;
}

// decomp: BATTLE.PRG func_800B98C0 (src/BATTLE/BATTLE.PRG/4A0A8.c:2018)
intptr_t Event_ScriptChangeRoom(u_char* instruction, short thread_index)
{
    Field_MpdDoorRecord door_record;

    if (Event_roomChangeState == 0) {

        door_record.destinationZoneId = instruction[1];
        door_record.destinationRoomId = instruction[2];
        door_record.entryTileX = instruction[3];
        door_record.entryTileZ = instruction[5];
        door_record.tileLocation.entryDirection = instruction[4];
        door_record.entryElevation = instruction[6];
        door_record.hasEntryStep = instruction[7];
        door_record.reserved06Bit15 = instruction[7] >> 1;

        if (instruction[1] == 0) {
            VS::Field::LiveBeginScriptedRoomTransition(&door_record, instruction[8], instruction[9]);
        } else {
            VS::Audio::LiveSetZoneMusicSwitchEnabled((instruction[7] >> 3) & 1);
            VS::Field::LiveBeginScriptedZoneTransition(&door_record, instruction[8], instruction[9]);
        }
        Event_roomChangeState = instruction[7] & 4 ? 1 : 3;
    }

    if (Event_roomChangeState == 2) {
        Event_roomChangeState = 0;
        return 0;
    }
    return 1;
}

}  // extern "C"
