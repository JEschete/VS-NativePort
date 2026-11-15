"""F01435 func_800CF218 (BATTLE.PRG 5BF94.c:5479), typed secondary effect spawn."""

from effect_runtime_differential import SPAWN_SECONDARY, configure

globals().update(
    configure(
        1435,
        SPAWN_SECONDARY,
        0x800CF218,
        0x800CF3F8,
        {
            "script-stride-5": {0x800CF284: 0x00031080},
            "toward-range-9": {0x800CF308: 0x2C420009},
            "priority-type-next": {0x800CF34C: 0x96620012},
            "parameter-token": {0x800CF360: 0xACB60D58},
            "busy-effect-55": {0x800CF374: 0x24020037},
            "busy-bits-1": {0x800CF3A0: 0x34420100},
            "token-7-bits": {0x800CF3C8: 0x32C2007F},
        },
    )
)
