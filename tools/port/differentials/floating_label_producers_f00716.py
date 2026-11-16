"""F00716 func_80093F24 (BATTLE.PRG 2842C.c:1666), typed Battle::QueueChainCount."""

from floating_label_producers_common import CHAIN, configure

globals().update(
    configure(
        716,
        CHAIN,
        0x80093F24,
        0x80093FEC,
        {
            "replaced-kind-4": {0x80093F4C: 0x24060004},
            "remove-call": {0x80093F84: 0x00000000},
            "zero-gate-inverted": {0x80093F74: 0x12000007},
            "kind-4": {0x80093FA0: 0x24020004},
            "cap-test-15-bit": {0x80093FA8: 0x32027FFF},
            "cap-at-1001": {0x80093FAC: 0x2C4203E9},
            "cap-value-1000": {0x80093FC0: 0x240203E8},
            "frame-unstored": {0x80093FB0: 0x00000000},
            "actor-unstored": {0x80093FB4: 0x00000000},
            "publish-two": {0x80093FD4: 0x24630002},
        },
    )
)
