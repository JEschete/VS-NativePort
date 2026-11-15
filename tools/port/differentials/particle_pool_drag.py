"""F01553 func_800D5294 (BATTLE.PRG 5BF94.c:7828), typed particle drag."""

from particle_pool_differential import DRAG, configure

globals().update(
    configure(
        1553,
        DRAG,
        0x800D5294,
        0x800D52A4,
        {
            "drag-from-next": {0x800D5294: 0x8C830004},
            "drag-kept": {0x800D52A0: 0x00000000},
            "drag-at-gravity": {0x800D52A0: 0xAC435620},
        },
    )
)
