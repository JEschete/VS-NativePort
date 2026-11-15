"""F00064 func_8006CDB8 (BATTLE.PRG 146C.c:2149), typed Effects::SignalActorEffectAnimation."""

from action_callbacks_common import SIGNAL, configure

globals().update(
    configure(
        64,
        SIGNAL,
        0x8006CDB8,
        0x8006CDD8,
        {
            "argument-incremented": {0x8006CDC4: 0x24840001},
            "callee-is-transform": {0x8006CDC0: 0x0C01ECA7},
            "ra-slot-moved": {0x8006CDBC: 0xAFBF0014},
        },
    )
)
