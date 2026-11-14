"""F02857 _droppableUnderCurrentDifficulty (MENUB.PRG 260.c:1113), typed loot rows."""

from loot_rows_differential import DIFFICULTY_MATCH, configure

globals().update(configure(2857, DIFFICULTY_MATCH, 0x801086DC, 0x801086F4, {
    "difficulty-not-offset": {0x801086E8: 0x00000000},
}))
