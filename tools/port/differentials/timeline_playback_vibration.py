"""F01565 func_800D5E00 (BATTLE.PRG 5BF94.c:8081), typed vibration track tick."""

from timeline_playback_differential import VIBRATION, configure

globals().update(
    configure(
        1565,
        VIBRATION,
        0x800D5E00,
        0x800D5F8C,
        {
            "due-8-bits": {0x800D5E30: 0x30A200FF},
            "actor-part-type-5": {0x800D5E88: 0x24020005},
            "target-actor-next-byte": {0x800D5EB0: 0x90820D39},
            "action-target-actor-next-byte": {0x800D5F00: 0x90620015},
            "pattern-4-bits": {0x800D5F5C: 0x3042001E},
            "source-actor-next-byte": {0x800D5E94: 0x90820D21},
        },
    )
)
