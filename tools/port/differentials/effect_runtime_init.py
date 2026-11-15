"""F01420 func_800CE64C (BATTLE.PRG 5BF94.c:5047), typed effect runtime start-up."""

from effect_runtime_differential import INIT, configure

globals().update(
    configure(
        1420,
        INIT,
        0x800CE64C,
        0x800CE67C,
        {
            "reset-skipped": {0x800CE654: 0x00000000},
            "resources-skipped": {0x800CE65C: 0x00000000},
            "pool-references-skipped": {0x800CE664: 0x00000000},
        },
    )
)
