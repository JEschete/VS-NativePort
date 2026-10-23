// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Actors/ActorModelSlot.h"
#include "Actors/ActorState.h"
#include "Actors/Location.h"
#include "Actors/ModelInstance.h"
#include "Actors/Skeleton.h"
#include "Battle/Actor.h"
#include "Core/Memory/Copy.h"
#include "Core/Memory/Heap.h"
#include "Event/ScriptVm.h"
#include "Event/Triggers.h"
#include "Field/Collision.h"
#include "Field/Gimmicks.h"
#include "Items/Inventory.h"
#include "Items/Loadout.h"
#include "Render/Palette.h"
#include "State/Flags.h"

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
    State_SetScriptFlag(0xA9, 0);
    if (startState == 0) {
        short i;
        for (i = 32; i < 64; ++i) {
            State_SetScriptFlag(i, 0);
        }
    }

    if (clearedSectionLen != 0) {
        Event_roomEventTriggers = Core_Memory_AllocateFromLowEnd(clearedSectionLen);
        memcpy(Event_roomEventTriggers, clearedSection, clearedSectionLen);
    } else {
        Event_roomEventTriggers = NULL;
    }

    if (doorSectionLen != 0) {
        Event_roomDoorEventData = Core_Memory_AllocateFromLowEnd(doorSectionLen);
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
        ((void (*)())Event_ReleaseScriptActorControl)();
    }
    if (Event_roomEventTriggers != 0) {
        Core_Memory_FreeFromLowEnd(Event_roomEventTriggers);
    }
    if (Event_roomDoorEventData != 0) {
        Core_Memory_FreeFromLowEnd(Event_roomDoorEventData);
    }
}

// decomp: BATTLE.PRG func_800BEBEC (src/BATTLE/BATTLE.PRG/4A0A8.c:4273)
void Event_ReleaseZoneEventResources(void) { }

