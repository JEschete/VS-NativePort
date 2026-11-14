"""F01572 func_800D63D0 (BATTLE.PRG 5BF94.c:8223), typed source action impact."""

from timeline_commands_differential import SOURCE_IMPACT, configure

globals().update(
    configure(
        1572,
        SOURCE_IMPACT,
        0x800D63D0,
        0x800D6418,
        {
            "source-type-5": {0x800D63E0: 0x24020005},
            "point-misplaced": {0x800D63FC: 0x24A50D34},
        },
    )
)
