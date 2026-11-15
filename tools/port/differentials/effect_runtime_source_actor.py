"""F01449 func_800CF8BC (BATTLE.PRG 5BF94.c:5697), typed source actor effect."""

from effect_runtime_differential import SOURCE_ACTOR, configure

globals().update(
    configure(
        1449,
        SOURCE_ACTOR,
        0x800CF8BC,
        0x800CF920,
        {
            "effect-57": {0x800CF8C0: 0x24020039},
            "actor-next-byte": {0x800CF8DC: 0x90435239},
            "target-actor-type": {0x800CF8E0: 0x24020004},
            "parameter-1": {0x800CF8E8: 0x24050001},
            "target-y-kept": {0x800CF8FC: 0x00000000},
            "other-count-kept": {0x800CF904: 0x00000000},
        },
    )
)
