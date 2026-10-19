// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Actors/CharacterInit.h"
#include "Actors/Spawning.h"
#include "Items/Equipment.h"
#include "Items/EquipmentInit.h"
#include "Items/Loadout.h"

// decomp: D_800FAB18 (D_800FAB18@src/BATTLE/INITBTL.PRG/18.c)
Actors_CharacterInit Actors_ashleyInitialLoadout = {
    .subType      = 174,
    .reach        = 7,
    .name         = "\x0A\x36\x2B\x2F\x28\x3C\xFA\x06\x1B\x2C\x32\x37\xE7",
    .hp           = 250,
    .mp           = 50,
    .strength     = 100,
    .intelligence = 100,
    .agility      = 100,
    .opaque23        = 1,
    .material        = 2,
    .walkSpeed    = 10,
    .opaque26        = 4,
    .runSpeed     = 20,
    .opaque30        = 4,
    .opaque32        = 3,
    .weapon = {
        .blade = {
            .id           = 14,
            .subId        = 14,
            .weaponModelId        = 16,
            .category     = 2,
            .strength     = 9,
            .intelligence = 1,
            .agility      = -3,
            .currentDp    = 12600,
            .maxDp        = 12600,
            .maxPp        = 136,
            .damageType   = 2,
            .costType     = 2,
            .cost         = 1,
            .range        = {
                .range            = 3,
                .secondRange             = 2,
                .thirdRange             = 3,
                .attackShape      = 1,
            },
            .classes      = {-1, -1, -2, -1, -1, -5},
            .affinities   = {8, 5, 5, -3, -3, 2, 2, 0},
        },
        .grip = {
            .id           = 96,
            .subId        = 1,
            .category     = 1,
            .strength     = 1,
            .agility      = -1,
            .types        = {0, 0, 4, 1},
        },
        .material = 3,
        .name = "\x0F\x24\x31\x27\x24\x31\x2A\x32\xE7",
    },
    .accessory = {
        .equipment = {
            .id           = 223,
            .subId        = 97,
            .category     = 7,
            .intelligence = 1,
            .classes      = {0, 0, 5, -5, -5, 5},
            .affinities   = {5, 0, 0, 0, 0, 5, -5, 0},
        },
    },
    .limbs = {
        {
            .hp = 200,
            .actions = {
                {.actionId = 1},
                {.index = 1},
                {.index = 2},
                {.index = 3},
            },
            .armor = {
                .equipment = {
                    .id           = 191,
                    .subId        = 65,
                    .category     = 5,
                    .strength     = 1,
                    .intelligence = 13,
                    .currentDp    = 3350,
                    .maxDp        = 3350,
                    .types        = {0, 1, 0, 0},
                    .affinities   = {2, 5, 5, -1, -1, -5, -5, 0},
                },
                .material = 2,
                .opaque32    = 1,
            },
            .dpSplash = {100, 0, 0, 5, 0, 0},
        },
        {
            .hp = 200,
            .actions = {
                {.actionId = 1},
                {.index = 1},
                {.index = 2},
                {.index = 3},
            },
            .armor = {
                .equipment = {
                    .id           = 191,
                    .subId        = 65,
                    .category     = 5,
                    .strength     = 1,
                    .intelligence = 13,
                    .currentDp    = 3350,
                    .maxDp        = 3350,
                    .types        = {0, 1, 0, 0},
                    .affinities   = {2, 5, 5, -1, -1, -5, -5, 0},
                 },
                .material = 2,
                .opaque32    = 1,
            },
            .dpSplash = {0, 100, 0, 5, 0, 0},
        },
        {
            .hp = 200,
            .actions = {
                {0},
                {.index = 1},
                {.index = 2},
                {.index = 3},
            },
            .armor = {
                .equipment = {
                    .id           = 143,
                    .subId        = 17,
                    .category     = 2,
                    .strength     = 1,
                    .intelligence = 8,
                    .currentDp    = 3800,
                    .maxDp        = 3800,
                    .affinities = {2, 5, 5, -1, -1, -5, -5, 0},
                },
                .material = 2,
                .opaque32    = 1,
            },
            .dpSplash = {0, 0, 100, 10, 0, 0},
        },
        {
            .hp = 200,
            .actions = {
                {0},
                {.index = 1},
                {.index = 2},
                {.index = 3},
            },
            .armor = {
                .equipment = {
                    .id           = 159,
                    .subId        = 33,
                    .category     = 3,
                    .strength     = 5,
                    .intelligence = 10,
                    .currentDp    = 5500,
                    .maxDp        = 5500,
                    .types        = {0, 1, 1, 0},
                    .affinities   = {2, 5, 5, -1, -1, -5, -5, 0},
                },
                .material = 2,
                .opaque32 = 1,
            },
            .dpSplash = {5, 5, 5, 100, 5, 0},
        },
        {
            .hp = 200,
            .actions = {
                {0},
                {.index = 1},
                {.index = 2},
                {.index = 3},
            },
            .armor = {
                .equipment = {
                    .id           = 175,
                    .subId        = 49,
                    .category     = 4,
                    .strength     = 1,
                    .intelligence = 12,
                    .currentDp    = 4050,
                    .maxDp        = 4050,
                    .affinities   = {2, 5, 5, -1, -1, -5, -5, 0},
                },
                .material = 2,
                .opaque32    = 1,
            },
            .dpSplash = {0, 0, 0, 5, 100, 0},
        },
        {
            .actions = {
                {0},
                {.index = 1},
                {.index = 2},
                {.index = 3},
            },
        },
    },
};

// decomp: D_8004FDD0 (D_8004FDD0@SLUS_010.40)
char Actors_placeholderActorName[20]; /* contents: module image (ModuleData.c) */

// decomp: D_8004FE88 (D_8004FE88@SLUS_010.40)
u_char Actors_usesSecondaryBodyModel[256]; /* contents: module image (ModuleData.c) */

