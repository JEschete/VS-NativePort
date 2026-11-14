"""F01430 func_800CEF64 (BATTLE.PRG 5BF94.c:5397), typed loaded check."""

from primary_effect_differential import IS_LOADED, configure

globals().update(
    configure(
        1430,
        IS_LOADED,
        0x800CEF64,
        0x800CEF74,
        {
            "loaded-byte-returned": {0x800CEF70: 0x00021021},
            "loaded-next-byte": {0x800CEF68: 0x904254A9},
        },
    )
)
