"""F00718 func_8009406C (BATTLE.PRG 2842C.c:1708), typed Battle::QueueActorStatusLabel."""

from floating_label_producers_common import STATUS, configure

globals().update(
    configure(
        718,
        STATUS,
        0x8009406C,
        0x800941FC,
        {
            "scan-kind-1": {0x800940C8: 0x24070001},
            "scan-any-actor": {0x800940EC: 0x00000000},
            "scan-counts-hidden": {0x80094100: 0x00000000},
            "frame-lead-13": {0x80094104: 0x2452000D},
            "frame-from-limb": {0x80094108: 0x9093FFFF},
            "anchor-tag-fa": {0x80094138: 0x240500FA},
            "screen-right-319": {0x80094158: 0x2C420140},
            "screen-bottom-239": {0x8009416C: 0x2C4200F0},
            "color-mask-ff": {0x80094178: 0x329400FF},
            "frame-limit-36": {0x80094184: 0x2A420024},
            "x-unstored": {0x80094188: 0x00000000},
            "limb-unstored": {0x800941B4: 0x00000000},
            "value-unstored": {0x800941C4: 0x00000000},
            "publish-two": {0x800941C8: 0x24630002},
        },
    )
)
