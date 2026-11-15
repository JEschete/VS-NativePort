"""F01590 func_800D6CCC (BATTLE.PRG 5BF94.c:8501), the texture animation clear."""

from particle_motion_differential import CLEAR, configure

globals().update(
    configure(
        1590,
        CLEAR,
        0x800D6CCC,
        0x800D6CF0,
        {
            "timer-kept": {0x800D6CD0: 0x00000000},
            "offsets-kept": {0x800D6CD4: 0x00000000},
            "cursor-kept": {0x800D6CD8: 0x00000000},
            "texture-kept": {0x800D6CE0: 0x00000000},
        },
    )
)
