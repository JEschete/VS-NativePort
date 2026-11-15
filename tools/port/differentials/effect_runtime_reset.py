"""F01421 func_800CE67C (BATTLE.PRG 5BF94.c:5054), typed effect instance reset."""

from effect_runtime_differential import RESET, configure

globals().update(
    configure(
        1421,
        RESET,
        0x800CE67C,
        0x800CE714,
        {
            "phase-kept": {0x800CE684: 0x00000000},
            "three-token-words": {0x800CE68C: 0x24040002},
            "started-kept": {0x800CE6A0: 0x00000000},
            "loaded-kept": {0x800CE6B0: 0x00000000},
            "secondary-kept": {0x800CE6C0: 0x00000000},
            "variant-kept": {0x800CE6E8: 0x00000000},
            "menu-reload-kept": {0x800CE6F0: 0x00000000},
        },
    )
)
