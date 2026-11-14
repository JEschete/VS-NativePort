"""F01424 func_800CE8F4 (BATTLE.PRG 5BF94.c:5139), typed primary effect instance destruction."""

from effect_instances_differential import DESTROY_PRIMARY, configure

globals().update(
    configure(
        1424,
        DESTROY_PRIMARY,
        0x800CE8F4,
        0x800CE9B0,
        {
            "first-kept": {0x800CE920: 0x00000000},
            "middle-kept": {0x800CE92C: 0x00000000},
            "particles-kept": {0x800CE94C: 0x00000000},
            "update-command": {0x800CE964: 0x24050002},
            "renderers-kept": {0x800CE97C: 0x00000000},
            "first-renderer-only": {0x800CE988: 0x00000000},
            "instance-kept": {0x800CE990: 0x00000000},
        },
    )
)
