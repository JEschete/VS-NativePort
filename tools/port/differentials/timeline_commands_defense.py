"""F01586 func_800D6860 (BATTLE.PRG 5BF94.c:8335), typed defense timing target."""

from timeline_commands_differential import DEFENSE, configure

globals().update(
    configure(
        1586,
        DEFENSE,
        0x800D6860,
        0x800D6894,
        {
            "defense-reads-emit-from": {0x800D6868: 0x94830D28},
            "defense-reads-target-actor": {0x800D6878: 0x90840D38},
        },
    )
)
