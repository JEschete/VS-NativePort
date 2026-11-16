"""F00717 func_80093FEC (BATTLE.PRG 2842C.c:1695), typed Battle::QueueActorStatusLabels."""

from floating_label_producers_common import STATUSES, configure

globals().update(
    configure(
        717,
        STATUSES,
        0x80093FEC,
        0x8009406C,
        {
            "mask-ored": {0x8009401C: 0x02711025},
            "anchor-tag-fa": {0x80094028: 0x240500FA},
            "index-from-bit": {0x8009402C: 0x02203021},
            "flags-dropped": {0x80094034: 0x00003821},
            "last-bit-27": {0x8009403C: 0x2A02001C},
            "bit-step-two": {0x80094044: 0x00118880},
            "return-1": {0x80094060: 0x24020001},
        },
    )
)
