"""F01554 func_800D52A4 (BATTLE.PRG 5BF94.c:7830), the particle motion integrator."""

from particle_motion_differential import INTEGRATE, configure

globals().update(
    configure(
        1554,
        INTEGRATE,
        0x800D52A4,
        0x800D5550,
        {
            "drag-sign": {0x800D52F8: 0x00968821},
            "gravity-shift": {0x800D5340: 0x000712C3},
            "mass-y": {0x800D535C: 0x8642000A},
            "pull-shift": {0x800D5480: 0x000712C3},
            "acceleration-y": {0x800D54B0: 0x8E030028},
            "no-attraction-z": {0x800D5514: 0x00000000},
        },
    )
)
