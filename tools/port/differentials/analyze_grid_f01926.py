"""F01926: independently execute PLG054.BIN's Analyze grid and its Live binding."""

from analyze_grid_common import case

globals().update(case(1926, 1, "EFFECT/PLG054.BIN", 0x800F9818, 0x800FA368))
