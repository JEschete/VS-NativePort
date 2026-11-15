"""F01453 func_800CFAAC (BATTLE.PRG 5BF94.c:5770), a line start stepped toward its end."""

from particle_motion_differential import STEP_POINT, configure

globals().update(
    configure(
        1453,
        STEP_POINT,
        0x800CFAAC,
        0x800CFB68,
        {
            "step-direction": {0x800CFAC0: 0x24080001},
            "major-branch": {0x800CFB08: 0x1440000B},
            "minor-half": {0x800CFB14: 0x00041080},
            "y-step": {0x800CFB2C: 0x00481021},
            "equal-check": {0x800CFABC: 0x14620028},
        },
    )
)
