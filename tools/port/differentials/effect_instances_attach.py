"""F01445 func_800CF694 (BATTLE.PRG 5BF94.c:5636), typed effect renderer attach."""

from effect_instances_differential import ATTACH, configure

globals().update(
    configure(
        1445,
        ATTACH,
        0x800CF694,
        0x800CF70C,
        {
            "slot-size-0x10": {0x800CF6B0: 0x24040010},
            "update-command": {0x800CF6C8: 0x24050002},
            "parameters-zero": {0x800CF6D0: 0x00003021},
            "newest-unlinked": {0x800CF6D4: 0x00000000},
            "first-kept": {0x800CF6D8: 0x00000000},
            "renderer-unset": {0x800CF6DC: 0x00000000},
            "state-kept": {0x800CF6E8: 0x00000000},
        },
    )
)
