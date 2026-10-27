// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Battle/Action.h"
#include "Battle/ActionResolution.h"
#include "Battle/BattleTypes.h"
#include "Battle/Hit.h"
#include "Core/Random.h"
#include "Items/Recovery.h"

// decomp: BATTLE.PRG _itemRecovery (src/BATTLE/BATTLE.PRG/146C.c:12109)
short Items_CalculateItemRecovery(Battle_ActionDefinition* action, Battle_HitResult* arg1 __attribute__((unused)),
    Battle_HitResult* arg2 __attribute__((unused)), int hit_index, int add_random_variation)
{
    short random_variation;
    short recovery_amount = action->hits[hit_index].statFactor * 5;

    if (add_random_variation != 0) {
        random_variation = Core_Random_NextAveraged(0xB) - 5;
    } else {
        random_variation = 0;
    }

    recovery_amount += random_variation;

    if (recovery_amount < 0) {
        recovery_amount = 0;
    }

    return recovery_amount;
}

