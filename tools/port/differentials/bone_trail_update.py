"""F01418 func_800CE174 (BATTLE.PRG 5BF94.c:4958), the bone trail renderer."""

from bone_trail_differential import UPDATE, configure

globals().update(
    configure(
        1418,
        UPDATE,
        0x800CE174,
        0x800CE644,
        {
            "newest-mask": {0x800CE214: 0x30420003},
            "tip-history": {0x800CE240: 0x24C60044},
            "draw-age": {0x800CE258: 0x28620005},
            "full-spans": {0x800CE26C: 0x24140004},
            "newest-phase": {0x800CE28C: 0x2410F001},
            "fade-life": {0x800CE600: 0x24020007},
            "weight-step": {0x800CE420: 0x2610FFFE},
            "span-phase": {0x800CE4F4: 0x26520400},
            "endless-life": {0x800CE5D8: 0x240200FE},
        },
    )
)
