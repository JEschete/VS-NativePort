"""F01589 func_800D6AEC (BATTLE.PRG 5BF94.c:8442), typed effect timeline tick."""

from timeline_playback_differential import DISPATCH, configure

globals().update(
    configure(
        1589,
        DISPATCH,
        0x800D6AEC,
        0x800D6CCC,
        {
            "finished-test-inverted": {0x800D6C98: 0x10400005},
            "all-targets-flag-bit-0": {0x800D6BF0: 0x30C60001},
            "retires-bit-1": {0x800D6C54: 0x00021040},
            "frame-plus-2": {0x800D6CA8: 0x24420002},
            "track-stride-8": {0x800D6B40: 0x001110C0},
            "kind-13-untable": {0x800D6B58: 0x2C62000C},
        },
    )
)
