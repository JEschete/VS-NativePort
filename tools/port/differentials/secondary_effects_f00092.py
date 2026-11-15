"""F00092 func_8006F848 (BATTLE.PRG 146C.c:3298), typed Effects::SpawnAbilityLearnedEffect."""

from secondary_effects_common import LEARNED, configure

globals().update(
    configure(
        92,
        LEARNED,
        0x8006F848,
        0x8006F89C,
        {
            "effect-21": {0x8006F84C: 0x24020015},
            "count-0": {0x8006F858: 0xA3A00012},
            "source-kind-4": {0x8006F85C: 0x24020004},
            "target-kind-5": {0x8006F864: 0x24020005},
            "part-0xFE": {0x8006F86C: 0x240200FE},
            "actor-0xFD": {0x8006F880: 0xA3A20024},
            "parameter-1": {0x8006F874: 0x24050001},
        },
    )
)
