"""F00085 func_8006F450 (BATTLE.PRG 146C.c:3171), typed Effects::SpawnGroundPlaneImpactEffect."""

from secondary_effects_common import GROUND, configure

globals().update(
    configure(
        85,
        GROUND,
        0x8006F450,
        0x8006F4B0,
        {
            "effect-37": {0x8006F454: 0x24020025},
            "source-kind-5": {0x8006F464: 0x24020005},
            "target-kind-4": {0x8006F46C: 0x24020004},
            "source-actor-5": {0x8006F478: 0xA3A20018},
            "z-from-second": {0x8006F48C: 0x94820002},
            "height-is-z": {0x8006F494: 0xA7A20026},
        },
    )
)
