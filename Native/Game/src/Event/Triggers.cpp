// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Core/Memory/HeapLive.hpp"
#include "State/ScriptFlagsLive.hpp"
extern "C" {
#include "Legacy/Psx.h"
#include "Event/ScriptVm.h"
#include "Event/Triggers.h"

// decomp: D_800F4BA8 (D_800F4BA8@BATTLE/BATTLE.PRG)
char Event_isDirectEventRequest;

// decomp: _mpdClearedSection (_mpdClearedSection@BATTLE/BATTLE.PRG)
void* Event_roomEventTriggers;

// decomp: _mpdRoomDoorSection (_mpdRoomDoorSection@BATTLE/BATTLE.PRG)
void* Event_roomDoorEventData;

// decomp: _mpdScriptSection (_mpdScriptSection@BATTLE/BATTLE.PRG)
short Event_hasRoomEntryScript;

// decomp: BATTLE.PRG _loadMpdClearedScriptDoorSections (src/BATTLE/BATTLE.PRG/4A0A8.c:4201)
int Event_LoadRoomEventSections(void* clearedSection, u_short clearedSectionLen,
    void* scriptSection, u_short scriptSectionLen, void* doorSection,
    u_short doorSectionLen, int startState)
{
    VS::State::SetLiveScriptFlag(0xA9, 0);
    if (startState == 0) {
        short i;
        for (i = 32; i < 64; ++i) {
            VS::State::SetLiveScriptFlag(i, 0);
        }
    }

    if (clearedSectionLen != 0) {
        Event_roomEventTriggers = VS::Core::Memory::AllocateLiveFromLowEnd(clearedSectionLen);
        memcpy(Event_roomEventTriggers, clearedSection, clearedSectionLen);
    } else {
        Event_roomEventTriggers = NULL;
    }

    if (doorSectionLen != 0) {
        Event_roomDoorEventData = VS::Core::Memory::AllocateLiveFromLowEnd(doorSectionLen);
        memcpy(Event_roomDoorEventData, doorSection, doorSectionLen);
    } else {
        Event_roomDoorEventData = NULL;
    }

    if (Event_roomChangeState == 1) {
        Event_roomChangeState = 2;
        return 1;
    }

    if (Event_roomChangeState == 3) {
        Event_playbackPhase = 0;
        Event_roomChangeState = 0;
        Event_isStartConfirmed = 0;
    }

    Event_hasRoomEntryScript = scriptSectionLen != 0;

    if (Event_hasRoomEntryScript != 0) {
        memcpy(Event_scriptBuffer, scriptSection, scriptSectionLen);
    }
    return 0;
}

// decomp: BATTLE.PRG func_800BEB34 (src/BATTLE/BATTLE.PRG/4A0A8.c:4250)
void Event_FreeRoomEventSections(void)
{
    if (Event_roomChangeState == 3) {
        ((void (*)(...))Event_ReleaseScriptActorControl)();
    }
    if (Event_roomEventTriggers != 0) {
        VS::Core::Memory::FreeLiveFromLowEnd(Event_roomEventTriggers);
    }
    if (Event_roomDoorEventData != 0) {
        VS::Core::Memory::FreeLiveFromLowEnd(Event_roomDoorEventData);
    }
}

// decomp: BATTLE.PRG func_800BEBEC (src/BATTLE/BATTLE.PRG/4A0A8.c:4273)
void Event_ReleaseZoneEventResources(void) { }

}  // extern "C"
