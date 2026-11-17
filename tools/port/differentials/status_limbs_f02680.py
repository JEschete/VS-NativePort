"""F02680 original selected condition, actual recursive formatter and text pointers."""

from status_limbs_common import configure

globals().update(
    configure(
        2680,
        {
            "actor-offset-zero": {0x80106338: 0x00000000},
            "limb-stride-half": {0x80106354: 0x00021040},
            "name-from-bone": {0x8010636C: 0x90820005},
            "name-halfword-not-byte-offset": {0x80106384: 0x00000000},
            "name-context-slot-two": {0x80106390: 0xAE020030},
            "condition-index-14": {0x8010639C: 0x2442000E},
            "condition-context-slot-three": {0x801063C4: 0xAE020034},
            "no-format-call": {0x801063C0: 0x00000000},
            "no-information-call": {0x801063CC: 0x00000000},
            "highlight-next-limb": {0x801063DC: 0x24840002},
        },
    )
)
