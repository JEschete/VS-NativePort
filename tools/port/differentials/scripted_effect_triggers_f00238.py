"""F00238 func_8007BE20 (BATTLE.PRG 146C.c:8581), typed Effects::StartScriptedActorEffect."""

from scripted_effect_triggers_common import START, configure

globals().update(
    configure(
        238,
        START,
        0x8007BE20,
        0x8007BEC4,
        {
            "source-kind-6": {0x8007BE40: 0x24020006},
            "target-kind-3": {0x8007BE48: 0x24020003},
            "effect-unwritten": {0x8007BE50: 0x00000000},
            "source-y-unwritten": {0x8007BE5C: 0x00000000},
            "target-actor-zero": {0x8007BE68: 0xA3A00024},
            "target-part-unwritten": {0x8007BE6C: 0x00000000},
            "target-included": {0x8007BE80: 0x00000000},
            "slots-15": {0x8007BE94: 0x2862000F},
            "count-unwritten": {0x8007BEA8: 0x00000000},
            "variant-1": {0x8007BEA4: 0x24050001},
        },
    )
)
