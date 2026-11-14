"""F01566 func_800D5F8C (BATTLE.PRG 5BF94.c:8128), typed source actor colour track."""

from timeline_tracks_differential import SOURCE, configure

globals().update(
    configure(
        1566,
        SOURCE,
        0x800D5F8C,
        0x800D6048,
        {
            "emit-from-type-5": {0x800D5FD4: 0x24020005},
            "visible-bit-0": {0x800D5FEC: 0x30420001},
            "green-not-centred": {0x800D600C: 0x24420000},
            "frame-from-timeline-flags": {0x800D5FC0: 0x84420000},
            "actor-from-receiver": {0x800D6000: 0x92260D44},
        },
    )
)