// decomp: BATTLE.PRG func_800BEC58 (src/BATTLE/BATTLE.PRG/4A0A8.c:4302)
short Event_EvaluateRoomEventTriggers(int arg0, int arg1, u_char* arg2, int arg3)
{
    char buf[64];
    Actors_ActorLocationQuery position;
    short type;
    short mode;
    short i;
    short otherActorId;
    short flagId;
    short actorId;
    short flagValue;
    int result;
    short negate;
    short eventId;
    u_char* p;
    short left;
    int right;

    type = arg0;
    mode = arg3;

    if (type == 0xFF) {
        if (Event_playbackPhase == 0) {
            return 0;
        }
        return 2;
    }
    if (type == 1) {
        int scriptSection = Event_hasRoomEntryScript;
        if (scriptSection & 1) {
            Event_hasRoomEntryScript = 0;
            Event_playbackPhase = 1;
            Event_eventSource = 1;
            Event_isDirectEventRequest = 1;
            return 1;
        }
    }
    if (type == 0x40) {
        Event_eventId = arg1;
        Event_eventSource = 0;
        Event_playbackPhase = 1;
        Event_isDirectEventRequest = 1;
        return 1;
    }
    if (Event_playbackPhase != 0 && Event_isDirectEventRequest != 0) {
        return 2;
    }

    for (i = 0; i < 16; ++i) {
        u_short offset = ((u_short*)Event_roomEventTriggers)[i];
        if (offset == 0xFFFF) {
            return 0;
        }
        p = (u_char*)Event_roomEventTriggers + offset;
        if (!(*p & type)) {
            continue;
        }
        p++;
        eventId = Core_Memory_ReadUnalignedShort(p);
        p += 2;
        negate = 0;
        while (1) {
            result = 0;
            switch (*p) {
            case 0xFF:
                if (eventId & 0x8000) {
                    Event_eventId = eventId & 0xF;
                    Event_eventSource = 2;
                } else {
                    Event_eventId = eventId;
                    Event_eventSource = 0;
                }
                Event_playbackPhase = 1;
                Event_isDirectEventRequest = 0;
                return 1;
            case 1:
                Actors_ActorPlacement(0, &position);
                if (position.location.packedTile.target.tileX == p[1]
                    && position.location.packedTile.target.tileZ == p[2]) {
                    result = 1;
                }
                p += 3;
                break;
            case 2:
                Actors_ActorPlacement(0, &position);
                if (position.location.packedTile.target.tileX >= p[1]
                    && position.location.packedTile.target.tileX < p[1] + p[3]
                    && position.location.packedTile.target.tileZ >= p[2]
                    && position.location.packedTile.target.tileZ < p[2] + p[4]) {
                    result = 1;
                }
                p += 5;
                break;
            case 3:
                left = State_ScriptFlag(((p[1] & 0xF) << 6) | (p[2] >> 2));
                right = p[3];
                goto compare;
            case 15:
                left = State_ScriptFlag(((p[1] & 0xF) << 6) | (p[2] >> 2));
                right = State_ScriptFlag(((p[2] & 3) << 8) | p[3]);
            compare:
                switch (p[1] & 0xF0) {
                case 0x00:
                    if (left <= right) {
                        result = 1;
                    }
                    break;
                case 0x10:
                    if (left >= right) {
                        result = 1;
                    }
                    break;
                case 0x20:
                    if (left == right) {
                        result = 1;
                    }
                    break;
                case 0x30:
                    if (left != right) {
                        result = 1;
                    }
                    break;
                case 0x40:
                    if (left < right) {
                        result = 1;
                    }
                    break;
                case 0x50:
                    if (left > right) {
                        result = 1;
                    }
                    break;
                }
                p += 4;
                break;
            case 4:
                left = Core_Memory_ReadUnalignedShort(p + 1);
                if (left & 0x1000) {
                    if (Actors_ActorAtSlot(left & 0x1F) != NULL) {
                        result = 1;
                    }
                } else if (Actors_FindActorByModelId(left) != 1) {
                    result = 1;
                }
                p += 3;
                break;
            case 5:
                left = flagValue = Actors_CountActorsByDisposition(p[1]);
                flagId = Core_Memory_ReadUnalignedShort(p + 2);
                if (flagId != 0) {
                    State_SetScriptFlag(flagId, flagValue);
                    result = 1;
                } else if (left != 0) {
                    result = 1;
                }
                p += 4;
                break;
            case 6:
                if (mode == 0) {
                    actorId = Event_ResolveScriptActorReference(Core_Memory_ReadUnalignedShort(p + 1));
                    otherActorId = Event_ResolveScriptActorReference(Core_Memory_ReadUnalignedShort(p + 3));
                    if (actorId != 1 && otherActorId != 1 && arg1 == actorId
                        && arg2 == otherActorId) {
                        result = 1;
                    }
                }
                p += 5;
                break;
            case 7:
                actorId = Event_ResolveScriptActorReference(Core_Memory_ReadUnalignedShort(p + 1));
                if (actorId == 1) {
                    negate = 0;
                    break;
                }
                left = flagValue = Actors_ActorHpPercentage(actorId);
                flagId = Core_Memory_ReadUnalignedShort(p + 3);
                if (flagId != 0) {
                    State_SetScriptFlag(flagId, flagValue);
                    result = 1;
                } else if (left != 0) {
                    result = 1;
                }
                p += 5;
                break;
            case 8:
                if (mode == 1 && ((u_char*)arg2)[0] == p[1]
                    && ((u_char*)arg2)[2] == p[2]) {
                    result = 1;
                }
                p += 3;
                break;
            case 9:
                if (!(Actors_ActorStateFlags(0) & 0x104000)) {
                    result = 1;
                }
                p += 1;
                break;
            case 10:
                left = flagValue =
                    Items_InventoryItemCount(Core_Memory_ReadUnalignedShort(p + 1));
                flagId = Core_Memory_ReadUnalignedShort(p + 3);
                if (flagId != 0) {
                    State_SetScriptFlag(flagId, flagValue);
                    result = 1;
                } else if (left != 0) {
                    result = 1;
                }
                p += 5;
                break;
            case 11:
                if ((short)((int (*)(int, void*))Field_GimmickPlacement)(p[1], &position) >= 0
                    && position.location.packedTile.target.tileX == p[2]
                    && position.location.packedTile.target.tileZ == p[3]
                    && (p[4] == 0 || -(position.location.position.vy / 128) + 1 == p[4])
                    && (p[5] == 0
                        || (position.location.packedTile.target.facing >> 2) + 1 == p[5])) {
                    result = 1;
                }
                p += 6;
                break;
            case 12:
                left = flagValue = -Field_GimmickSupportHeight(p[1], p[2]) / 128;
                flagId = Core_Memory_ReadUnalignedShort(p + 3);
                if (flagId != 0) {
                    State_SetScriptFlag(flagId, flagValue);
                    result = 1;
                } else if (left != 0) {
                    result = 1;
                }
                p += 5;
                break;
            case 13:
                actorId = Event_ResolveScriptActorReference(Core_Memory_ReadUnalignedShort(p + 1));
                if (actorId == 1) {
                    negate = 0;
                    break;
                }
                Actors_ActorPlacement(actorId, &position);
                if (position.location.packedTile.target.tileX == p[3]
                    && position.location.packedTile.target.tileZ == p[4]) {
                    result = 1;
                }
                p += 5;
                break;
            case 14:
                actorId = Event_ResolveScriptActorReference(Core_Memory_ReadUnalignedShort(p + 1));
                if (actorId == 1) {
                    negate = 0;
                    break;
                }
                Actors_ActorPlacement(actorId, &position);
                if (position.location.packedTile.target.tileX >= p[3]
                    && position.location.packedTile.target.tileX < p[3] + p[5]
                    && position.location.packedTile.target.tileZ >= p[4]
                    && position.location.packedTile.target.tileZ < p[4] + p[6]) {
                    result = 1;
                }
                p += 7;
                break;
            case 16:
                actorId = Event_ResolveScriptActorReference(Core_Memory_ReadUnalignedShort(p + 1));
                if (actorId == 1) {
                    negate = 0;
                    break;
                }
                if (((int (*)(int))Actors_IsActorRetainedAfterDefeat)(actorId) != 0) {
                    result = 1;
                }
                p += 3;
                break;
            case 0xFE:
                negate = 1;
                p += 1;
                continue;
            default:
                sprintf(buf, "UNKNOWN TRIGGER DETECTED:%02x", *p);
                break;
            }
            if (result == negate) {
                break;
            }
            negate = 0;
        }
    }
}

