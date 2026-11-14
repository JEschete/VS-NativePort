"""F01533 func_800D4B90 (BATTLE.PRG 5BF94.c:7645), typed branch on the particle count."""

from effect_script_opcode_differential import PARTICLE_EQUAL, configure

globals().update(configure(1533, PARTICLE_EQUAL, 0x800D4B90, 0x800D4BD0, {
    "reads-priority": {0x800D4BA8: 0x92040009},
    "branch-unless-equal": {0x800D4BB8: 0x0004202B},
}))
