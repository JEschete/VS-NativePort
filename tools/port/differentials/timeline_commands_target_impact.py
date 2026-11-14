"""F01571 func_800D6388 (BATTLE.PRG 5BF94.c:8213), typed target action impact."""

from timeline_commands_differential import TARGET_IMPACT, configure

globals().update(
    configure(
        1571,
        TARGET_IMPACT,
        0x800D6388,
        0x800D63D0,
        {
            "reads-source-type": {0x800D6394: 0x94A30D1C},
            "actor-from-vx": {0x800D63B0: 0x84A40D38},
            "point-passed-for-actor": {0x800D63AC: 0x00000000},
        },
    )
)
