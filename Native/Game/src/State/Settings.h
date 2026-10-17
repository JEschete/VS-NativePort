// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_STATE_SETTINGS_H_TYPES
#define VS_STATE_SETTINGS_H_TYPES
#include "Legacy/Psx.h"

typedef struct State_Settings State_Settings;

// decomp: vs_main_settings_t (src/SLUS_010.40/main.h:163)
typedef struct State_Settings {
    u_char reserved0Bit0 : 4;
    u_char containerEmpty : 1;
    u_char abilityTimingDisplay : 1;
    u_char weaponStatChange : 1;
    u_char armorStatChange : 1;
    char cursorMemory;
    u_short menuFlags;
    u_int slotState;
    char information;
    char simpleMap;
    char monoSound;
    char vibrationOn;
    char mappedChainAbilities[4];
    char mappedDefenseAbilities[4];
    u_int key;
    u_short cumulativeSaveCount;
    u_short currentGameSaveCount;
    u_int saveFileGeneration;
} State_Settings;
_Static_assert(sizeof(State_Settings) == 0x20, "State_Settings must keep its PS1 layout");

#endif // VS_STATE_SETTINGS_H_TYPES
