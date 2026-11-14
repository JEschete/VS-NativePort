"""F01568 func_800D61AC (BATTLE.PRG 5BF94.c:8172), typed other actor colour track."""

from timeline_tracks_differential import OTHER, configure

globals().update(
    configure(
        1568,
        OTHER,
        0x800D61AC,
        0x800D6298,
        {
            "others-at-0xc0": {0x800D6214: 0x920400C0},
            "count-from-targets": {0x800D61FC: 0x90620002},
            "visibility-ignored": {0x800D6224: 0x00000000},
            "red-not-centred": {0x800D624C: 0x24E70000},
        },
    )
)
