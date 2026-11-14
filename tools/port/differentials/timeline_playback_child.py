"""F01563 func_800D5A98 (BATTLE.PRG 5BF94.c:8005), typed child effect track tick."""

from timeline_playback_differential import CHILD, configure

globals().update(
    configure(
        1563,
        CHILD,
        0x800D5A98,
        0x800D5D74,
        {
            "frame-from-final-actor": {0x800D5ACC: 0x92230006},
            "actor-part-type-5": {0x800D5BE4: 0x24020005},
            "delay-from-frame": {0x800D5C3C: 0x92230007},
            "child-count-4-bits": {0x800D5C90: 0x30427800},
            "final-actor-from-actor-5": {0x800D5D28: 0x92230005},
            "index-plus-2": {0x800D5BC4: 0x24630002},
            "slot-from-byte-4": {0x800D5BF0: 0x90A20004},
        },
    )
)
