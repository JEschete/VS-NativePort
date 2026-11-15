"""F01436 func_800CF3F8 (BATTLE.PRG 5BF94.c:5532), typed secondary effect start."""

from effect_runtime_differential import BEGIN_SECONDARY, configure

globals().update(
    configure(
        1436,
        BEGIN_SECONDARY,
        0x800CF3F8,
        0x800CF458,
        {
            "select-action": {0x800CF414: 0x24040001},
            "saved-is-effect": {0x800CF420: 0x248454D8},
            "parameter-dropped": {0x800CF43C: 0x00003021},
            "token-7-bits": {0x800CF44C: 0x3042007F},
        },
    )
)
