"""F01432 func_800CF060 (BATTLE.PRG 5BF94.c:5427), typed effect runtime frame."""

from effect_runtime_differential import UPDATE_RUNTIME, configure

globals().update(
    configure(
        1432,
        UPDATE_RUNTIME,
        0x800CF060,
        0x800CF0E8,
        {
            "flip-kept": {0x800CF088: 0x00000000},
            "secondary-selects-action": {0x800CF094: 0x24040001},
            "camera-always": {0x800CF0C0: 0x00000000},
            "secondary-dropped": {0x800CF0C4: 0x02201025},
            "primary-dropped": {0x800CF0D0: 0x02001025},
        },
    )
)
