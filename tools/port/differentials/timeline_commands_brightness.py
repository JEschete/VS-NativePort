"""F01581 func_800D65D8 (BATTLE.PRG 5BF94.c:8272), typed emitter weapon brightness."""

from timeline_commands_differential import BRIGHTNESS, configure

globals().update(
    configure(
        1581,
        BRIGHTNESS,
        0x800D65D8,
        0x800D6628,
        {
            "always-on": {0x800D65F0: 0x00000000},
            "brightness-reads-target": {0x800D65E0: 0x94830D34},
            "enable-flag-2": {0x800D65F4: 0x24050002},
        },
    )
)
