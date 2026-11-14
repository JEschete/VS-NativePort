"""F01422 func_800CE714 (BATTLE.PRG 5BF94.c:5074), typed effect instance start."""

from effect_instances_differential import INIT, configure

globals().update(
    configure(
        1422,
        INIT,
        0x800CE714,
        0x800CE83C,
        {
            "particle-count-kept": {0x800CE718: 0x00000000},
            "priority-from-0x11": {0x800CE720: 0x90820011},
            "child-count-kept": {0x800CE73C: 0x3463FFFF},
            "token-from-0x21": {0x800CE748: 0x90820021},
            "fifteen-transforms": {0x800CE7A0: 0x28E2000F},
            "transform-stride-0xcc": {0x800CE7A8: 0x24C600CC},
            "emit-to-from-target": {0x800CE80C: 0x8C820008},
            "renderers-kept": {0x800CE830: 0x00000000},
            "parameter-kept": {0x800CE838: 0x00000000},
        },
    )
)
