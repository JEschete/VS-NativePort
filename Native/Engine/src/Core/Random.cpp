// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
extern "C" {
#include "Legacy/Psx.h"
#include "Core/Random.h"

// decomp: _keystreamState (_keystreamState@BATTLE/BATTLE.PRG)
u_int Core_interfaceRandomState; /* contents: module image (ModuleData.cpp) */

// decomp: randArr (randArr@SLUS_010.40)
int Core_randomShuffleTable[97];

// decomp: randIndex (randIndex@SLUS_010.40)
int Core_randomShuffleIndex;

// decomp: BATTLE.PRG vs_battle_keystreamBits (src/BATTLE/BATTLE.PRG/5BF94.c:4496)
u_int Core_Random_NextInterfaceRandomBits(int bit_count)
{
    u_int previous_state = Core_interfaceRandomState;
    Core_interfaceRandomState = previous_state * 0x19660D;
    return previous_state >> (32 - bit_count);
}

// decomp: BATTLE.PRG vs_battle_randUniformInt (src/BATTLE/BATTLE.PRG/5BF94.c:5810)
int Core_Random_RandomRangeExclusive(int bound_a, int bound_b)
{
    if (bound_a != bound_b) {
        int offset;

        if (bound_b < bound_a) {
            offset = Core_Random_Rand() % (bound_a - bound_b);
            return offset + bound_b;
        }

        offset = Core_Random_Rand() % (bound_b - bound_a);
        return offset + bound_a;
    }

    return bound_a;
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
    int picked_sample;
    int scaled_result;
    int scaled_index;

    scaled_index = Core_randomShuffleTable[Core_randomShuffleIndex] * (sizeof Core_randomShuffleTable / sizeof Core_randomShuffleTable[0]);

    if (scaled_index < 0) {
        scaled_index += 0x7FFF;
    }

    Core_randomShuffleIndex = scaled_index >> 0xF;

    picked_sample = 0x7FFF - Core_randomShuffleTable[Core_randomShuffleIndex];
    Core_randomShuffleTable[Core_randomShuffleIndex] = Core_Random_Rand();

    scaled_result = picked_sample * max;

    if (scaled_result < 0) {
        scaled_result += 0x7FFF;
    }

    return scaled_result >> 0xF;
}

// decomp: SLUS_010.40 vs_main_getRandSmoothed (src/SLUS_010.40/main.c:8277)
int Core_Random_NextAveraged(int max)
{
    return (Core_Random_NextBounded(max) + Core_Random_NextBounded(max) + Core_Random_NextBounded(max)
               + Core_Random_NextBounded(max) + Core_Random_NextBounded(max) + Core_Random_NextBounded(max)
               + Core_Random_NextBounded(max) + Core_Random_NextBounded(max) + Core_Random_NextBounded(max)
               + Core_Random_NextBounded(max) + Core_Random_NextBounded(max) + Core_Random_NextBounded(max))
         / 12;
}

}  // extern "C"
