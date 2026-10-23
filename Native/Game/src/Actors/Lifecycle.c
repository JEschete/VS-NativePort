// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "ActorAssets.h"
#include "Actors/ActorModelSlot.h"
#include "Actors/ActorState.h"
#include "Actors/Lifecycle.h"
#include "Actors/ModelInstance.h"
#include "Actors/RoomState.h"
#include "Actors/Skeleton.h"
#include "Actors/Spawning.h"
#include "Battle/Actor.h"
#include "Battle/Ai/ActorCleanup.h"
#include "Battle/Damage.h"
#include "Battle/Feedback.h"
#include "Core/Memory/Heap.h"
#include "Core/Memory/WordCopy.h"
#include "Field/Gimmicks.h"
#include "Items/Equipment.h"
#include "Items/Inventory.h"
#include "Items/Loadout.h"
#include "Items/Weapons.h"
#include "Render/Palette.h"

// decomp: BATTLE.PRG _insertActor (src/BATTLE/BATTLE.PRG/146C.c:884)
int Actors_LinkActorAfterAshley(Battle_Actor* actor)
{

    if ((actor->kind & 7) && (actor->id != 0)) {
        int i;

        for (i = 0; i < 16; ++i) {
            if ((Battle_actors[i] != NULL) && (Battle_actors[i]->next == actor)) {
                return 0;
            }
        }

        if (actor->id != 0) {
            actor->next = Battle_actors[0]->next;
            Battle_actors[0]->next = actor;
        } else {
            actor->next = NULL;
        }

        return 1;
    }

    return 0;
}

// decomp: BATTLE.PRG _removeActor (src/BATTLE/BATTLE.PRG/146C.c:909)
int Actors_UnlinkActorFromRoomList(Battle_Actor* actor)
{
    if (actor->id != 0) {
        int i;

        for (i = 0; i < 16; ++i) {
            if (Battle_actors[i] != NULL && Battle_actors[i]->next == actor) {
                Battle_actors[i]->next = actor->next;
                actor->next = NULL;
                return 1;
            }
        }
    }

    return 0;
}

// decomp: BATTLE.PRG _removeActorAtIndex (src/BATTLE/BATTLE.PRG/146C.c:926)
int Actors_RemoveActorAtSlot(u_int index, int arg1)
{
    int i;

    if (index < 16) {

        Battle_Actor* actor = Battle_actors[index];

        if (actor != NULL) {

            Actors_StoreRoomActorState(index, arg1);

            if (actor->kind == 0x10) {
                Field_DropUnsupportedGimmickStack(index);
                Field_ReleaseGimmickInstance(index);
            } else {
                Content_ReleaseActorModel(index);
                if (actor->allocationKind == 2) {
                    Content_ReleaseActorModel(index + 4);
                }
            }

            Battle_Ai_FreeActorAiState(actor);

            for (i = 0; i < 16; ++i) {
                if (Battle_actors[i] != NULL && Battle_actors[i]->next == actor) {
                    Battle_actors[i]->next = actor->next;
                    break;
                }
            }

            Core_Memory_FreeFromLowEnd(actor);

            Battle_actors[index] = NULL;
        }

        return 1;
    }

    return 0;
}

// decomp: BATTLE.PRG func_8006C250 (src/BATTLE/BATTLE.PRG/146C.c:1842)
void Actors_FreezeAllActorAnimations(void)
{
    int i;

    for (i = 0; i < 16; ++i) {
        int temp_v0 = Actors_ActorStateFlags(i);
        if (!(temp_v0 & 4)) {
            Actors_SetActorDisplayFlags(i, temp_v0 | 4);
        }
    }
}

// decomp: BATTLE.PRG func_8006C2A0 (src/BATTLE/BATTLE.PRG/146C.c:1854)
void Actors_UnfreezeAllActorAnimations(void)
{
    int i;

    for (i = 0; i < 16; ++i) {
        int temp_v0 = Actors_ActorStateFlags(i);
        if (temp_v0 & 4) {
            Actors_SetActorDisplayFlags(i, temp_v0 & ~4);
        }
    }
}

// decomp: BATTLE.PRG func_8007647C (src/BATTLE/BATTLE.PRG/146C.c:6028)
void Actors_InitializePlaceholderActorStats(int arg0, int arg1)
{
    int i;

    Battle_Actor* temp_s1 = Battle_actors[arg0];
    Battle_ActorStats* temp_s0 = temp_s1->stats;

    Core_Memory_ZeroAlignedWords(temp_s0, sizeof *temp_s0);

    temp_s0->maxHp = 1;
    temp_s0->currentHp = 1;

    for (i = 0; i < 24; ++i) {
        temp_s1->stats->name[i] =
            Actors_placeholderActorName[i]; // Cube // Ashley Riot? But also reads 4 bytes over...
    }

    switch (arg1 & 0xFF) {
    case 1:
        temp_s0->cancelsHitResults = 1;
        Battle_SetActorMarker(arg0, 8);
        break;
    case 2:
        /* fallthrough */
    case 4:
        temp_s0->cancelsHitResults = 0;
        break;
    default:
        temp_s0->cancelsHitResults = 1;
    }
    temp_s1->roomSpawnIndex = 0x80;
    temp_s1->killCounted = 0;
    temp_s1->defeated = 0;
    temp_s0->actorId = 0x80;
}

