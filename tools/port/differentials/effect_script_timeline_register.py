"""F01540 func_800D4DB0 (BATTLE.PRG 5BF94.c:7713), typed register timeline load."""

from effect_script_timeline_differential import REGISTER, configure

globals().update(configure(1540, REGISTER, 0x800D4DB0, 0x800D4E5C, {
    "children-remap": {0x800D4DD4: 0x00000000},
    "ids-at-0x17": {0x800D4E00: 0x90430017},
    "zero-id-remaps": {0x800D4E08: 0x00000000},
    "unsigned-count": {0x800D4E28: 0x0062102B},
    "no-minus-one": {0x800D4E34: 0x24650000},
}))
# The remap's parent and zero-id guards need a parent or zero id beside a remappable value.
MUTANT_VECTORS = 256
