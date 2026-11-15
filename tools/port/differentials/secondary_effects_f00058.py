"""F00058 func_8006C4C4 (BATTLE.PRG 146C.c:1927), typed Effects::QueueActionHitEffect."""

from secondary_effects_common import HIT, configure

globals().update(
    configure(
        58,
        HIT,
        0x8006C4C4,
        0x8006C5AC,
        {
            "count-2": {0x8006C4E0: 0x24020002},
            "other-count-1": {0x8006C4F0: 0xA3A20013},
            "queue-mask-7": {0x8006C4F8: 0x30630007},
            "source-part-is-actor": {0x8006C524: 0xA3A2001A},
            "position-test-inverted": {0x8006C534: 0x1040000B},
            "position-kind-6": {0x8006C538: 0x24020006},
            "limb-unsigned": {0x8006C550: 0x90E50001},
            "bone-kind-3": {0x8006C558: 0x24060003},
            "position-low-word-from-high": {0x8006C57C: 0xBBA90024},
            "variant-signed": {0x8006C58C: 0x82020002},
            "parameter-1": {0x8006C590: 0x24050001},
        },
    )
)
