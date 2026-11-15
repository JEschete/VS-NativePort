"""F01592 func_800D6D24 (BATTLE.PRG 5BF94.c:8521), the texture animation step."""

from particle_motion_differential import ADVANCE, configure

globals().update(
    configure(
        1592,
        ADVANCE,
        0x800D6D24,
        0x800D6E24,
        {
            "timer-step": {0x800D6D34: 0x2442FFFF},
            "hold-mask": {0x800D6DC4: 0x304200FF},
            "offset-x-shift": {0x800D6DEC: 0x000514C0},
            "frame-cursor": {0x800D6DD8: 0x24630008},
            "texture-table": {0x800D6DC0: 0x8C64009C},
            "offset-y-shift": {0x800D6E00: 0x00021443},
        },
    )
)
