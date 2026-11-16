"""F00715 vs_battle_renderBattleAbilityTimingResult (BATTLE.PRG 2842C.c:1643), typed Battle::QueueAbilityTimingResult."""

from floating_label_producers_common import TIMING, configure

globals().update(
    configure(
        715,
        TIMING,
        0x80093E64,
        0x80093F24,
        {
            "replaced-kind-5": {0x80093E90: 0x24060005},
            "remove-call": {0x80093EA8: 0x00000000},
            "scan-stride-20": {0x80093ECC: 0x26100014},
            "kind-5": {0x80093EDC: 0x24020005},
            "frame-unstored": {0x80093EE0: 0x00000000},
            "actor-unstored": {0x80093EE8: 0x00000000},
            "value-unstored": {0x80093EF0: 0x00000000},
            "jitter-mask-f": {0x80093EF4: 0x3042000F},
            "publish-two": {0x80093F14: 0x24630002},
        },
    )
)
