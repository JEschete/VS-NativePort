"""F00086 func_8006F4B0 (BATTLE.PRG 146C.c:3187), typed Effects::SpawnTrapTileEffect."""

from secondary_effects_common import TRAP, configure

globals().update(
    configure(
        86,
        TRAP,
        0x8006F4B0,
        0x8006F53C,
        {
            "effect-34": {0x8006F4B4: 0x24020022},
            "tile-scale-64": {0x8006F4E8: 0x00021180},
            "centre-0": {0x8006F4EC: 0x24420000},
            "z-from-layer": {0x8006F4F4: 0x90820001},
            "height-from-x": {0x8006F510: 0x94830004},
            "busy-bit-1": {0x8006F514: 0x30420001},
            "busy-inverted": {0x8006F518: 0x10400004},
        },
    )
)
