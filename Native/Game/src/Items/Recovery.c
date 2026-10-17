// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Battle/Action.h"
#include "Battle/ActionResolution.h"
#include "Battle/BattleTypes.h"
#include "Battle/Hit.h"
#include "Core/Random.h"
#include "Items/Recovery.h"

// decomp: BATTLE.PRG _itemRecovery (src/BATTLE/BATTLE.PRG/146C.c:12109)
short Items_CalculateItemRecovery(Battle_ActionDefinition* arg0, Battle_HitResult* arg1 __attribute__((unused)),
    Battle_HitResult* arg2 __attribute__((unused)), int arg3, int arg4)
{
    short temp_v0;
    short temp_s0 = arg0->hits[arg3].statFactor * 5;

    if (arg4 != 0) {
        temp_v0 = Core_Random_NextAveraged(0xB) - 5;
    } else {
        temp_v0 = 0;
    }

    temp_s0 += temp_v0;

    if (temp_s0 < 0) {
        temp_s0 = 0;
    }

    return temp_s0;
}

