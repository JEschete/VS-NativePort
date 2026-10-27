// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Actors/ActorModelSlot.h"
#include "Actors/Animation.h"
#include "Actors/ModelInstance.h"
#include "Actors/Skeleton.h"
#include "Battle/Abilities.h"
#include "Battle/Actor.h"
#include "Battle/BattleMode.h"
#include "Battle/Damage.h"
#include "Battle/Hud.h"
#include "Items/Inventory.h"
#include "Items/Loadout.h"
#include "Render/Palette.h"

// decomp: BATTLE.PRG func_8007C088 (src/BATTLE/BATTLE.PRG/146C.c:8655)
int Battle_IsActorWeaponDrawn(int actor_slot) { return Battle_actors[actor_slot]->weaponDrawn & 1; }

// decomp: BATTLE.PRG func_8007C0AC (src/BATTLE/BATTLE.PRG/146C.c:8657)
void Battle_SetActorBattleMode(int actor_slot, int battle_mode)
{
    if (battle_mode == 2) {
        Battle_actors[actor_slot]->weaponDrawn ^= 1;

        if (Battle_actors[actor_slot]->weaponDrawn & 1) {
            Actors_SetActorWeaponDrawn(actor_slot, 1, 0);
        } else {
            Actors_SetActorWeaponDrawn(actor_slot, 0, 0);
        }
    } else if (actor_slot != 0) {
        if ((Battle_actors[actor_slot]->kind == 4)
            || (Battle_actors[actor_slot]->kind == 8)) {
            if (battle_mode != 0) {
                Battle_actors[actor_slot]->kind = 4;
            } else {
                Battle_actors[actor_slot]->kind = 8;
            }
        }
    } else {
        (*Battle_actors)->weaponDrawn &= ~1;
        (*Battle_actors)->weaponDrawn |= battle_mode;
        Actors_SetActorWeaponDrawn(0, battle_mode, 1);
    }
    if (actor_slot == 0) {
        Battle_SetHudBattleMode(Battle_characterState->weaponDrawn & 1);
    }
}

