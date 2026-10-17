// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Actors/ActorAllocation.h"
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
int Battle_IsActorWeaponDrawn(int arg0) { return Battle_actors[arg0]->weaponDrawn & 1; }

// decomp: BATTLE.PRG func_8007C0AC (src/BATTLE/BATTLE.PRG/146C.c:8657)
void Battle_SetActorBattleMode(int arg0, int arg1)
{
    if (arg1 == 2) {
        Battle_actors[arg0]->weaponDrawn ^= 1;

        if (Battle_actors[arg0]->weaponDrawn & 1) {
            Actors_SetActorWeaponDrawn(arg0, 1, 0);
        } else {
            Actors_SetActorWeaponDrawn(arg0, 0, 0);
        }
    } else if (arg0 != 0) {
        if ((Battle_actors[arg0]->kind == 4)
            || (Battle_actors[arg0]->kind == 8)) {
            if (arg1 != 0) {
                Battle_actors[arg0]->kind = 4;
            } else {
                Battle_actors[arg0]->kind = 8;
            }
        }
    } else {
        (*Battle_actors)->weaponDrawn &= ~1;
        (*Battle_actors)->weaponDrawn |= arg1;
        Actors_SetActorWeaponDrawn(0, arg1, 1);
    }
    if (arg0 == 0) {
        Battle_SetHudBattleMode(Battle_characterState->weaponDrawn & 1);
    }
}

