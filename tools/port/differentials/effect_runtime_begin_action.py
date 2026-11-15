"""F01433 func_800CF0E8 (BATTLE.PRG 5BF94.c:5444), typed action effect start."""

from effect_runtime_differential import BEGIN_ACTION, configure

globals().update(
    configure(
        1433,
        BEGIN_ACTION,
        0x800CF0E8,
        0x800CF1A8,
        {
            "tail-kept": {0x800CF138: 0x00000000},
            "variant-unconditional": {0x800CF13C: 0x00000000},
            "source-5": {0x800CF148: 0x24020005},
            "argument-dropped": {0x800CF15C: 0x00000000},
            "token-dropped": {0x800CF170: 0x00000000},
            "effect-camera-skipped": {0x800CF184: 0x00000000},
            "phase-5": {0x800CF190: 0x24040005},
        },
    )
)
