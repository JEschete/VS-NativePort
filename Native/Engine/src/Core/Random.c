// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Core/Random.h"

// decomp: _keystreamState (_keystreamState@BATTLE/BATTLE.PRG)
u_int Core_interfaceRandomState; /* data: contents loaded in Step 5 */

// decomp: randArr (randArr@SLUS_010.40)
int Core_randomShuffleTable[97];

// decomp: randIndex (randIndex@SLUS_010.40)
int Core_randomShuffleIndex;

// decomp: BATTLE.PRG vs_battle_keystreamBits (src/BATTLE/BATTLE.PRG/5BF94.c:4496)
u_int Core_Random_NextInterfaceRandomBits(int value)
{
    u_int temp_a1 = Core_interfaceRandomState;
    Core_interfaceRandomState = temp_a1 * 0x19660D;
    return temp_a1 >> (32 - value);
}

// decomp: BATTLE.PRG vs_battle_randUniformInt (src/BATTLE/BATTLE.PRG/5BF94.c:5810)
int Core_Random_RandomRangeExclusive(int arg0, int arg1)
{
    if (arg0 != arg1) {
        int new_var;

        if (arg1 < arg0) {
            new_var = Core_Random_Rand() % (arg0 - arg1);
            return new_var + arg1;
        }

        new_var = Core_Random_Rand() % (arg1 - arg0);
        return new_var + arg0;
    }

    return arg0;
}

// decomp: SLUS_010.40 _initRand (src/SLUS_010.40/main.c:8239)
void Core_Random_InitializeShuffleTable(void)
{
    int i;

    Core_Random_Seed(1);

    for (i = 0; i < (int)(sizeof Core_randomShuffleTable / sizeof Core_randomShuffleTable[0]); ++i) {
        Core_randomShuffleTable[i] = Core_Random_Rand();
    }
    Core_randomShuffleIndex = (sizeof Core_randomShuffleTable / sizeof Core_randomShuffleTable[0]) - 1;
}

// decomp: SLUS_010.40 vs_main_getRand (src/SLUS_010.40/main.c:8251)
int Core_Random_NextBounded(int max)
{
    int temp_s0;
    int var_a0;
    int var_v0;

    var_v0 = Core_randomShuffleTable[Core_randomShuffleIndex] * (sizeof Core_randomShuffleTable / sizeof Core_randomShuffleTable[0]);

    if (var_v0 < 0) {
        var_v0 += 0x7FFF;
    }

    Core_randomShuffleIndex = var_v0 >> 0xF;

    temp_s0 = 0x7FFF - Core_randomShuffleTable[Core_randomShuffleIndex];
    Core_randomShuffleTable[Core_randomShuffleIndex] = Core_Random_Rand();

    var_a0 = temp_s0 * max;

    if (var_a0 < 0) {
        var_a0 += 0x7FFF;
    }

    return var_a0 >> 0xF;
}

// decomp: SLUS_010.40 vs_main_getRandSmoothed (src/SLUS_010.40/main.c:8277)
int Core_Random_NextAveraged(int arg0)
{
    return (Core_Random_NextBounded(arg0) + Core_Random_NextBounded(arg0) + Core_Random_NextBounded(arg0)
               + Core_Random_NextBounded(arg0) + Core_Random_NextBounded(arg0) + Core_Random_NextBounded(arg0)
               + Core_Random_NextBounded(arg0) + Core_Random_NextBounded(arg0) + Core_Random_NextBounded(arg0)
               + Core_Random_NextBounded(arg0) + Core_Random_NextBounded(arg0) + Core_Random_NextBounded(arg0))
         / 12;
}

