"""F00060 func_8006C84C (BATTLE.PRG 146C.c:2044), typed Effects::QueueActionImpactEffects."""

from secondary_effects_common import IMPACTS, configure

globals().update(
    configure(
        60,
        IMPACTS,
        0x8006C84C,
        0x8006CA20,
        {
            "position-effect-0x27": {0x8006C8E0: 0x24040027},
            "defined-limit-0x29": {0x8006C8EC: 0x2C420029},
            "own-prerequisite-4": {0x8006C8F4: 0x24030004},
            "prerequisite-shift-8": {0x8006C920: 0x00021202},
            "impact-limit-8": {0x8006C944: 0x2C620008},
            "default-limit-0x17": {0x8006C96C: 0x2C420017},
            "default-effect-2": {0x8006C980: 0x24040002},
            "kind-one-effect-0x28": {0x8006C994: 0x24040028},
            "sound-attacker-0": {0x8006C9D8: 0x00002021},
            "target-stride-0x40": {0x8006C9F0: 0x26310040},
            "return-0": {0x8006CA14: 0x24020000},
        },
    )
)
