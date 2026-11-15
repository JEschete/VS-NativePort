"""F01425 func_800CE9B0 (BATTLE.PRG 5BF94.c:5170), typed primary effect update."""

from effect_runtime_differential import UPDATE_PRIMARY, configure

globals().update(
    configure(
        1425,
        UPDATE_PRIMARY,
        0x800CE9B0,
        0x800CED60,
        {
            "purge-wait-skipped": {0x800CEA18: 0x00000000},
            "configure-skipped": {0x800CEA20: 0x00000000},
            "effect-without-file-loads": {0x800CEA50: 0x24040002},
            "purge-kept": {0x800CEA90: 0x00000000},
            "plugins-play": {0x800CEAC8: 0x24040004},
            "pending-from-1": {0x800CEB14: 0x24040001},
            "track-count-kept": {0x800CEB2C: 0x00000000},
            "loaded-kept": {0x800CEB64: 0x00000000},
            "frame-kept": {0x800CEBFC: 0x00000000},
            "awaiting-swapped": {0x800CEC30: 0x0082102A},
            "token-kept": {0x800CECD4: 0x00000000},
            "camera-reset-skipped": {0x800CECE4: 0x00000000},
            "reload-state-5": {0x800CED10: 0x24020005},
            "idle-busy": {0x800CED38: 0x24120001},
        },
    )
)
