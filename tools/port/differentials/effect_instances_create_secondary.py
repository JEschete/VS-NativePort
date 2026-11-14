"""F01443 func_800CF55C (BATTLE.PRG 5BF94.c:5593), typed secondary effect instance creation."""

from effect_instances_differential import CREATE_SECONDARY, configure

globals().update(
    configure(
        1443,
        CREATE_SECONDARY,
        0x800CF55C,
        0x800CF614,
        {
            "record-size-0xd58": {0x800CF568: 0x24040D58},
            "first-priority-signed": {0x800CF59C: 0x80A30009},
            "first-compare-unsigned": {0x800CF5A8: 0x0043102B},
            "ties-first": {0x800CF5AC: 0x14400004},
            "first-not-linked": {0x800CF5BC: 0x00000000},
            "next-priority-signed": {0x800CF5D0: 0x80830009},
            "next-ties-first": {0x800CF5E0: 0x14400004},
            "next-not-followed": {0x800CF5E8: 0x00000000},
        },
    )
)
