"""F01579 func_800D6574 (BATTLE.PRG 5BF94.c:8259), typed source actor to signal."""

from timeline_commands_differential import SIGNAL, configure

globals().update(
    configure(
        1579,
        SIGNAL,
        0x800D6574,
        0x800D65A8,
        {
            "signal-reads-target-type": {0x800D657C: 0x94830D34},
            "signal-reads-part": {0x800D658C: 0x90840D22},
        },
    )
)
