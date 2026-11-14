"""F01567 func_800D6048 (BATTLE.PRG 5BF94.c:8144), typed target actor colour track."""

from timeline_tracks_differential import TARGET, configure

globals().update(
    configure(
        1567,
        TARGET,
        0x800D6048,
        0x800D61AC,
        {
            "target-stride-8": {0x800D6120: 0x26100008},
            "loop-bound-other-count": {0x800D6110: 0x92620003},
            "all-targets-flag-ignored": {0x800D6094: 0x00000000},
            "receiver-type-from-emitter": {0x800D612C: 0x96030D28},
            "target-blue-not-centred": {0x800D6104: 0x24420000},
        },
    )
)
