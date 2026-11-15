"""F01505 func_800D2698 (BATTLE.PRG 5BF94.c:6779), typed particle pool acquisition."""

from particle_pool_differential import ACQUIRE, configure

globals().update(
    configure(
        1505,
        ACQUIRE,
        0x800D2698,
        0x800D278C,
        {
            "second-reference-kept": {0x800D26BC: 0x00000000},
            "stride-0x74": {0x800D26E0: 0x24830074},
            "first-previous-kept": {0x800D26EC: 0x00000000},
            "loop-previous-kept": {0x800D2714: 0x00000000},
            "used-kept": {0x800D2734: 0x00000000},
            "capacity-kept": {0x800D2740: 0x00000000},
            "first-references-two": {0x800D2744: 0x24020002},
            "last-next-kept": {0x800D2774: 0x00000000},
            "free-at-last": {0x800D2778: 0xAC4455F4},
        },
    )
)
