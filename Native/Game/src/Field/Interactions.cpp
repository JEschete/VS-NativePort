// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Audio/SfxLive.hpp"
#include "Field/TrapsLive.hpp"
#include "Items/LootLive.hpp"
#include "State/ScriptFlagsLive.hpp"
extern "C" {
#include "Legacy/Psx.h"
#include "Battle/Feedback.h"
#include "Field/Doors.h"
#include "Field/InteractionFocusLive.h"
#include "Field/Interactions.h"
#include "Field/Mpd.h"
#include "Items/Loot.h"
#include "Items/TreasureChests.h"

// decomp: D_800F1910 (D_800F1910@BATTLE/BATTLE.PRG)
Field_InteractionFocus Field_interactionFocus;

// decomp: D_800F196C (D_800F196C@BATTLE/BATTLE.PRG)
u_int Field_controlMode;

// decomp: BATTLE.PRG func_8006CE70 (src/BATTLE/BATTLE.PRG/146C.c:2163)
void Field_ApplyUnlockEffectToRoomObject(SVECTOR* target_position)
{
    int room_object_count;
    Field_MpdLockTrigger* door_records;
    Items_TreasureChest* chest_records;
    int lock_level_upper_exclusive = 4;
    int minimum_breakable_lock_level = 2;

    switch (target_position->pad & 0x1F0) {
    case 0x10:
        door_records = Field_RoomInteractiveDoors(&room_object_count);

        if ((door_records[target_position->pad & 0xF].trigger.lockState.lock.state < lock_level_upper_exclusive)
            && (door_records[target_position->pad & 0xF].trigger.lockState.lock.state >= minimum_breakable_lock_level)
            && (VS::Field::RollLiveRiskLockBreak() != 0)) {

            door_records[target_position->pad & 0xF].trigger.lockState.lock.state = 0;

            VS::Audio::LivePlaySfxCentered(0x7E, 0x3F);
            VS::Audio::LivePlaySfxCentered(0x7E, 0x40);

        } else {
            ((void (*)(void*))Battle_QueueWorldLockedLabel)(target_position);
        }

        break;

    case 0x20:
    case 0x100:
        chest_records = VS::Items::LiveRoomTreasureChests(&room_object_count);

        if ((chest_records[target_position->pad & 0xF].lockLevel < lock_level_upper_exclusive)
            && (chest_records[target_position->pad & 0xF].lockLevel >= 2)
            && !VS::State::LiveScriptFlag(chest_records[target_position->pad & 0xF].openedFlag + 0x3C0)
            && (VS::Field::RollLiveRiskLockBreak() != 0)) {

            chest_records[target_position->pad & 0xF].lockLevel = 0;

            VS::Audio::LivePlaySfxCentered(0x7E, 0x3F);
            VS::Audio::LivePlaySfxCentered(0x7E, 0x40);

        } else {
            ((void (*)(void*))Battle_QueueWorldLockedLabel)(target_position);
        }
        break;
    }
}

}  // extern "C"
