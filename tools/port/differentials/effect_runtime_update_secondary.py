"""F01426 func_800CED60 (BATTLE.PRG 5BF94.c:5321), typed secondary effect update."""

from effect_runtime_differential import UPDATE_SECONDARY, configure

globals().update(
    configure(
        1426,
        UPDATE_SECONDARY,
        0x800CED60,
        0x800CEEBC,
        {
            "current-kept": {0x800CED90: 0x00000000},
            "pool-0xfe": {0x800CEDC4: 0x240400FE},
            "starting-to-3": {0x800CEDC8: 0x24040003},
            "renderer-command-1": {0x800CEE18: 0x24050001},
            "detach-skipped": {0x800CEE34: 0x00000000},
            "pool-kept": {0x800CEE60: 0x00000000},
            "frame-kept": {0x800CEE7C: 0x00000000},
            "busy-mask-3": {0x800CEE8C: 0x30420003},
        },
    )
)
