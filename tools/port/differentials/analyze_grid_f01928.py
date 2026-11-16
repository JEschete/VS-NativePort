"""F01928: independently execute PLG055.BIN's Analyze grid and its Live binding."""

from analyze_grid_common import case

globals().update(case(1928, 2, "EFFECT/PLG055.BIN", 0x800F9818, 0x800FA368))
