"""F01442 func_800CF514 (BATTLE.PRG 5BF94.c:5582), typed effect token release."""

from primary_effect_differential import RELEASE, configure

globals().update(
    configure(
        1442,
        RELEASE,
        0x800CF514,
        0x800CF55C,
        {
            "release-keeps-only-bit": {0x800CF54C: 0x00021025},
            "release-word-16": {0x800CF52C: 0x00031903},
            "release-bit-mod-16": {0x800CF538: 0x00031900},
        },
    )
)
