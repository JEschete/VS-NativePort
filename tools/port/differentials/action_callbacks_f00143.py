"""F00143 func_80074294 (BATTLE.PRG 146C.c:5064), typed Effects::ActionEffectBusyFlags."""

from action_callbacks_common import BUSY, configure

globals().update(
    configure(
        143,
        BUSY,
        0x80074294,
        0x800742A4,
        {
            "mask-1": {0x800742A0: 0x30420001},
            "mask-7": {0x800742A0: 0x30420007},
            "busy-from-next-word": {0x80074298: 0x8C42186C},
        },
    )
)
