"""F01423 func_800CE83C (BATTLE.PRG 5BF94.c:5114), typed primary effect instance creation."""

from effect_instances_differential import CREATE_PRIMARY, configure

globals().update(
    configure(
        1423,
        CREATE_PRIMARY,
        0x800CE83C,
        0x800CE8F4,
        {
            "record-size-0xd58": {0x800CE848: 0x24040D58},
            "first-priority-signed": {0x800CE87C: 0x80A30009},
            "first-compare-unsigned": {0x800CE888: 0x0043102B},
            "ties-first": {0x800CE88C: 0x14400004},
            "first-not-linked": {0x800CE89C: 0x00000000},
            "next-priority-signed": {0x800CE8B0: 0x80830009},
            "next-ties-first": {0x800CE8C0: 0x14400004},
            "next-not-followed": {0x800CE8C8: 0x00000000},
        },
    )
)
