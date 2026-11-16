"""F00738 equipment stat-change producer (func_80096768) against its complete original instructions and reserve."""

from differentials.floating_placement_common import QUEUE, configure

globals().update(
    configure(
        738,
        QUEUE,
        0x80096768,
        0x8009695C,
        {
            "weapon-setting-bit": {0x80096794: 0x30620020},
            "ring-maps-to-four": {0x80096844: 0x24100004},
            "stack-counts-pending-only": {0x80096880: 0x2C420001},
            "queued-kind-six": {0x800968AC: 0x24020006},
            "actor-seven-e": {0x800968B4: 0x2402007E},
            "clamp-at-one-thousand": {0x800968C0: 0x286203E9},
            "frame-not-cleared": {0x800968CC: 0x00000000},
            "not-published": {0x80096930: 0x24420000},
        },
    )
)
